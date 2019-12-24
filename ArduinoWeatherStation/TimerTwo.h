#include <Arduino.h>

#define MILLIS_PER_SECOND 1000UL
#define TIMER2_RESOLUTION 256UL
#define MAX_DIVIDER 1024UL
#define TIMER2_TOP 249

//We replace default timer2 functionality to wake up less often...
class TimerTwo
{
public:
  static constexpr unsigned long MillisPerTick = (TIMER2_TOP + 1) * MILLIS_PER_SECOND * MAX_DIVIDER / F_CPU;
  static volatile unsigned long _ticks;
  static void (*_interruptAction)(void);

  static void initialise();


  static unsigned long millis();

  static unsigned long micros();

  static void attachInterrupt(void (*interruptAction)(void));
};