#include <Arduino.h>
#include <Chint.h>


void Chint::responseError(int errCode)
{
	lastError = errCode;
    retCode = 0;
	isDiscovered = false;
	readTimer = millis() + 30000L;

	if(retryCnt < 10) retryCnt++;
	if(retryCnt > 5)  offLine = true;

	#ifdef DEBUG
		Serial.print(F("responseError="));Serial.print(errCode);Serial.print(F(" cmd="));Serial.println(currCmd, HEX);
	#endif
}


void Chint::loop()
{
	/*
	 * collect the response in the responseBuf
	 */
	if(responseTimer>0)
	{
		while(serialPort->available() &&  responsePtr < sizeof(responseBuf) )
		{
			responseBuf[responsePtr++] = serialPort->read();
		}

		if( responsePtr >= sizeof(responseBuf) )
		{
			retCode = ERR_CHINT_READ_OVERFLOW;
			responseTimer = 0;
		}
	}


	/*
	 * when the responseTimer ends analyse the responseBuf depending on the command send
	 */
	if( responseTimer>0
	 && millis() > responseTimer
	){
		int responseDataSize = 0;
		#ifdef DEBUG
				hexPrintResponse();
		#endif
		if(currCmd == CMD_Reset)
		{
			isReset = true;
			isAssociated = false;
		}
		else if( responsePtr < 1
		){
			responseError(ERR_CHINT_NO_RESPONSE);
		}
		else if( responseBuf[6] != requestBuf[6]
		      || responseBuf[7] != ( requestBuf[7] | 0x80 )
		){
			responseError(ERR_CHINT_INVALID_RESPONSE);
		}
		else
		{
			// copy data from te responceBut to the dataBuf
			responseDataSize = responseBuf[8];       // the length of the serial nr

			if(currCmd == CMD_Discover)
			{
				serialnrSize = responseDataSize;
			    memcpy( &serialnr[0], &responseBuf[9], serialnrSize );  // TODO check if this is working serialSize replaced by responseDataSize
			    serialnr[serialnrSize] = 0x00; // null terminated string
				#ifdef DEBUG
					  Serial.print(F("the serial nr = ")); Serial.println( (char*) &serialnr[0] );
				#endif
			    isDiscovered = true;
			    isReset = false;
			    isAssociated = false;
			}

			else if(currCmd == CMD_AdresRegistration)
			{
				isAssociated = true;
				//layoutSize = 0;
			}

			else if(currCmd == CMD_StatusFrameStructureRequest)
			{
				layoutSize = responseDataSize;
				memcpy( &layout[0],  &responseBuf[9], layoutSize );
			}

			else if(currCmd == CMD_StatusRequest)
			{
				retCode = statusResponse(responseDataSize);
				if(retCode<0)
				{
					if( retryCnt < 10 ) retryCnt++;
					isReset = false;
				}
				else
				{
					offLine = false;
					retryCnt = 0;
				}
			}
		}

		responseTimer = 0;
	}

	/*
	 * trigger the next command to the Chint.
	 *
	 * Start with a reset
	 * is Serialnumber is unknown you need to Discover it
	 * Once the serialnumber is known you need to associate it with a dest adress.
	 * Then we can query the layout of the status msg.
	 *
	 * Only then we can start querying the status. statusResponse() will analyse the status response using the layout retrieved earlier.
	 *
	 */
	if( active
	 && millis() > readTimer
	 && responseTimer == 0
	){
		if( ! isReset)
		{
			sendCmd ( 0x0000, CMD_Reset);
		}
		else if( ! isDiscovered)
		{
			sendCmd ( 0x0000, CMD_Discover);
		}
		else if( ! isAssociated )
		{
		    int dataSize = serialnrSize;
		    memcpy( &dataBuf[0], &serialnr[0], dataSize );
		    dataBuf[dataSize++] = highByte(dest);   // append the new destination adress to the serialnr just found
		    dataBuf[dataSize++] = lowByte(dest);

		    sendCmd ( 0x0000, CMD_AdresRegistration, dataSize, dataBuf );

		    responseTimer = millis() + CHINT_RESPONSE_TIME + 100;
		}
		else if (layoutSize < 1)
		{
			sendCmd( CMD_StatusFrameStructureRequest );
		}
		else
		{
			sendCmd( CMD_StatusRequest );
			readTimer = millis() + readInterval;
		}

		return;
	}


	if( uploadFunc!=0
	 && retCode > 0
	){
		if(prevEToday_f != eToday_f)		upload(55);
		else if( millis() > eTotalTimer)	upload(54);
		else if( millis() > eTodayTimer)	upload(55);
		else if( millis() > tempTimer)		upload(56);
		else if( millis() > powerTimer )	upload(57);
	}


	if(retCode<0) lastError = retCode;

	if( lastError != lastErrorUploaded
	 && errorFunc !=0
	){
		if( ( lastErrorLogLevel > 0 && ( lastError == -67 || lastError == -68 ))
//		 ||	( lastErrorLogLevel > 1 && ( lastError == -51 || lastError == -51 ))
		){
			lastErrorUploaded = lastError;
		}
		else
		{
			if(errorFunc(id, lastError) >= 0 )
			{
				lastErrorUploaded = lastError;
			}
		}
	}
}

