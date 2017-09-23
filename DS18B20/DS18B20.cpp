
#include <Arduino.h>
#include <DS18B20.h>


void DS18B20::upload()
{
	if(uploadFunc==0 || retCode < 1 ) return;

	if( uploadFunc(id, temp, timeStamp ) > 0)
	{
		syncTimer = millis() + syncInterval ;
		prevTemp = temp;
		//available = 0;
	}
	else
	{
		syncTimer = millis() + id ;
	}
}


void DS18B20::loop(void)  //  0 when nothing changed,
//  1 when a new valid value was found.
//   < 0 error -1 when an error occurred
{

	if( uploadFunc!=0
	 && ( millis() > syncTimer
	   || prevTemp != temp)
	){
		upload() ;
	}

	if( errorFunc!=0
	 && lastError!=lastErrorUploaded)
	{
		if(errorFunc(id, lastError) >= 0 )
		{
			lastErrorUploaded = lastError;
		}
	}

	// release lock after 3 seconds
	if(staticReadTimer > 0 && millis() > staticReadTimer + 3000)
	{
		staticReadTimer = 0;
		readTempTimer = 0;
		senseTimer = millis() + senseInterval;
		lastError = ERR_DS18B20_STATIC_TIMER_RELEASED;
		#ifdef DEBUG
				Serial.println(F("reset staticReadTimer"));
		#endif
	}


	// sence ( prepare for read)
	if( millis() > senseTimer
	 && staticReadTimer == 0)
	{
		prevTemp = temp;
		senseTimer = millis() + senseInterval;

		retCode = senseTemp();

		#ifdef DEBUG
			Serial.print(F("ds sense id=")); Serial.println(id);
		#endif

		if(retCode < 0)
		{
			setErrorCnt(true);
			lastError = retCode;
			senseTimer = millis() + 7000L;	// quickly retry

			#ifdef DEBUG
				Serial.print(F("ds sense err=")); Serial.println(retCode);
			#endif

			//return;
		}
		else
		{
			if(async)
			{
				readTempTimer = millis() + readDelay_ms;
			}
			else
			{
				delay(readDelay_ms);
				readTempTimer = millis();
			}

			readStart = millis();
			staticReadTimer = readTempTimer;	// lock the library for this instance
		}
	}


	// read a previous sense
	if( readTempTimer > 0
	 && readTempTimer < millis() )
	{
		//Serial.print(F("read id ")); Serial.println(id);

		retCode = readTemp();

		readTempTimer = 0;
		staticReadTimer = readTempTimer;					// release lock

		if(retCode >= 0)
		{
			if(  ( tempMax != 0 && temp > tempMax)
			  || ( tempMin != 0 && temp < tempMin)
			  || ( deltaTempMax > 0 && prevTemp > -27400
			     && (temp > prevTemp + deltaTempMax
			        || temp < prevTemp - deltaTempMax) )
			  )
			{
				retCode = ERR_DS18B20_INVALID_TEMP;  // issue with new value found
			}
		}

		if(retCode < 0)
		{
			#ifdef DEBUG
					Serial.print (F("ds read err="));Serial.println(retCode);
			#endif

			lastError = retCode;
			setErrorCnt(true);
			senseTimer = ( errorCnt<2 ) ? millis() + 5 : millis() + senseInterval;
		}
		else
		{
			#ifdef DEBUG
						Serial.print("@");Serial.print(millis()/1000); Serial.print (F("sense temp="));Serial.println(temp);
			#endif

			//if(prevTemp != temp ) available=1;  //( ret >= 0 );//&& temp != prevTemp);

			timeStamp = readStart;

			setErrorCnt(false);

			senseTimer = millis() + senseInterval;

			// don't wait
			long syncTimeToGo = syncTimer - senseTimer;
			if(syncTimeToGo < senseInterval && syncTimeToGo > 10L)
			{
				syncTimer = millis();
			}
		}
	}
}

void DS18B20::loop(int retries)
{
	bool async = this->async;
	this->async = false;
	senseTimer = 0L;

	for(int i=retries; i>0;i--)
	{
		loop();

		if(retCode<0) delay(50); else break;
	}

	senseTimer = millis() + senseInterval;
	this->async = async;
}

int DS18B20::senseTemp()
{
	if(!initAdress)
	{
		listAdres(false); // just pick one of the adresses found. If only one connected this is it!
		if( addr[0] == 0x00)
		{
			return ERR_DS18B20_NOT_FOUND;
		}
		else
		{
			if (OneWire::crc8(addr, 7) != addr[7])
			{
			    return ERR_DS18B20_ADDRESS_CRC;
			}
			if (addr[0] != 0x28)
			{
			    return ERR_DS18B20_ADDRESS_0X28;
			}
			initAdress = true;
		}
	}


	if(OneWire::reset() != 1) return ERR_DS18B20_NOT_FOUND;


	OneWire::select(addr);


	if(resolution != 12)
	{
		OneWire::write(0x4e, 1);
		OneWire::write(0x00, 1);
		OneWire::write(0x00, 1);
		if(resolution == 9)
		OneWire::write(0x00, 1);
		if(resolution == 10)
		OneWire::write(0x20, 1);
		if(resolution == 11)
		OneWire::write(0x40, 1);
		if(resolution == 12)
		OneWire::write(0x70, 1);   // default setting
	}

	if(OneWire::reset() != 1) return ERR_DS18B20_NOT_FOUND;

	OneWire::select(addr);

	OneWire::write(0x44, 1); // start conversion, with parasite power on at the end

	return 0;
}


