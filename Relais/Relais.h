/*   
  Relais.h
  Copyright (c) 2013-2014 J&L Computing.  All right reserved.
  Rev 1.0 - 2013-10-02

2017-01-10  sendDataWhenAvailable retry
  
  This is an switch utility. You can specify a reference value where the switch changes
  Also we can invert = exchange on with off
  The reference value can have a hysteresis 
  There is a minimal delay periode in seconds where the state can not change to prevent knipperen
  Also we can specify an auto active periode i.e. once every 24 hours a pomp should work for 1 minuut.

  Use checkValue(...) 			to compare the value which can trigger a status change. Minimal load allows calling from an network interrupt
  Use loop() frequently 	to check if the status can change

2015-02-06 v1.02 LLO add _switchOn/OffValue
2015-01-18 v1.01 LLO add timeStamp
  
*/
 
//#define DEBUG

#include <Arduino.h> 
#include <inttypes.h>

//#include "NetwBase.h"

#ifndef Relais_h
#define Relais_h

//#define compileWithDebug        // enable Serial.print messages for monitoring/debugging

class Relais
{
  public:
	int id = 0;
//	int autoId = 0;

    unsigned long	timeStampLastSetPin;

	boolean 	istValue;				// current logical value
	boolean		sollValue;
	boolean 	inverted;			// verwissel aan/uit: (false: on when value > switchValue,  true: on when value < switchValue)
	long		startDelay_s;
	boolean		test;
	long 		minimalActive_s;
	long 		minimalInactive_s;
	long syncInterval = 58000L;   // default sence every 60 sec
	unsigned long syncTimer = 500;
	// TODO

	int 		available = 0;		// new value available
	bool 		checkActive = false;
	float		floatValue;
	float		_floatValue;
	int			frequency;
    float 		_switchOnValue;
	float 		_switchOffValue;
//	time_t      lastNetwUpdateTimer = 0;

    Relais( int pin )
    {
    	//_value 				= false;
        startDelay_s		= 3;
        this->inverted 		= false;
    	init( pin );
    }
    Relais( int pin, bool inverted )
    {
    	//_value 				= false;
        startDelay_s		= 3;
    	this->inverted 		= inverted;

    	init( pin );
    }
    Relais( int pin, bool inverted, int ID )
    {
    	//_value 				= false;
        startDelay_s		= 3;
    	this->inverted 		= inverted;
    	id=ID;
    	init( pin );
    }


    void init( int pin )
    {
    	_inverted				= inverted;
    	istValue 				= _inverted;
    	sollValue 			    = istValue;

    	_autoActivatePeriode_s = 0;				// disable autoActivate by default
    	_pin 					 = pin;				// store the pin

    	_hysteresis			 = 0;
    	setRefValue(0);								// this will also init  _switchOnValue _switchOffValue

    	_minimalActive_s 		 = 20;				// default debounce time
    	_minimalInactive_s 	 = _minimalActive_s;

    	timeStampLastSetPin 			 = millis();
    	timeStamp 		 = millis() + 1000 * startDelay_s;
    	_prevTimeStamp 		 = millis();

    	test				 = false;
    	checkActive = false;
    	frequency = 0;								// switches per hour
    	for ( int i = 0
    			; i < ( sizeof( relaisChanges )/sizeof( unsigned long ))
    			; i++
    	) relaisChanges[i] = 0;

    	pinMode(_pin, OUTPUT);  // OUTPUT always without pullup
    }

	int (*uploadFunc) (int id, long val, unsigned long timeStamp) = 0;
    void onUpload( int (*function)(int id, long val, unsigned long timeStamp) )
    {
    	uploadFunc = function;
    }
    void 		upload(void);
    boolean 	loop( float currentValue );                       // process basic checks ie. timers/status changes

    //          in tiende graden i.e.:   105 = 10.5 degrees
    void		setRefValue( int sollValue ); 			// the minimal number of seconds before the state can change to prevent klaperen
    void	  	setHysteresis( int hysteresis );

