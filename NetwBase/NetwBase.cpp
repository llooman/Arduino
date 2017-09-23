#include "NetwBase.h"
//

void NetwBase::loop()  // TODO
{
	if( txBuf[txBufOut].timestamp == 0
	 && txBufOut != txBufIn
	){
		Serial.print (F("fixTxBuf in="));Serial.print (txBufIn);Serial.print (F(" uit="));Serial.println(txBufOut);Serial.flush();
		txBufOut++;
		txBufOut = txBufOut % NETW_TX_BUF_CNT;
	}

	if(isReady() )
	{
		if( txBuf[txBufOut].timestamp == 0)
		{
//			if(readCount>=readCountNextUpload)
//			{
//				upload(isParent?81:71);
//			}
//			else if(sendCount>=sendCountNextUpload)
//			{
//				upload(isParent?84:74);
//			}
//			else
			if(sendErrorCount>=sendErrorNextUpload)
			{
				upload(isParent?85:75);
			}
//			else if(sendRetryCount>=sendRetryNextUpload)
//			{
//				upload(isParent?86:76);
//			}
		}
		else
		{
			if(txBuf[txBufOut].tries>2)
			{
				txCancel();
				sendErrorCount++;
			}
			else
			{
				//Serial.print(F("write try ")); Serial.println(txBuf[txBufOut].tries );

				if(txBuf[txBufOut].tries==0)
				{
					sendCount++;
					txBuf[txBufOut].tries++;
				}
				else
				{
					sendRetryCount++;
				}

				txBufCurrOut=txBufOut;
				int node = txBuf[txBufOut].data.msg.node;
				int ret= 0;
				ret = writeTxBuf();

				if(ret<0)
				{
					lastError = ret;
					#ifdef DEBUG
						{ Serial.print(F("writeTxBuf ret=")); Serial.println(ret);Serial.flush();  }
					#endif
				}
				else
				{
					//if(nodeId==node) pingTimer = millis() + PING_TIMER;
					//if(txAutoCommit)
					txCommit();
				}

				//return;
			}
		}
	}


//	findPayLoadRequest();

	if( user_onReceive )
	{
		if( rxBuf[rxBufOut].timestamp >0
		){
			if( isMeshEnabled )
			{
				saveMeshConn( &rxBuf[rxBufOut].data.msg );
			}

			int ret = user_onReceive( &rxBuf[rxBufOut]  );

			//if(ret>=0)
			rxCommit();
			return;
		}
	}

	if( lastError != lastErrorUploaded
	 && errorFunc !=0
	){
		if( ( lastErrorLogLevel > 0 && ( lastError == -49 || lastError == -42 ))
		 ||	( lastErrorLogLevel > 1 && ( lastError == -51 || lastError == -51 ))
		){
			lastErrorUploaded = lastError;
		}
		else
		{
			if(errorFunc(isParent?80:70, lastError) >= 0 )
			{
				lastErrorUploaded = lastError;
			}
		}
	}

	if(isParent) loopPing();
}

void NetwBase::loopPing()
{
	if( isReady()
	 && millis() > netwTimer
	 && millis() > pingTimer
	 && nodeId > 0
	){

		if( System::ramFree() < ramFree )
		{
			if( uploadFunc( 4 , System::ramFree(), 0 ) >= 0  )
			{
				ramFree = System::ramFree();
			}
		}
		else
		{
			uploadFunc(5, 0, 0);
		}

		pingTimer = millis()+PING_TIMER;
		return;
	}
}

int NetwBase::upload(int id)
{
	if(uploadFunc==0 ) return 0;

	int ret=0;

	switch( id )
	{
	case 80: ret=uploadFunc(id, lastErrorLogLevel, millis() ); break;

	case 81:
		ret = uploadFunc(id, readCount, millis() );
	//	if(isParent) readCountNextUpload = readCount+5;
		break;
	case 84:
		ret = uploadFunc(id, sendCount, millis() );
	//	if(isParent)   sendCountNextUpload = sendCount+10;
		break;
	case 85:
		ret = uploadFunc(id, sendErrorCount, millis() );
		if(isParent)  sendErrorNextUpload = sendErrorCount+2;
		break;
	case 86:
		ret = uploadFunc(id, sendRetryCount, millis() );
	//	if(isParent)  sendRetryNextUpload = sendRetryCount+2;
		break;

	case 71:
		ret = uploadFunc(id, readCount, millis() );
	//	if(!isParent) readCountNextUpload = readCount+5;
		break;
	case 74:
		ret = uploadFunc(id, sendCount, millis() );
	//	if(!isParent)   sendCountNextUpload = sendCount+10;
		break;
	case 75:
		ret = uploadFunc(id, sendErrorCount, millis() );
		if(!isParent)  sendErrorNextUpload = sendErrorCount+2;
		break;
	case 76:
		ret = uploadFunc(id, sendRetryCount, millis() );
	//	if(!isParent)  sendRetryNextUpload = sendRetryCount+2;
		break;
	default: return ret;
	}
	return ret;
}