int DS18B20::readTemp()   // set temp (C *100), errorCnt ret 1=ok <0= error
{
	byte dataBuf[12];  //9 zou genoeg moeten zijn ???

	// we might do a ds.depower() here, but the reset will take care of it.

	OneWire::reset();
	OneWire::select(addr);
	OneWire::write(0xBE);         // request to read 8 bytes (Scratchpad)

	for(int i = 0; i < 9; i++)
	{           // we need 9 bytes
		dataBuf[i] = OneWire::read();
	}

#ifdef compileWithDebug
	Serial.print(F("data="));
	for( int i = 0; i < 9; i++)
	{
		Serial.print(dataBuf[i], HEX);Serial.write(' ');
	}Serial.println();
#endif

	if(dataBuf[2] != 0x00 || dataBuf[3] != 0x00)  return ERR_DS18B20_CONVERSION;  // check conversion ready by comparing high low temp
	if(OneWire::crc8(dataBuf, 8) != dataBuf[8])   return ERR_DS18B20_READ_CRC;

	int16_t rawTemp = (dataBuf[1] << 8) | dataBuf[0];   //data=80 1 0  0  1F FF 10 10 AE  = 24.00 Celsius,
	                                          //data=50 5 4B 46 7F FF C  10 1C = 85.00 Celsius,

	byte cfg = (dataBuf[4] & 0x60);
	// at lower res, the low bits are undefined, so let's zero them

	// default is 12 bit resolution, 750 ms conversion time

	if(cfg == 0x00)								// 9 bit resolution, 93.75 ms
		rawTemp = rawTemp & ~7;
	else if(cfg == 0x20)						// 10 bit res, 187.5 ms
		rawTemp = rawTemp & ~3;
	else if(cfg == 0x40)						// 11 bit res, 375 ms
		rawTemp = rawTemp & ~1;

	temp = (rawTemp / 16.0) * 100;  			// C *100

	return 1;
}


void DS18B20::setErrorCnt(bool errorOccurred)
{
	if(errorCnt < 3 && errorOccurred)
	errorCnt++;
	if(errorCnt > 0 && !errorOccurred)
	errorCnt--;
	if(errorCnt > 0 && ! initAdress)
	listAdres(false);					// here we re-try to get an address
	if(errorCnt > 0)
	{
		temp = -27400;  				// still an error
		senseTimer = millis() + 7000L;	// quickly retry
	}
	#ifdef DEBUG
		Serial.print(F("@")); Serial.print(millis()/1000); Serial.print(F(": errorCnt ")); Serial.println(errorCnt);
	#endif
}
void DS18B20::setAddr( const byte addres[8])
{
	//memcpy(this->addr, addres, sizeof(addr));
	for(int i = 0; i < 8; i++) this->addr[i] = addres[i];
}

void DS18B20::setSenseInterval(int periode_s)
{
	senseInterval = periode_s * 1000L;
}
void DS18B20::setSyncInterval(int periode_s)
{
	syncInterval = periode_s * 1000L;
}


void DS18B20::setResolution(int resolution)
{
	this->resolution = resolution;
	if(resolution == 9) readDelay_ms = 200;
	if(resolution == 10) readDelay_ms = 400;
	if(resolution == 11) readDelay_ms = 750;
	if(resolution == 12) readDelay_ms = 900; // maybe 750ms is enough, maybe not
}

int DS18B20::getTemp()
{
	return retCode > 0 ? temp : -27300;
}
int DS18B20::getGraden()
{
	return retCode > 0 ? temp / 100 : -274;
}

float DS18B20::getCelcius()
{
	return retCode > 0 ? (float) temp / 100 : -274;
}

void DS18B20::trace()
{ trace(""); }
void DS18B20::trace(char*  id  )
{
	Serial.print(F("@ "));
	Serial.print(millis()/1000);
	Serial.print(id);
	Serial.print(F(": "));
	Serial.print(getCelcius());
	Serial.print(F("C, interval="));	Serial.print(senseInterval);
	Serial.print(F(", errorCnt="));	Serial.print(errorCnt);
	Serial.print(F(", lastError="));	Serial.print(lastError);
	Serial.print(F(", errUploaded="));	Serial.print(lastErrorUploaded);
//	Serial.print(F(", errSend="));	Serial.print(lastErrorSend);
//	Serial.print(F(", errTs="));
//	Serial.print(errorTimeStamp);
	Serial.print(F(", temp="));	Serial.print( temp);
	Serial.print(F(", prevTemp="));	Serial.print( prevTemp);
	Serial.print(F(", nextSync="));	Serial.print( syncTimer);

	Serial.print(F(", "));

	printAdres( );
	//Serial.println();
}

