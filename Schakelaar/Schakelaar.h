/*   
  Schakelaar.h  
  Copyright (c) 2013-2014 J&L Computing.  All right reserved.
  Rev 1.0 - 2013-10-02
  
  This is an switch utility. You can specify a reference value where the switch changes
  Also we can invert = exchange on with off
  The reference value can have a hysteresis 
  There is a minimal delay periode in seconds where the state can not change to prevent knipperen
  Also we can specify an auto active periode i.e. once every 24 hours a pomp should work for 1 minuut.  

2015-01-18 v1.01 LLO add timeStamp
  
*/
 
#include <Arduino.h> 
#include <inttypes.h>

#ifndef Schakelaar_h
#define Schakelaar_h

//#define compileWithDebug        // enable Serial.print messages for monitoring/debugging

class Schakelaar
{
  public:

    Schakelaar  (int pin);							// enter the switch value and the hysteresis   
    Schakelaar  (int pin, bool inverted);			// enter the switch value and the hysteresis
    void 		init  (int pin);					// enter the switch value and the hysteresis
    void		setRefValue(float value); 			// the minimal number of seconds before the state can change to prevent klaperen
    void	  	setHysteresis(float hysteresis);
	void 		setAutoActivatePeriode(unsigned long autoActivatePeriode);
    boolean 	evaluate();                        	// read switch (on/off)   
    void 		setPin();                        	   
    void     	checkValue(float value);


	boolean		isActive();							// logical value
	boolean		isInactive();
	void		activate();
	void		deactivate();
	boolean		value();							// physical value
	boolean		newValue();

    void  		setMinimalActive(unsigned long val); 							// force to on state	
    void  		setMinimalInactive(unsigned long val); 							// force to on state	
 
	long 	period();								// period last value in sec.
	float 	refValue;							    // switch on new value +/- hysteresis
	float 	hysteresis;   					  	    // 								^

    boolean inverted;								// verwissel aan/uit: (false: on when value > switchValue,  true: on when value < switchValue) 

													// debouncing: default 20 seconds
    void 			setDebounce(unsigned long val); // set debounce time in sec.
    unsigned long 	minimalActive;					// minimal time between switching in sec.
	unsigned long 	minimalInactive;				// minimal time between switching in sec.

	boolean _value;
	boolean _newValue;
	unsigned long 	timeStamp;

  private:  
    boolean _inverted;								// verwissel aan/uit: (false: on when value > switchValue,  true: on when value < switchValue) 
	unsigned long 	_nextTimeStamp;                 // minimal timeStamp for changing the switch
	unsigned long 	_prevTimeStamp;
	unsigned long   _autoActivatePeriode ;          // activate every sec  default off
	unsigned long   _autoActivateTimeStamp ;        // activate every xxxx msec
	float 	_minValue;
	float 	_maxValue;
	int		_pin;									// digital pin
};

// implementation
Schakelaar::Schakelaar(int pin ) {  
	_value = false;
	_inverted = false;
	inverted = _inverted;
	init(pin);
}
Schakelaar::Schakelaar(int pin, bool inverted ) {
	this->_inverted = inverted;
	this->inverted = inverted;
	_value = inverted;
	init(pin);
}
void Schakelaar::init(int pin ){
	  refValue = 0;
	  _newValue = _value;
	  _autoActivatePeriode = 0;
	  _pin = pin;
	  _minValue = refValue  ;
	  _maxValue = refValue ;
	  minimalActive = 20;
	  minimalInactive = minimalActive;
	  _prevTimeStamp = millis();
	  _nextTimeStamp = millis() + minimalInactive * 1000;
	  timeStamp = millis();

	  pinMode(_pin, INPUT);
	  digitalWrite(_pin, LOW); // turn off pullup
	  pinMode(_pin, OUTPUT);
}