//int NetwBase::getVal( int id, long *value ) //bool sending, int id,
//{
//	switch(id)
//	{
//	case 81: *value = readCount ;   break;
////	case 82: value = readErrorCount ;    break;
////	case 83: value = lastReadError ;    break;
//	case 84: *value = sendCount ;   break;
//	case 85: *value = sendErrorCount ;   break;
//	case 86: *value = sendRetryCount ;   break;
//
// 	default: return -1;	break;
//	}
//	return 1;
//}

int NetwBase::setVal( int id, long value ) //bool sending, int id,
{
	switch(id)
	{
	case 80: lastErrorLogLevel = value;  break;  // network sleep
	case 89: netwTimer = millis() + (value * 1000L);  break;  // network sleep

 	default: return -1; break;
	}
	return 1;
}

int NetwBase::getMeshConn(int node)
{
	int conn;
	int offSet = 1024-node*2;
	EEPROM_readAnything(offSet, conn);
	if(conn<1)conn=node;
	#ifdef DEBUG
	 Serial.print(F("getMesh(")); Serial.print(node);Serial.print(F(")>"));Serial.println(conn);
	#endif

	return conn;
}

void NetwBase::saveMeshConn(RxMsg *rxMsg)
{
	#ifdef DEBUG
		 Serial.print(F("saveMesh("));   Serial.print((char)rxMsg->cmd);Serial.print(F(","));Serial.print(rxMsg->node);Serial.print(F(","));Serial.print(rxMsg->conn);Serial.println(F(")"));
	#endif

	if( 1==2
//	 || rxMsg->node==rxMsg->conn
	 || rxMsg->node==2
	 || (rxMsg->cmd != 'U' && rxMsg->cmd != 'u')
//	 || meshTablePtr == 0
	 || rxMsg->node==0
	){
		return;
	}

	#ifdef DEBUG
		 Serial.print(F("saveMesh("));   Serial.print((char)rxMsg->cmd);Serial.print(F(","));Serial.print(rxMsg->node);Serial.print(F(","));Serial.print(rxMsg->conn);Serial.println(F(")"));
	#endif

	int tempInt;
	int offSet = 1024-rxMsg->conn*2;
	EEPROM_readAnything(offSet, tempInt);
	if( rxMsg->conn != tempInt )EEPROM_writeAnything(offSet, rxMsg->conn);
}


int NetwBase::write( RxData *rxData )
{
	Serial.println(F("?NetwBase::write.RxData?"));
	return -1;
}



void NetwBase::onRollBack( void (*function)(byte id, long val) )
{
	user_onRollBack = function;
}
void NetwBase::rxAddCommit()
{
	uploadNode = rxBuf[rxBufIn].data.msg.conn;		// save nodeId on incomming to be used for outgoing
	rxBuf[rxBufIn++].timestamp = millis();
	rxBufIn = rxBufIn % NETW_RX_BUF_CNT;
}
void NetwBase::rxCommit()
{
	rxBuf[rxBufOut++].timestamp = 0;
	rxBufOut=rxBufOut%NETW_RX_BUF_CNT;
}

void NetwBase::txCancel()
{
	if(txBufOut==txBufCurrOut)
	{
		txBuf[txBufCurrOut].timestamp = 0;
		//Serial.println(F("txCan"));
		txBufOut++;
		txBufOut=txBufOut%NETW_TX_BUF_CNT;
	}
}