void DS18B20::listAdres()
{
	listAdres(false);
}

void DS18B20::printAdres( )
{
	Serial.print(F(" addr:"));	// this will be used when a read without addr is fired !!
	for(int i = 0; i < 8; i++)
	{
		Serial.write(' ');
	    Serial.print(addr[i], HEX);
	}
	Serial.println();
}

void DS18B20::listAdres(boolean print)
{
	OneWire::reset_search();
	//Serial.println(F("listAdres"));
	while(OneWire::search(addr))
	{ 	// the last one is remembered in addr.
		if(print) printAdres();
	}

	return;
}

//class QueueItem
//{
//public:
//	int delta;
//	int temp;
//	int opt;
//	int delta2;
//	//   QueueItem( int delta, int temp, int opt )
////   {
////     this->delta 	= delta;
////     this->temp 	= temp;
////     this->opt 		= opt;
////     this->delta2 	= 0;
////   }
//	QueueItem(int delta, int temp, int opt, int delta2)
//	{
//		this->delta = delta;
//		this->temp = temp;
//		this->opt = opt;
//		this->delta2 = delta2;
//	}
//};

//
//#ifndef dsxx
//#define dsxx() byte dsxx[] {0x28, 0x66, 0x7f, 0x58, 0x05, 0x00, 0x00, 0x9c};
//#endif
/*
 switch (addr[0]) {
 case 0x10:
 Serial.println("  Chip = DS18S20");  // or old DS1820
 break;
 case 0x28:
 Serial.println("  Chip = DS18B20");
 break;
 case 0x22:
 Serial.println("  Chip = DS1822");
 break;
 default:
 Serial.println("Device is not a DS18x20 family device.");
 return;
 }
 */
//#define _ds00 {0x00}
//#define _ds01 {0x28, 0x80, 0x7e, 0x00, 0x05, 0x00, 0x00, 0x0e}
////#define _ds02 {0x28, 0x53, 0x06, 0x01, 0x05, 0x00, 0x00, 0x4f}  //  _ds05
//#define _ds03 {0x28, 0x11, 0x41, 0x00, 0x05, 0x00, 0x00, 0x63}   // Addr = 28 11 41 0 5 0 0 63
//
//#define _ds04 {0x28, 0x2b, 0x57, 0x2b, 0x05, 0x00, 0x00, 0x45}   // Addr = 28 2B 57 2B 5  0  0 45
//#define _ds05 {0x28, 0x53, 0x06, 0x01, 0x05, 0x00, 0x00, 0x4f}   // Addr = 28 53 6  1  5  0  0 4F
//#define _ds06 {0x28, 0x00, 0x57, 0x2b, 0x05, 0x00, 0x00, 0x0b}   // Addr = 28 0  57 2B 5  0  0 B
//#define _ds07 {0x28, 0xc4, 0xb0, 0x2b, 0x05, 0x00, 0x00, 0xc3}   // Addr = 28 C4 B0 2B 5  0  0 C3
//#define _ds08 {0x28, 0x92, 0x5f, 0x2b, 0x05, 0x00, 0x00, 0xea}   // Addr = 28 92 5F 2B 5  0  0 EA
//#define _ds09 {0x28, 0xad, 0xb4, 0x2b, 0x05, 0x00, 0x00, 0x89}   // Addr = 28 AD B4 2B 5  0  0 89
//#define _ds10 {0x28, 0x60, 0x9E, 0x8C, 0x06, 0x00, 0x00, 0xD2}   // Addr = 28 AD B4 2B 5  0  0 89
//#define _ds11 {0x28, 0x24, 0xDF, 0x8C, 0x06, 0x00, 0x00, 0x5F}   // Addr = 28 AD B4 2B 5  0  0 89
//
//// met metalen omhuls
//#define _ds101 {0x28, 0x66, 0x7f, 0x58, 0x05, 0x00, 0x00, 0x9c}   // 28 66 7F 58 5 0 0 9C
//#define _ds102 {0x28, 0x4f, 0x90, 0x54, 0x05, 0x00, 0x00, 0x1b}   // 28 4F 90 54 5 0 0 1B test 28 4F 90 54 5 0 0 1B
//#define _ds103 {0x28, 0x2f, 0xec, 0x54, 0x05, 0x00, 0x00, 0x94}   // 28 2F EC 54 5 0 0 94
//#define _ds104 {0x28, 0xde, 0xc3, 0x57, 0x05, 0x00, 0x00, 0xce}   // 28 DE C3 57 5 0 0 CE
//#define _ds105 {0x28, 0xFF, 0x37, 0x36, 0x66, 0x14, 0x03, 0x9D}   // 28 DE C3 57 5 0 0 CE

