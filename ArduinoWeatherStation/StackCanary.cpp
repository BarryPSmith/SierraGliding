//Found this on the AVR-freaks forum. Very, very useful.
#include <stdint.h>
#include <avr/io.h>

extern uint8_t _end;
extern uint8_t __stack;
extern uint16_t oldSP;
#define STACK_CANARY 0xc5
void StackPaint(void) __attribute__ ((naked)) __attribute__ ((section (".init1"))) __attribute__ ((used));

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