void NetwBase::txCommit()
{
	if(txBufOut==txBufCurrOut)
	{
		txBuf[txBufCurrOut].timestamp = 0;
		txTimeOut=0;
		//Serial.print(F("txComm @"));Serial.println(millis());
		txBufOut++;
		txBufOut=txBufOut%NETW_TX_BUF_CNT;
		pingTimer = millis() + PING_TIMER;
	}
}


int NetwBase::putTxBuf(byte cmd, int node, byte id, long val, unsigned long timeStamp)  // false when full
{
		#ifdef DEBUG
	Serial.print(F("putTx*["));Serial.print(txBufIn);Serial.print(F("<"));
	Serial.print(id);Serial.print(F(","));Serial.print(val);
	Serial.print(F(" @"));Serial.println(millis());
		#endif


	if(txBuf[txBufIn].timestamp!=0)
	{
		//Serial.println(F("txBuf full")); Serial.flush();
		lastError =	ERR_TX_FULL;
		return ERR_TX_FULL;
	}

	if(timeStamp==0) timeStamp=millis();

	txBuf[txBufIn].data.msg.cmd = cmd;
	txBuf[txBufIn].data.msg.node = node;
	txBuf[txBufIn].data.msg.id = id;
	txBuf[txBufIn].data.msg.val = val;
	txBuf[txBufIn].timestamp = timeStamp;
	txBuf[txBufIn++].tries = 0;
	txBufIn = txBufIn % NETW_TX_BUF_CNT;

	return 1;
}

int NetwBase::putTxBuf(RxItem *rxItem)  // false when full
{
	unsigned long timeStamp = millis() - (rxItem->data.msg.deltaMillis * 100);
	return putTxBuf( rxItem->data.msg.cmd, rxItem->data.msg.node, rxItem->data.msg.id, rxItem->data.msg.val, timeStamp );
}
int NetwBase::writeTxBuf() // opt: 0=all, 1=val, 2=cmd
{
	RxData rxData;

	if(nodeId<1)
	{
		#ifdef DEBUG
			Serial.println("twSnd: nodeId<1");
		#endif
		lastError = ERR_TX_NODEID;
		return ERR_TX_NODEID;
	}

    int delta = ( millis() - txBuf[txBufOut].timestamp  ) / 100L;

	if(delta > 3000)
	{
		lastError = ERR_TX_DELTA_3000;
		txCancel();
		return ERR_TX_DELTA_3000;  // skip > 300 seconds (5 minutes) in the past
	}
	if(delta < 0)
	{
		lastError = ERR_TX_DELTA_LT_0	;
		txCancel();
		return ERR_TX_DELTA_LT_0	;  // skip  in the furture
	}

	rxData.msg.cmd 	= txBuf[txBufOut].data.msg.cmd;
	rxData.msg.node	= txBuf[txBufOut].data.msg.node;
	rxData.msg.id	= txBuf[txBufOut].data.msg.id;
	rxData.msg.conn = nodeId;  	// set current node as connector

	rxData.msg.val 	= txBuf[txBufOut].data.msg.val;
	rxData.msg.deltaMillis = delta;

	//Serial.print(F("tx["));Serial.print(txBufOut);Serial.print(F(">"));Serial.print(rxData.msg.id);

	return write(&rxData);
}



void NetwBase::serialize( RxMsg *msg, char *txt )
{
//	if( msg->cmd=='u' || msg->cmd=='e' || msg->cmd=='r' || msg->cmd=='s' )
//	{
//		int nodeSensor = msg->node*100+msg->id;
//
//		if(msg->deltaMillis > 0)
//			sprintf(txt, "{%c,%u,%u,%ld,%u}", msg->cmd, nodeSensor, msg->conn, msg->val, msg->deltaMillis  );
//		else if(msg->val != 0)
//			sprintf(txt,    "{%c,%u,%u,%ld}", msg->cmd, nodeSensor, msg->conn, msg->val  );
//		else
//			sprintf(txt,       "{%c,%u,%u}", msg->cmd, nodeSensor, msg->conn  );
//
//		return;
//	}

	if(msg->deltaMillis > 0)
		sprintf(txt, "{%c,%u,%u,%u,%ld,%u}", msg->cmd, msg->node, msg->id, msg->conn, msg->val, msg->deltaMillis  );
	else if(msg->val != 0)
		sprintf(txt,    "{%c,%u,%u,%u,%ld}", msg->cmd, msg->node, msg->id, msg->conn, msg->val  );
	else
		sprintf(txt,       "{%c,%u,%u,%u}", msg->cmd, msg->node, msg->id, msg->conn  );
}




