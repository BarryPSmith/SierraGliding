#include "TimerTwo.h"

volatile unsigned long TimerTwo::_ticks = 0;

void timerTwo_defaultInterrupt() {}
void (*TimerTwo::_interruptAction)() = &timerTwo_defaultInterrupt;

ISR(TIMER2_COMPA_vect)
{
  TimerTwo::_ticks++;
  TimerTwo::_interruptAction();
}

void TimerTwo::initialise()
{
  //PRTIM2 - Power consumption pg 36
  //TCNT2 - Counter
  //TIFR2 - Interrupt flags
  //TIMSK2 - Interrupt mask
  //Set Normal mode
  // TCCR2A: CTC Mode
  TCCR2A = _BV(WGM21);//0x02;
  // TCCR2B: 1024 prescaler:
  TCCR2B = _BV(CS20) | _BV(CS21) | _BV(CS22);
  // Overflow exactly every 16ms (+/- clock accuracy). 32ms at 8MHz.
  OCR2A = TIMER2_TOP; // Note: Decimal. Also note that the timer will have a value of 249 for a full 1024 cycles and only then reset to zero, so it actually counts 250 * 1024 cycles.
  // Enable OCR
  TIMSK2 = _BV(OCIE2A);
  // Leave the async stuff.
}

unsigned long TimerTwo::millis()
{
  auto sreg = SREG;
  cli();
  unsigned long m = _ticks;
  unsigned long t = TCNT2;
  if (TIFR2 & _BV(OCF2A) && t < 249)
    m++;
  SREG = sreg;

  return (m * MillisPerTick) + t * MillisPerTick / 250;
}

unsigned long TimerTwo::micros()
{
  auto sreg = SREG;
  cli();
  unsigned long m = _ticks;
  unsigned long t = TCNT2;
  if (TIFR2 & _BV(OCF2A) && t < 249)
    m++;
  SREG = sreg;

  return (m * MillisPerTick * 1000) + t * MillisPerTick * 4;
}

void TimerTwo::attachInterrupt(void (*interruptAction)(void))
{
  auto sreg = SREG;
  cli();
  _interruptAction = interruptAction;
  SREG = sreg;
}