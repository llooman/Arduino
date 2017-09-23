#ifndef NETWTWI_H
#define NETWTWI_H

#include "NetwBase.h"

//#define DEBUG

#define TWI_BUFFER_LENGTH   	20
#define TWI_RESTART 			21000L			// restart after 21 seconds inactivity



#define TWI_READY 0
#define TWI_MRX   1

#define TWI_MTX   2

#define TWI_SRX   3
#define TWI_STX   4

class NetwTWI: public NetwBase
{
public:

	volatile uint8_t 	tw_slarw;
	volatile uint8_t 	tw_state;
	// not jet used
//	volatile uint8_t 	tw_sendStop;			// should the transaction end with a stop
//	volatile uint8_t 	tw_inRepStart;			// in the middle of a repeated start

	volatile unsigned long timeOut = TWI_RESTART;
	volatile uint8_t 	tw_stopIssued;
	volatile uint8_t 	tw_error;

	uint8_t tw_masterBuffer[TWI_BUFFER_LENGTH];
	volatile uint8_t tw_masterBufferIndex;
	volatile uint8_t tw_masterBufferLength;

	uint8_t tw_txBuffer[TWI_BUFFER_LENGTH];
	volatile uint8_t tw_txBufferIndex;
	volatile uint8_t tw_txBufferLength;

	uint8_t tw_rxBuffer[TWI_BUFFER_LENGTH];
	volatile uint8_t tw_rxBufferIndex=0;
	volatile unsigned long tw_rxTimeOut=0;

    byte slaveAdr = 0x00;
    byte twUploadNode = 0x09;  // default for upload


	NetwTWI(){}
	virtual ~NetwTWI(){}  // suppress warning

	void begin();
	void tw_restart(void);
	void tw_disable(void);
	void tw_setAddress(uint8_t address);
	void tw_int(void);
	void tw_reply(uint8_t ack);
	void tw_stop(void);
	void tw_releaseBus(void);
	void tw_stopMTX(void);

	void txCommit(void);
	void rxCommit(void);

	void loop(void);

    void trace(char* id);

    bool isReady(void);
	bool isBusy(void);
	int write(RxData *rxData);

	void pullUpsOff()
	{
	  digitalWrite(SDA, 0);
	  digitalWrite(SCL, 0);
	}
	void pullUpsOn()
	{
	  digitalWrite(SDA, 1);
	  digitalWrite(SCL, 1);
	}

	void openLines(int digitalPort);
};

#endif
