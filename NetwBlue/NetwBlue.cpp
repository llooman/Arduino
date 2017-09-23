#include "Arduino.h"
#include <avr/wdt.h>        // watchdog
#include "NetwBlue.h"

const char blue100[] PROGMEM = {"A81B6AAE55C2"};
const char blue101[] PROGMEM = {"A81B6AAE4F66"};
const char blue102[] PROGMEM = {"A81B6AAE5922"};

const char blue103[] PROGMEM = {"...B6AAE55C2"}; // 4

const char* const bleNodes[] PROGMEM = {blue100, blue101, blue102, blue103 };


void NetwBlue::loop()  // TODO  server or client ???? call only when in parent mode
{
	while( serialPtr->available() )
	{
		char cc = serialPtr->read();

		if(cc=='{')
		{
			msgTimer = millis() + 3;  // msg should arrive within 3 millis
		}


		if( millis() > msgTimer
		 || msgTimer==0
		){
			msgTimer = 0;
			if(cc < 'a' || cc > 'z')  // skip lowercase for BLE AT
			{
				if(verbose) Serial.print(cc);

				respBuf[respPtr++] = cc;
				respBuf[respPtr] = 0x00;
			}

			if(atTimer>0) atTimer = millis() + BLE_RESPONSE_MILLIS;

		}
		else
		{
			pushChar(cc);

			if(cc=='}') msgTimer = 0;
		}

//		//Serial.print(F("("));Serial.print(port);Serial.print(F(")"));Serial.print(cc);
//        //if(cc < 'a' || cc > 'z')
//        {
//    		if(verbose)
//    			Serial.print(cc);
//
//			if(atTimer>0 || loopPending )
//			{
//				respBuf[respPtr++] = cc;
//				respBuf[respPtr] = 0x00;
//
//				if( respPtr > 3
//				 && respBuf[respPtr-1] == 'n'
//				 && respBuf[respPtr-2] == 'c'
//				 && respBuf[respPtr-3] == '.'
//				 && respBuf[respPtr-4] == 'o'
//				){
//					if(respPtr<15)
//						respPtr=0;
//					else
//						respPtr=respPtr-15;
//					respBuf[respPtr]=0x00;
//				}
//
//				if(atTimer>0) atTimer = millis() + BLE_RESPONSE_MILLIS;
//			}
//			else
//			{
//				pushChar(cc);
//			}
//        }
	}

    if(atTimer>0) 						responseLoop();

	if(writeFlag>0 && atTimer==0) 		writeLoop();
	if(resetFlag>0 && atTimer==0) 		resetLoop();
    if(initFlag>0 && atTimer==0) 		initLoop();
	if(connectFlag>0 && atTimer==0)  	connectLoop();
	if(masterFlag>0 && atTimer==0) 		masterLoop();
	if(slaveFlag>0 && atTimer==0) 		slaveLoop();
	if(infoFlag>0 && atTimer==0) 		infoLoop();
	if(discoverFlag>0 && atTimer==0) 	discoverLoop();
    if(queryFlag>0 && atTimer==0) 		queryLoop();

    if(isReady())
    {
		if( isMaster() )
		{
			if( ! isMasterByDefault) slaveFlag=1;
		}
		else
		{
			if( isMasterByDefault)   masterFlag=1;
		}
    }

    if( ! isBusy() ) findPayLoadRequest();

    NetwBase::loop();
}


void NetwBlue::responseLoop()
{
	for(int i=0; i<respPtr; i++)
	{
		if( respBuf[i]=='O'
		 && respBuf[i+1]=='K'
	    ){
			if( respBuf[i+2]=='+'
			 || atOkOnly
			){
				atOkFound = true;
				//atTimer=0;
			}
			break;
		}
	}

	if( millis()>atTimer
	 || ( atOkOnly && atOkFound )
	){
		//if(showResponse)
			//Serial.println(respBuf);
		if(atOkFound)
		{
			//if(verbose)Serial.println(respBuf);
		}
		else
		{
			//if(verbose && millis() > 700 && !showResponse)Serial.println(F("atTimeout"));
		}
		atTimer=0;
	}

}


bool NetwBlue::ATSet(char * atCmd, char * atVal ) //, int responseSleep
{
	atOkOnly = atCmd[0]==0x00;
	bool val = atVal[0]!=0x00;
	atOkFound = false;

	respPtr = 0;
	respBuf[respPtr] = 0x00;

	if( atTimer <= millis() ) atTimer =  millis()+BLE_RESPONSE_TIMEOUT_MILLIS;
	if( netwTimer <= millis() )  netwTimer = millis()+BLE_RESPONSE_TIMEOUT_MILLIS;

	if(verbose)
	{
		Serial.print(port);Serial.print(F(">"));Serial.print(F("AT"));
		if(!atOkOnly)
		{
			Serial.print(F("+"));
			Serial.print(atCmd);
		}
		if(val) Serial.print(atVal);
		Serial.print(F("<"));
	}

	serialPtr->print(F("AT"));
	if(!atOkOnly)
	{
		serialPtr->print('+');
		serialPtr->print(atCmd);
	}
	if(val) serialPtr->print(atVal);

	if (atLineFeed)
	{
		if(verbose)Serial.println(F("sendLF"));
		serialPtr->println();
	}
}