void NetwBase::debug( char* id, RxItem *rxItem  )
{
	serialize( &rxItem->data.msg, strTmp);

	Serial.print(id);  Serial.println(strTmp);Serial.flush();
}
void NetwBase::debug( char* id, RxMsg *rxMsg  )
{
	serialize( rxMsg, strTmp);

	Serial.print(id);  Serial.println(strTmp);Serial.flush();
}

void NetwBase::debugTxBuf( char* id   )
{
	//if(txBuf[txBufOut].timestamp==0) return;

	sprintf(strTmp, "[%u] {%c,%u,%u,%ld}%lu", txBufOut, txBuf[txBufOut].data.msg.cmd, txBuf[txBufOut].data.msg.node, txBuf[txBufOut].data.msg.id, txBuf[txBufOut].data.msg.val, txBuf[txBufOut].timestamp   );

	Serial.print(id);  Serial.println(strTmp);    Serial.flush();
}

void NetwBase::pushChar(char c)
{
	if(c!='\r' && ( c < 0x20 || c > 0x7E )) return;
	if(c=='\r') c='}';
	//if(c==']') c='}';

	// if overflow override oldest request
	if(!empty && payLin == payLout && eolCount<1 ) empty=true;  // just skip all
 	if(!empty && payLin == payLout )
	{
		#ifdef DEBUG
		Serial.println(F("override"));
		#endif
		for(int j=0;j<PAYLOAD_LENGTH;j++)
		//for(;payLout!=payLin;payLout++)
		{
			char cc = payLoad[payLout];

			if( (cc == '{' ) && j>0 )  //||cc == '['
			{
				break; // leave the {
			}

			payLout++;
			payLout=payLout % PAYLOAD_LENGTH;

			if(   cc == '}'  && j>0 )
			{
				eolCount--;
				break;  // } removed
			}
		}
	}

	if(c=='}' )eolCount++;

	if( c=='{'
	 && eolCount==0
	 && payLin!=payLout)
	{
		payLin=payLout;   // trunc before {
	}


	empty = false;
	payLoad[payLin++] = c;
	payLin = payLin % PAYLOAD_LENGTH;
}
void NetwBase::pushChars (char str[])
{
	pushChars( str, PAYLOAD_LENGTH );
}
void NetwBase::pushChars (char str[], int len)
{
	int strPtr=0;
	char c=str[strPtr++];
	while( c != 0x00 && strPtr <  len)
	{
		pushChar(c);
		c=str[strPtr++];
	}
	return;
}

bool NetwBase::charRequestAvailable()
{
	return eolCount > 0;
}


void  NetwBase::resetPayLoad(void)
{
	empty=true;
	eolCount=0;
	payLout=0;
	payLin=0;
	Serial.println(F("pyldRst")); Serial.flush();

}

