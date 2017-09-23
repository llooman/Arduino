#include "NetwTWI.h"
#include <compat/twi.h>

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

void NetwTWI::rxCommit()
{
	timeOut = millis()+TWI_RESTART;
	NetwBase::rxCommit();
}

void NetwTWI::txCommit()
{
	timeOut = millis()+TWI_RESTART;
	NetwBase::txCommit();
}

void NetwTWI::loop()  // TODO  parentFlag or client ????
{

	if(tw_stopIssued)
	{
		if(!( TWCR & _BV(TWSTO) ))
		{
			if( tw_state == TWI_MTX)
			{
				tw_masterBufferIndex = 0;
				tw_masterBufferLength = 0;
			}
			tw_state = TWI_READY;
			tw_stopIssued = false;
		}
	}


	if( tw_rxBufferIndex > 0     //tw_rxBufferIndex   tw_rxBufferIndex[tw_rxBufferIndex++]
	 && tw_state == TWI_READY )
	{
		readCount++;
		if( isRxFull() )
		{
			#ifdef DEBUG
				{  Serial.println(F("rxBuf overflow!")); Serial.flush();}
			#endif
			lastError =	ERR_RX_FULL;
		}
		else
		{
			for(int i=0; i<sizeof(RxMsg); i++)
			{
				rxBuf[rxBufIn].data.raw[i] = i<tw_rxBufferIndex ? tw_rxBuffer[i] : 0x00;
			}

			#ifdef DEBUG
				{  Serial.print(F("rx["));Serial.print(rxBufIn);Serial.println(F("<")); Serial.flush();}
			#endif

			rxAddCommit( );
//			rxBuf[rxBufIn++].timestamp = millis();
//			rxBufIn = rxBufIn % NETW_RX_BUF_CNT;
		}

		tw_rxBufferIndex=0;
	}

	NetwBase::loop();

	if( txTimeOut > 0
	 && millis()>txTimeOut)
	{
		tw_releaseBus();
		lastError = ERR_TX_TIMEOUT;
		//Serial.println(F("txTimeOut"));
		return;
	}

	if( tw_rxTimeOut > 0
	 && millis()>tw_rxTimeOut)
	{
		tw_releaseBus();
		lastError = ERR_RX_TIMEOUT;
		//Serial.println(F("tw_rxTimeOut"));
		return;
	}

	if( timeOut > 0
	 && millis()>timeOut)
	{
		timeOut = millis()+TWI_RESTART;
		tw_restart();
		lastError = ERR_TIMEOUT;
		//Serial.println(F("TWI.timeOut"));
		return;
	}

}


int NetwTWI::write(RxData *rxData)  // TODO
{
	if(tw_state != TWI_READY)
	{
		#ifdef DEBUG
			Serial.println("twSnd: !ready");
		#endif
		//lastError = ERR_TWI_NOT_READY;  // no need for reporting
		return ERR_TWI_NOT_READY;
	}

	uint8_t toAddress = rxData->msg.node;

	if( isParent
	 || rxData->msg.cmd == 'U'
	 || rxData->msg.cmd == 'u'
	 || rxData->msg.cmd == 'E'
	 || rxData->msg.cmd == 'e'
	){
		toAddress = twUploadNode; // override address where the nodeId is from instead of to
	}
	else if (isMeshEnabled)
	{
		int connId = getMeshConn(toAddress);
		if(connId>0)
		{
			toAddress = connId;
		}
	}

	#ifdef DEBUG
		Serial.print(millis());
		Serial.print(F(">"));
		Serial.print(toAddress);
		Serial.print(F(">"));
		Serial.print((char)rxData->msg.cmd);
		Serial.print(F(" "));
		Serial.print( rxData->msg.node);
		Serial.print(F("."));
		Serial.print( rxData->msg.id);
		Serial.print(F("."));
		Serial.print( nodeId);
		Serial.print(F(" "));
		Serial.print( rxData->msg.val);
		Serial.print(F(" "));
		Serial.print( rxData->msg.deltaMillis);
		Serial.println();
		Serial.flush();
	#endif


	for(int i=0; i<sizeof(RxMsg); i++)
	{
		tw_masterBuffer[i]=rxData->raw[i];
	}

	tw_masterBuffer[4]=lowByte(nodeId);
	tw_masterBuffer[5]=highByte(nodeId);

	int twSendSize=6;
	if( rxData->msg.val!=0) twSendSize=10;
	if( rxData->msg.deltaMillis >0)twSendSize=12;

	//Serial.print("wrtAddr=");Serial.println(address);

	// build sla+w, slave device address + w bit
	tw_slarw = TW_WRITE;
	tw_slarw |= toAddress << 1;

	tw_masterBufferIndex = 0;
	tw_masterBufferLength = twSendSize;

	tw_state = TWI_MTX;
    tw_error = 0;
    txTimeOut = millis()+TWI_TX_TIMEOUT;

	netwTimer = millis()+TWI_SEND_INTERVAL;

//	if(nodeId==rxData->msg.node) pingTimer = millis() + PING_TIMER;

	//	Serial.print(F("w>"));Serial.flush();

	TWCR = _BV(TWINT) | _BV(TWEA)  | _BV(TWIE) | _BV(TWEN) | _BV(TWSTA);	// send start condition // enable INTs

	//while(!(TWCR & (1<<TWINT))){
	//Serial.print(F("<w"));Serial.flush();
	//}

	return 0;
}