void NetwBlue::writeLoop()  //TODO
{
	if(resetFlag>0 || connectFlag>0 || masterFlag>0 || slaveFlag>0 || infoFlag >0 || queryFlag>0) return;

	atTimer = millis()+BLE_RESPONSE_MILLIS;
	netwTimer = millis()+BLE_RESPONSE_TIMEOUT_MILLIS; //AT_RESPONSE_TIMEOUT;

	loopPending=true;

	if(writeFlag==1) 	// switch to master
	{

		if( ! isMaster() )
		{
			if(masterFlag<0)
				writeFlag = masterFlag;
			else
			{
				masterFlag=1;
			}
		}
		else
		{
			writeFlag++;
		}
	}

	if(writeFlag==2) 	// reset or connect if needed
	{
		if( masterFlag < 0)
		{
			writeFlag = masterFlag;
		}
		else if( connectedTo == writeTo)
		{
			writeFlag=5;  //  already connected just write
		}
		else if ( ! autoConnect )
		{
			//if(verbose)
			{ Serial.println(F("write2 autoConn err"));   Serial.flush(); }
			writeFlag = ERR_BLE_CONN;
		}
		else
		{
			connectFlag = 1;  // trigger connect
		}
	}

	if(writeFlag==3) // connect if needed
	{
		if(connectedTo == writeTo)
		{
			writeFlag++; 	//  next step immediate
		}
		else if(autoConnect)
		{
			//atTimer = millis()+BLE_RELAX_MILLIS;
			connectFlag = 1;  // trigger connect
		}
		else
		{
			//if(verbose)
			{ Serial.println(F("write3 Conn err"));   Serial.flush(); }
			writeFlag = ERR_BLE_CONN;
		}
	}

	if(writeFlag==4) // send if connected
	{
		if(connectedTo != writeTo)
		{
			//if(verbose)
			{ Serial.println(F("write4 Conn err"));   Serial.flush(); }
			writeFlag = ERR_BLE_CONN;
		}
	}

	if(writeFlag==5 ) // write
	{
		print(writeBuf);
	}

	if(writeFlag==6 ) // reset conn
	{
		//if(autoConnect || autoDisconnect)
		{
			//print("{x,1,1,1,1}");
			//atTimer = 0;
			resetFlag=1;
		}
	}

	if(writeFlag>=7)
	{
		netwTimer = millis()+BLE_RESPONSE_MILLIS; //BLE_RESPONSE_TIMEOUT_MILLIS;  //BLE_RESPONSE_MILLIS
		writeFlag=0;
		loopPending=false;
		return;
	}

	if(writeFlag<0)
	{
		lastError = writeFlag;
		netwTimer = millis()+1;
		loopPending=false;
		return;
	}

	writeFlag++;
}

void NetwBlue::resetLoop()
{
	if(resetFlag==1)
	{
		if(loopPending)
		{
			resetTriggered = true  ;
		}
		else
		{
			resetTriggered = false;
			loopPending=true;
			atTimer = millis()+BLE_RESPONSE_MILLIS;
			netwTimer = millis()+BLE_RESPONSE_TIMEOUT_MILLIS; //AT_RESPONSE_TIMEOUT;
		}
		if(verbose)
		{ Serial.print(F("reset ")); Serial.println(nodeId);  Serial.flush(); }

		respPtr = 0;
		respBuf[respPtr] = 0x00;

	//	if(connectedTo>0) print("{x,102,0,100,1}");  // terminate connected slave

		resetFlag++; // next immediate
		resetFlag++; // next immediate
//		if(connectedTo==0)resetFlag++; // skip sending reset
	}

	if(resetFlag==2)
	{
		//if(verbose) { Serial.print(port); Serial.println(":resetFlag=2"); Serial.flush(); }
		//if(connectedTo>0) print("{x,102,0,100,1}");;  // terminate connected slave
	}

	if(resetFlag==3)
	{
		//if(verbose) { Serial.print(port); Serial.println(":resetFlag=3");   Serial.flush(); }
 		digitalWrite(powerPin ,LOW);
 		atTimer = millis()+300;
	}
	if(resetFlag==4)
	{
		//if(verbose) { Serial.print(port); Serial.println(":resetFlag=4");   Serial.flush(); }
 		digitalWrite(powerPin,HIGH);
 		atTimer = millis()+400; // 500
	}

	if(resetFlag>=5)
	{
		//if(verbose) { Serial.println(port); Serial.print(":resetFlag>=5");   Serial.flush(); }
		connectedTo = 0;
		resetFlag = 0;
		respPtr = 0;
		respBuf[respPtr] = 0x00;
		if(!resetTriggered)
		{
			loopPending=false;
			netwTimer = millis()+BLE_RESPONSE_MILLIS;  //BLE_RESPONSE_TIMEOUT_MILLIS;  //BLE_RESPONSE_MILLIS
		}
		return;
	}

	resetFlag++;
}


