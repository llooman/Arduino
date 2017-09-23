/*
 Remote.h
 Copyright (c) 2013-2014 J&L Computing.  All right reserved.
 Rev 1.0 -

 Timer 2 used

 #define IR_PIN 11

 #include <Time.h>
 #include <IRremote.h>     // needed for Remote.h
 #include <Remote.h>

 Remote myRemote;

 setup:
 myRemote.setup();
 myRemote.setEventHandler( RCKeyPressedEvent );

 loop:
 myRemote.loop();

 data:
 int RCKeyPressedEvent(unsigned long value, boolean shift){

 switch ( value )
 {
 case 437:
 case 2485:
 Serial.print("@"); Serial.print(millis());
 if(shift)  Serial.println(" PLAY"); else Serial.println(" play");
 break;


 2015-08-10 v1.00 LLO  translating codes received from an IR remote controll



 */

#include <Arduino.h>
#include <inttypes.h>
#include <IRremote.h>     // needed for Remote.h

#ifndef Remote_h
#define Remote_h

//#define IR_SAMPLE_TIME 25					// loop delay most likely 25
#define IR_KEY_PRESS_TIME 500  // 300
#define IR_SHIFT_TIME 300	// 150

class Remote: public IRrecv
{
public:
	int irPin;
	Remote(int irPin) :
			IRrecv(irPin)
	{
		this->irPin = irPin;
	}
	;

	void startup();
	void loop();

	void trace();
	void setEventHandler(int (*function)(unsigned long value, boolean shift));

	unsigned long timeStamp = 0;

	unsigned long keyPressed = 0;
	unsigned long prevKeyPressed = 0;
	boolean keyProcessed = false;
	unsigned long firstTimePressed_t = 0;
	unsigned long lastTimePressed_t = 0;



private:
	int (*eventHandler)(unsigned long value, boolean shift);
	decode_results results;

};

void Remote::startup()
{
	IRrecv::enableIRIn(); // Start the receiver
}

void Remote::loop()
{

	// collect timing info when a valid key is being pressed
	if (IRrecv::decode(&results))
	{
		if (results.decode_type > 0)
		{
			lastTimePressed_t = millis();
			keyProcessed = false;

			if (keyPressed != results.value)
			{
				keyPressed = results.value;
				firstTimePressed_t = millis();
			}
		}
		//   Serial.print("decode_type");Serial.print (results.decode_type);
//		Serial.print(F(": firstTimePressed_t=")); Serial.print ( firstTimePressed_t);
//		Serial.print(F(": lastTimePressed_t=")); Serial.print ( lastTimePressed_t);
//		Serial.print(", value");Serial.println(results.value);

		IRrecv::resume(); // Receive the next value
	}

	if (keyProcessed)
		return;  // no need for checking

	if (millis() - firstTimePressed_t >= IR_KEY_PRESS_TIME)
	{

		boolean shift = lastTimePressed_t - firstTimePressed_t >= IR_SHIFT_TIME;

		prevKeyPressed = keyPressed;

		if (eventHandler != NULL)
			eventHandler(keyPressed, shift);

		keyProcessed = true;

		if (shift)
			firstTimePressed_t = lastTimePressed_t - IR_SHIFT_TIME; // reset timing so next same key is always SHIFTED
	}

}

void Remote::setEventHandler(
		int (*function)(unsigned long value, boolean shift))
{
	eventHandler = function;
}

void Remote::trace()
{
	Serial.print(F("@ "));
	Serial.print(micros());
	Serial.print(F(": keyPressed="));
	Serial.print(keyPressed);
	Serial.print(F(": prevKeyPressed="));
	Serial.print(prevKeyPressed);
	Serial.print(F(": firstTimePressed_t="));
	Serial.print(firstTimePressed_t);
	Serial.print(F(": lastTimePressed_t="));
	Serial.print(lastTimePressed_t);
	Serial.println("");
}

#endif
