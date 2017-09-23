/*
Voltage.h
 Copyright (c) 2016-2016 J&L Computing.  All right reserved.
 Rev 1.0 - Sept 18th, 2016

 */

#include <Arduino.h>
#include <inttypes.h>


#define VOLTAGE_DELAY 7

#ifndef Voltage_h
#define Voltage_h


class Voltage
{
public:
	unsigned long readTimer = 3000;
	long syncInterval = 58000L;   // default sence every 60 sec
	unsigned long syncTimer = 500;

	long val = 0;
	long prevVal = val;
	long valSend = 0;
	double uFactor = 12.1;
    double rFactor = 11;
	int _pin = 0;
	int id = 0;

	bool available()
	{
		if(val != valSend)
		{
			valSend = val;
			return true;
		}
		return false;
	}

	Voltage(int pin, int id)
	{
		_pin = pin;
		this->id=id;

#ifdef  __AVR_ATmega2560__
		analogReference(INTERNAL1V1);
#else
		analogReference(INTERNAL);  //DEFAULT
#endif
		uFactor = rFactor * 1.1 / 1023;
	}
	Voltage(int pin, double newRFactor, int id)
	{
		_pin = pin;
		this->id=id;
#ifdef  __AVR_ATmega2560__
		analogReference(INTERNAL1V1);
#else
		analogReference(INTERNAL);
#endif
		rFactor = newRFactor;
		uFactor = rFactor * 1.1 / 1023;
	}
	~Voltage()
	{
		//delete _OneWire;
	}			// release memory
	int (*uploadFunc) (int id, long val, unsigned long timeStamp) = 0;
    void onUpload( int (*function)(int id, long val, unsigned long timeStamp) )
    {
    	uploadFunc = function;
    }


	void setRFactor(double newRFactor)
	{
		rFactor = newRFactor;
		uFactor = rFactor * 1.1 / 1023;
	}

	void upload()
	{
		if(uploadFunc==0) return;

		if( uploadFunc(id, val, millis() ) > 0)
		{
			syncTimer = millis() + syncInterval ;
			prevVal = val;
		}
		else
		{
			syncTimer = millis() + id ;
		}
	}

	void loop()
	{
		if( millis() > syncTimer
		 || prevVal != val
		){
			upload() ;
		}

		double UV = 0.0;
		if( millis() >= readTimer )
		{
			readTimer = millis() + VOLTAGE_DELAY * 1000L;

			prevVal = val;
			float analogVal = analogRead(_pin);

			// R1 100.000
			// R2  10.000

			// rFactor = 11
			// analogVal = 1.1 / 1023

			// U = analogVal * rFactor bv: (500 * 1.1 / 1023 ) * 11 = 0.5376 * 11 = 5,9 volt

			//UmV = 11.70 * analogVal ; // 1.1 * analogVal  * 10.0
			  // rfactor 10,881
	   		//  UV = 0.01170 * analogVal ; // 1.1 * analogVal  * 10.0
	   		UV = uFactor * analogVal ; // 1.1 * analogVal  * 10.0
	   		val =  UV * 10.0;
		}
	}


	void trace(char sensorId[])
	{
		Serial.print(F("@ "));
		Serial.print(millis()/1000);
		Serial.print(F(" "));
		Serial.print(sensorId);
		Serial.print(F(": "));
		Serial.print(val/10.0,1);
		Serial.print(F(" V"));
		Serial.println();
	}

private:

};


#endif
