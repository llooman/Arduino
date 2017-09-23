/*

  The Chint util class/library by LLO 2013-10-19

2017-01-12 switch to netw.timeStamp


  Copyright (c) 2013-2013 J&L Computing.  All right reserved.

  The Arduino Mega (or nano) is connected to the Chint using the RS232 protocol
  Before you can get data from the Chint you need to associate! (send the serialnr and a destignation adress to the Chint)
  After a succesful associate you then need the destignation adres to comminicate
  You don't need to assosiate before every query, the chint will hold its adres.
  Only after the night because the Chint did shut off you will need a new associate 

  You can query for the parameters and the status of the chint
  For the power produced you only need the status!
  When getting parameters or staus you first query the stucture.
  The bytes in the structure will tell you what you can read
  I found the definition of those bytes in a perl script
  You might check your data by comparing it with the display of the Chint
  
  !! Probably you can have more invertors connected to the same RS232 kabel.
  The dest is then used to query a specific invertor

  i.e. on a Arduino MEGA you can use Serial1 pin 18, 19
  to overcome the 5v to 12v there are several MAX... chips
  
		5V    -----------------------|
                18 Tx ---- TTL(5v) ---- Rx   -----------   Rs232(12v) Tx ---\/--- Tx pin 2   
Arduino MEGA    19 Rx ---- TTL(5v) ---- Tx   | MAX3223 |   Rs232(12v) Rx ---/\--- Rx pin 3  Chint
                GND   ---------------- Gnd - -----------   ---------- Gnd ---------- pin 5		  

  !! Tx is connected to Rx and vice versa



  2015-02-12 llo 1.0.1 replace begin by adding the discover to assosiate so begin is not necessary anymore

  Rev 1.0 - May 15th, 2013

  TODO semafore to prevent buffer overwrite when 2 actions run at the same     requestBusy
       getResponse generic return valu

*/

//#define DEBUG

#include <Arduino.h> 

#define CHINT_SOURCE 	 0x0100  	// default adress for the Arduino
#define CHINT_DEST 		 0x0001     // default  adress for the Chint

#define CHINT_RESPONSE_TIME 200 // millis

#define ERR_CHINT_READ				-60
#define ERR_CHINT_ASSOCIATE			-61
#define ERR_CHINT_REQUEST			-62
#define ERR_CHINT_GET_STRUCT		-63
#define ERR_CHINT_GET_STATUS		-64
#define ERR_CHINT_DATA				-65
#define ERR_CHINT_READ_OVERFLOW		-66
#define ERR_CHINT_NO_RESPONSE		-67
#define ERR_CHINT_INVALID_RESPONSE	-68

#ifndef Chint_h
#define Chint_h

#define CMD_Reset                          0x0004        // Reset communications
#define CMD_Discover                       0x0000        // Computer discovers devices
#define CMD_AdresRegistration              0x0001        // Address registration
#define CMD_ParameterFrameStructureRequest  0x0101        // Parameter frame structure request
#define CMD_ParameterRequest               0x0104        // Parameter request
#define CMD_StatusFrameStructureRequest    0x0100         
#define CMD_StatusRequest                  0x0102         
#define CMD_VersionStringRequest           0x0103         


#define Parm