int Chint::statusResponse(int dataSize)
{
	long  			_power;
	float 			_temp;
	float 			_etoday;
	unsigned int   eAcHighBit;
	unsigned int   eAcLowByte;
	long   			_iDc;
	long   			_uDc;

	boolean eAc_HighBitFound = false;
	boolean eAc_LowByteFound = false;

	// de-tokenise the status bytes
	for (int i = 0; i < layoutSize; i++)
	{
		byte parameter = layout[i];
		int varValue = (responseBuf[ 9 + i*2 ] << 8) | responseBuf[ 9 + (i*2)+1 ];

		if ( parameter == 0x00 ) _temp =  varValue * 0.1 ; 			//var1=36.2  00 *.1 graden C  Temp
		if ( parameter == 0x0D ) _etoday =  varValue * 0.01 ; 		//var2=.78 kwh   etoday		0D *.01 kwh etoday
		if ( parameter == 0x44 ) _power =  varValue  ;				//var7=748   W  pac 		44 *1 W output power
		if ( parameter == 0x41 ) _iDc =  varValue  * 0.1 ;			//var4=3.2  A ?			41 *.1 A stroom panelen
		if ( parameter == 0x40 ) _uDc =  varValue  * 0.1 ;			//var3=172.4			40 *.1 v voltage panelen

		/*    Etotal is in 2 status bytes
			var9=0    			47 * 256 kwh etotal high bit
			var10=25.3  eac			48 * .1 kwh etotal low bytes   */
		if ( parameter == 0x47 ) { eAcHighBit = varValue; eAc_HighBitFound = true;   }
		if ( parameter == 0x48 ) { eAcLowByte = varValue; eAc_LowByteFound = true;   }
	}

	/*
	var5=236.7 v			42 *.1 v voltage net
	var6=50.06  hz			43 *.01 Hz net
	var8=-1				45 * mOhm net weerstand
	working hours
	var11=0				49 * 256 hrs working hours
	var12=141			4A *1 hrs
	var13=1				4C operating mode  0=wait, 1=normal
	ERRORS:
	var14=0				78  gv
	var15=0				79	GF
	var16=0				7A	GZ
	var17=0				7B	TMP
	var18=0				7C PV1
	var19=0				7D	GFC1
	var20=0				7E  ERROR MODE
	var21=0				7F  ??
	*/


		// only if both etotal (=eAc) bytes are found then it make sence to calc the etotal

		if( !eAc_HighBitFound  &&  !eAc_LowByteFound ) return ERR_CHINT_DATA;


	  	timeStamp = millis();  // remember the timestamp of these values
		this->power_l = _power;
		this->temp_f = _temp;
		this->eToday_f = _etoday;
		this->iDc = _iDc;
		this->uDc = _uDc;
		this->eTotal_f = eAcLowByte * 0.1 + eAcHighBit * 265.0;

		#ifdef DEBUG
			Serial.print(F("total ")); Serial.print(eTotal_f); Serial.println(F(" kwh"));
			Serial.print(F("uDc ")); Serial.print(_uDc); Serial.println(F(" V"));
			Serial.print(F("Temp ")); Serial.print(_temp); Serial.println(F(" graden C"));
			Serial.print(F("Etoday ")); Serial.print(_etoday); Serial.println(F(" kwh"));
			Serial.print(F("Power ")); Serial.print(_power); Serial.println(F(" w"));
			Serial.print(F("iDc ")); Serial.print(_iDc); Serial.println(F(" A"));
		#endif

		return 1;


}