void NetwTWI::begin( )
{
  id=isParent?81:82;
  txAutoCommit = false;
  isMeshEnabled = !isParent;

  txTimeOut = 0;
  tw_rxTimeOut = 0;
  timeOut = millis()+60000;

  if(isParent)
  {
	  slaveAdr = nodeId ;
  }
  else
  {
	  slaveAdr = twUploadNode;
  }

  // initialize state
  tw_state = TWI_READY;
//  tw_sendStop = true;		// default value
//  tw_inRepStart = false;
  tw_stopIssued = false;


  // activate internal pullups for twi.
  // llo  pinMode SDA, INPUT_PULLUP
#ifdef NETW_TWI_OPEN_LINES

#else
  digitalWrite(SDA, 1);
  digitalWrite(SCL, 1);
#endif

  // initialize twi prescaler and bit rate
  cbi(TWSR, TWPS0);
  cbi(TWSR, TWPS1);
  TWBR = ((F_CPU / TWI_FREQ) - 16) / 2;

  /* twi bit rate formula from atmega128 manual pg 204
  SCL Frequency = CPU Clock Frequency / (16 + (2 * TWBR))
  note: TWBR should be 10 or higher for master mode
  It is 72 for a 16mhz Wiring board with 100kHz TWI */

  // set twi slave address (skip over TWGCE bit)
  TWAR = slaveAdr << 1;

  // enable twi module, acks, and twi interrupt
  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
}
/*
 * void init_TWI(void) // Most TWI commands From http://www.embedds.com/programming-avr-i2c-interface/
{
	TWSR = 0x00; // Clears the Status Code Register and all prescalers.
	TWBR = 0x08; // Sets the Bus frequency to 100kHz
	TWCR |= (1<<TWEN); // Enables the TWI Interface.
}
 * */
void NetwTWI::tw_disable(void)
{
  // disable twi module, acks, and twi interrupt
  TWCR &= ~(_BV(TWEN) | _BV(TWIE) | _BV(TWEA));

  // deactivate internal pullups for twi.
  digitalWrite(SDA, 0);
  digitalWrite(SCL, 0);
  txTimeOut = 0;
  tw_rxTimeOut = 0;
}


void NetwTWI::tw_setAddress(uint8_t address)
{
  // set twi slave address (skip over TWGCE bit)
  TWAR = address << 1;
}


void NetwTWI::tw_reply(uint8_t ack)
{
  // transmit master read ready signal, with or without ack
  if(ack)
  {
	  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT) | _BV(TWEA);
  }
  else
  {
	  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT);
  }
}

bool  NetwTWI::isBusy(void){ return (  tw_state != TWI_READY ); } // Wire.isBusy()
bool  NetwTWI::isReady(void){ return ( millis() > netwTimer &&  tw_state == TWI_READY ); }//  && Wire.isReady()