	void 		setAutoActivatePeriode_s( long autoActivatePeriode );
    void     	checkValue( float sollValue );

    int			getSwitchFrequency();

	boolean		ist();			// logical value
	boolean		soll();
	boolean		value();
	boolean		isRunning();			// absolute/physical value
	boolean		isStopped();

	void		on();

	void		off();

	void		switchOn();
	void		switchOff();
 
	long 		period();						// period last value in sec.

	// debouncing: default 20 seconds
    void  		setMinimalActive_s		( long val ); 	//
    void  		setMinimalInactive_s	( long val ); 	//
    void 		setDebounce_s			( long val ); 	// set debounce time in sec.
    void  		setSwitchOnValue		( float ); 				//
    void  		setSwitchOffValue		( float ); 				//

  private:  
    unsigned long 		relaisChanges[10];			// track timestamps of changing active
	void  		setAutoActivateTimer();
	void 		setNewTimeStamp();
	boolean 	setPin();

	boolean 	_inverted;					// verwissel aan/uit: (false: on when value > switchValue,  true: on when value < switchValue)
	unsigned long 		timeStamp;             // minimal timeStamp for changing the switch
	unsigned long 		_prevTimeStamp = 0;				// for checking millis() overflow
	long   		_autoActivatePeriode_s ;    // activate every sec  default off = 0
	unsigned long   	timeStampAutoActivate;			// activate timestamp msec
    long 		_minimalActive_s;			// minimal time between switching in sec.
	long 		_minimalInactive_s;			// minimal time between switching in sec.


	float 		_refValue;					// switch on new value +/- hysteresis
	float 		_hysteresis;   				// 								^

	int			_pin;						// digital pin
};

// *****  implementation  ******

void Relais::upload()
{
	if(uploadFunc==0   ) return;

	if( uploadFunc(id, isRunning(), millis() ) > 0)
	{
		syncTimer = millis() + syncInterval ;
		available = 0;
	}
	else
	{
		syncTimer = millis() + id ;
	}
}

boolean Relais::loop( float currentValue   )
{
	if ( millis() < _prevTimeStamp )
	{
		// handle overflow millis() once every 50 day's
		setNewTimeStamp();
		setAutoActivateTimer();
	}


	if( millis() > syncTimer
	 || available > 0
	){
		upload() ;
	}


    if ( timeStamp > millis() ) return istValue;

    checkValue( currentValue );

	if (  timeStampAutoActivate < millis() && _autoActivatePeriode_s > 0  )   // trigger the auto activate
	{
#ifdef DEBUG
 Serial.print(F("@")); Serial.print(millis()/1000); Serial.println(F(": autoActivated"));
#else
#endif
		on();
	    //	if(netw != NULL && autoId > 0 ) netw.sendData( autoId);
	}

	//if (test) Serial.println("Relais.loop active");

	if( minimalActive_s   != _minimalActive_s )  _minimalActive_s = minimalActive_s;
	if( minimalInactive_s != _minimalInactive_s) _minimalInactive_s = minimalInactive_s;
	if( inverted   		  != _inverted )
	{
		_inverted = inverted;
		if (test)  {  Serial.println(F("Relais.inverted was changed")); }
	}


	if(  sollValue != istValue )
	{
		setPin();
		available = 1; // the value has been evaluated

		// always reset the auto activate timer after switch to inactive. Prevent unnecessary auto activations
		if ( istValue == true && _autoActivatePeriode_s > 0 )
		  setAutoActivateTimer();

	}

	frequency = getSwitchFrequency();			// calc the switching frequence per hour


    //if (test)  {  Serial.print(F("Relais.loop return=")); Serial.println(_value); }
	setNewTimeStamp();

	return istValue;
}

void Relais::setNewTimeStamp()
{

	timeStamp = millis() + 1000 *  ( isRunning() ? _minimalActive_s : _minimalInactive_s );

#ifdef DEBUG
	 Serial.print(F("@")); Serial.print(millis()/1000); Serial.print(F(": timeStamp ")); Serial.println(timeStamp/1000);
#else
#endif																	// store for millis() overflow checking
}

