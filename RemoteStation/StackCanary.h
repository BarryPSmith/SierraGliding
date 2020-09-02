#pragma once
#define STACK_CANARY 0xc5

#if defined(WATCHDOG_LOOPS) && WATCHDOG_LOOPS > 0
extern unsigned volatile char watchdogLoops;
#endif


#ifndef STACK_DUMP_SIZE
#define STACK_DUMP_SIZE 224
#endif

uint16_t StackCount(void);

extern volatile uint16_t oldSP;
extern volatile uint8_t& oldStack; //[STACK_DUMP_SIZE];
extern volatile uint8_t MCUSR_Mirror;
extern volatile bool wdt_dontRestart;