void NetwBlue::connectLoop()
{
	if(resetFlag>0 || queryFlag>0 || initFlag>0) return;

	uint8_t enterFlag = connectFlag;

	if(connectFlag==1)
	{
		if(loopPending)
		{
			connectTriggered = true  ;
		}
		else
		{
			connectTriggered = false;
			loopPending=true;
			atTimer = millis()+BLE_RESPONSE_MILLIS;
			netwTimer = millis()+BLE_RESPONSE_TIMEOUT_MILLIS; //AT_RESPONSE_TIMEOUT;
		}

		if( connectedTo == 0
		 || connectedTo == writeTo
		 || !autoConnect
		){
			connectFlag++;
		}
		else
		{
			if(verbose) { Serial.println(F("reset Conn"));   Serial.flush(); }
			resetFlag=1;
		}
	}

	if(connectFlag==2) // if not connected conn to writeTo
	{
		if(connectedTo == writeTo || !autoConnect)
		{
			if(verbose && connectedTo == writeTo) { Serial.print(F("allrdy conn to "));  Serial.println(writeTo); Serial.flush(); }
			connectFlag++; //
			connectFlag++; // allready connected so skip
		}
		else
		{
			char addr[13];

			getAddress(writeTo, addr);

			char cmd[7];
			pgmcpy(cmd, atCON, 7);
			ATSet(cmd, addr);
		}
	}

	if(connectFlag==3) // check conn response
	{
		if(connectedTo == writeTo || !autoConnect)
		{
			connectFlag++;
		}
		else if(atOkFound)
		{
		    if( respBuf[3]=='C' && respBuf[6]=='N' && respBuf[7]=='A')
		    {
				#ifdef DEBUG
					{Serial.println(F(" connected")); Serial.flush();}
				#endif

				connectedTo = writeTo;
		    }
		    else
		    {
		    	connectFlag = ERR_BLE_CONN; // stop: conn failed
				#ifdef DEBUG
					{Serial.println (F(" conn err"));   Serial.flush();}
				#endif
		    }
		}
		else
		{
			connectFlag = ERR_BLE_CONN; // stop: conn failed
		}
	}

	if(connectFlag>=4)
	{
		if(!connectTriggered)
		{
			loopPending=false;
			netwTimer = millis()+1;
		}

		if(enterFlag==1)
		{
			if(verbose) Serial.println(F("already Connected"));
			atTimer = 0;
		}
		else
		{
			atTimer=millis()+BLE_RELAX_MILLIS;
		}

		connectFlag=0;
		return;
	}

	if(connectFlag<0)
	{
		connectedTo = 0;
		lastError = connectFlag;
		if(!connectTriggered)
		{
			loopPending=false;
			netwTimer = millis()+1;
		}
		return;
	}
	connectFlag++;
}

void NetwBlue::queryLoop()  // always called from other loop!
{
	atTimer = millis()+BLE_RESPONSE_MILLIS;

	if(queryFlag==1)
	{
		ATquery(atROLE);
	}

	if(queryFlag==2)
	{
		if(atOkFound && respBuf[4] == ':')
		{
			ROLE = respBuf[5];
		}
		else
		{
			ROLE = '-';
			if(verbose){ Serial.println(F("ROLE err"));Serial.flush();}
		}
		ATquery(atMODE);
	}

	if(queryFlag==3)
	{
		if(atOkFound && respBuf[4] == ':')
		{
			MODE = respBuf[5];
		}
		else
		{
			MODE = '-';
			if(verbose){ Serial.println(F("MODE err"));Serial.flush();}
		}
		ATquery(atIMME);
	}

	if(queryFlag==4)
	{
		if(atOkFound && respBuf[4] == ':')
		{
			IMME = respBuf[5];
		}
		else
		{
			IMME = '-';
			if(verbose){ Serial.println(F("IMME err"));Serial.flush();}
		}
		ATquery(atNOTI);
	}

	if(queryFlag==5)
	{
		if(atOkFound && respBuf[4] == ':')
		{
			NOTI = respBuf[5];
		}
		else
		{
			NOTI = '-';
			if(verbose){ Serial.println(F("NOTI err"));Serial.flush();}
		}
	}

	if(queryFlag>=6)
	{
		queryFlag=0;
		return;
	}

	if(queryFlag<0)
	{
		lastError = queryFlag;
		return;
	}

	queryFlag++;
}

