
#include "NetwI2C.h"
//#include <math.h>
//#include <stdlib.h>
//#include <inttypes.h>
//#include <avr/io.h>
//#include <avr/interrupt.h>

//#include "pins_arduino.h"
//#include "Arduino.h" // for digitalWrite

unsigned long NetwI2C::timeStamp()
{
	return *(i2cTimestampsPtr + *i2cOutPtr);
}

bool  NetwI2C::isBusy(void){ return ( Wire.isBusy()  ); } //
bool  NetwI2C::isReady(void){ return ( millis() > netwTimer && Wire.isReady() ); }//! Wire.isBusy() &&

unsigned long NetwI2C::latestTimeStamp()
{
	return reqTimestamp;
}


void  NetwI2C::debug(void)
{
	if( ! Wire.isBusy()
	 && millis() > netwTimer
	 && *(i2cTimestampsPtr + *i2cOutPtr)  >  0  // req in the readbuffer
	 )
	{
		unionRequest *i2cMesgPtr = (i2cMessagesPtr + *i2cOutPtr);
		Serial.print(F("debug="));
		for(int i=0;i<NETW_REQUEST_BUFFER_LENGTH;i++)
		{
			Serial.print(i2cMesgPtr->raw[i], HEX);
			Serial.print(F("."));
		}

		Serial.flush();
		commit();

	}

	//return getCharRequest(req);  // enter requests by serial for debuging


}

void NetwI2C::commit()
{
	*(i2cTimestampsPtr + *i2cOutPtr)=0L;  // clear timestamp to free this buffer
	(*i2cOutPtr)++;// = *i2cOutPtr+1;
	*i2cOutPtr = *i2cOutPtr % NETW_I2C_BUF_CNT;
	//if(*i2cOutPtr >= NETW_REQUEST_BUFFER_CNT) *i2cOutPtr = 0;
}


int  NetwI2C::getRequest(   unionRequest *req)
{
	if( ! Wire.isBusy()
	 && millis() > netwTimer
	 && *(i2cTimestampsPtr + *i2cOutPtr)  >  0  // req in the readbuffer
	 )
	{
		unionRequest *i2cMesgPtr = (i2cMessagesPtr + *i2cOutPtr);
//		memcpy(req, i2cMesgPtr, NETW_REQUEST_BUFFER_LENGTH);
		req->data2.cmd = i2cMesgPtr->data2.cmd;
		req->data2.node = i2cMesgPtr->data2.node;
		req->data2.id = i2cMesgPtr->data2.id;
		req->data2.conn = i2cMesgPtr->data2.conn;
		req->data2.val =  i2cMesgPtr->data2.val;
		req->data2.deltaMillis = i2cMesgPtr->data2.deltaMillis;

		reqTimestamp=*(i2cTimestampsPtr + *i2cOutPtr);				 // remember received time

		#ifdef DEBUG
//			Serial.print(F("ParentReq cmd="));Serial.print((char)req->data2.cmd );
//			Serial.print(F("_"));Serial.print( req->data2.cmd, HEX );
//			Serial.print(F(", nodeIdConn="));Serial.print(req->data2.node );
//			Serial.print(F("."));Serial.print(req->data2.id );
//			Serial.print(F("."));Serial.print(req->data2.conn );
//			Serial.print(F(", val2="));Serial.println(req->data2.val );
//			Serial.flush();
		#endif

		return 1;
	}

	//return getCharRequest(req);  // enter requests by serial for debuging

	return 0;
}



void NetwI2C::begin(byte slaveAdr)
{
	this->slaveAdr = slaveAdr;

	if(slaveAdr == 0) { Serial.println(F("slave0!"));delay(15); }

	Wire.begin(slaveAdr);
}
void NetwI2C::loop()  // TODO  parentFlag or client ????
{

	if(isParent)
	{
		if( millis() > timeOutTimer  )
		{
			#ifdef DEBUG
				Serial.println(F("timeOutTimer"));
			#endif
			netwTimer = millis()+1000L;
			TwoWire::resetConn();
			timeOutTimer = millis() + TIMEOUT;
			return;
		}
	}
	else
	{
		if( millis() > getReadTimeout()  )
		{
			#ifdef DEBUG
				Serial.println(F("readTimeout"));
			#endif
			netwTimer = millis()+100L;
			TwoWire::resetConn();
			setReadTimeout();
		}
	}

	//TwoWire::loop();



	NetwBase::loop();

	//TODO reset conn on timeout
}

int NetwI2C::write(unionRequest *req )  // TODO
{

	req->data2.conn = nodeId;


	if( nodeId < 1)
	{
		//#ifdef DEBUG
			Serial.println(F("err send own nodeId < 1"));
		//#endif
		return -1;
	}


	if( req->data2.deltaMillis>650000)
	{
		Serial.println(F("Err timeDelta>650000"));
		return -2;
	}


	int to = req->data2.node;
	if( isParent
	 || req->data2.cmd == 'U'
	 || req->data2.cmd == 'u'
	 || req->data2.cmd == 'E'
	 || req->data2.cmd == 'e'
	){
		to = EVENT_SERVER;
	}
	else
	{
		int childConn = getChildConn(req->data2.node);
		//Serial.print("getChildConn=");Serial.println(req->data2.node);
		if(childConn>0)
		{
			to = childConn;
			//Serial.print("setChildConn=");Serial.println(childConn);
		}
	}

	int ret = sendI2C( req, to, sizeof(unionRequest) );

	return ret;
}

