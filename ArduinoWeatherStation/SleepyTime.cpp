//SleepyTime puts the system into idle, while still allowing millis() to work well enough for our purposes of timing the weather messages
//We disable timer0 so that it won't wake the system with millis ticks, then manually add to millis.
//We need to use timer1 to add to millis in case we're woken by a different interrupt:
//Wind ticking or serial comms

#include <LowPower.h>
#include <TimerOne.h>
#include <avr/Power.h>
#include "ArduinoWeatherStation.h"
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif 
extern volatile unsigned long timer0_millis;

void timerInterrupt();
void stopMillis();
void startMillis();

volatile bool timerOn = false;
volatile unsigned long millisToAdd = 0;

void setupSleepy()
{
  Timer1.initialize(3000000);
  Timer1.stop();
  Timer1.attachInterrupt(timerInterrupt);
}

void sleepUntilNextWeather()
{
  //Flush the serial or we risk writing garbage or being woken by a send complete message.
  Serial.flush();
  //Gotta disable interrupts before checking timerOn, otherwise we might accidently send the system to sleep forever
  noInterrupts();
  //If the timer is already enabled, it means we should be asleep. Go back to sleep.
  if (timerOn)
  {
    //This re-enables interrupts, and sleeps until either our timer or something else wakes us
    LowPower.idle(SLEEP_FOREVER,
                  ADC_OFF,
                  TIMER2_OFF,
                  TIMER1_ON,
                  TIMER0_OFF,
                  SPI_OFF,
                  USART0_ON,
                  TWI_OFF);
    return;
  }
  unsigned long curMillis = millis();
  //If we need to send soon, just let the loop run
  if (curMillis - lastWeatherMillis > weatherInterval - wakePeriod)
  {
    interrupts();
    return;
  }
  //Stop the millis timer, in case we're woken up before our timer goes off. Note that millis() will return a constant value until our timer ticks.
  stopMillis();
  unsigned long timeTillSend = weatherInterval - (curMillis - lastWeatherMillis);
  millisToAdd = timeTillSend - wakePeriod;
  timerOn = true;
  //Our timer period can't be more than 8.3 seconds. We're going to limit it to 8 seconds.
  if (millisToAdd > 8000)
    millisToAdd = 8000;    
  Timer1.setPeriod(millisToAdd * 1000);
  //Note: We changed the code for Timer1.restart to set the TCNT1=1 instead of 0. Setting it to zero causes it to fire an interrupt immediately, waking us from our slumber.
  Timer1.restart();
  //LowPower.idle will sleep the system until the timer ticks, or something else interrupts us. It reenables interrupts.
  LowPower.idle(SLEEP_FOREVER,
                ADC_OFF,
                TIMER2_OFF,
                TIMER1_ON,
                TIMER0_OFF,
                SPI_OFF,
                USART0_ON,
                TWI_OFF);
}

void stopMillis()
{
  cbi(TIMSK0, TOIE0);
}

void startMillis()
{
  //Seems we need to enable the power before we set the bit...
  power_timer0_enable();
  sbi(TIMSK0, TOIE0);
}

void timerInterrupt()
{
  timerOn = false;
  timer0_millis  += millisToAdd;
  startMillis();
  Timer1.stop();
}