void  NetwBase::findPayLoadRequest(void)
{
 	if( empty
 	 || eolCount < 1
 	 || rxBuf[rxBufIn].timestamp >0  // full
	){
 		return;
 	}

	char * endPointer;
    char parm[16];
    int  parmPtr = 0;
    int  parmCnt = 0;
    bool truncated = false;
    bool old = false;

    for(int i=0;i<sizeof(RxMsg);i++) rxBuf[rxBufIn].data.raw[i]=0x00;
    rxBuf[rxBufIn].data.msg.node = 2; //local cmd

	while(payLout!=payLin)
	{
		char c = payLoad[payLout++];
		payLout = payLout % PAYLOAD_LENGTH;
		// string to stuct
		if(c == '{' )
		{
			parmCnt=0;
			parmPtr=0;
		}

		else if(c == ';' || c == ',' || c == ':' || c == ' ' || c == '}'   ) //|| payLout==payLin ??
		{
			//Serial.println("nextParm");
			parmCnt++;
			parm[parmPtr]=0x00;
			if(parmPtr>=sizeof(parm))
			{
				truncated = true;
				parmPtr=0;
				//Serial.println(F("truncated"));
			}
			//Serial.print(F("parmCnt++="));Serial.println(parmCnt);

			long xx;

			switch(parmCnt)
			{
		 	case 1:
		 		rxBuf[rxBufIn].data.msg.cmd =  parm[0];
		 		old = ( rxBuf[rxBufIn].data.msg.cmd == 's'
		 			 || rxBuf[rxBufIn].data.msg.cmd == 'r'
		 			 || rxBuf[rxBufIn].data.msg.cmd == 'u'
		 			 || rxBuf[rxBufIn].data.msg.cmd == 'e' );
		 		break;
		 	case 2:
				if(!old)
				{
					rxBuf[rxBufIn].data.msg.node = strtol( &parm[0], &endPointer, 10);
				}
				else
				{
					xx = strtol( &parm[0], &endPointer, 10);
					rxBuf[rxBufIn].data.msg.node = xx/100;
					rxBuf[rxBufIn].data.msg.id = xx%100;
				}
		 		break;
		 	case 3:
		 		if(!old)
					rxBuf[rxBufIn].data.msg.id = strtol( &parm[0], &endPointer, 10);
		 		else
		 			rxBuf[rxBufIn].data.msg.conn = strtol( &parm[0], &endPointer, 10);
		 		break;
		 	case 4:
		 		if(!old)
					rxBuf[rxBufIn].data.msg.conn = strtol( &parm[0], &endPointer, 10);
		 		else
		 			rxBuf[rxBufIn].data.msg.val = strtol( &parm[0], &endPointer, 10);
		 		break;
		 	case 5:
		 		if(!old)
					rxBuf[rxBufIn].data.msg.val = strtol( &parm[0], &endPointer, 10);
		 		else
		 		{
					xx = strtol( &parm[0], &endPointer, 10);
					rxBuf[rxBufIn].data.msg.deltaMillis = xx;  //strtol( &parm[0], &endPointer, 10);
		 		}
		 		break;
		 	case 6:
		 		if(!old)
		 		{
					xx = strtol( &parm[0], &endPointer, 10);
					rxBuf[rxBufIn].data.msg.deltaMillis = xx;  //strtol( &parm[0], &endPointer, 10);
		 		}
		 		break;
			default:
				break;
			}


			parmPtr=0;
		}
		else
		{
			//Serial.print("+");Serial.print(c);
			parm[parmPtr++]=c;
		}

		// handle end of line
		if( c=='}'  || payLout==payLin )
		{
			//Serial.print(F("msgEnd"));Serial.println(eolCount);
			if(eolCount>0)eolCount--;

			if(payLout==payLin)
			{
				empty = true;
			}

			if(truncated) return;

			int ret = handleEndOfLine(rxBuf[rxBufIn].data.msg.cmd, parmCnt );

			if(ret>0 || isConsole)
			{
				#ifdef DEBUG
//						Serial.print(F("rx["));	Serial.print(rxBufIn);
//						debug("<", &rxBuf[rxBufIn] );
//						Serial.flush();
				#endif
				readCount++;
				rxAddCommit();
			}
			return;
		}
		// loop next char in buffer
	}
}


