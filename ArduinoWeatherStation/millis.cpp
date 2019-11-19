// redirect millis to the timer1 implementations:
#include <TimerOne.h>

unsigned long millis() {
	return Timer1.millis();
}

unsigned long micros() {
  return Timer1.micros();
}
