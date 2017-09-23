//#define DEBUG

// commments

#include "Arduino.h"
#include <inttypes.h>
#include <ArdUtils.h>

#ifndef NETWBASE_H
#define NETWBASE_H

#include <avr/wdt.h>        // watchdog

#define PING_TIMER 						7000L
#define TIMEOUT 						20000L

#define NETW_SEND_SERIAL_DATA_INTERVAL 	7L

#define NETW_I2C_SEND_ERROR_INTERVAL	70L

#define TWI_FREQ 100000L  // (250.000L TWBR=24),  100.000L > TWBR=72  50.000L > 152
// change  TWI_FREQ  		100000=1020 12byte treansactions, 50000L=520 12Byte transactions   	32000L// slow down iic for RPI bug
#define TWI_TX_TIMEOUT			2
#define TWI_RX_TIMEOUT			2
#define TWI_SEND_INTERVAL 		2
#define TWI_SEND_ERROR_INTERVAL	70

#define SPI_SEND_INTERVAL 		2L

#define TCP_SEND_INTERVAL 		2
#define TCP_SEND_ERROR_INTERVAL	500
#define TCP_SEND_TIMEOUT 		100

#define BLUE_SEND_INTERVAL 		2L
#define BLE_RESPONSE_ERROR_SLEEP	500


#define NETW_RX_BUF_CNT    	10
#define NETW_TX_BUF_CNT    	10

#define NETW_MESH_TABLE_SIZE 12
#define NETW_MESH_TABLE_KEEPALIVE 24500L  // milliseconds

#define PAYLOAD_LENGTH 32
#define NETW_MSG_LENGTH 32

//#define EVENT_SERVER 				9

//#define I2C_BUFFER_LENGTH_ERROR		-14
//#define I2C_CHECKSUM_ERROR 			-10
//#define I2C_READ_ERROR 				-13    // exception on i2cDevice.write or i2cDevice.read
//#define I2C_END_TRANSMISSION_ERROR 	-15
//#define I2C_REQUEST_FROM_ERROR 		-16   //less bytes returned than excpected
//#define I2C_READ_BUF_OVERFLOW 		-63   //less bytes returned than excpected
//#define I2C_READ_MSG_OVERFLOW 		-64   //less bytes returned than excpected


#define ERR_TIMEOUT					-40
#define ERR_TX_TIMEOUT				-41
#define ERR_RX_TIMEOUT				-42

#define ERR_TX_FULL					-43
#define ERR_RX_FULL					-44

#define ERR_TX_DELTA_3000			-45
#define ERR_TX_DELTA_LT_0			-46

#define ERR_TX_NODEID				-47
#define ERR_TWI_NOT_READY			-48

#define ERR_TWI_MTX_SLA_NACK		-49
#define ERR_TWI_MTX_DATA_NACK		-50
#define ERR_TWI_MTX_ARB_LOST		-51
#define ERR_TWI_SRX_FULL			-52
#define ERR_TWI_BUS_ERROR			-53

#define ERR_TCP_NOT_READY			-54
#define ERR_TCP_SEND				-55

#define ERR_BLE_CONN				-56
#define ERR_BLE_DISC				-57
#define ERR_BLE_INIT				-58
#define ERR_BLE_QUERY				-59
#define ERR_BLE_SET					-60
#define ERR_BLE_MASTER				-61

struct RxMsg //   12 lang   cmd 21 = getParm,   cmd 14, 16 =setParm
{
	byte 			cmd;   			// upl(1), err(3), set(16), refresh(21)
	int 			node;  			// origianl source node
	byte 			id;    			// sensorId
	int 			conn;  			// via node
	long			val;  			// always long
	unsigned int 	deltaMillis;   	// max 64K * 0.1 seconde = 1,82 uur  of 0.01 seconde  = 10,9 minuten
};

union RxData
{
	byte 				raw[ sizeof(RxMsg)];
	RxMsg				msg;
};
struct RxItem
{
	RxData			data;
	unsigned long   timestamp;
};