void Schakelaar::setDebounce(unsigned long val){
	  minimalActive = val;
	  minimalInactive = minimalActive;
	  _nextTimeStamp = millis();
}

void Schakelaar::checkValue(float value2check ){

	#ifdef compileWithDebug
		Serial.print("evaluate newValue=");
		Serial.print( _newValue ) ;
		Serial.print(" _minValue=");
		Serial.print( _minValue);
		Serial.print(" _value=");
		Serial.println(_value);
	#endif

    if ( value2check >= _maxValue )
      _newValue = true;
    if ( value2check < _minValue )
      _newValue = false;

	//return evaluate();   // TODO skip this because of load when calling by a network event.
}

boolean Schakelaar::evaluate(){

	// handle the millis() overflow once every 50 day's
	if ( millis() < _prevTimeStamp ) {
		_nextTimeStamp = millis() + 1000 * _value ? minimalActive : minimalInactive;
		_prevTimeStamp = millis();
		_autoActivateTimeStamp = millis() + _autoActivatePeriode * 1000;
	}

	// trigger the auto activate
	if ( _autoActivatePeriode > 0
	    && millis() > _autoActivateTimeStamp )  {	// when the timestamp is passed
		#ifdef compileWithDebug
//			Serial.println("autoActivate! ");  
		#endif
		_newValue = true;
	}

	timeStamp = millis();

    if ( millis() < _nextTimeStamp ) return _value;
	
	if (_inverted != inverted) {     // only when inverted did change
		_inverted = inverted;
		setPin();
		return _value;
	}

    if ( _newValue == _value ) return _value;

	setPin();

	if ( _autoActivatePeriode > 0  	// reset the auto activate timer
	   && _value == true ) {										
	  _autoActivateTimeStamp = millis() + _autoActivatePeriode * 1000;
	}

	return _value;
}

void Schakelaar::setPin(){
	_value = _newValue;
	_nextTimeStamp = millis() + 1000 * ( _value ? minimalActive : minimalInactive ) ;
	_prevTimeStamp = millis();
	#ifdef compileWithDebug
		Serial.print("---- Set relais "); Serial.println(value() ? "aan" : "uit");
	#endif
	digitalWrite( _pin,  value() ? HIGH : LOW); // relais not active
	timeStamp = millis();
}



void Schakelaar::setMinimalActive(unsigned long val){
	minimalActive = val;
	if ( _value )
		_nextTimeStamp = millis() + (val * 1000);
}
void	Schakelaar::setMinimalInactive(unsigned long val){
	minimalInactive = val;
	if ( ! _value )
		_nextTimeStamp = millis() + (val * 1000);
}


void Schakelaar::setHysteresis(float _hysteresis){
	hysteresis = _hysteresis; 
    _minValue = refValue - hysteresis;
    _maxValue = refValue + hysteresis;
}
void Schakelaar::setRefValue(float val){
	refValue = val; 
    _minValue =  val - hysteresis;
    _maxValue =  val + hysteresis;
}
void Schakelaar::setAutoActivatePeriode(unsigned long autoActivatePeriode){
	_autoActivatePeriode = autoActivatePeriode;
	_autoActivateTimeStamp = millis() + autoActivatePeriode * 1000;
}

boolean Schakelaar::isActive() { return value() == true ;   }    // return evaluate()
boolean Schakelaar::isInactive() { return value() == false ;   }  //  return ! evaluate()

boolean Schakelaar::value() {  return _inverted ? !_value : _value; }  // evaluate();
boolean Schakelaar::newValue() {return _inverted ? !_newValue : _newValue; }  // evaluate();


long 	Schakelaar::period() { return  ( millis() - _prevTimeStamp) / 1000 ; }  // period last value in sec.
void  	Schakelaar::activate() { _newValue = true;  }	 // at least switch to on for one periode evaluate();
void  	Schakelaar::deactivate() { _newValue = false;  }	 // at least switch to on for one periode evaluate();

#endif
