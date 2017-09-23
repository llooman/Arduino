/*   
   Hysteresis.cpp 
   Copyright (c) 2013-2013 Luc Looman.  All right reserved.
   Rev 1.0 - May 26th, 2013
*/

//#define DEBUG

#include <Arduino.h> 
#include <inttypes.h>
#include <ArdUtils.h>

void EEProm::readAll()
{
	setSample( readLong(offsetof(EEProm, samplePeriode_sec)));
	bootCount  = readLong(offsetof(EEProm, bootCount))+1;
	write(offsetof(EEProm, bootCount), bootCount);
}
void EEProm::writeAll( )
{
	write(offsetof(EEProm, samplePeriode_sec), samplePeriode_sec);
	write(offsetof(EEProm, bootCount), bootCount);
}


int EEProm::handleRequest(RxItem *rxItem)  // cmd, to, parm1, parm2  // TODO
{
	switch (rxItem->data.msg.cmd)
	{
	case 'N':
		bootMessages=0;
		break;

	case 'b':
		bootCount=0;
		changed=true;
		break;
	case 'B':
			#ifdef DEBUG
				Serial.println(F("EEProm handleParentReq B"));
			#endif
		//parent->hang=true;
		hang = true;
		break;

 	default: return -1; break;
	}
	return 0;
}

void EEProm::loop()
{
	if(hang)
	{
		if( ! user_onReBoot
		 || user_onReBoot()
		){
			wdt_enable(WDTO_15MS);
			while(1 == 1)
				delay(500);
		}
	}
}

int EEProm::setVal( int id, long value ) //bool sending, int id,
{
	switch(id)
	{
	case 0:
		bootCount=0;
		changed=true;
		//write(offsetof(EEProm, bootCount), bootCount);
		break;
	case 2: setSample(   value ); 			break;
 	case 50:digitalWrite(LED_BUILTIN,(value>0));break;
 	default: return -1; break;
	}
	return 0;
}

int EEProm::upload(int id)
{
	if(uploadFunc==0 ) return 0;

	int ret=0;

	switch( id )
	{
	case 1:ret = uploadFunc(id, millis(), 0 ); 		break;
	case 2:ret = uploadFunc(id, samplePeriode_sec, 0 ); 		break;
	case 5:ret = uploadFunc(id, 0, 0 );					break;   //ping
	case 50:ret = uploadFunc(id, digitalRead(LED_BUILTIN), 0 );	break;
	case 92:ret = uploadFunc(id, TWBR, 0);						break;   // twi speed
	default: return ret;										break;
	}
	return ret;
}

int EEProm::getVal( int id, long *value ) //bool sending, int id,
{
	switch(id)
	{
	//case 0: *value = bootCount ;   break;
	case 2: *value = samplePeriode_sec ;   break;

	case 5: *value = 0 ;  break;

	case 50: *value = digitalRead(LED_BUILTIN) ;   break;
	case 92: *value = TWBR;   break;

 	default: return -1;	break;
	}
	return 1;
}

