//32kb program memory. 2kb dynamic memory
#define DEBUG_Speed 0
#define DEBUG_Direction 0

#include <avr/wdt.h>
#include <spi.h>
//#include <TimerOne.h>
#include <LowPower.h>
#include "lib/RadioLib/src/Radiolib.h"
#include "ArduinoWeatherStation.h"
#include "WeatherProcessing.h"
#include "MessageHandling.h"
#include "SleepyTime.h"
#include "TimerTwo.h"
#include "PermanentStorage.h"

unsigned long weatherInterval = 2000; //Current weather interval.
unsigned long overrideStartMillis;
unsigned long overrideDuration;
bool overrideShort;

#ifdef DEBUG
extern volatile bool windTicked;
#endif // DEBUG

#ifndef STACK_DUMP_SIZE
#define STACK_DUMP_SIZE 128
#endif
volatile uint16_t oldSP __attribute__ ((section (".noinit")));
volatile byte oldStack[STACK_DUMP_SIZE] __attribute__ ((section (".noinit")));

//Recent Memory
unsigned long lastStatusMillis = 0;
unsigned long lastPingMillis = 0;

void setup();
void loop();
void disableRFM69();
void sleepUntilNextWeather();
void restart();

int main()
{
  //Arduino library initialisaton:
  init();
  TimerTwo::initialise();
  
  setup();

  while (1)
  {
    loop();
    if (serialEventRun) serialEventRun();
  }
  return 0;
}

extern uint8_t __stack;
ISR (WDT_vect, ISR_NAKED)
{
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
  sei();
  while (1);
#endif
}

void setup() {
  randomSeed(analogRead(A0));
  
  //Enable the watchdog early to catch initialisation hangs (Side note: This limits initialisation to 8 seconds)
  wdt_enable(WDTO_8S);
  WDTCSR |= (1 << WDIE);

  setupWeatherProcessing();
#ifdef DEBUG
  Serial.begin(tncBaud);
#else //Use the serial LEDs as status lights:
  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  digitalWrite(0, LOW);
  digitalWrite(1, LOW);
#endif // DEBUG
  delay(50);
  
  if (oldSP >= 0x100)
  {
    AWS_DEBUG_PRINT(F("Previous Stack Pointer: "));
    AWS_DEBUG_PRINTLN(oldSP, HEX);
    AWS_DEBUG_PRINT(F("Previous Stack: "));
    for (int i = 0; i < STACK_DUMP_SIZE; i++)
    {
      AWS_DEBUG_PRINT(oldStack[i], HEX);
      AWS_DEBUG_PRINT(' ');
    }
    AWS_DEBUG_PRINTLN();
  }
  AWS_DEBUG_PRINTLN(F("Starting..."));

  PermanentStorage::initialise();
  
#if 0 //Useful to be able to output a state with pins. However, it looks like we might use them.
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
#endif

#ifdef DISABLE_RFM69
  disableRFM69();
#endif
  
  InitMessaging();
  if (oldSP > 0x100)
  {
    MESSAGE_DESTINATION_SOLID msg(false);
    msg.appendByte('S');
    msg.appendByte(stationID);
    msg.appendByte(0x00);
	msg.appendT(oldSP);
    msg.append((byte*)oldStack, STACK_DUMP_SIZE);
  }

  AWS_DEBUG_PRINTLN(F("Messaging Initialised"));
  
  sendStatusMessage();

  lastStatusMillis = millis();
}

void loop() {
  readMessages();
  
  noInterrupts();
  bool localWeatherRequired = weatherRequired;
  weatherRequired = false;
#ifdef DEBUG
  messageDebugAction();
#endif
  interrupts();
  
  if (localWeatherRequired)
  {
    sendWeatherMessage();
    #if DEBUG_Speed
    if (speedDebugging)
      sendSpeedDebugMessage();
    #endif
    #if DEBUG_Direction
    if (debugThisRound)
      sendDirectionDebug();
    debugThisRound = !debugThisRound;
    #endif
    AWS_DEBUG_PRINTLN(F("Weather message sent."));
  }
  
  if (millis() - lastStatusMillis > millisBetweenStatus)
  {
    sendStatusMessage();
  }

  sleepUntilNextWeather();

  if (millis() - lastPingMillis > maxMillisBetweenPings)
    restart();

  wdt_reset();
}

//A test board has a RFM69 on it for reading Davis weather stations.
// But some testing revealed the davis station was faulty.
//So we put it to sleep.
void disableRFM69()
{
  SPI.begin();
#if 1
  {
    Module rfmMod(10, -1, -1);
    RF69 rf69 = &rfmMod;
    auto rfmSleep = rf69.sleep();
    if (rfmSleep != ERR_NONE)
    {
      AWS_DEBUG_PRINT(F("Unable to sleep RFM69: "));
      AWS_DEBUG_PRINTLN(rfmSleep);
    }
    else
      AWS_DEBUG_PRINTLN(F("RFM Asleep."));
  }
#endif
}

void sleepUntilNextWeather()
{
#ifdef DEBUG
  //Flush the serial or we risk writing garbage or being woken by a send complete message.
  Serial.flush();
#endif
#if false
  LowPower.idle(SLEEP_FOREVER,
                ADC_OFF,
                TIMER2_ON,
                TIMER1_OFF,
                TIMER0_OFF,
                SPI_ON,
#ifdef DEBUG
                USART0_ON,
#else
                USART0_OFF,
#endif          
                TWI_OFF);
#else
  LowPower.powerSave(SLEEP_FOREVER, //we're actually going to wake up on our next timer2 or wind tick.
                     ADC_OFF,
                     BOD_ON,
                     TIMER2_ON);
#endif
}

void restart()
{
  // Just allow the watchdog timer to timeout. Signal error as we're going down.
  while (1)
  {
    AWS_DEBUG_PRINT(F("RESTARTING!")); //By including this in the loop we will flash the LEDs in debug mode too.
    delay(500);
    SIGNALERROR(3, 200);
  }
}

#ifdef DEBUG
void AwsRecordState(int i)
{
  digitalWrite(5, i & 1 ? 1 : 0);
  digitalWrite(6, i & 2 ? 1 : 0);
  digitalWrite(7, i & 4 ? 1 : 0);
  digitalWrite(8, i & 8 ? 1 : 0);
}
#endif

#ifndef DEBUG
//Export signalError here.
extern inline void signalError(byte count, unsigned long delay_ms);
#endif