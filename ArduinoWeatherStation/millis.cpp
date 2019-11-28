// redirect millis to the timer1 implementations:
#include "TimerTwo.h"

unsigned long millis() {
	return TimerTwo::millis();
}

unsigned long micros() {
  return TimerTwo::micros();
}
