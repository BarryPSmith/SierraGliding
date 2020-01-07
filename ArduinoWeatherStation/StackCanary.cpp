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
volatile uint8_t MCUSR_Mirror __attribute__ ((section (".noinit")));

#if defined(WATCHDOG_LOOPS) && WATCHDOG_LOOPS > 0
unsigned volatile char watchdogLoops = 0;
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

    //Set the zero reg and zero the status reg so we can use regular C code:
    __asm volatile("eor r1, r1\n\t"
                  "out	0x3f, r1"::);

    // optiboot gives us the actual MCUSR in r2.
    // (normal optiboot preserves MCUSR, but dual optiboot on the Moteino doesn't)
#ifdef USE_ACTUAL_MCUSR
    MCUSR_Mirror = MCUSR;
#else
    register uint8_t oldMCUSR asm("r2");
    MCUSR_Mirror = oldMCUSR;
#endif
    MCUSR = 0;

#ifndef ALWAYS_DUMP_STACK
    if (!(MCUSR_Mirror & _BV(WDRF)))
      oldSP = 0xAA;
#endif


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
  //Be very careful if modifying this function:
  //It's naked, because sometimes it doesn't return and we want a good stack trace.
  //But if it returns, you need to take care of all of that yourself.
#if defined(WATCHDOG_LOOPS) && WATCHDOG_LOOPS > 0
  __asm volatile(
    ""
    //We're in a naked ISR, gotta manually push/pop stuff:
      /*
    */
    "push r0\n\t"
    "in r0, %[sreg]\n\t"
    "push r0\n\t"
    "push r24\n\t"
    //Load watchdogLoops,
    "lds r24, watchdogLoops\n\t"
    //check if it's greater than WATCHDOG_LOOPS
    "subi r24, %[maxWatchdogLoops]\n\t"
    "brcc .die\n\t"
    //If it's not greater, restore and increment
    "subi r24, %[minusOneMinusMaxWatchdogLoops]\n\t"
    //store...
    "sts watchdogLoops, r24\n\t"
    
    //and re-enable the watchdog interrupt
    "lds r24, %[wdtcsr]\n\t"
    "sbr r24, %[wdie]\n\t"
    "sts %[wdtcsr], r24\n\t"
    //pop and return
    "pop r24\n\t"
    "pop r0\n\t"
    "out %[sreg], r0\n\t"
    "pop r0\n\t"
    "reti\n\t"

    //Too many loops passed without watchdogLoops being reset.
    //Store the stack trace and die.
    ".die:\n\t"
    "pop r24\n\t"
    "pop r0\n\t"
    "out %[sreg], r0\n\t"
    "pop r0\n\t"
    : //output (nothing)
    : //input
      [sreg] "I" (_SFR_IO_ADDR(SREG)),
      [maxWatchdogLoops] "M" (WATCHDOG_LOOPS),
      [minusOneMinusMaxWatchdogLoops] "M" (255 - WATCHDOG_LOOPS),
      [wdtcsr] "n" (_SFR_MEM_ADDR(WDTCSR)),
      [wdie] "M"(_BV(WDIE))
    : //clobbers (none, we handle the push/pop ourselves
      );
  /*if (watchdogLoops < WATCHDOG_LOOPS)
  {
    watchdogLoops++;
    WDTCSR |= (1 << WDIE);
    return;
  }*/
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
  SP = __stack;
  asm volatile("jmp .stackPaintStart");
#else
  //Just wait for the WDT to time out a second time:
  //sei();
  while (1);
#endif
}