/*void TWI_Start(void)
{
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN); // Sends the start signal
	while (!(TWCR & (1<<TWINT)))
	{
		//Waits until the start signal has been sent.
	}
}
 * */


void NetwTWI::tw_stop(void)
{
  // send stop condition
  if(tw_state == TWI_MTX) tw_stopMTX();


  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWINT) | _BV(TWSTO);

  // wait for stop condition to be exectued on bus
  // TWINT is not set after a stop condition!

  tw_stopIssued = true;

}
void  NetwTWI::tw_releaseBus(void)
{
//	if(tw_state == TWI_MTX) tw_stopMTX();

	// release bus
	TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWINT);

	// update twi state
	tw_state = TWI_READY;

	if(millis()>txTimeOut)
	{
		netwTimer = millis()+( TWI_SEND_INTERVAL*2L); //
	}
	txTimeOut = 0;
	tw_rxTimeOut = 0;

}

void NetwTWI::tw_stopMTX()
{
	if( tw_masterBufferIndex >= tw_masterBufferLength )
	{
		txCommit();
		netwTimer = millis() + TWI_SEND_INTERVAL;
		#ifdef DEBUG
			{Serial.println(F("&"));   Serial.flush();}
		#endif
	}
	else
	{
		//int tries = sendBuffer[sendBufOut].send.retries;
		#ifdef DEBUG
			{Serial.print(F("!")); Serial.print(tw_masterBufferIndex);  Serial.flush();}
		#endif

		netwTimer = millis() + TWI_SEND_INTERVAL * txBuf[txBufOut].tries;  //NETW_I2C_SEND_ERROR_INTERVAL;
		#ifdef DEBUG
		//	{  Serial.println(- tw_error); Serial.flush();}
		#endif
	}
}



void NetwTWI::tw_restart(void)
{
	// disable twi module, acks, and twi interrupt
	TWCR &= ~(_BV(TWEN) | _BV(TWIE) | _BV(TWEA));

	#ifdef DEBUG
		{  Serial.println("twi timeOut restart"); Serial.flush();}
	#endif

	// deactivate internal pullups for twi.
	//		digitalWrite(SDA, 0);
	//		digitalWrite(SCL, 0);
	//		txTimeOut = 0;
	//		tw_rxTimeOut = 0;

	delay(50);
	begin();  // TODO save settings
}