void Chint::upload(int id)
{
	if(uploadFunc==0 || timeStamp < 1) return;

	switch( id )
	{
	case 80: uploadFunc(id, lastErrorLogLevel, millis() ); break;

	case 54:
		if( uploadFunc(id, eTotal_f, 0 ) > 0)  //timeStamp
			eTotalTimer = millis() + uploadInterval ;
		else
			eTotalTimer = millis() + id ;
		break;
	case 55:
		if( uploadFunc(id, eToday_f * 100.0 , 0 ) > 0)
		{
			eTodayTimer = millis() + uploadInterval ;
			prevEToday_f = eToday_f;
		}
		else
			eTodayTimer = millis() + id ;
		break;
	case 56:
		// ?? check age  skip to old values
		if(offLine)
			tempTimer = millis() + 300000L;
		else if( uploadFunc(id, temp_f * 10.0, timeStamp ) > 0)
			tempTimer = millis() + uploadInterval ;
		else
			tempTimer = millis() + id ;
		break;
	case 57:
		if(offLine)
			tempTimer = millis() + 300000L;
		else if( uploadFunc(id, power_l , timeStamp ) > 0)
			powerTimer = millis() + uploadInterval ;
		else
			powerTimer = millis() + id ;
		break;
	}
}

int Chint::setVal( int id, long value )
{
	switch(id)
	{
	case 80: lastErrorLogLevel = value;  break;


 	default: return -1; break;
	}
	return 1;
}


void Chint::begin( void )
{
	active = true;
	isAssociated = false;
	readTimer = millis() + 1000 ; // initialy wait for half a second.
}

void Chint::setReadInterval(int periode_s)
{
	readInterval = periode_s * 1000L;
}
void Chint::setUploadInterval(int periode_s)
{
	uploadInterval = periode_s * 1000L;
}


void Chint::sendCmd( int cmd ) 							{ sendCmd( dest, cmd, 0, dataBuf );}   // just send a command using no data and using the assosiated destignation
void Chint::sendCmd( int dest, int cmd )				{ sendCmd( dest, cmd, 0, dataBuf );}  // send a command using no data and specify a dest. This is nessesary for i.e. a reset which uses dest = 0x0000
void Chint::sendCmd( int cmd, int dataSize, byte *data)	{ sendCmd( dest, cmd, dataSize, data );} // an assosiate needs to send a serial nr in the data and also set the new dest adres!
void Chint::sendCmd( int dest, int cmd, int dataSize, byte *data )
{
    currCmd = cmd;

	#ifdef DEBUG
		Serial.print(F("sendCmd="));
		if(currCmd == CMD_Reset) Serial.println(F("Reset"));
		else if (currCmd == CMD_Discover) Serial.println(F("Discover"));
		else if (currCmd == CMD_AdresRegistration) Serial.println(F("AdresRegistration"));
		else if (currCmd == CMD_StatusFrameStructureRequest) Serial.println(F("StatusFrameStructureRequest"));
		else if (currCmd == CMD_StatusRequest) Serial.println(F("StatusRequest"));
		else   Serial.println(currCmd, HEX);
	#endif

	unsigned int checksum = 0;
	requestSize = 9 + dataSize;

	requestBuf[0] = 0xaa;           // wakeup
	requestBuf[1] = 0xaa;
	requestBuf[2] = highByte(source);
	requestBuf[3] = lowByte(source);
	requestBuf[4] = highByte(dest);
	requestBuf[5] = lowByte(dest);
	requestBuf[6] = highByte(cmd);
	requestBuf[7] = lowByte(cmd);
	requestBuf[8] = (byte) dataSize;

	if (dataSize > 0)
	  memcpy( &requestBuf[9], data, dataSize);

	for (int x = 0; x < requestSize; x++)
	  checksum += requestBuf[x];

	requestBuf[requestSize++] =  highByte(checksum);
	requestBuf[requestSize++] = lowByte(checksum);

	#ifdef DEBUG
		hexPrintRequest();
	#endif

	responsePtr = 0;
	responseBuf[6] = 0x00;	// clean for checking received data
	responseBuf[7] = 0x00;
	serialPort->write(requestBuf, requestSize);
	responseTimer = millis() + CHINT_RESPONSE_TIME;
}

void Chint::hexPrintRequest(void)
{
    hexPrint( requestBuf, requestSize );
    Serial.println(">");
}

void Chint::hexPrintResponse(void)
{
    hexPrint( responseBuf, responsePtr );
    Serial.println("<");
}

void Chint::hexPrint(byte buf[], int size)
{
  for (int i = 0; i < size; i++) {
    if (buf[i] < 0x10)
      Serial.print("0");
    Serial.print(buf[i],HEX);
  }
}
