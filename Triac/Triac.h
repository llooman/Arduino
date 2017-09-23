/*   
 Triac.h
 Copyright (c) 2013-2014 J&L Computing.  All right reserved.
 Rev 1.0 - 2013-10-02

2017-01-15 zeroCrossing(): limit on 100Hz
2017-01-13 add timeStampZeroCross
2015-05-10 v1.00 LLO  Triac  and Fet

 //   16Mhz divider 255 = 65536 ticks/sec  =  1310 /Hz  =  655 /ZeroCrossing
 //   50Hz & 16MHz divider 255 > 1 half sinus = 655 timer ticks
 //   100 halve sinusen in 1000 ms = 10 ms per halve sinus

 //  if( millis() = timeStampZeroCross + (  delay / 65 ) ) netw.timeStamp = millis()+1

 #include <Time.h>
 #include <Triac.h>

 Triac myTriac ;

 setup:
 myTriac.setup();
 attachInterrupt( myTriac.interupt(), zeroCrossingInterrupt, RISING);

 loop:
 myTriac.loop();

 data:
 level = myTriac.level;  		// sync level min/max adjustments
 myTriac.set( int );			// returns the current validated value
 myTriac.up();				// + 20%
 myTriac.down(); 				// - 20%


 *  bridge to Triac class. It is not possible to fix this in class code.

 void zeroCrossingInterrupt(){ myTriac.zeroCrossing();  }
 ISR(TIMER1_COMPA_vect)      { myTriac.timerEvent();    } // timer1 comparator match
 ISR(TIMER1_OVF_vect)        { myTriac.timerOverflow(); }

 uses pin 11 = Timer2


 // Triac Triac v 1.0 2015-05-10
 // oposite of fet Triac.
 //
 // This arduino sketch is for use with the heater
 // control circuit board which includes a zero
 // crossing detect fucntion and an opto-isolated triac.
 //
 // AC Phase control is accomplished using the internal
 // hardware timer1 in the arduino
 //
 // Timing Sequence
 // * timer is set up but disabled
 // * zero crossing detected on pin 2
 // * timer starts counting from zero
 // * comparator set to "delay to on" value
 // * counter reaches comparator value
 // * comparator ISR turns on triac gate
 // * counter set to overflow - pulse width
 // * counter reaches overflow
 // * overflow ISR truns off triac gate
 // * triac stops conducting at next zero cross

 // digitalPinToInterrupt
 */

//#include <Arduino.h>
//#include <inttypes.h>

#ifndef Triac_h
#define Triac_h


#define PULSE_WITH 7              // trigger pulse width for triac  (in timer counts)
#define MIN_LEVEL 0
#define MAX_LEVEL 100
// set the reach (bereik) in timer ticks
#define MIN_DELAY_TIMER_TICKS 110    // sin 0,5 > 1/6 van 655 = 110
#define MAX_DELAY_TIMER_TICKS 590    // 5/6 van 655 = 545

class Triac
{
private:
	int minDelay = MIN_DELAY_TIMER_TICKS;
	int maxDelay = MAX_DELAY_TIMER_TICKS;
	int prevLevel;
	int delayBereik;
	int levelBereik = MAX_LEVEL - MIN_LEVEL;
	boolean prevActive = active;

	unsigned long prevUSec = 0;

	boolean autoFadeUp = true;		// start up direction when doing autoFade

	long acTicks = -20;
	long prevAcTicks = 0;

public:
	int id=0;

	volatile unsigned long tsZeroCross;
	volatile unsigned long tsGateHigh;
	volatile unsigned long tsNextZeroCross;
	volatile int delayTicks;

//	bool available = false;
//	unsigned long availableTimer = 0;

	long syncInterval = 58000L;   // default sence every 60 sec
	unsigned long syncTimer = 50;

	int levelIst = MIN_LEVEL - 5;		// off during startup
	int levelSoll = MIN_LEVEL;
	int prevLevelSoll = levelSoll;

	bool active = false;

	int TIMER_IN_USE;     // Timer 1 or Timer 2
	int gatePin;

	int zeroCrossingPin;


	Triac(int timerInUse, int gatePin, int zeroCrossingPin, int id)
	{
		TIMER_IN_USE = timerInUse;
		this->zeroCrossingPin = zeroCrossingPin;
		this->gatePin = gatePin;
		this->id = id;

		delayTicks = MIN_DELAY_TIMER_TICKS;
		prevLevel = levelSoll - 1; 				// trigger  calcDelayTicks on initialize
		delayBereik = maxDelay - minDelay;
	}

	int (*uploadFunc) (int id, long val, unsigned long timeStamp) = 0;
    void onUpload( int (*function)(int id, long val, unsigned long timeStamp) )
    {
    	uploadFunc = function;
    }
    void upload(void);
	void begin(void);
	void trace(void);
	void zeroCrossing(void);
	void gateHigh(void);
	void gateLow(void);
//	int interupt();
	void loop(void);
	void loop(int level);

	int up(void);
	int down(void);
	int set(int newLevel);
	void autoFade(void);
};


//#define MIN_DELAY_TIMER_TICKS 110    // sin 0,5 > 1/6 van 655 = 110
//#define MAX_DELAY_TIMER_TICKS 590    // 5/6 van 655 = 545
void Triac::upload()
{
	if(uploadFunc==0 ) return;

	if( uploadFunc(id, levelSoll, millis() ) > 0)
	{
		syncTimer = millis() + syncInterval ;
		prevLevelSoll = levelSoll;
	}
	else
	{
		syncTimer = millis() + id ;
	}
}

