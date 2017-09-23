
#ifndef NETWTCP_H
#define NETWTCP_H

//#define DEBUG

#include "NetwBase.h"

#include <SPI.h>
#include <Ethernet.h>
#include "ArdUtils.h"

#define SS_SD_CARD   4
#define SS_ETHERNET 10

#define LOCAL_IP 192,168,178,100
#define LOCAL_PORT 9000
#define IOT_SERVER 192,168,178,48  // 51 raspbian2  48 raspbian3
#define IOT_PORT 9001

class NetwTCP : public NetwBase
{
public:

	virtual ~NetwTCP(){}  // suppress warning
	NetwTCP(void){}

	uint8_t status();
	EthernetClient requestClient = 0;
    bool isSending = false;
	char sendBuf[NETW_MSG_LENGTH];

	boolean ack = false;
	boolean ok = false;
	unsigned long  tcpSendTimer;

	void setup(void);
	void begin(void);

	int write( RxData *rxData);
	void loop(void);
	bool isBusy(void);
    bool isReady(void);
	bool isAvailable();

	void trace(char* id);

};

#endif