void NetwBlue::initLoop()
{
	if(queryFlag>0)return;

	atTimer = millis()+BLE_RELAX_MILLIS;

	loopPending=true;

	if(initFlag==1) ATOK();

 	if(initFlag==2) // switch baud rate if necessary
	{
		if( ! atOkFound)
		{
			if(baudRate==9600L)
			{
				NetwSerial::setup( port, 115200);
				baudRate=115200;
				if(verbose)
					{ Serial.println(F("try 115200"));Serial.flush();}
			}
			else
			{
				NetwSerial::setup( port, 9600);
				baudRate=9600L;
				if(verbose)
					{Serial.println(F("try 9600"));Serial.flush();}
			}
		}
		else
		{
			initFlag++;
		}

	}

	if(initFlag==3)
	{
		if( ! atOkFound) ATOK(); // first retry
	}

	if(initFlag==4)
	{
		if(! atOkFound)
		{
			ATOK(); // second retry if necessary
		}
		else
		{
			initFlag++; // = 7;  // immediate query role
		}
	}

	if(initFlag==5) // check retries
	{
		if( ! atOkFound)
		{
			initFlag = ERR_BLE_INIT; // cancel after second retry
			if(verbose){ Serial.println(F("cancel after second retry"));Serial.flush();}
		}
		else
		{
			queryFlag=1;
		}
	}


	if(initFlag>=6)  // check query result
	{
		if( queryFlag == 0)
		{
			payLout=payLin;
			eolCount=0;
			empty=true;
			loopPending=false;
			netwTimer = millis()+1;

			if(verbose){ Serial.println();Serial.print(F("OKNode"));Serial.print(nodeId);Serial.flush();  }

			initFlag=0;  			// init finished
			return;
		}
		else
		{
			initFlag = ERR_BLE_QUERY; // cancel after second retry
			if(verbose){ Serial.print(F("ERR_BLE_QUERY node"));Serial.println(nodeId);Serial.flush();}
		}
	}

	if(initFlag<0)
	{
		lastError = initFlag;
		netwTimer = millis()+1;
		loopPending=false;
		return;
	}

	initFlag++;
}






bool NetwBlue::isBusy(void)
{
	return ( loopPending || atTimer > 0 );   //atPending
}

bool NetwBlue::isReady(void)
{
	//{ Serial.print("isReady "); Serial.println(millis() > netwTimer && !atPending && writeFlag<1);  Serial.flush(); }

	return ( millis() > netwTimer && !loopPending && atTimer == 0);
}

int NetwBlue::write( RxData *rxData ) // TODO  make it work opt: 0=cmd, 1=val, 2=all
{
	if(!isReady())  //should check isReady before calling!!
	{
		//{ Serial.println("! isReady ");   Serial.flush(); }
		return -2;
	}

	writeTo = rxData->msg.node;

	if( isParent
	 || rxData->msg.cmd == 'U'
	 || rxData->msg.cmd == 'u'
	 || rxData->msg.cmd == 'E'
	 || rxData->msg.cmd == 'e'
	){
		writeTo = uploadNode; // override address where the nodeId is from instead of to
		if(uploadNode == 0)  //should check isReady before calling!!
		{
			//{ Serial.println("! isReady ");   Serial.flush(); }
			return -2;
		}
	}
	else if (isMeshEnabled)
	{
		int connId = getMeshConn(writeTo);
		if(connId>0)
		{
			writeTo = connId;
		}
	}

	#ifdef DEBUG
//		Serial.print(millis());
//		Serial.print(F(">"));
//		Serial.print(writeTo);
//		Serial.print(F(">"));
//		Serial.print((char)rxData->msg.cmd);
//		Serial.print(F(" "));
//		Serial.print( rxData->msg.node);
//		Serial.print(F("."));
//		Serial.print( rxData->msg.id);
//		Serial.print(F("."));
//		Serial.print( nodeId);
//		Serial.print(F(" "));
//		Serial.print( rxData->msg.val);
//		Serial.print(F(" "));
//		Serial.print( rxData->msg.deltaMillis);
//		Serial.println();
//		Serial.flush();
	#endif

	netwTimer = millis()+BLUE_SEND_INTERVAL;

	serialize(&rxData->msg, writeBuf);

	writeFlag=1;

	if(verbose)
		{ Serial.print("write "); Serial.println(writeBuf);  Serial.flush(); }

	return 0;
}

/*
 * 1 reset Conn
 * 2 atCON
 * 3 CONOK
 * 4 print
 * 5 reset Conn
 */

bool NetwBlue::println(char * cmd )
{

	#ifdef DEBUG
	Serial.print(F("PrtLn:"));
		Serial.print(nodeId);
		Serial.print(F(">"));
		Serial.print(cmd);
		if(!isMaster())Serial.print(F("<"));
	#endif

	serialPtr->println(cmd);

}

bool NetwBlue::print(char * cmd )  //TODO
{

	#ifdef DEBUG
	Serial.print(F("Prt:"));
		Serial.print(nodeId);
		Serial.print(F(">"));
		Serial.print(cmd);
		if(!isMaster())Serial.print(F("<"));
	#endif

	serialPtr->print(cmd);

	if (atLineFeed)
	{
		#ifdef DEBUG
			Serial.println(F("(lf)"));
		#endif
		serialPtr->println();
	}

	if( connectedTo>0)
	{
		Serial.println();
		return true;
	}

	#ifdef DEBUG
	Serial.print(F("<"));
	#endif

}



bool NetwBlue::connect(int toNode)       // portStatus[ ]     0=disConn, 1=master, 2=slave
{
	writeTo = toNode;
	connectFlag = 1;
}

