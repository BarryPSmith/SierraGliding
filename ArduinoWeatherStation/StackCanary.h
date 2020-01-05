#pragma once
#define STACK_CANARY 0xc5

#if defined(WATCHDOG_LOOPS) && WATCHDOG_LOOPS > 0
extern unsigned volatile int watchdogLoops;
#endif


#ifndef STACK_DUMP_SIZE
#define STACK_DUMP_SIZE 128
#endif

uint16_t StackCount(void);

extern volatile uint16_t oldSP;
extern volatile uint8_t oldStack[STACK_DUMP_SIZE];