boolean Relais::setPin()
{
	istValue = sollValue;
	digitalWrite( _pin, _inverted ? !istValue : istValue );

    _prevTimeStamp = millis();
	timeStampLastSetPin = millis();

#ifdef DEBUG
	Serial.print(F("@")); Serial.print(millis()/1000); Serial.print(F(": setPin ")); Serial.println(istValue);
#else
#endif
	// remember last 10 swith timeStamps
	for ( int i = ( sizeof( relaisChanges ) / sizeof( unsigned long ) ) - 1
			; i > 0
			; i--
	)	relaisChanges[i] = relaisChanges[i-1];

	relaisChanges[0] = millis();				// add current switch to the list

	return true;
}



int	Relais::getSwitchFrequency()
{
	if ( relaisChanges[9] == 0 ) return 0;  // when less then 10 changes we don't report a frequency

	unsigned long timePassed_m = ( millis() - relaisChanges[9] ) / 60000 ;

	int freq  =	 60 / ( timePassed_m  / 5 );   // freq in changes per hour

	return freq;
}



void Relais::setSwitchOnValue ( float newVal ) { _switchOnValue = newVal;  	checkActive = true;}
void Relais::setSwitchOffValue( float newVal ) { _switchOffValue = newVal;	checkActive = true;}

void Relais::setHysteresis(int hysteresis)  //  !! tiende graden
{
	_hysteresis = (float) hysteresis / (float) 10 ;
	_switchOnValue  = _refValue + _hysteresis / (float)2;
	_switchOffValue = _refValue - _hysteresis / (float)2;
	checkActive = true;
}


void Relais::setRefValue(int refValue) // !! tiende graden
{
	_refValue = (float) refValue / (float) 10 ;
	_switchOnValue  = _refValue + _hysteresis / (float)2;
	_switchOffValue = _refValue - _hysteresis / (float)2;
	setHysteresis( _hysteresis );
	checkActive = true;
}


void Relais::setMinimalActive_s( long val )
{
	minimalActive_s = val;
}


void Relais::setMinimalInactive_s( long val ){
	minimalInactive_s = val;
}
void Relais::setDebounce_s( long val)
{
	minimalActive_s = val;
	minimalInactive_s = _minimalActive_s;
	timeStamp = millis();
}

void Relais::setAutoActivatePeriode_s( long newPeriode )
{
	_autoActivatePeriode_s = newPeriode;
	setAutoActivateTimer();
}


void Relais::setAutoActivateTimer()
{
#ifdef DEBUG
	Serial.print(F("@")); Serial.print(millis()/1000); Serial.println(F(": setAutoActivateTimer"));
#else
#endif
	timeStampAutoActivate = millis() + _autoActivatePeriode_s * 1000;
}

void Relais::checkValue(float value )
{
	if(!checkActive) return;
#ifdef DEBUG
	Serial.print(F("@")); Serial.print(millis()/1000); Serial.print(F(": checkValue ")); Serial.println(value);
#else
#endif

    if ( istValue == false )
    {
    	if ( value >= _switchOnValue ) on();
    }
    else
    {
    	if (   value < _switchOffValue
    		|| ( value == _switchOffValue && _switchOffValue < _switchOnValue) ) off();
    }
}

boolean Relais::ist()  { return istValue; }  // logical on
boolean Relais::soll() { return sollValue; }  // logical

boolean Relais::isRunning()  { return   istValue; }  // return evaluate()
boolean Relais::isStopped() { return ! istValue; }  // return ! evaluate()

//boolean Relais::value()    { return _inverted ? !istValue : istValue; }  // evaluate();

long 	Relais::period() { return  ( millis() - _prevTimeStamp) / 1000 ; }  // period last value in sec.
void  	Relais::on() { sollValue = true;  }	 // at least switch to on for one periode evaluate();
void  	Relais::off() { sollValue = false;  }	 // at least switch to on for one periode evaluate();



#endif