void NetwBlue::setup( int pin, int port, bool masterByDefault, long baudrate)
{
	//this->bleRole = bleRole;
	isMasterByDefault = masterByDefault;
	uploadNode = 100;
	powerPin = pin;
	#ifdef DEBUG
		verbose=true;
	#endif
	pinMode( powerPin,OUTPUT);
	digitalWrite(pin,HIGH);

	NetwSerial::setup( port, baudrate);

	netwTimer=millis()+AT_SLEEP_ON_START;
}






bool NetwBlue::auth()
{
	//if(ports[portNr] == 'c' ) { send(portNr, "AT+RENEW"); send(portNr, "AT+RESET"); }
//	if(ports[portNr] == 'n' )
	{
		bool ret = ATquery(atTYPE);
		Serial.print(port);Serial.print(F(">"));Serial.print(F("AT+TYPE"));
		if(ret)
		{
			char c = respBuf[4];  //OK+Get:0
			if(c =='0')       {Serial.println(F("(0) Auth no Pin"));}
			else if(c == '1') {Serial.println(F("(1) Auth not need PIN"));}
			else if(c == '2') {Serial.println(F("(2) Auth with PIN"));}
			else if(c == '3') {Serial.println(F("(3) Auth and bond"));}
			else {Serial.println(F("Auth ??"));}
		}
		return ret;
	}
}
void NetwBlue::setAuth(char auth ) // 0=Peripheral, 1=Central
{
//	if(ports[portNr] == 'n' )

	{
        if(auth=='0')      {sendAny( "AT+TYPE0");}
        else if(auth=='1') {sendAny( "AT+TYPE1");}
        else if(auth=='2') {sendAny( "AT+TYPE2");}
        else if(auth=='3') {sendAny( "AT+TYPE3");}
        else {Serial.println(F("setAuth ??"));}
	}
}
void NetwBlue::restore()
{
	{ sendAny("AT+DEFAULT");}
//	if(ports[portNr] == 'c' ) { sendAny(portNr, "AT+RENEW", true); sendAny(portNr, "AT+RESET", true); }
}
void NetwBlue::restart()
{
 	if(port==1)
 	{
 		digitalWrite(4,LOW);
 		delay(200);
 		digitalWrite(4,HIGH);
 		delay(300);
 		return;
 	}

 
	digitalWrite(powerPin ,LOW);
	delay(1000);
	digitalWrite(powerPin ,HIGH);

 	delay(100);

}

bool NetwBlue::sendAny(char * cmd )
{
	//bool isAT = cmd[0] == 'a' || cmd[0] == 'A';

	if(verbose)
	{
		Serial.print(port);
		Serial.print(F(">"));
		Serial.print(cmd);
		if(!isMaster())Serial.print(F("="));
	}

	serialPtr->print(cmd);
	if (atLineFeed )//|| ! isAT || isConnected )
	{

		if(verbose)	Serial.println(F("(lf)"));

		serialPtr->println();
	}

	if( connectedTo>0)
	{
		Serial.println();
		return true;
	}


	if(verbose)	Serial.print(F("="));


	respPtr=0;
	int wwwTeller=0;
	bool isOK = false;
	unsigned long respTimer = millis() + AT_RESPONSE_TIMEOUT;

	while ( respTimer > millis() )
	{
		if(serialPtr->available() )
		{
			char cc = serialPtr->read();
			respBuf[respPtr++] = cc;
			//if(verbose) Serial.print(cc);
			respTimer = millis() + AT_RESPONSE_SLEEP;	// once receiving don't wait too long
			isOK = true;

			if( ( wwwTeller==0 && cc == 'o' )
			 || ( wwwTeller==1 && cc == '.' )
			 || ( wwwTeller==2 && cc == 'c' )
			 || ( wwwTeller==3 && cc == 'n' )
			){
				wwwTeller++;
				if(wwwTeller==4)
				{
					//isOK=false;
					respPtr=respPtr-15;
					wwwTeller=0;
					if(respPtr<0)respPtr=0;
					if(verbose) Serial.println(F("_.cn_"));
					respTimer = millis() + AT_RESPONSE_TIMEOUT;
				}
			}
			else
			{
				wwwTeller = 0;
			}
		}
	}

	respBuf[respPtr] = 0x00;

	if(isOK)
	{

		if(verbose)	Serial.println(respBuf);


		netwTimer = millis()+AT_RESPONSE_SLEEP;
	}
	else
	{

		if(verbose)	Serial.println(F("err"));


		netwTimer = millis()+AT_RESPONSE_SLEEP_ON_ERROR;
	}

	return isOK;
}

bool NetwBlue::isSlave(){	return (MODE=='2' && IMME=='0' && NOTI=='1' && ROLE=='0');}
bool NetwBlue::isMaster(){	return (MODE=='0' && IMME=='1' && NOTI=='1' && ROLE=='1');}

bool NetwBlue::ATOK()
{
	char cmd[3];
	pgmcpy(cmd, atEMPTY, 2);
	char val[3];
	pgmcpy(val, atEMPTY, 2);

	return ATSet( cmd, val );
}


//bool NetwBlue::ATquery( const char atCmd[]  )
//{
//	return ATquery(atCmd, AT_RESPONSE_SLEEP );
//}