/*  cmd's not tested jet
    CMD_RMV =    0x0002        # Disconnect
    CMD_RMV_R =    0x0082        # Disconnect ACK
    CMD_RCT =    0x0003        # Reconnect all devices
    CMD_SP0 =    0x0200        # Set Vpv-Start
    CMD_SP0_R =    0x0280        # Set Vpv-Start ACK
    CMD_SP1 =    0x0201        # Set T-Start
    CMD_SP1_R =    0x0281        # Set T-Start ACK
    CMD_SP2 =    0x0204        # Set Vac-Min
    CMD_SP2_R =    0x0284        # Set Vac-Min ACK
    CMD_SP3 =    0x0205        # Set Vac-Max
    CMD_SP3_R =    0x0285        # Set Vac-Max ACK
    CMD_SP4 =    0x0206        # Set Fac-Max
    CMD_SP4_R =    0x0286        # Set Fac-Max ACK
    CMD_SP5 =    0x0207        # Set Fac-Min
    CMD_SP5_R =    0x0287        # Set Fac-Min ACK
    CMD_SP6 =    0x0208        # Set DZac-Max
    CMD_SP6_R =    0x0288        # Set DZac-Max ACK
    CMD_SP7 =    0x0209        # Set DZac
    CMD_SP7_R =    0x0289        # Set DZac ACK
    CMD_ZRO =    0x0300        # Reset inverter E-Total and h-Total
    CMD_ZRO_R =    0x0380        # Reset inverter E-Total and h-Total ACK

*/

class Chint
{
  private:
		int source = CHINT_SOURCE;         // just an adress for the Arduino
		int dest = CHINT_DEST;            // just an adress for the Chint
		void hexPrint(byte buf[], int size);

  public:
		bool 	active = false;
	    HardwareSerial* serialPort;  //  i.e. serialPort = &Serial1;
	    int 		retCode = 0;
	    int 		retryCnt = 0;

		bool 	isDiscovered = false;
		bool 	isReset = false;
		bool 	isAssociated = false;

		bool 	offLine = false;

	    int 	serialnrSize = 10;
	    char 	serialnr[16] ;

		int 	layoutSize = 0;
		byte 	layout[32];  // temp buf storing the format

	    int 	currCmd;
	    unsigned long responseTimer = 0;
	    byte 	responseBuf[100];
	    int		responsePtr = 0;

	    byte 	dataBuf[64];

	    int 	requestSize = 0;
	    byte 	requestBuf[24];

	    byte 	parameterFormat[16];

	    volatile int  lastError=0;
	    int lastErrorUploaded = lastError;
	    uint8_t lastErrorLogLevel = 1;

		unsigned long timeStamp;

		long readInterval  = 58000L;
		long uploadInterval = 58000L;   // default sence every 60 sec

		unsigned long readTimer = 1000;
		unsigned long eTotalTimer = 1000;
		unsigned long eTodayTimer = 1100;
		unsigned long tempTimer = 1200;
		unsigned long powerTimer = 1300;

		long 		prevEToday  = 0;
		int  		id = 0;

	    // keep track last valid values read from the chint
	    long  		power_l = 500;
		float 		temp_f = 60.5;
	    float 		eToday_f = 0.87;  // energie vandaag
	    float       prevEToday_f = eToday_f;
	    float 		eTotal_f = 3300.0; 	 // energie totaal
	    long  		iDc = 3;  // stroom panelen
	    long  		uDc = 128;  // spanning panelen

		int (*uploadFunc) (int id, long val, unsigned long timeStamp) = 0;
		int (*errorFunc) (int id, long val ) = 0;
	    void onUpload( int (*function)(int id, long val, unsigned long timeStamp) )
	    {
	    	uploadFunc = function;
	    }

	    void onError( int (*function)(int id, long val ) )
	    {
	    	errorFunc = function;
	    }

    Chint( HardwareSerial & serial )  //int source, int dest,
    {
		serialPort = &serial;
		requestSize = 0;

		strcpy(( char*) serialnr, "1104DN0178"); // loopback test value
		serialnrSize = 10;
		isDiscovered = true;
	}

	int setVal( int id, long value );
    void responseError(int errCode);
    void loop(void);
    void begin(void);

    int statusResponse(int dataSize);

    void upload(int id);

    void sendCmd ( int cmd );			// rs232 send to chint
    void sendCmd ( int dest, int cmd );
    void sendCmd ( int cmd, int dataSize, byte *data );
    void sendCmd ( int dest, int cmd, int dataSize, byte *data );

	void setReadInterval(int);
	void setUploadInterval(int);

	void hexPrintRequest(void);
    void hexPrintResponse(void);
};


#endif