struct TxMsg //   13 lang
{
	byte 			cmd;   			// upl(1), err(3), set(16), refresh(21)
	int 			node;  			// origianl source node
	byte 			id;    			// sensorId
	long			val;  			// always long
};
union TxData
{
	byte 			raw[ sizeof(TxMsg)];
	TxMsg			msg;
};
struct TxItem
{
	TxData 			data;
	unsigned long   timestamp;
	uint8_t   		tries;
};


//struct MeshItem
//{
//     int 			node;
//     int 			conn;
//     unsigned long 	lastMillis;
//};


//class NetwMesh
//{
//public:
//    MeshItem meshTable[NETW_MESH_TABLE_SIZE];
//    int getMeshConn(int node);
//    void saveMeshConn(int node, int conn);
//    void saveMeshConn( RxMsg *rxMsg);
//};

class NetwBase
{
private:

public:
//    volatile long readErrorCount=0;

	volatile long sendHealthCount=0;
	volatile int  lastSendError=0;
    int lastSendErrorUploaded = lastSendError;

    volatile int  lastReadError = 0;

    volatile int  lastError=0;
    int lastErrorUploaded = lastError;
    uint8_t lastErrorLogLevel = 1;

	volatile unsigned long netwTimer = 100;
	volatile unsigned long bootTimer;
	volatile unsigned long pingTimer;
	volatile unsigned long timeOutTimer = TIMEOUT;
	volatile unsigned long sleepTimer = 0;

	int nodeId = 0;
	int id = 80;
	bool isParent = false;
	bool isConsole = false;
	bool isMeshEnabled = false;
	uint8_t  uploadNode=0;
//	bool skipCR = false;
    int ramFree = 2048;

//    char payloadLower = '<';
    char payLoad[PAYLOAD_LENGTH];
//    char payloadUpper = '>';
    volatile int  payLin = 1;  // one based pointer
    volatile int  payLout = 1;
    volatile bool empty = true;
    volatile int  eolCount = 0;

	bool    txAutoCommit = true;
	bool isTxFull(void){return     txBuf[txBufIn].timestamp > 0 ;}
    TxItem 	txBuf[NETW_TX_BUF_CNT];  //NETW_RX_BUF_CNT

    volatile uint8_t txBufIn = 0;
    volatile uint8_t txBufOut = 0;
    volatile uint8_t txBufCurrOut = 0;
    volatile uint8_t txBufIndex = 0;

    volatile unsigned long txTimeOut = 0;

	volatile unsigned int sendCount=0;
	volatile unsigned int sendErrorCount=0;
	volatile unsigned int sendRetryCount=0;
	unsigned int sendCountNextUpload=3;
	unsigned int sendErrorNextUpload=0;
	unsigned int sendRetryNextUpload=0;

	bool isRxFull(void){return rxBuf[rxBufIn].timestamp > 0 ;}
	RxItem 	 rxBuf[NETW_RX_BUF_CNT];  // timestamp = 0 > empty
    volatile uint8_t rxBufIn = 0;
    volatile uint8_t rxBufOut = 0;
    volatile uint8_t rxBufIndex = 0;

    volatile unsigned int readCount=0;
	unsigned int readCountNextUpload=0;

    char  strTmp[NETW_MSG_LENGTH];

    int getMeshConn(int node);
    void saveMeshConn( RxMsg *rxMsg);

    int (*user_onReceive) (RxItem *rxItem);
    void onReceive( int (*function)(RxItem *rxItem) )
    {
    	user_onReceive = function;
    }

    void (*user_onRollBack)(byte id, long val);
    void onRollBack( void (*)(byte id, long val));

	int (*uploadFunc) (int id, long val, unsigned long timeStamp) = 0;
    void onUpload( int (*function)(int id, long val, unsigned long timeStamp) )
    {
    	uploadFunc = function;
    }

	int (*errorFunc) (int id, long val ) = 0;
    void onError( int (*function)(int id, long val ) )
    {
    	errorFunc = function;
    }


	virtual ~NetwBase(){}
	NetwBase( )
	{
		netwTimer = 100;
		bootTimer = netwTimer;
		pingTimer = bootTimer;
		for(int i=0;i<NETW_RX_BUF_CNT;i++) rxBuf[i].timestamp =0;
		for(int i=0;i<NETW_TX_BUF_CNT;i++) txBuf[i].timestamp =0;
	}
    int upload(int id);

//	int getVal( int id, long * value );
	int setVal( int id, long value );

