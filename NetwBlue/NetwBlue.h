
//#define DEBUG

#ifndef NetwBlue_H
#define NetwBlue_H

#define AT_RESPONSE_TIMEOUT 1000
//#define AT_ROLE_TIMEOUT  25
#define AT_RESPONSE_SLEEP 60
#define AT_SLEEP_ON_START 1000
#define AT_RESPONSE_SLEEP_ON_ERROR 500

#define BLE_RESPONSE_MILLIS 50
#define BLE_RELAX_MILLIS 400
#define BLE_RESPONSE_TIMEOUT_MILLIS 1000
#define BLE_RESPONSE_DISC_MILLIS 5000

#include "ArdUtils.h"
#include "NetwSerial.h"
#include <avr/pgmspace.h>

const char atIMME[] PROGMEM = "IMME";  // Module work type 0: work immediately,  1: When module is powered Until AT + START, AT+CON, AT+CONNL
const char atROLE[] PROGMEM = "ROLE";  // Master and Slaver Role  0: Peripheral, 1: Central
const char atNOTI[] PROGMEM = "NOTI";  // Notify information 0: Don’t Notify, 1: Notify //when link ESTABLISHED or LOSTED send OK+CONN or OK+LOST
const char atMODE[] PROGMEM = "MODE";  // Module Work Mode 0: Transmission Mode, 1: PIO collection Mode + Mode 0, 2: Remote Control Mode + Mode 0

const char atNOTP[]  PROGMEM = "NOTP"; // notify mode 0: without address 1: with address

const char atEMPTY[] PROGMEM = "";
const char atCON[]   PROGMEM = "CON";
const char atNAME[]  PROGMEM = "NAME";
const char atVERS[]  PROGMEM = "VERS";
const char atADDR[]  PROGMEM = "ADDR";
const char atPOWE[]  PROGMEM = "POWE";  // Module Power 0: -23dbm, 1: -6dbm, 2: 0dbm, 3: 6dbm
const char atTEMP[]  PROGMEM = "TEMP";
const char atUUID[]  PROGMEM = "UUID";
const char atADTY[]  PROGMEM = "ADTY";
const char atSAVE[]  PROGMEM = "SAVE"; // Module save connected address information 0:Save when connected, 1:Don’t Save
const char atSHOW[]  PROGMEM = "SHOW";
const char atRENEW[] PROGMEM = "RENEW";
const char atRESET[] PROGMEM = "RESET";
const char atTYPE[]  PROGMEM = "TYPE";  // 0~2  0:Not need PIN Code, 1:Auth not need PIN, 2:Auth with PIN, 3:Auth and bond
const char atBAUD[]  PROGMEM = "BAUD";
const char atCOMP[]  PROGMEM = "COMP";
const char atDISC[]  PROGMEM = "DISC";
const char atSCAN[]  PROGMEM = "SCAN";  // Module discovery time when module in master role 1 ~ 9 Default: 3 Seconds
const char atTCON[]  PROGMEM = "TCON";  // connect remote device timeout value 000000~999999  millis
const char atPWRM[]  PROGMEM = "PWRM"; // Module sleep type 0:Auto sleep, 1:don’t auto sleep


class NetwBlue : public NetwSerial
{
public:

	uint8_t bleRole = 0;  //bleRole: 0 normaly slave, 1=fixed master server, 2=fixed slave

	bool    isMasterByDefault = false;

    int 	respPtr=0;
    char  	respBuf[84];
	char    writeBuf[PAYLOAD_LENGTH];

    bool    autoConnect=true;
    bool    autoDisconnect=true;
    bool    verbose=false; //true
    bool    verboseSave=verbose;
//    bool 	showResponse=false;
    int	 	powerPin = 0;

    bool    atLineFeed=false;
    bool    atMode=false;			// true=don't inpetreted commands rx for

    char    ROLE='-';
    char    MODE='-';
    char    IMME='-';
    char    NOTI='-';

    unsigned long 	msgTimer = 0;
    unsigned long 	atTimer = 700;
	bool 			atOkFound = false;
	bool 			atOkOnly = false;


    bool    resetTriggered;
    uint8_t	resetFlag=0;
    bool    connectTriggered;
    uint8_t connectFlag=0;
    uint8_t	infoFlag=0;
    bool    masterTriggered;
    uint8_t	masterFlag=0;
    bool    slaveTriggered;
    uint8_t	slaveFlag=0;

    uint8_t	discoverFlag=0;

    uint8_t queryFlag=0;
    uint8_t	writeFlag=0;
    uint8_t	initFlag=1;

    int		writeTo=0;
    int 	connectedTo=0;

    bool    loopPending=false;
    bool	atResult=0;



    unsigned long   discoverTimer = 0;


    //const char *bleAddres[4] = { " ", "04A31606FC6F", "A81B6AAE5922", "A81B6AAE4F66" };


	virtual ~NetwBlue(){}  // suppress warning

	NetwBlue( )
	{
		serialPtr = &Serial;
		this->port=0;
	}
	NetwBlue(int nodeId )
	{
		serialPtr = &Serial;
		this->port = 0;
		this->nodeId=nodeId;
	}

	void setup( int pin )									{setup(pin, 0,    0, 			   115200);}
	void setup( int pin, int port)							{setup(pin, port, 0,               115200);}
	void setup( int pin, int port, bool masterByDefault)	{setup(pin, port, masterByDefault, 115200);}
	void setup( int pin, int port, bool masterByDefault, long baudrate);
	bool getAddress(int nodeId, char *dest);


	void version();
	void restore();  // default settings
	bool auth();
	void setAuth(char auth);
	void restart();
	void reset();

	bool sendAny(char * cmd);
	bool ATOK();
	bool ATquery( const char cmd[] );
//	bool ATquery( const char cmd[], int responseSleep );
	bool ATSet(const char * atCmd, int atVal);
	bool ATSet(const char * atCmd, char * atVal);
	bool ATSet(const char * atCmd, const char * atVal);
	bool ATSet(char * atCmd,       char * atVal);
//	bool ATSet(char * atCmd, char * atVal, int responseSleep);
	bool ATResponse(bool cmd); //, int responseSleep


	bool isReady(void);
	bool isBusy(void);
	bool isMaster(void);
	bool isSlave(void);

	void initLoop();
	void masterLoop();
	void slaveLoop();
	void infoLoop();
	void resetLoop();
	void connectLoop(void);
	void discoverLoop();
	void queryLoop();
	void writeLoop(void);

	int write( RxData *req);


	bool print(char * cmd);
	bool println(char * cmd);

	void responseLoop(void);

	void loop();
	bool connect(int toNode);
	bool connect(const char * address);


    void trace(char* id);

private:

};

#endif
