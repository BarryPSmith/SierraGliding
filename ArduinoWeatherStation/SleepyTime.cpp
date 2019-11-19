#if 0
//SleepyTime puts the system into idle, while still allowing millis() to work well enough for our purposes of timing the weather messages
//We disable timer0 so that it won't wake the system with millis ticks, then manually add to millis.
//We need to use timer1 to add to millis in case we're woken by a different interrupt:
//Wind ticking or serial comms

#include <TimerOne.h>
#include <LowPower.h>
#include <avr/Power.h>
#include "ArduinoWeatherStation.h"
//#include <RadioLib/src/RadioLib.h>
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif 
extern volatile unsigned long timer0_millis;

void stopMillis();
void startMillis();

volatile bool timerOn = false;
volatile unsigned long millisOnTick = 0;

void setupSleepy()
{

}



//We've changed our code to use timer1 millis.
//We also link against a custom millis and micros implementation so that other libraries will use it too.
void stopMillis()
{
  //return;
  cbi(TIMSK0, TOIE0);
}

void startMillis()
{
  //return;
  //Seems we need to enable the power before we set the bit...
  power_timer0_enable();
  sbi(TIMSK0, TOIE0);
}
#endif