	void pushChar (char c);
	void pushChars (char str[]);
	void pushChars (char str[], int len);
	bool charRequestAvailable();
	void flushBuf(char *desc);
	int  getCharRequest(RxData *req);
	void resetPayLoad(void);
	void findPayLoadRequest(void);
	int  handleEndOfLine(char cmd, int parmCnt );

	char getPayloadChar()
	{
		char x = payLoad[payLout];
		payLoad[payLout++]=0x00;
		payLout = payLout % PAYLOAD_LENGTH;
		return x;
	}
	int getPayloadInt()
	{
		int x = payLoad[payLout];
		payLoad[payLout++]=0x00;
		payLout = payLout % PAYLOAD_LENGTH;
		return x;
	}

	virtual void loop();
			void loopPing(void);
			void loopRx(void);

	virtual void txCommit(void);
	virtual void txCancel(void);
	virtual void rxCommit(void);
			void rxAddCommit(void); //

	virtual bool isBusy() {return ( millis() <= netwTimer);}  // diff between isBusy and isReady
	virtual bool isReady(){return ( millis() >  netwTimer);}

	// put into sendBuf
	int txUpload(byte id, long val) 							{return putTxBuf('U', nodeId, id, val, millis());}
	int txUpload(byte id, long val, unsigned long timeStamp) 	{return putTxBuf('U', nodeId, id, val, timeStamp);}
	int txError (byte id, long val) 							{return putTxBuf('E', nodeId, id, val, millis());}
	int txCmd   (byte cmd, int nodeId)  						{return putTxBuf(cmd, nodeId, 0,    0, millis());}
	int txCmd   (byte cmd, int nodeId, byte id)					{return putTxBuf(cmd, nodeId, id,   0, millis());}
	int txCmd   (byte cmd, int nodeId, byte id, long val)		{return putTxBuf(cmd, nodeId, id, val, millis());}

	int putTxBuf(byte cmd, int nodeId, byte id, long val, unsigned long timeStamp);
	int putTxBuf(RxItem *rxItem);

	// physical write
	virtual int write( RxData *rxData);
    int writeTxBuf(void);

	void serialize( RxMsg *msg, char *txt );
	void trace(char* id);
	void debug(char* id, RxItem *rxItem );
	void debug(char* id, RxMsg *rxMsg );
	void debugTxBuf( char* id   );

	void pgmcpy(char *src,  const char *pgm, int len )
	{
		for(int i = 0; i < len; i++) src[i] = pgm_read_word_near(pgm +i);
	}
};

#endif



//
//struct Err //   10 lang   cmd 21 = getParm,   cmd 14, 16 =setParm
//{
//	byte 	cmd;   // 'E'
//	int 	node;  // server=0
//	byte 	id;    // sensorId
//	int 	conn;  // via node
//	long	val;  // always long
//};
//
//struct Set //   12 lang   cmd 21 = getParm,   cmd 14, 16 =setParm
//{
//	byte 	cmd;   // 'S'
//	int 	node;  // server=0
//	byte 	id;    // sensorId
//	int 	conn;  // via node
//	long	val;  // always long
//};
//struct CmdS //   12 lang   cmd 21 = getParm,   cmd 14, 16 =setParm
//{
//	byte 	cmd;   // 'S'
//	int 	node;  // server=0
//	byte 	id;    // sensorId
//	int 	conn;  // via node
//	long	val;  // always long
//};
//struct Cmd //   12 lang   cmd 21 = getParm,   cmd 14, 16 =setParm
//{
//	byte 	cmd;   // 'C'
//	int 	node;  // server=0
//	byte 	id;    // sensorId
//	int     conn;
//	long    val;
//};
//struct CmdP //   12 lang   cmd 21 = getParm,   cmd 14, 16 =setParm
//{
//	byte 	cmd;
//	char    text[];
//};
//struct Upload //   12 lang   cmd 21 = getParm,   cmd 14, 16 =setParm
//{
//	byte 			cmd;   // upl(1), err(3), set(16), refresh(21)
//	int 			node;  // server=0
//	byte 			id;    // sensorId
//	int 			conn;  // via node
//	long			val;  // always long
//	unsigned int 	deltaMillis;
//};
