#include <Arduino.h>

#define MILLIS_PER_SECOND 1000UL
#define TIMER2_RESOLUTION 256UL
#ifdef CRYSTAL_FREQ
#define DIVIDER 32UL
#define TIMER2_TOP 255
#else
#define DIVIDER 1024UL
#define TIMER2_TOP 249
#endif

#define STR(A) #A
#define XSTR(A) STR(A)
#define XXSTR(A) XSTR(A)

//We replace default timer2 functionality to wake up less often, or run from an external crystal
class TimerTwo
{
public:
  static constexpr unsigned long MillisPerTick = 
#ifdef CRYSTAL_FREQ
    (TIMER2_TOP + 1) * MILLIS_PER_SECOND * DIVIDER / CRYSTAL_FREQ;
#else
    (TIMER2_TOP + 1) * MILLIS_PER_SECOND * DIVIDER / F_CPU;
#endif
  static volatile unsigned long _ticks;
  static void (*_interruptAction)(void);

  static void initialise();

  static unsigned long millis();

  static unsigned long micros();

  static void attachInterrupt(void (*interruptAction)(void));
};