long EEProm::readLong(int offSet)
{
	long tempLong;
	EEPROM_readAnything(offSet, tempLong);
	return tempLong;
}
byte EEProm::readByte(int offSet)
{
	byte _byte;
	EEPROM_readAnything(offSet, _byte);
	return _byte;
}
int EEProm::readInt(int offSet)
{
	int tempInt;
	EEPROM_readAnything(offSet, tempInt);
	return tempInt;
}
bool EEProm::readBool(int offSet  )
{
	bool tempBool;
	EEPROM_readAnything(offSet, tempBool);
	return tempBool;
}
float EEProm::readFloat(int offSet  )
{
	float tempFloat;
	EEPROM_readAnything(offSet, tempFloat);
	return tempFloat;
}
/*void myEEwrite(int address, byte val)
{
  if (EEPRO.read(address) == val) return;  // if the value already exist, no need to write.
  EEPROM.write(address.value);
}*/
void EEProm::write(int offSet, long newVal )
{
	long tempLong;
	EEPROM_readAnything(offSet, tempLong);
	if( newVal != tempLong )EEPROM_writeAnything(offSet, newVal);
}
void EEProm::write(int offSet, unsigned long newVal )
{
	unsigned long tempLong;
	EEPROM_readAnything(offSet, tempLong);
	if( newVal != tempLong )EEPROM_writeAnything(offSet, newVal);
}
void EEProm::write(int offSet, int newVal )
{
	int tempInt;
	EEPROM_readAnything(offSet, tempInt);
	if( newVal != tempInt )EEPROM_writeAnything(offSet, newVal);
}
void EEProm::write(int offSet, bool newVal )
{
	bool tempBool;
	EEPROM_readAnything(offSet, tempBool);
	if( newVal != tempBool )EEPROM_writeAnything(offSet, newVal);
}
void EEProm::write(int offSet, float newVal )
{
	float tempFloat;
	EEPROM_readAnything(offSet, tempFloat);
	if( newVal != tempFloat )EEPROM_writeAnything(offSet, newVal);
}


ArdUtils::ArdUtils(  ) {
  _pin = LED_BUILTIN;
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
}

ArdUtils::ArdUtils(int pin )
{
	setPin(pin);
}
void ArdUtils::setPin(int pin)
{
	  _pin = pin;
	  pinMode(pin, OUTPUT);
	  digitalWrite(_pin, LOW);
}
void ArdUtils::setLedPin(int pin)
{
	  _pin = pin;
	  pinMode(pin, OUTPUT);
	  digitalWrite(_pin, LOW);
}
void ArdUtils::loop()
{
	if(hang)
	{
//		if( ! user_onReBoot
//		 || user_onReBoot()
//		){
			wdt_enable(WDTO_15MS);
			while(1 == 1)
				delay(500);
//		}
	}

	flickerLoop();
}

int ArdUtils::handleRequest(RxItem *rxItem)  // cmd, to, parm1, parm2  // TODO
{
	switch (rxItem->data.msg.cmd)
	{
	case 'F':
		flicker();
		break;
	case 'B':
		hang = true;
		break;
	default:
		return -1;
		break;
	}
	return 0;
}


void ArdUtils::flickerLoop()
{
	if( flickerTimer > millis() || flickerTimer == 0) return;

	//Serial.print(F("flickerLoop cnt="));  Serial.print(flickerCount); Serial.print(F(" timer="));  Serial.println(flickerTimer/1000);

	if(flickerCount<1)
	{
		digitalWrite(_pin, LOW);
		flickerTimer = 0;
		return;
	}

	int currStats = digitalRead(_pin);

	flickerCount--;
	digitalWrite(_pin, currStats == HIGH ? LOW : HIGH);
	flickerTimer = millis() + (flickerLengthMs);

}

void ArdUtils::setFlicker( int count, int lengthMs)
{
	//Serial.print(F("setFlicker cnt="));  Serial.print(count); Serial.print(F(" length="));  Serial.println(lengthMs );
	flickerCount = count;
	flickerLengthMs = lengthMs;
	flickerTimer = millis() + ( flickerLengthMs);
	digitalWrite(_pin, HIGH);
}

void ArdUtils::flicker( )
{
	setFlicker(3, 100);
}

void ArdUtils::flicker(int times)
{
	setFlicker(times, 100);
}

void ArdUtils::slowFlicker(int times){

  for(int i =1 ;i<times; i++)  {
    digitalWrite(_pin, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(250 );               // wait for a second
    digitalWrite(_pin, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(250 );               // wait for a second
  }
}
void ArdUtils::hexPrint(int val )  {
  String hexValue =  String(val, HEX);
  if ( hexValue.length() == 1 )
    hexValue = "0" + hexValue;
  Serial.print(hexValue);
}

int System::ramFree () {
  extern int __heap_start, *__brkval; 
  int v;
  int a = (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
  return a;
}

int System::ramSize() {
  int v;
  int a = (int) &v;  
  return a;
}



