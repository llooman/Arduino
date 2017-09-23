#ifndef NETWI2C_H
#define NETWI2C_H

#define DEBUG
#define NETW_I2C_BUF_CNT    	5
//#define DEBUG_ERROR_ONLY
//#include "ArdUtils.h"



#include "NetwBase.h"
#include <Wire.h>

volatile static int     i2cIn=0;
volatile static int     i2cOut=0;
volatile static unsigned long i2cTimestamps[NETW_I2C_BUF_CNT];
static unionRequest i2cMessages[NETW_I2C_BUF_CNT];

volatile static long	staticI2CReadCount=0;
volatile static long	staticI2CReadErrorCount=0;
volatile static int  	staticI2CLastReadError=0;
volatile static unsigned long  	staticI2CReadTimeout=TIMEOUT;


class NetwI2C: public NetwBase , public TwoWire
{
public:
	volatile  int *i2cInPtr;
	volatile  int *i2cOutPtr;
	volatile  unsigned long *i2cTimestampsPtr;
	unionRequest *i2cMessagesPtr;
	volatile  long *i2cReadCountPtr;
	volatile  long *i2cReadErrorCountPtr;
	volatile  int *i2cLastReadErrorPtr;
	volatile  unsigned long *i2cReadTimeoutPtr;

    byte slaveAdr = 0x00;

	unsigned long reqTimestamp = 0;

	NetwI2C(int nodeId)
	{
		NetwBase::nodeId=nodeId;
		//this->slaveAdr = slaveAdr;
		i2cInPtr  = &i2cIn;
		i2cOutPtr  = &i2cOut;
		i2cTimestampsPtr = &i2cTimestamps[0] ;
		i2cMessagesPtr = &i2cMessages[0];
		i2cReadCountPtr = &staticI2CReadCount;
		i2cReadErrorCountPtr = &staticI2CReadErrorCount;
		i2cLastReadErrorPtr = &staticI2CLastReadError;
		i2cReadTimeoutPtr = &staticI2CReadTimeout;

		for(int i=0;i<NETW_I2C_BUF_CNT;i++)
		{
			i2cTimestamps[i]=0;
		}
		TwoWire::onReceive( receiveI2CData );
	}

	static void  receiveI2CData(int howMany)
	{
		staticI2CReadCount++;
		bool bufferOverRun =  i2cTimestamps[i2cIn] != 0 ;
		if(bufferOverRun) staticI2CLastReadError = I2C_READ_BUF_OVERFLOW;

		unsigned int length=0;
		int value = Wire.read();
		while(value >= 0 )
		{
			if(length>=sizeof(unionRequest))
			{
				bufferOverRun = true;
				staticI2CLastReadError = I2C_READ_MSG_OVERFLOW;
			}
			if(!bufferOverRun)
			{
				i2cMessages[i2cIn].raw[length++] = value;
			}
			value = Wire.read();
		}

		if(!bufferOverRun ) //&& length == sizeof(reqNew2)
		{
			i2cTimestamps[i2cIn++] = millis();
			i2cIn = i2cIn % NETW_I2C_BUF_CNT;
			staticI2CReadTimeout = millis()+TIMEOUT; //
		}
		else
		{
			staticI2CReadErrorCount++;
		}
	}

	virtual ~NetwI2C(){}  // suppress warning



	void setup(int nodeId){this->nodeId = nodeId; }
	void begin(byte slaveAdr );
//	void beginParent(byte slaveAdr);
//	void beginChildren(byte slaveAdr);
    void trace(char* id);
    unsigned long timeStamp();

//	bool isAvailable();
//	bool isAvailable(unionRequest *req);
    void debug(void);
	int getRequest( unionRequest *req);
	void commit();

    bool isReady(void);
	bool isBusy(void);

//	bool isRequestPending(void){return *(i2cTimestampsPtr + *i2cOutPtr) >  0; }
	unsigned long latestTimeStamp();

	long getReadCount() {return *i2cReadCountPtr;}  // Serial.println(F("NetwI2C::getReadCount"));
	long getReadErrorCount() {return *i2cReadErrorCountPtr;}
	long getLastReadError() {return *i2cLastReadErrorPtr;}
	long getReadTimeout() {return *i2cReadTimeoutPtr;}
	void setReadTimeout() {*i2cReadTimeoutPtr = millis()+TIMEOUT;}

	int write( unionRequest *req);
	int sendI2C( unionRequest * req, int to, int len);  // ?? private

	void pullUpsOff()
	{
	  digitalWrite(SDA, 0);
	  digitalWrite(SCL, 0);
	}
	void pullUpsOn()
	{
	  digitalWrite(SDA, 1);
	  digitalWrite(SCL, 1);
	}

	void loop();

	void openLines(int digitalPort);


};

#endif
