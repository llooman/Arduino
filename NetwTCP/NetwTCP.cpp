
#include "NetwTCP.h"

byte mac2[] = { 0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xCC };
IPAddress myIp2(LOCAL_IP); //192, 168, 178, 100);
IPAddress domoSrv2(IOT_SERVER); //192,168,178,51);

EthernetClient ethernetClient2;
EthernetServer ethernetServer2(9000);

void NetwTCP::setup(void)
{
	pinMode(SS_SD_CARD, OUTPUT);
	pinMode(SS_ETHERNET, OUTPUT);
	digitalWrite(SS_SD_CARD, HIGH);  // SD Card not active
	digitalWrite(SS_ETHERNET, HIGH); // Ethernet  active
	delay(4);
}

void NetwTCP::begin(void)
{
	id=isParent?85:86;
	txAutoCommit = false;
	digitalWrite(SS_ETHERNET, LOW); // Ethernet  active
	Ethernet.begin(mac2, myIp2);
	ethernetServer2.begin();
}

void NetwTCP::loop(void)
{
	if(isSending)
	{
		if (!ack && millis() < tcpSendTimer )
		{
			if (ethernetClient2.available())
			{
				char inChar = ethernetClient2.read();
				if (inChar == '1') ok = true;
				if (inChar == '&') ok = true;
				if (inChar == '\n') ack = true;
			}
		}

		if (ack || millis() >= tcpSendTimer )
		{
			isSending = false;
			ethernetClient2.stop();

			#ifdef DEBUG
				Serial.print(":ack=");
				Serial.print(ack);
				Serial.print(" ok=");
				Serial.println(ok); Serial.flush();
			#endif

			if(ok)
			{
				txCommit();
//				if(!ack) Serial.print(F("tcpSend ok, nack!"));
			}
			else
			{
				lastError = ERR_TCP_SEND;
				netwTimer = millis() + TCP_SEND_ERROR_INTERVAL;
			}
		}
	}
	else if ( ethernetClient2.status() == 0 )
	{
		requestClient = ethernetServer2.available();

		if (requestClient )
		{
			#ifdef DEBUG
				Serial.print(F("readTcp: cnt="));
			#endif

			while (requestClient.connected() && requestClient.available() )
			{

				char c = requestClient.read();
				pushChar(c);
				if (c == '\n')
				{
					requestClient.println(0);
					requestClient.stop();       // close the connection:
				}
			}

			#ifdef DEBUG
			Serial.print(i);
			Serial.print(F(", newLine="));Serial.println(hasNewLine);

			#endif
		}
	}

	findPayLoadRequest();
	NetwBase::loop();

}


int NetwTCP::write( RxData *rxData ) // opt: 0=all, 1=val, 2=cmd
{
	//char  str[NETW_MSG_LENGTH];
	rxData->msg.conn = nodeId;
	netwTimer = millis() + TCP_SEND_INTERVAL;

	serialize( &rxData->msg, sendBuf);
	//serialize( &rxData->msg, str);
	isSending = true;

	#ifdef DEBUG
		Serial.print("sendTcp:");Serial.print(sendBuf);
	#endif

	if (!ethernetClient2.connected())
		ethernetClient2.connect(domoSrv2, IOT_PORT);

	if (!ethernetClient2.connected())
	{
		Serial.println(F("no conn!"));Serial.flush();
		netwTimer = millis() + TCP_SEND_ERROR_INTERVAL;
		return ERR_TCP_NOT_READY;
	}

	ack = false;
	ok = false;
	tcpSendTimer = millis() + TCP_SEND_TIMEOUT;

	ethernetClient2.println(sendBuf);

	return 0;
}


bool NetwTCP::isReady(void)
{
	return ethernetClient2.status() == 0 && millis() > netwTimer && !isSending;
}

bool NetwTCP::isBusy(void)
{
	return ethernetClient2.status() != 0 || millis() < netwTimer || isSending;
}

bool NetwTCP::isAvailable(  )
{
	return charRequestAvailable() && millis() > netwTimer;
}

uint8_t NetwTCP::status()
{
	return ethernetClient2.status();
}

void NetwTCP::trace(char* id)
{
	Serial.print(F("@ "));
	Serial.print(millis()/1000);
	Serial.print(F(" "));Serial.print(id);
	Serial.print(F(": TCP="));	 Serial.print(nodeId);


	Serial.print(F(", nwTmr=")); Serial.print( netwTimer/1000L );

	Serial.print(F(", rx=")); Serial.print( rxBufIn );
	Serial.print(F("-")); Serial.print( rxBufOut );
	Serial.print(F("-")); Serial.print( rxBuf[rxBufOut].timestamp>0 );
	Serial.print(F(" cnt=")); Serial.print( readCount );
	Serial.print(F(", eolCnt=")); Serial.print( eolCount );

//	Serial.print(F(", rxPtr="));Serial.print( rxBufIndex );

	Serial.print(F(", tx=")); Serial.print( txBufIn );
	Serial.print(F("-")); Serial.print( txBufOut );
	Serial.print(F("-")); Serial.print( txBuf[txBufOut].timestamp>0 );
	Serial.print(F(" err=")); Serial.print( sendErrorCount );
	Serial.print(F(" rtry=")); Serial.print( sendRetryCount );
	Serial.print(F(" txCommit=")); Serial.print( txAutoCommit );

	Serial.print(F(", ethStat="));   Serial.print( status() );

	Serial.println();
}
