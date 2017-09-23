
//#define DEBUG

#ifndef DS18B20_h
#define DS18B20_h


#define ERR_DS18B20_NOT_FOUND		-30
#define ERR_DS18B20_CONVERSION		-31
#define ERR_DS18B20_READ_CRC		-32
#define ERR_DS18B20_ADDRESS_CRC		-33
#define ERR_DS18B20_ADDRESS_0X28	-34  // Address does not start with 0x28
#define ERR_DS18B20_INVALID_TEMP	-35
#define ERR_DS18B20_STATIC_TIMER_RELEASED	-36

#include <Arduino.h> 
#include <inttypes.h>
#include <OneWire.h>
#include <avr/pgmspace.h>


static unsigned long staticReadTimer = 0 ; // prevent conflicts with other DS18B20 on the same pin
//const byte ds00[] PROGMEM = {0x00};

//const byte ds01[] PROGMEM = {0x28, 0x80, 0x7e, 0x00, 0x05, 0x00, 0x00, 0x0e};
//const byte ds03[] PROGMEM = {0x28, 0x11, 0x41, 0x00, 0x05, 0x00, 0x00, 0x63};
//const byte ds04[] PROGMEM = {0x28, 0x2b, 0x57, 0x2b, 0x05, 0x00, 0x00, 0x45};
//const byte ds05[] PROGMEM = {0x28, 0x53, 0x06, 0x01, 0x05, 0x00, 0x00, 0x4f};
//const byte ds06[] PROGMEM = {0x28, 0x00, 0x57, 0x2b, 0x05, 0x00, 0x00, 0x0b};
//const byte ds07[] PROGMEM = {0x28, 0xc4, 0xb0, 0x2b, 0x05, 0x00, 0x00, 0xc3};
//const byte ds08[] PROGMEM = {0x28, 0x92, 0x5f, 0x2b, 0x05, 0x00, 0x00, 0xea};
//const byte ds09[] PROGMEM = {0x28, 0xad, 0xb4, 0x2b, 0x05, 0x00, 0x00, 0x89};
//const byte ds10[] PROGMEM = {0x28, 0x60, 0x9E, 0x8C, 0x06, 0x00, 0x00, 0xD2};
//const byte ds11[] PROGMEM = {0x28, 0x24, 0xDF, 0x8C, 0x06, 0x00, 0x00, 0x5F};
//
//const byte ds12[] PROGMEM = {0x28, 0xff, 0xc8, 0x75, 0x82, 0x15, 0x01, 0x1d};  //28 FF C8 75 82 15 1 1D
//const byte ds13[] PROGMEM = {0x28, 0xff, 0x0e, 0x77, 0x82, 0x15, 0x01, 0xa4};  //28 FF  E 77 82 15 1 A4
//
////28 FF 1C 5E 0 16 1 C0
//
//const byte ds101[] PROGMEM = {0x28, 0x66, 0x7f, 0x58, 0x05, 0x00, 0x00, 0x9c};
//const byte ds102[] PROGMEM = {0x28, 0x4f, 0x90, 0x54, 0x05, 0x00, 0x00, 0x1b};
//const byte ds103[] PROGMEM = {0x28, 0x2f, 0xec, 0x54, 0x05, 0x00, 0x00, 0x94};
//const byte ds104[] PROGMEM = {0x28, 0xde, 0xc3, 0x57, 0x05, 0x00, 0x00, 0xce};
//const byte ds105[] PROGMEM = {0x28, 0xFF, 0x37, 0x36, 0x66, 0x14, 0x03, 0x9D};  //28 FF 37 36 66 14 3 9D
//const byte ds106[] PROGMEM = {0x28, 0xff, 0x1c, 0x5e, 0x00, 0x16, 0x01, 0xc0};  //28 FF 1C 5E 0 16 1 C0

// ds107 28 37 DB 8C 6 0 0 42

//const byte ds108[] PROGMEM = {0x28, 0xff, 0x5e, 0x97, 0x66, 0x14, 0x02, 0x07};  //28 FF 5E 97 66 14 2 7


