/*
 *
2017-01-14 add netw as local souce
           in loop check for netw.hang + bootMsg + ramSize
           removed signatureChanged

2017-01-12 fix signatureChanged

   Hysteresis.h  
   Copyright (c) 2013-2013 J&L Computing.  All right reserved.
  Rev 1.0 - May 15th, 2013
*/

#include <Arduino.h>
#include <inttypes.h>

#ifndef ArdUtils_h
#define ArdUtils_h

#include <EEPROM.h>

#include "NetwBase.h"

	template <class T> int EEPROM_writeAnything(int ee, const T& value)
	{
		const byte* p = (const byte*)(const void*)&value;
		unsigned int i;
		for (i = 0; i < sizeof(value); i++)
			  EEPROM.write(ee++, *p++);
		return (i);
	}

	template <class T> int EEPROM_readAnything(int ee, T& value)
	{
		byte* p = (byte*)(void*)&value;
		unsigned int i;
		for (i = 0; i < sizeof(value); i++)
			  *p++ = EEPROM.read(ee++);
		return (i);
	}

// System Class.   * @author Matthias Maderer  @version 1.1.7
class System {
public:
  static int ramFree();
  static int ramSize();

};


class EEProm
{
public:
	EEProm(){ }// x(class Netw &netww){ this->parent = &parentNode; } NetwBase        *parent;
	//EEProm( class NetwBase &parentNode ){ this->parent = &parentNode; *hangPtr = &hang; }

    // ATmega328 has 1024 byte space for variables
//	EEProm(int localParmsOffSet )
//	{
//		this->localParmsOffSet = localParmsOffSet;
//	}

	int (*uploadFunc) (int id, long val, unsigned long timeStamp) = 0;
    void onUpload( int (*function)(int id, long val, unsigned long timeStamp) )
    {
    	uploadFunc = function;
    }
    int upload(int id);

	volatile long 	samplePeriode_sec  = 51L;
	volatile long 	bootCount          = 0;

	volatile bool 	changed	= true;
	bool 			hang = false;
	uint8_t  		bootMessages = 0;
	unsigned long 	bootTimer = 100;

	virtual int setVal( int id, long value );
	virtual int getVal( int id, long * value );
	virtual int handleRequest( RxItem *rxItem);

	void setSample( long newVal)
	{
		samplePeriode_sec = (int) newVal;
		if(samplePeriode_sec < 5) samplePeriode_sec = 5;
		if(samplePeriode_sec > 90) samplePeriode_sec = 90;
		changed = true;
	}

	void  resetBootCount()
	{
		bootCount=0;
		write(offsetof(EEProm, bootCount), bootCount);
	}

    bool (*user_onReBoot)(void);
    void onReBoot( bool (*function)(void)){user_onReBoot = function;}

    long  readLong(int offSet);
    int   readInt(int offSet);
    bool  readBool(int offSet);
    float readFloat(int offSet);
    byte  readByte(int offSet);
    void write(int offSet, unsigned long newVal);
    void write(int offSet, long newVal);
    void write(int offSet, int newVal);
    void write(int offSet, bool newVal);
    void write(int offSet, float newVal);
    void readAll();
    void writeAll();
	void loop();
};

class ArdUtils
{
  public:
	bool hang = false;
	int _pin;
	int flickerCount=0;

	ArdUtils();
    ArdUtils(int pin);	
    void setPin(int pin);
    void setLedPin(int pin);

    void hexPrint(int val);
	unsigned long flickerTimer = 0;
	unsigned long flickerLengthMs = 50;
    void flicker(int times);  			// flicker 
    void flicker();  			// flicker
    void flickerLoop();  				// flicker
    void loop();
    int handleRequest(RxItem *rxItem);
    void setFlicker( int count, int lengthMs);
    void slowFlicker(int times);  			// flicker

	private:

};


#endif