void  NetwTWI::tw_int()
{
	#ifdef DEBUG
		//  Serial.print(F("^"));Serial.print(TW_STATUS);Serial.flush();
	#endif
  switch(TW_STATUS)
  {
	  /*
	   *  All Master
	   */
    case TW_START:     // sent start condition
    case TW_REP_START: // sent repeated start condition
      // copy device address and r/w bit to output register and ack

      TWDR = tw_slarw;
      tw_reply(1);
      break;

    /*
     *   Master Transmitter
     */
    case TW_MT_SLA_ACK:  // slave receiver acked address
    case TW_MT_DATA_ACK: // slave receiver acked data
      // if there is data to send, send it, otherwise stop
		#ifdef DEBUG
		//	  Serial.print(F("+"));Serial.flush();
		#endif
      if(tw_masterBufferIndex < tw_masterBufferLength)
      {
        // copy data to output register and ack
        TWDR = tw_masterBuffer[tw_masterBufferIndex++];
        tw_reply(1);
      }
      else
      {
//		if (twi_sendStop)
//		{
			tw_stop();

//		}
//		else
//		{
//		  twi_inRepStart = true;	// we're gonna send the START
//		  // don't enable the interrupt. We'll generate the start, but we
//		  // avoid handling the interrupt until we're in the next transaction,
//		  // at the point where we would normally issue the start.
//		  TWCR = _BV(TWINT) | _BV(TWSTA)| _BV(TWEN) ;
//		  twi_state = TWI_READY;
//		}
      }
      break;

    case TW_MT_SLA_NACK:  // address sent, nack received
      tw_error = TW_MT_SLA_NACK;
      lastError = ERR_TWI_MTX_SLA_NACK;
		#ifdef DEBUG
			  Serial.println(F("-")); Serial.flush();
		#endif
      tw_stop();
      break;
    case TW_MT_DATA_NACK: // data sent, nack received
      tw_error = TW_MT_DATA_NACK;
      lastError = ERR_TWI_MTX_DATA_NACK;
		#ifdef DEBUG
			  Serial.println(F("-")); Serial.flush();
		#endif
      tw_stop();
      break;
    case TW_MT_ARB_LOST: // lost bus arbitration
      tw_error = TW_MT_ARB_LOST;
		#ifdef DEBUG
			  Serial.println(F("-")); Serial.flush();
		#endif
      lastError = ERR_TWI_MTX_ARB_LOST;
      tw_releaseBus();
      break;

      /*
       *   Master Receiver
       */
    case TW_MR_DATA_ACK: // data received, ack sent
      // put byte into buffer
      tw_masterBuffer[tw_masterBufferIndex++] = TWDR;
    case TW_MR_SLA_ACK:  // address sent, ack received
      // ack if more bytes are expected, otherwise nack
      if(tw_masterBufferIndex < tw_masterBufferLength)
      {
        tw_reply(1);
      }
      else
      {
        tw_reply(0);
      }
      break;

    case TW_MR_DATA_NACK: // data received, nack sent
      // put final byte into buffer
      tw_masterBuffer[tw_masterBufferIndex++] = TWDR;
//		if (twi_sendStop)
//		{
			tw_stop();
//		}
//		else
//		{
//		  twi_inRepStart = true;	// we're gonna send the START
//		  // don't enable the interrupt. We'll generate the start, but we
//		  // avoid handling the interrupt until we're in the next transaction,
//		  // at the point where we would normally issue the start.
//		  TWCR = _BV(TWINT) | _BV(TWSTA)| _BV(TWEN) ;
//		  twi_state = TWI_READY;
//		}
	break;
    case TW_MR_SLA_NACK: // address sent, nack received
      tw_stop();
      break;
    // TW_MR_ARB_LOST handled by TW_MT_ARB_LOST case


      /*
       *   Slave Receiver
       */
    case TW_SR_SLA_ACK:   // addressed, returned ack
    case TW_SR_GCALL_ACK: // addressed generally, returned ack
    case TW_SR_ARB_LOST_SLA_ACK:   // lost arbitration, returned ack
    case TW_SR_ARB_LOST_GCALL_ACK: // lost arbitration, returned ack
//        if(isRxFull())
//        {
//			#ifdef DEBUG
//			  Serial.println(F("!")); Serial.flush();
//			#endif
//			lastError = ERR_RX_FULL;
//			tw_reply(0);  // otherwise nack
//        }
//        else
        {
		  // enter slave receiver mode
		  tw_state = TWI_SRX;
		  // indicate that rx buffer can be overwritten and ack
		  tw_rxBufferIndex=0;  //      rxBufIndex = 0;
		  tw_rxTimeOut = 0;
		  tw_reply(1);
        }
      break;
    case TW_SR_DATA_ACK:       // data received, returned ack
    case TW_SR_GCALL_DATA_ACK: // data received generally, returned ack
      // if there is still room in the rx buffer
//      if(isRxFull())
//      {
//		#ifdef DEBUG
//		  Serial.println(F("!")); Serial.flush();
//		#endif
//		lastError = ERR_RX_FULL;
//		tw_reply(0);  // otherwise nack
//      }
//      else
    	  if(tw_rxBufferIndex < TWI_BUFFER_LENGTH )
      {
        // put byte in buffer and ack
			#ifdef DEBUG
		//		Serial.println(F("&")); Serial.flush();
			#endif
    	  tw_rxBuffer[tw_rxBufferIndex++]= TWDR;   // rxBuf[rxBufIn].data.raw[rxBufIndex++] = TWDR;
    	  tw_rxTimeOut = millis()+TWI_RX_TIMEOUT;
    	  tw_reply(1);
      }
      else
      {
		#ifdef DEBUG
    	  Serial.println(F("!")); Serial.flush();
		#endif
        lastError = ERR_TWI_SRX_FULL;
        tw_reply(0);  // otherwise nack
      }
      break;
    case TW_SR_STOP: // stop or repeated start condition received
      // ack future responses and leave slave receiver state
      tw_releaseBus();
      // put a null char after data if there's room
      if(tw_rxBufferIndex < TWI_BUFFER_LENGTH)
      {
    	  tw_rxBuffer[tw_rxBufferIndex]= 0x00;  // rxBuf[rxBufIn].data.raw[rxBufIndex] = 0x00;
      }
      break;

    case TW_SR_DATA_NACK:       // data received, returned nack
    case TW_SR_GCALL_DATA_NACK: // data received generally, returned nack
#ifdef DEBUG
      Serial.println(F("~")); Serial.flush();
#endif
      // nack back at master
      tw_reply(0);
      tw_rxBufferIndex=0;
	  tw_rxTimeOut = 0;

      break;

      /*
       *   Slave Transmitter
       */
    case TW_ST_SLA_ACK:          // addressed, returned ack
    case TW_ST_ARB_LOST_SLA_ACK: // arbitration lost, returned ack
      // enter slave transmitter mode
      tw_state = TWI_STX;
      // ready the tx buffer index for iteration
      tw_txBufferIndex = 0;
      // set tx buffer length to be zero, to verify if user changes it
      tw_txBufferLength = 0;
      // request for txBuffer to be filled and length to be set
      // note: user must call twi_transmit(bytes, length) to do this
//      twi_onSlaveTransmit();
      // if they didn't change buffer & length, initialize it
      if(0 == tw_txBufferLength)
      {
        tw_txBufferLength = 1;
        tw_txBuffer[0] = 0x00;
      }
      // transmit first byte from buffer, fall
    case TW_ST_DATA_ACK: // byte sent, ack returned
      // copy data to output register
      TWDR = tw_txBuffer[tw_txBufferIndex++];
      // if there is more to send, ack, otherwise nack
      if(tw_txBufferIndex < tw_txBufferLength)
      {
        tw_reply(1);
      }
      else
      {
        tw_reply(0);
      }
      break;

    case TW_ST_DATA_NACK: // received nack, we are done
    case TW_ST_LAST_DATA: // received ack, but we are done already!
      tw_reply(1);				// ack future responses
      tw_state = TWI_READY;   	// leave slave receiver state
      break;

      /*
       *   All
       */
    case TW_NO_INFO:   // no state information
      break;
    case TW_BUS_ERROR: // bus error, illegal stop/start
      tw_error = TW_BUS_ERROR;
      tw_stop();
      lastError = ERR_TWI_BUS_ERROR;
      break;
  }
}