/*
 DS18B20.h

 2017-01-10 DS18B20::sendCurrentTemp sendError
			DS18B20::sendCurrentTemp retry netw.sendData

 Copyright (c) 2013-2013 J&L Computing.  All right reserved.
 Rev 1.0 - May 15th, 2013

 on the pin a 4.7K resistor is necessary for combined power and data

 #define DS18B20_PIN 6
 DS18B20 xxx;
 DS18B20 xxx( DS18B20_PIN, ds06 );

 setup:
 xxx.setup();								use DS18B20_PIN and defaults
 xxx.setTimerPeriod ( xx );  				sense every xx seconds default = 7
 xxx.setResolution( [9, 10, 11, 12] ); 	default = 9
 xxx.listAdres();							find adress
 xxx.listAdres( true );					print adresses found
 xxx.deltaTempMax;      					error when change > delatTempMax, default 0
 xxx.tempMax;								error when temp > tempMax, default 100
 xxx.tempMin;								error when temp < tempMin, default -10
 xxx.async;
 loop:
 int retCode = xxx.loop();					check timer to read
 //  int retCode = xxx.read();
 //  int retCode = xxx.read( float *var );
 //  int retCode = xxx.read( addr, float *var);

 data:
 xxx.trace();
 int  temp = xxx.temp;  			in 1/100 graden
 float temp = xxx.getTemp()		in 1/100 graden
 float temp = xxx.getCelcius()		in graden
 time_t lastRead = xxx.timeStamp;
 xxx.errorCnt;
 xxx.lastError

 */

class DS18B20: public OneWire
{
public:
	int temp = -27400;    // in 1/100 graden
	unsigned long timeStamp = 0;
	int prevTemp = temp;

	byte addr[8];

	int id=0;

	unsigned long syncTimer = 500;
	long syncInterval = 58000L;   // default sence every 60 sec

	bool async = true;

	bool initAdress = false;

	int deltaTempMax = 5000; // 0 = inactive, when the temp change is > delatTempMax we assume its an error
	int tempMax = 10000;  // + 100 C
	int tempMin = -1000;  // -10 C

	int retCode = -1;
	int errorCnt = 1;

    volatile int  lastError=0;
    int lastErrorUploaded = lastError;

	DS18B20(int pin) : OneWire(pin)
	{
		_pin = pin;
		addr[0] = 0x00;
		setup( );
	}
	DS18B20(int pin, int ID) : OneWire(pin)
	{
		_pin = pin;
		id = ID;
		addr[0] = 0x00;
		setup( );
	}

	DS18B20(int pin, const byte addres[8]) : OneWire(pin)  // a 4.7K resistor is necessary for 2 wire power steal
	{
		_pin = pin;
		setup( );
		for(int i = 0; i < 8; i++) this->addr[i] = pgm_read_word_near(addres +i);
		initAdress = true;
	}

	DS18B20(int pin, const byte addres[8], int ID) : OneWire(pin)  // a 4.7K resistor is necessary for 2 wire power steal
	{
		_pin = pin;
		id = ID;
		setup( );
		for(int i = 0; i < 8; i++) this->addr[i] = pgm_read_word_near(addres +i);
		initAdress = true;
	}

	~DS18B20()
	{
		//delete _OneWire;
	}			// release memory

	int (*uploadFunc) (int id, long val, unsigned long timeStamp) = 0;
    void onUpload( int (*function)(int id, long val, unsigned long timeStamp) )
    {
    	uploadFunc = function;
    }
    void upload(void);

	int (*errorFunc) (int id, long val ) = 0;
    void onError( int (*function)(int id, long val ) )
    {
    	errorFunc = function;
    }


    void loop(void);
	void loop(int retries);
	int senseTemp();
	int readTemp();

	void setAddr( const byte addres[8]);
	void setSenseInterval(int);
	void setSyncInterval(int);

	void setResolution(int resolution);

	void listAdres();
	void printAdres();
	void listAdres(boolean);
	int getTemp();
	int getGraden();
	float getCelcius();

	void trace(void);
	void trace(char sensorId[]);

//	unsigned long errorTimeStamp = 0;
//	bool isErrorTimeStampReported = false;
//	bool debug = false;
//	void setup( const byte addres[8] )
//	{
//		memcpy(this->addr, addres, sizeof(addr));
//		setup();
//	}
private:
//	void processQueue();
	void setErrorCnt(bool);

	long senseInterval = 7000L;   // default sence every 7 sec
	unsigned long senseTimer;


	int resolution = 9;   // default precision
	int readDelay_ms = 200;

	unsigned long readStart;
	unsigned long readTempTimer = 0;

	int _pin;

	void setup( )
	{
		temp = -27400;
		prevTemp = temp;

		senseTimer = millis() + 500L; // initialy wait for half a second.
		syncTimer = millis() + 1000L;

		staticReadTimer = 0;
	}

};


#endif