int NetwI2C::sendI2C( unionRequest *req, int to, int len )
{
	req->data2.conn = nodeId;

    int msgSize=sizeof(unionRequest);

	     if(req->data2.cmd == 'U') { msgSize = sizeof(Upload);    }
	else if(req->data2.cmd == 'u') { msgSize = sizeof(Upload);     }
	else if(req->data2.cmd == 'E') { msgSize = sizeof(Err);     }
	else if(req->data2.cmd == 'e') { msgSize = sizeof(Err);     }
	else if(req->data2.cmd == 92)  { msgSize = sizeof(Cmd);     }
	else if(req->data2.cmd == 'B')  { msgSize = sizeof(Cmd);     }
	else if(req->data2.cmd == 'b')  { msgSize = sizeof(Cmd);     }

	else if(req->data2.cmd == 'C') msgSize = sizeof(Cmd);

	else if(req->data2.cmd == 16) msgSize = sizeof(Set);
	else if(req->data2.cmd == 'S') msgSize = sizeof(Set);
	else if(req->data2.cmd == 's') msgSize = sizeof(Set);

	else if(req->data2.cmd == 21)  msgSize = sizeof(Cmd);
	else if(req->data2.cmd == 'R')  msgSize = sizeof(Cmd);
	else if(req->data2.cmd == 'r')  msgSize = sizeof(Cmd);

	else if(req->data2.cmd == 20) msgSize = sizeof(Cmd);
	else if(req->data2.cmd == 99) msgSize = sizeof(Cmd);
	else if(req->data2.cmd == 92) msgSize = sizeof(Cmd);

	if(nodeId==req->data2.node) pingTimer = millis() + PING_TIMER;


	//msgSize = msgSize+2;   // TODO remove after all on data2

	int ret = 0;


	if( ! Wire.isReady())
	{
		ret = -49;
	}
	else
	{
		Wire.beginTransmission(to);

		int sendCnt = Wire.write(req->raw, msgSize);

	#ifdef DEBUG
		//Serial.print(F("sndReq cnt="));Serial.println(sendCnt );delay(5);
	#endif

		ret = Wire.endTransmission();
		if(ret != 0) ret = -40 - ret ;
	}

	if(ret==0)
	{
		sendCount++;
		//if(sendHealthCount>0) sendHealthCount--;
		timeOutTimer = millis()+TIMEOUT; //
		netwTimer = millis() + NETW_I2C_SEND_DATA_INTERVAL;
	}
	else
	{
		lastSendError = ret;
		sendErrorCount++;
		//sendHealthCount++;
		netwTimer = millis() + NETW_I2C_SEND_ERROR_INTERVAL;
	}

	#ifdef DEBUG
//		#ifdef DEBUG_ERROR_ONLY
		if(ret<0)
		{
//		#endif
			Serial.print(F("I2C.send "));Serial.print((char) req->data2.cmd);
			Serial.print(F(" to="));Serial.print(to); Serial.print(F(", len="));Serial.print(msgSize);
			Serial.print(F(" "));Serial.print(req->data2.node); Serial.print(F("."));Serial.print(req->data2.id);Serial.print(F("."));Serial.print(req->data2.conn);
			Serial.print(F(" val2="));Serial.print(req->data2.val);
			Serial.print(F(" dly="));Serial.print(req->data2.deltaMillis);
			Serial.print(F(" < "));Serial.println(ret);
			Serial.flush();
//		#ifdef DEBUG_ERROR_ONLY
		}
//		#endif
	#endif

	return ret;

	/*    0:success
	 -41:data too long to fit in transmit buffer
	 -42: NACK on address
	 -43: NACK on data
	 -44: other error

	 -45:timeout on getting the bus
	 -46:timeout on waiting response
	 -47:timeout on release the bus
	 -52 Nack received
	 */
}

void NetwI2C::openLines(int digitalPort)
{
	//linesPort = digitalPort;
	pinMode(digitalPort, OUTPUT);
	digitalWrite(digitalPort, HIGH);
	delay(50);
//	#ifdef TWI_NO_PULLUPS
//	digitalWrite(SDA, 0);
//	digitalWrite(SCL, 0);
//	#endif
}


void NetwI2C::trace(char* id)
{
	Serial.print(F("@ "));
	Serial.print(millis()/1000);
	Serial.print(F(" "));Serial.print(id);
	Serial.print(F(": "));
	Serial.print(F(", nodeId="));	 Serial.print(nodeId);
	Serial.print(F(", slave="));	 Serial.print(slaveAdr);

	Serial.print(F(", netwTimer=")); Serial.print( netwTimer/1000L );

	Serial.print(F(", i2cSend="));  Serial.print( sendCount  );
	Serial.print(F("-"));  			Serial.print( sendErrorCount );
	Serial.print(F(", read="));  	Serial.print( *i2cReadCountPtr  );
	Serial.print(F("-"));  			Serial.print( *i2cReadErrorCountPtr );

	Serial.print(F(", lastRdErr="));  	Serial.print( *i2cLastReadErrorPtr  );

	Serial.print(F(", i2cIn=")); Serial.print( *i2cInPtr );
	Serial.print(F(", i2cOut=")); Serial.print( *i2cOutPtr );
	Serial.print(F(", i2cTimestamps[0]=")); Serial.print( *i2cTimestampsPtr );
	Serial.print(F(", i2cTimestamps[i2cOut]=")); Serial.print(  *(i2cTimestampsPtr + *i2cOutPtr) );



	Serial.println();
}