void NetwTWI::openLines(int digitalPort)
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


void NetwTWI::trace(char* id)
{
	Serial.print(F("@ "));
	Serial.print(millis()/1000);
	Serial.print(F(" "));Serial.print(id);
	Serial.print(F(": TWI="));	 Serial.print(nodeId);
	Serial.print(F(", slave="));	 Serial.print(slaveAdr);
	Serial.print(F(", mesh="));	 Serial.print(isMeshEnabled);


	Serial.print(F(", nwTmr=")); Serial.print( netwTimer/1000  );
	Serial.print(F(", ping=")); Serial.print( pingTimer/1000  );

	Serial.print(F(", rx=")); Serial.print( rxBufIn );
	Serial.print(F("-")); Serial.print( rxBufOut );
	Serial.print(F("-")); Serial.print( rxBuf[rxBufOut].timestamp>0 );
	Serial.print(F(" cnt=")); Serial.print( readCount );

//	Serial.print(F(", rxPtr="));Serial.print( rxBufIndex );

	Serial.print(F(", tx=")); Serial.print( txBufIn );
	Serial.print(F("-")); Serial.print( txBufOut );
	Serial.print(F("-")); Serial.print( txBuf[txBufOut].timestamp>0 );
	Serial.print(F(" cnt=")); Serial.print( sendCount );
	Serial.print(F(" err=")); Serial.print( sendErrorCount );
	Serial.print(F(" rtry=")); Serial.print( sendRetryCount );
	Serial.print(F(" intrv="));Serial.print( TWI_SEND_INTERVAL );

	Serial.print(F(", tw_state="));Serial.print( tw_state );
	Serial.print(F(" err="));Serial.print( tw_error );
	Serial.print(F(" lastError="));Serial.print( lastError );


	Serial.println();
}