bool NetwBlue::ATquery( const char atCmd[] ) //, int responseSleep
{
	char cmd[7];
	pgmcpy(cmd, atCmd, 7);
	char val[2] = {"?"};
	return ATSet(cmd, val); //responseSleep
}
bool NetwBlue::ATSet(const char * atCmd, int atVal )
{
	char cmd[7];
	pgmcpy(cmd, atCmd, 7);
	char val[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	if(atVal<10)
	{
		val[0] = '0' + atVal;			// '0' - '9'
	}
	else if( atVal > 9 &&  atVal < 16)
	{
		val[0] = 'A' + atVal-10;		// 'A' - 'F'
	}
	else
	{
		Serial.println(F("ATSet.err atVal  > 10!!"));
	}
	return ATSet(cmd, val );
}

bool NetwBlue::ATSet(const char * atCmd, const char * atVal )
{
	char cmd[7];
	char val[20];

	pgmcpy(cmd, atCmd, 7);
	strncpy(val, atVal, 20);

	return ATSet(cmd, val);
}

/*1:at:=OK+DISCS
OK+DIS0:A81B6AAE5922OK+NAME:N102
OK+DIS0:A81B6AAE4F66OK+NAME:N101
OK+DISCE


OK+DIS0:A81B6AAE5922OK+DIS0:A81B6AAE4F66OK+DISCE
 * */

void NetwBlue::discoverLoop()
{
	atTimer = millis()+BLE_RESPONSE_MILLIS;
	netwTimer = millis()+BLE_RESPONSE_DISC_MILLIS; //AT_RESPONSE_TIMEOUT;
	loopPending=true;

	if(discoverFlag==1)
	{
		if(verbose) { Serial.print("disc "); Serial.println(port);  Serial.flush(); }
	}
	if(discoverFlag==2)
	{
		atTimer = millis()+BLE_RESPONSE_DISC_MILLIS;
		print("AT+DISC?");
		discoverTimer = millis()+BLE_RESPONSE_DISC_MILLIS;
	}
	if(discoverFlag==3)
	{
 		//Print1>AT+DISC?<OK+DISCSOK+DIS0:A81B6AAE5922OK+DIS0:A81B6AAE4F66OK+DISCE
		bool disceFound = false;
		if(verbose)
		{
			Serial.print(F("discoverFlag==3 respPtr="));
			Serial.println( respPtr);

		}
		if(respPtr>5)
		{
			for(int i=respPtr-4; i>0; i--)
			{
				if( respBuf[i]=='D'
				 && respBuf[i+1]=='I'
				 && respBuf[i+2]=='S'
				 && respBuf[i+3]=='C'
				 && respBuf[i+4]=='E'
				){
					disceFound = true;
					break;
				}
			}
		}

		if( ! disceFound )
		{
			if(millis()>discoverTimer)
			{
				discoverFlag=ERR_BLE_DISC;

				if(verbose)Serial.println(F("discover timeout"));
			}
			else
			{
				atTimer = millis()+1000;//BLE_RESPONSE_MILLIS;
				return;
			}
		}
 	}

	if(discoverFlag==4)
	{
		int  findCnt = 0;

		for(int i=0; i<respPtr;i++)
		{
			if( respBuf[i]=='D'
			 && respBuf[i+1]=='I'
			 && respBuf[i+2]=='S'
			 && respBuf[i+3]=='0'
			 && respBuf[i+4]==':'
			){
				int strt = i + 5;
				if( respBuf[strt+12]=='O'
				 &&	respBuf[strt+13]=='K'
				){
					findCnt++;
					if(verbose)
					{
						for(int j=0;j<12;j++)
							Serial.print(respBuf[strt+j]);
						Serial.println();
					}
				}
			}
		}

		Serial.print(F("findCnt="));Serial.println(findCnt );
		discoverFlag=0;
		loopPending=false;
		netwTimer = millis()+1;
		return;
 	}

	if(discoverFlag<0)
	{
		lastError = discoverFlag;
		netwTimer = millis()+1;
		loopPending=false;
		return;
	}

	discoverFlag++;
}





void NetwBlue::infoLoop()
{
	netwTimer = millis()+BLE_RESPONSE_TIMEOUT_MILLIS;
	loopPending=true;

	if(infoFlag==1)
	{
		//showResponse = true;
		verboseSave = verbose;
		verbose = true;
		Serial.print(F("port=")); Serial.print (port);
		Serial.print(F(" lf=")); Serial.print( atLineFeed );
		Serial.print(F(" nodeId=")); Serial.print( nodeId );
		Serial.println(F(":"));Serial.flush();
	}
	else
	{
		Serial.println();
	}
	if(infoFlag==2)  ATOK();
	if(infoFlag==3)  ATquery(atNAME);
	if(infoFlag==4)  ATquery(atVERS); // version();
	if(infoFlag==5)  ATquery(atPOWE);
	if(infoFlag==6)  ATquery(atMODE) ;
	if(infoFlag==7)  ATquery(atIMME) ;
	if(infoFlag==8)  ATquery(atNOTI) ;
	if(infoFlag==9)  ATquery(atROLE) ;   //AT_ROLE_TIMEOUT , AT_ROLE_TIMEOUT
	if(infoFlag==10) ATquery(atUUID) ;
	if(infoFlag==11) ATquery(atADDR) ;
	if(infoFlag==12) ATquery(atTEMP) ;

	if(infoFlag==13) ATquery(atTYPE) ;//infoFlag--; else Serial.println(F("-----"));
//	if(infoFlag==11) if(!auth()) ;//infoFlag--; else Serial.println(F("-----"));

	if(infoFlag==14) if(!ATquery(atADTY)) ;
	if(infoFlag==15) if(!ATquery(atBAUD)) ;
	if(infoFlag==16) if(!ATquery(atCOMP)) ;
	if(infoFlag==17) if(!ATquery(atSAVE)) ;
	if(infoFlag==18) if(!ATquery(atSCAN)) ;
	if(infoFlag==19)  ATquery(atTCON)  ;
	if(infoFlag==20)  ATquery(atPWRM)  ;

	if(infoFlag>=21)
	{
		loopPending=false;
		netwTimer = millis()+1;
		infoFlag=0;
		//showResponse = false;
		verbose = verboseSave;
		return;
	}

	infoFlag++;
	//verbose = myVerbose;
	return;


//	if( ports[currPort] == 'n')
//	{
		auth();delay(10);
		Serial.println("-----");
		//send("AT+FILT?"); delay(100); // f0 set to find all modules   f1 only find hm10's

	//	send("AT+STATE"));
		ATquery( atMODE);delay(10);
		ATquery( atADTY);delay(10);
	//	ATquery( atSAVE);delay(50);
	//	ATquery( atSHOW);delay(10);


	//	send("AT+CHAR");
//	}
//		if( ports[currPort] == 'c')
//		{
//			Serial.println("-----");
//
//			send("AT+PWRM");
//			send("AT+STATE");
//			send("AT+CHAR");
//			send("AT+PIN");
//			send("AT+IBEA");
//		}
}

void NetwBlue::version()
{
	{ sendAny("AT+VERS?"); }  //send(portNr,"AT+VERS?");
//	if(ports[portNr] == 'c' ) { sendAny(portNr, "AT+VERSION", true); } //send(portNr,"AT+VERSION");
}

bool NetwBlue::getAddress(int nodeId, char *dest)
{
	dest[0]=0x00;
	if(nodeId<100 || nodeId>110) return false;

	strcpy_P(dest, (char*)pgm_read_word(&(bleNodes[nodeId-100])));
	return true;
}

void NetwBlue::masterLoop()
{
	atTimer = millis()+BLE_RESPONSE_MILLIS;
	uint8_t enterFlag = masterFlag;

	if(masterFlag==1)
	{
		if(loopPending)
		{
			masterTriggered = true  ;
		}
		else
		{
			masterTriggered = false;
			verboseSave = verbose;
			//verbose = true;
			loopPending=true;
			atTimer = millis()+BLE_RESPONSE_MILLIS;
			netwTimer = millis()+BLE_RESPONSE_TIMEOUT_MILLIS; //AT_RESPONSE_TIMEOUT;
		}
		if(verbose)
		{ Serial.print(nodeId); Serial.println(" setMaster"); Serial.flush(); }

		masterFlag++; // next immediate
	}

	if(masterFlag>2)
	{
		if(!atOkFound)
		{
			masterFlag = ERR_BLE_SET;
			if(verbose) { Serial.print(nodeId);Serial.print(":masterLoopErr ");  Serial.println(masterFlag); Serial.flush(); }
		}
		else
			if(verbose) Serial.println();
	}

	if(masterFlag==2) if(MODE!='0') ATSet( atMODE, 0); else masterFlag++;
	if(masterFlag==3) if(IMME!='1') ATSet( atIMME, 1); else masterFlag++;
	if(masterFlag==4) if(NOTI!='1') ATSet( atNOTI, 1); else masterFlag++;
	if(masterFlag==5)
	{
		if(ROLE!='1')
		{
			ATSet( atROLE, 1);
		}
		else
		{
			masterFlag++;
		}
	}

	if(masterFlag>=6)
	{
		MODE='0';
		IMME='1';
		NOTI='1';
		ROLE='1';
		if(!masterTriggered)
		{
			loopPending=false;
			verbose = verboseSave;
			netwTimer = millis()+1;
		}

		if(enterFlag==1)
		{
			if(verbose) Serial.println(F("already Master"));
			atTimer = 0;
		}
		else
		{
			atTimer=millis()+BLE_RELAX_MILLIS;
		}

		masterFlag=0;
		return;
	}

	if(masterFlag<0)
	{
		lastError = masterFlag;
		if(!masterTriggered)
		{
			loopPending=false;
			verbose = verboseSave;
			atTimer=0;
			netwTimer = millis()+1;//millis()+BLE_RESPONSE_MILLIS;
		}
		return;
	}

	masterFlag++;
}

void NetwBlue::slaveLoop()
{
	atTimer = millis()+BLE_RESPONSE_MILLIS;
	uint8_t enterFlag = slaveFlag;

	if(slaveFlag==1)
	{
		if(loopPending)
		{
			slaveTriggered = true  ;
		}
		else
		{
			slaveTriggered = false;

			verboseSave = verbose;
			//verbose = true;
			loopPending=true;
			atTimer = millis()+BLE_RESPONSE_MILLIS;
			netwTimer = millis()+BLE_RESPONSE_TIMEOUT_MILLIS; //AT_RESPONSE_TIMEOUT;
		}

		if(verbose)
		{ Serial.print(nodeId); Serial.println(" setSlave"); Serial.flush(); }

		slaveFlag++; // next immediate
	}

	if(slaveFlag>2)
	{
		if(!atOkFound)
		{
			slaveFlag = ERR_BLE_SET;
			if(slaveFlag==3) MODE='-';
			if(slaveFlag==4) IMME='-';
			if(slaveFlag==5) NOTI='-';
			if(slaveFlag==6) ROLE='-';

			if(verbose) { Serial.print(nodeId);Serial.print(":slaveLoopErr ");  Serial.println(slaveFlag); Serial.flush(); }
		}
		else
			if(verbose) Serial.println();
	}

	if(slaveFlag==2) if(MODE!='2') ATSet( atMODE, 2); else slaveFlag++;
	if(slaveFlag==3) if(IMME!='0') ATSet( atIMME, 0); else slaveFlag++;
	if(slaveFlag==4) if(NOTI!='1') ATSet( atNOTI, 1); else slaveFlag++;
	if(slaveFlag==5) if(ROLE!='0') ATSet( atROLE, 0); else slaveFlag++;

	if(slaveFlag>=6)
	{
		MODE='2';
		IMME='0';
		NOTI='1';
		ROLE='0';
		if(!slaveTriggered)
		{
			loopPending=false;
			verbose = verboseSave;
			netwTimer = millis()+1;
		}

		if(enterFlag==1)
		{
			if(verbose) Serial.println(F("already Slave"));
			atTimer = 0;
		}
		else
		{
			atTimer=millis()+BLE_RELAX_MILLIS;
		}

		slaveFlag=0;
		return;
	}

	if(slaveFlag<0)
	{
		lastError = slaveFlag;
		if(!slaveTriggered)
		{
			loopPending=false;
			verbose = verboseSave;
			netwTimer = millis()+BLE_RESPONSE_MILLIS;
		}
		return;
	}

	slaveFlag++;
}


void NetwBlue::trace(char* id)
{
	Serial.print(F("@ "));
	Serial.print(millis()/1000);
	Serial.print(F(" "));Serial.print(id);
	Serial.print(F(":"));
	Serial.print(F("nodeId="));	 Serial.print(nodeId);
	Serial.print(F(", isMaster="));	 Serial.print(isMaster());
	Serial.print(F(", isSlave=")); Serial.print( isSlave() );
//	Serial.print(F(", initFlag=")); Serial.print( initFlag );
	Serial.print(F(", netwTimer=")); Serial.print( netwTimer );
	Serial.print(F(", atTimer=")); Serial.print( atTimer );
	Serial.print(F(", loopPending=")); Serial.print( loopPending );
	Serial.print(F(", atOkOnly=")); Serial.print( atOkOnly );
	Serial.print(F(", atOkFound=")); Serial.print( atOkFound );
//	Serial.print(F(", resetFlag=")); Serial.print( resetFlag );
//	Serial.print(F(", connectFlag=")); Serial.print( connectFlag );
//	Serial.print(F(", discoverFlag=")); Serial.print( discoverFlag );
//	Serial.print(F(", ROLE=")); Serial.print( ROLE );
//	Serial.print(F(", MODE=")); Serial.print( MODE );
//	Serial.print(F(", IMME=")); Serial.print( IMME );
//	Serial.print(F(", NOTI=")); Serial.print( NOTI );
	Serial.print(F(", slaveFlag=")); Serial.print( slaveFlag );
	Serial.print(F(", masterFlag=")); Serial.print( masterFlag );
	Serial.print(F(", writeFlag=")); Serial.print( writeFlag );
	Serial.print(F(", connectFlag=")); Serial.print( connectFlag );

	Serial.print(F(", connTo=")); Serial.print( connectedTo );
//	Serial.print(F(", initOk=")); Serial.print( initOk );
//	Serial.print(F(", initFlag=")); Serial.print( initFlag );
//	Serial.print(F(", pin(")); Serial.print( powerPin );
//	Serial.print(F(")=")); Serial.print( digitalRead(powerPin) );
//	Serial.print(F(", payLin=")); Serial.print( payLin );
//	Serial.print(F(", payLout=")); Serial.print( payLout );
//	Serial.print(F(", empty=")); Serial.print( empty );
//	Serial.print(F(", eolCount=")); Serial.print( eolCount );

	Serial.print(F(", pLoad="));
//	Serial.print(payloadLower );
	//for(int i=0;i<=PAYLOAD_LENGTH;i++)
	for(int i=payLout;i!=payLin;i++)
	{
		i = i % PAYLOAD_LENGTH;
		Serial.print((char)payLoad[i]);
	}

//	Serial.print(payloadUpper );

	Serial.println();
	Serial.flush();
}


