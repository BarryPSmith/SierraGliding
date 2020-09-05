#include "TimerTwo.h"

volatile unsigned long TimerTwo::_ticks __attribute__ ((section (".noinit")));
volatile unsigned char TimerTwo::_ofTicks __attribute__ ((section (".noinit")));

void timerTwo_defaultInterrupt() {}
void (*TimerTwo::_interruptAction)() = &timerTwo_defaultInterrupt;

ISR(TIMER2_COMPA_vect)
{
  TimerTwo::_ticks++;
  if (bit_is_set(SREG, SREG_C))
    TimerTwo::_ofTicks++;
  TimerTwo::_interruptAction();
}

#ifdef CRYSTAL_FREQ
void TimerTwo::initialise()
{
  TIMSK2 = 0;
  ASSR = _BV(AS2); //Run it off the 32 kHz crystal
  //Wait 1 seconds for crystal to stabilise
  for (byte i = 0; i < 16; i++)
    delayMicroseconds(65535);
  TCNT2 = 1;
  OCR2A = TIMER2_TOP;
  OCR2B = 0;
  
  TCCR2A = _BV(WGM21); //CTC Mode
  TCCR2B = _BV(CS21) | _BV(CS20); // Prescaler of 32: 4 ticks per second at 32768 / 255 top
  
  while (ASSR & (_BV(TCN2UB) | _BV(OCR2AUB) | _BV(OCR2BUB) | _BV(TCR2AUB) | _BV(TCR2BUB)))
  {
    //Wait for ASSR to change to indicating these have been set.
  }
  
  //Enable interrupts
  TIFR2 = 0;
  TIMSK2 = _BV(OCIE2A);


}
#else
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
  // Overflow exactly every 16ms (+/- clock accuracy). 32ms at 8MHz. 256ms at 1MHz.
  OCR2A = TIMER2_TOP; // Note: Decimal. Also note that the timer will have a value of 249 for a full 1024 cycles and only then reset to zero, so it actually counts 250 * 1024 cycles.
  // Enable OCR
  TIMSK2 = _BV(OCIE2A);
  // Leave the async stuff.
}
#endif

unsigned long TimerTwo::seconds()
{
  unsigned long ret = 0;
  auto sreg = SREG;
  cli();
#ifdef CRYSTAL_FREQ
  static_assert(MILLIS_PER_SECOND / MillisPerTick == 4);
  static_assert(MILLIS_PER_SECOND % MillisPerTick == 0);
  *(((byte*)&ret) + 3) = _ofTicks << 6;
  ret |= _ticks >> 2;
#else
  unsigned long long ticks = _ticks;
  *(((unsigned long*)&ticks) + 1) = _ofTicks;
  ret = ticks * (short)MillisPerTick / 1000;
#endif
  SREG = sreg;
  return ret;
}

void TimerTwo::setSeconds(unsigned long seconds)
{
  auto sreg = SREG;
  cli();
#ifdef CRYSTAL_FREQ
  static_assert(MILLIS_PER_SECOND / MillisPerTick == 4);
  static_assert(MILLIS_PER_SECOND % MillisPerTick == 0);
  _ofTicks = (seconds >> 30) & 0b11;
  _ticks = seconds * (MILLIS_PER_SECOND / MillisPerTick);
#else
  unsigned long long ticks = (unsigned long long)seconds * 1000 / (short)MillisPerTick;
  _ofTicks = ticks >> 32;
  _ticks = ticks;
#endif
  SREG = sreg;
}

unsigned long TimerTwo::millis()
{
  auto sreg = SREG;
  cli();
  unsigned long m = _ticks;
  byte t = TCNT2;
  if (TIFR2 & _BV(OCF2A) && t < 249)
    m++;
  SREG = sreg;

  return (m * MillisPerTick) + t * MillisPerTick / (TIMER2_TOP + 1);
}

unsigned long TimerTwo::micros()
{
  return millis() * 1000;

  auto sreg = SREG;
  cli();
  unsigned long m = _ticks;
  unsigned long t = TCNT2;
  if (TIFR2 & _BV(OCF2A) && t < TIMER2_TOP)
    m++;
  SREG = sreg;

  return (m * MillisPerTick * 1000) + t * MillisPerTick * 1000 / (TIMER2_TOP + 1);
}

void TimerTwo::attachInterrupt(void (*interruptAction)(void))
{
  auto sreg = SREG;
  cli();
  _interruptAction = interruptAction;
  SREG = sreg;
}