void Triac::loop()
{
//	int ticks2Go = ( micros() - prevUSec ) * 1;

	if( millis() > syncTimer
	 || prevLevelSoll != levelSoll
	){
		upload() ;
	}

	// fade when level change
	if (levelSoll != levelIst)
	{
		if (levelIst < levelSoll)
			levelIst = levelIst + max(7, levelSoll - levelIst);
		if (levelIst > levelSoll)
			levelIst = levelIst - max(7, levelIst - levelSoll);
		if (levelIst > MAX_LEVEL)
		{
			levelIst = MAX_LEVEL;
			levelSoll = levelIst;
		}
		if (levelIst < MIN_LEVEL)
		{
			levelIst = MIN_LEVEL;
			levelSoll = levelIst;
		}

		//available = true;
		//availableTimer = millis() + 7L;
	}

	// calc level > delayTicks
	if (prevLevel != levelIst)
	{
		// inverted because the higher the perc the lower the delay
		float perc = (float) levelIst / levelBereik;
		delayTicks = maxDelay - (delayBereik * perc);
		if (delayTicks > maxDelay)
			delayTicks = maxDelay;
		if (delayTicks < minDelay)
			delayTicks = minDelay;

		prevLevel = levelIst;

		active = levelIst > MIN_LEVEL ? true : false;
		tsGateHigh = tsZeroCross+(delayTicks/65)-1;
	}

}

void Triac::loop(int level)
{
	this->levelSoll = level;
	loop();
}


int Triac::set(int newLevel)
{
	levelSoll = newLevel;
	if (levelSoll > MAX_LEVEL)
		levelSoll = MAX_LEVEL;
	if (levelSoll < MIN_LEVEL)
		levelSoll = MIN_LEVEL;
	return levelSoll;
}
int Triac::up()
{
	levelSoll = levelSoll + 20;
	if (levelSoll > MAX_LEVEL)
		levelSoll = MAX_LEVEL;
	return levelSoll;
}
int Triac::down()
{
	levelSoll = levelSoll - 20;
	if (levelSoll < MIN_LEVEL)
		levelSoll = MIN_LEVEL;
	return levelSoll;
}

void Triac::begin()   // Used 0 or 1
{

	if (gatePin > 0)
	{
		pinMode(gatePin, OUTPUT);      //triac gate control
	}

#ifdef RELAIS
	if (relaisPin > 0)
		pinMode(relaisPin, OUTPUT);    // relais master 230 V
#endif

	// set up Timer1
	//(see ATMEGA 328 data sheet pg 134 for more details)
	if (TIMER_IN_USE == 1)
	{
		TCCR1B = 0x00;    // timer disabled
		TIMSK1 = 0x03;    //enable comparator A and overflow interrupts
		TCCR1A = 0x00;    //timer control registers set for
	}
	if (TIMER_IN_USE == 2)
	{
		TCCR2B = 0x00;    // timer disabled
		TIMSK2 = 0x03;    //enable comparator A and overflow interrupts
		TCCR2A = 0x00;    //timer control registers set for
	}

	pinMode(zeroCrossingPin, INPUT_PULLUP);

}


void Triac::gateHigh()
{
	if (active)
	{
		digitalWrite(gatePin, HIGH);  //set triac gate to high
		//prevUSec=micros();
		if (TIMER_IN_USE == 1)
		{
			TCNT1 = 65536 - PULSE_WITH; // set timer close to overflow, this will trigger the below overflow
		}
		if (TIMER_IN_USE == 2)
		{
			TCNT2 = 65536 - PULSE_WITH; // set timer close to overflow, this will trigger the below overflow
		}
	}
}

void Triac::gateLow()
{
	digitalWrite(gatePin, LOW); //turn off triac gate
	if (TIMER_IN_USE == 1)
	{
		TCCR1B = 0x00;          //disable timer stopd unintended trigger
	}
	if (TIMER_IN_USE == 2)
	{
		TCCR2B = 0x00;          //disable timer stopd unintended trigger
	}
}

void Triac::zeroCrossing()
{
	if(millis() < tsNextZeroCross) return; // limit on 100Hz

	//noInterrupts();
	tsZeroCross = millis();
	tsGateHigh = tsZeroCross+(delayTicks/65)-1;
	tsNextZeroCross = tsZeroCross+9;

	// prevents flickering! don't know why
	digitalWrite(gatePin, LOW); // always turn off triac gate = prevents flickering

	acTicks++;

	if (active)
	{
		if (TIMER_IN_USE == 1)
		{
			OCR1A = (int) delayTicks;
			TCNT1 = 0;   //reset timer - count from zero
			TCCR1B = 0x04; //start timer with divide by 256 input
		}
		if (TIMER_IN_USE == 2)
		{
			OCR2A = (int) delayTicks;
			TCNT2 = 0;   //reset timer - count from zero
			TCCR2B = 0x04; //start timer with divide by 256 input
		}
	}
}





void Triac::autoFade()
{
//	if( currLevel >= MAX_LEVEL ) autoFadeUp = false;
//	if( currLevel <= MIN_LEVEL ) autoFadeUp = true;
//
//	level = level + ( autoFadeUp ? 20 : - 20);

	levelSoll = levelSoll + 3;
}


void Triac::trace()
{

	float deltaAcTicks = acTicks - prevAcTicks;
	prevAcTicks = acTicks;

	Serial.print("@ ");
	Serial.print(millis()/1000L);
	Serial.print(": ac=");
	Serial.print(deltaAcTicks);
	Serial.print(", active=");
	Serial.print(active);
	Serial.print(", soll=");
	Serial.print(levelSoll);
	Serial.print(", ist=");
	Serial.print(levelIst);
	Serial.print(", delayTicks=");
	Serial.print(delayTicks);
	Serial.println("");

}

#endif