int  NetwBase::getCharRequest(RxData *rxData)
{
	//if( eolCount < 0) eolCount=0;
 	if( empty || eolCount < 1  || sleepTimer > millis())
 	{
 	//	eolCount=0;
 	//	payLout=payLin;
 		return 0;
 	}

	char * endPointer;
    char parm[16];
    int  parmPtr = 0;
    int  parmCnt = 0;
    bool truncated = false;
//    bool nieuw = false;
    bool old = false;

	rxData->msg.cmd = 0x00; //16;
	rxData->msg.node = 2;	// local
	rxData->msg.id = 0;
	rxData->msg.conn = 0;
	rxData->msg.val = 0;
	rxData->msg.deltaMillis = 0L;

	while(payLout!=payLin)
	{
		char c = payLoad[payLout++];
		payLout = payLout % PAYLOAD_LENGTH;
		// string to stuct
		if(c == '{' )
		{
			parmCnt=0;
			parmPtr=0;
//			payloadType = 0;
			//Serial.println(F("new{"));
		}

		else if(c == ';' || c == ',' || c == ':' || c == ' ' || c == '}'   ) //|| payLout==payLin ??
		{
			//Serial.println("nextParm");
			parmCnt++;
			parm[parmPtr]=0x00;
			if(parmPtr>=sizeof(parm))
			{
				truncated = true;
				parmPtr=0;
				//Serial.println(F("truncated"));
			}
			//Serial.print(F("parmCnt++="));Serial.println(parmCnt);

			long xx;

			switch(parmCnt)
			{
		 	case 1:
		 		rxData->msg.cmd =  parm[0];
		 		old = (rxData->msg.cmd == 's' || rxData->msg.cmd == 'r' || rxData->msg.cmd == 'u' || rxData->msg.cmd == 'e' );
		 		break;
		 	case 2:
				if(!old)
				{
					rxData->msg.node = strtol( &parm[0], &endPointer, 10);
				}
				else
				{
				xx = strtol( &parm[0], &endPointer, 10);
				rxData->msg.node = xx/100;
				rxData->msg.id = xx % 100;
				}
		 		break;
		 	case 3:
		 		if(!old)
					rxData->msg.id = strtol( &parm[0], &endPointer, 10);
		 		else
		 		rxData->msg.conn = strtol( &parm[0], &endPointer, 10);
		 		break;
		 	case 4:
		 		if(!old)
					rxData->msg.conn = strtol( &parm[0], &endPointer, 10);
		 		else
		 		rxData->msg.val = strtol( &parm[0], &endPointer, 10);
		 		break;
		 	case 5:
		 		if(!old)
					rxData->msg.val = strtol( &parm[0], &endPointer, 10);
		 		else
		 		{
				xx = strtol( &parm[0], &endPointer, 10);
				rxData->msg.deltaMillis = xx;  //strtol( &parm[0], &endPointer, 10);
		 		}
		 		break;
		 	case 6:
		 		if(!old)
		 		{
					xx = strtol( &parm[0], &endPointer, 10);
					rxData->msg.deltaMillis = xx;  //strtol( &parm[0], &endPointer, 10);
		 		}
		 		break;
			default:
				break;
			}


			parmPtr=0;
		}
		else
		{
			//Serial.print("+");Serial.print(c);
			parm[parmPtr++]=c;
		}

		// handle end of line
		if( c=='}'  || payLout==payLin )
		{
			//Serial.print(F("msgEnd"));Serial.println(eolCount);
			if(eolCount>0)eolCount--;
//			if(eolCount<1)
//			{
//				//empty = true;
//				//payLout=payLin;
//				//eolCount=0;
//			}

			if(payLout==payLin)
			{
				empty = true;
				//eolCount=0;
			}

			if(truncated) return -31;
			int ret = handleEndOfLine(rxData->msg.cmd, parmCnt );
		//	if(ret==1 &&  c ==']' ) ret++;

			if(ret>0) readCount++;
			return ret;
//			memcpy(req, &newReqBuf, sizeof(newReqBuf));
//			if(ret==1)
//			return ret;
		}
		// loop next char in buffer
	}
	return 0; // no valid command found
}

int NetwBase::handleEndOfLine(char cmd, int parmCnt )
{
	//if(rxData->msg.conn==0 || rxData->msg.id==0 || rxData->msg.node==0) return -33;
	switch( cmd )
	{
 	case 0x00: // no command found
 		return -32;
 		break;
 	case '}': // empty command found
 	case ']': // empty command found
 		return 0;
 		break;
 	case 'S':
 	case 's':
 		return (parmCnt<4) ? -2:1;
 		break;
 	case 'R':
 	case 'r':
 		return (parmCnt<2) ? -3:1;
 		break;
 	//case 'U':
 	case 'u':
 		return (parmCnt<4) ? -4:1;
 		break;
 	//case 'E':
 	case 'e':
 		return (parmCnt<4) ? -5:1;
 		break;
	}
	return 1;
}

void  NetwBase::flushBuf(char *desc)
{
 	if( empty || eolCount < 1)
 	{
 		return;
 	}

 	Serial.print(desc); Serial.print(F("<"));

	while(payLout!=payLin)
	{
		char c = payLoad[payLout++];
		payLout = payLout % PAYLOAD_LENGTH;
		Serial.print(c);
		if(c == '}' ||c ==']')
		{
			eolCount--;
			break;
		}
	}
	if(payLout==payLin) empty=true;
	Serial.println();
}
