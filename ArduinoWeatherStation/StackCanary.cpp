//Found this on the AVR-freaks forum. Very, very useful.
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <string.h>
#include "StackCanary.h"

extern uint8_t _end;
extern uint8_t __stack;
void StackPaint(void) __attribute__ ((naked)) __attribute__ ((section (".init1"))) __attribute__ ((used));

volatile uint16_t oldSP __attribute__ ((section (".noinit")));
volatile uint8_t oldStack[STACK_DUMP_SIZE] __attribute__ ((section (".noinit")));

#if defined(WATCHDOG_LOOPS) && WATCHDOG_LOOPS > 0
unsigned volatile int watchdogLoops = 0;
#endif


void StackPaint(void)
{
#if 0
    uint8_t *p = &_end;

    while(p <= &__stack)
    {
        *p = STACK_CANARY;
        p++;
    }
#else
    // Technically this isn't StackPaint, but it's in .init1 and it makes sense to sit here.
    // This is to be able to tell if we crashed and the WDT saved us
    if (!(MCUSR & _BV(WDRF)))
      oldSP = 0xAA;
    MCUSR = 0;


    __asm volatile (".stackPaintStart:\n"
                    "    ldi r30,lo8(_end)\n"
                    "    ldi r31,hi8(_end)\n"
                    "    ldi r24,lo8(0xc5)\n" /* STACK_CANARY = 0xc5 */
                    "    ldi r25,hi8(__stack)\n"
                    "    rjmp .cmp\n"
                    ".loop:\n"
                    "    st Z+,r24\n"
                    ".cmp:\n"
                    "    cpi r30,lo8(__stack)\n"
                    "    cpc r31,r25\n"
                    "    brlo .loop\n"
                    "    breq .loop"::);
#endif
}

uint16_t StackCount(void)
{
    const uint8_t *p = &_end;
    uint16_t       c = 0;

    while(*p == STACK_CANARY && p <= &__stack)
    {
        p++;
        c++;
    }

    return c;
}

ISR (WDT_vect, ISR_NAKED)
{
#if defined(WATCHDOG_LOOPS) && WATCHDOG_LOOPS > 0
  if (watchdogLoops < WATCHDOG_LOOPS)
  {
    watchdogLoops++;
    WDTCSR |= (1 << WDIE);
    return;
  }
#endif

  //Dump the stack:
  oldSP = SP;
  auto stackSize = (uint16_t)(&__stack) - oldSP;
  if (stackSize > STACK_DUMP_SIZE)
    stackSize = STACK_DUMP_SIZE;
  memcpy((void*)oldStack,(void*) (oldSP + 1), stackSize);
   


#ifdef SOFT_WDT
	// Disable the WDT and jump to where we start painting the stack.
  // We don't jump to reset vector, so that the newly running code is able to tell whether SP has been reset
	wdt_disable();
  SP = 0;
  asm volatile("jmp .stackPaintStart");
#else
  //Just wait for the WDT to time out a second time:
  //sei();
  while (1);
#endif
}
