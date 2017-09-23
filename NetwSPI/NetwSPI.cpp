#include "Arduino.h"
#include <avr/wdt.h>        // watchdog
#include "NetwSPI.h"

void NetwSPI::loop()  // TODO  server or client ???? call only when in parent mode
{
	if( ! isMaster)
	{
		if(msgSize==0)
	    {
	    	if(isSending)
	    	{
				digitalWrite(slavePin, HIGH);
				isSending=false;
				txCommit();
	    	}
	    }
	    else
	    {
	    	if(!isSending)
	    	{
				digitalWrite(slavePin, LOW);
				isSending=true;
	    	}
	    }
	}
	else
	{
	    if( !digitalRead(slavePin)
	     &&	!isSending)
	    {
	        digitalWrite(SS, LOW);
	        delayMicroseconds(SPI_RESPONSE_SLEEP);

	        byte rx = SPI.transfer(0x03);
	        if(rx!=0x00) pushChar(rx);
	        isSending=true;
	        return;
	    }

	    if( writeBufTo == writeBufFrom )
	    {
	    	if(isSending)
	    	{
				digitalWrite(SS, HIGH);
				isSending=false;
				txCommit();
	    	}
	    }
	    else
	    {
	    	if(!isSending)
	    	{
				digitalWrite(SS, LOW);
				isSending=true;
	    	}

			while(writeBufTo!=writeBufFrom)
			{
				delayMicroseconds(SPI_RESPONSE_SLEEP);
				SPI.transfer(writeBuf[writeBufFrom++]);
				writeBufFrom = writeBufFrom % PAYLOAD_LENGTH;
				if(msgSize>0)
					msgSize--;
				else
					msgSize++;
			}
			delayMicroseconds(SPI_RESPONSE_SLEEP);
			return;
	    }
	}

	if(isSending) return;

	//NetwBase::loopRx();
	findPayLoadRequest();
	NetwBase::loop();
}



void NetwSPI::setup(int pin, bool isMaster)
{
	this->slavePin = pin;
	this->isMaster = isMaster;

	txAutoCommit=false;

	if(isMaster)
	{
	    pinMode(slavePin, INPUT_PULLUP);
	    pinMode(SS, OUTPUT);
	    digitalWrite(SS, HIGH);

	    SPI.begin();
	    SPI.setClockDivider(SPI_CLOCK_DIV8);

	    delay(1);
	    if(verbose){ Serial.print(F("spiMaster "));Serial.println(nodeId); Serial.flush();}

	}
	else
	{
	    pinMode(slavePin, OUTPUT);
	    digitalWrite(slavePin, HIGH);
	    pinMode(SCK, INPUT);
	    pinMode(MOSI, INPUT);
	    pinMode(MISO, OUTPUT);
	    pinMode(SS, INPUT_PULLUP);

	    delay(1);
	    SPCR = (1<<SPE)|(0<<MSTR);

	    delay(1);
	    SPI.attachInterrupt();
	    if(verbose){ Serial.print(F("spiSlave "));Serial.println(nodeId); Serial.flush();}
	}

}

bool NetwSPI::isBusy(void){ return (  isSending ); } // Wire.isBusy()
bool NetwSPI::isReady(void){ return ( millis() > netwTimer &&  !isSending ); }//  && Wire.isReady()

int NetwSPI::write( RxData *rxData ) // TODO  make it work opt: 0=cmd, 1=val, 2=all
{
	//char str[NETW_MSG_LENGTH];

	//if(verbose)Serial.println(F("spiWrite"));

	if(msgSize>0)
	{
		//if(verbose)
			Serial.println(F("SPI err 41"));
		return -41;
	}

	rxData->msg.conn = nodeId;

	serialize(&rxData->msg, strTmp);

	netwTimer = millis()+SPI_SEND_INTERVAL;

	if( nodeId == rxData->msg.node) pingTimer = millis() + PING_TIMER;

	#ifdef DEBUG
		Serial.print(F(">"));Serial.println(strTmp); Serial.flush();
	#endif

	print(strTmp);

	return 0;
}


//bool NetwSPI::print(RxData *rxData ) // TODO  make it work opt: 0=cmd, 1=val, 2=all
//{
//	write(rxData);
//	return true;
//}


void NetwSPI::print(char *msg)  // TODO check override
{
	if(msgSize!=0)
	{
		//if(verbose)
			Serial.println(F("send SizeErr!"));
		return;
	}

    int ptr=0;
    //msgSize=0;

	while( msg[ptr] != 0x00)
	{
		writeBuf[writeBufTo++] = msg[ptr++] ;
		writeBufTo = writeBufTo % PAYLOAD_LENGTH;
		msgSize++;
	}

	if(!isMaster){ msgSize++;writeBufTo++;} // one extra




	//if(verbose){Serial.print(msgSize); Serial.print(F(">")); Serial.println(msg);}
}



void NetwSPI::trace(char* id)
{
	Serial.print(F("@ "));
	Serial.print(millis()/1000);
	Serial.print(F(" "));Serial.print(id);
	Serial.print(F(": "));
	Serial.print(F(", isMaster="));	 Serial.print(isMaster );
	Serial.print(F(", parent=")); Serial.print( isParent );
	Serial.print(F(", pingTimer=")); Serial.print( pingTimer/1000 );


	Serial.print(F(", nodeId="));	 Serial.print(nodeId);
	Serial.print(F(", nwTmr=")); Serial.print( netwTimer/1000L );

	Serial.print(F(", eolCnt=")); Serial.print( eolCount );
	Serial.print(F(", payLin=")); Serial.print( payLin );
	Serial.print(F(", payLout=")); Serial.print( payLout );
	Serial.print(F(", empty=")); Serial.print( empty );

	Serial.print(F(", pLoad>"));
	//Serial.print(payloadLower );
	for(int i=payLout;i!=payLin;i++)
	{
		i = i % PAYLOAD_LENGTH;
		Serial.print((char)payLoad[i]);
	}
	Serial.print(F("<"));

//	Serial.print(F(", isConn=")); Serial.print( isConnected );
//	Serial.print(F(", addr=")); Serial.print( addr );
//	Serial.print(F(", pin(")); Serial.print( pinSystemKey );
//	Serial.print(F(")=")); Serial.print( digitalRead(pinSystemKey) );
//	Serial.print(F(", payLin=")); Serial.print( payLin );
//	Serial.print(F(", user_onReceive="));   if(user_onReceive) Serial.print( 1);


	Serial.print(F(", rx=")); Serial.print( rxBufIn );
	Serial.print(F("-")); Serial.print( rxBufOut );
	Serial.print(F("-")); Serial.print( rxBuf[rxBufOut].timestamp>0 );
	Serial.print(F(" cnt=")); Serial.print( readCount );

	Serial.print(F(", tx=")); Serial.print( txBufIn );
	Serial.print(F("-")); Serial.print( txBufOut );
	Serial.print(F("-")); Serial.print( txBuf[txBufOut].timestamp>0 );
	Serial.print(F(" cnt=")); Serial.print( sendCount );
	Serial.print(F(" err=")); Serial.print( sendErrorCount );
	Serial.print(F(" rtry=")); Serial.print( sendRetryCount );
	Serial.print(F(" isSending=")); Serial.print( isSending );


	Serial.println( );
	Serial.flush();
}


