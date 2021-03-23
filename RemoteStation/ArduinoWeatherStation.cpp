#include <avr/wdt.h>
#include <spi.h>
//#include <TimerOne.h>
#include <LowPower.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include "lib/RadioLib/src/Radiolib.h"
#include "ArduinoWeatherStation.h"
#include "WeatherProcessing/WeatherProcessing.h"
#include "MessageHandling.h"
#include "TimerTwo.h"
#include "PermanentStorage.h"
#include "StackCanary.h"
#include "RemoteProgramming.h"
#include "PWMSolar.h"
#include "Flash.h"
#include "Database.h"

unsigned long weatherInterval = 2000; //Current weather interval.
/*unsigned long overrideStartMillis;
unsigned long overrideDuration;
bool overrideShort;*/
SleepModes solarSleepEnabled = SleepModes::powerSave;
SleepModes dbSleepEnabled = SleepModes::powerSave;
bool continuousReceiveEnabled = true;
// deep sleep disables timer2, and allows the MCU to turn off everything.
// That greatly drops sleep current, but the unit cannot do its weather reporting.
// It'll still respond to messages.
// It'll even relay messages but weather messages will only be relayed when the buffer is filled
bool doDeepSleep = false;

#ifdef DEBUG
extern volatile bool windTicked;
#endif // DEBUG

//Recent Memory
unsigned long lastStatusMillis = 0;
unsigned long lastPingMillis = 0;

void setup();
void loop();
void sleep(adc_t adc_state);
void restart();
void sendStackTrace();
void storeStackTrace();
void sendNoPingMessage();
void testCrystal(bool sendAnyway);

void TestBoard();
void savePower();

#ifdef DEBUG_BASE
#define BASE_PRINT AWS_DEBUG_PRINT
#define BASE_PRINTLN AWS_DEBUG_PRINTLN
#define BASE_PRINTVAR PRINT_VARIABLE
#else
#define BASE_PRINT(...)  do { } while (0)
#define BASE_PRINTLN(...)  do { } while (0)
#define BASE_PRINTVAR(...)  do { } while (0)
#endif

#ifdef ATMEGA328PB
volatile byte* PRR1 = (byte*)0x65;
#endif

int main()
{ 
  savePower();

  //Arduino library initialisaton:
  init();
  TimerTwo::initialise();

  setup();
  //TestBoard();

  while (1)
  {
    loop();
  }
  return 0;
}

#ifdef CLOCK_DIVIDER
constexpr byte CalcClockDividerByte()
{
  switch (CLOCK_DIVIDER)
  {
  case 1: return 0;
  case 2: return _BV(CLKPS0);
  case 4: return _BV(CLKPS1);
  case 8: return _BV(CLKPS1) | _BV(CLKPS0);
  default: return 0;
  }
}
#endif
void savePower()
{
#if defined(CLOCK_DIVIDER) && CLOCK_DIVIDER != 1
  CLKPR = _BV(CLKPCE);
  CLKPR = CalcClockDividerByte();
#endif

#if CURRENT_SENSE != A1
  pinMode(A1, INPUT_PULLUP);
#endif
#if SX_SWITCH != A3
  pinMode(A3, INPUT_PULLUP);
#endif
#if TEMP_PWR_PIN != 6
  pinMode(6, INPUT_PULLUP);
#endif
#if CURRENT_SENSE_PWR != 7
  pinMode(7, INPUT_PULLUP);
#endif


#ifndef DEBUG
  power_usart0_disable();
#endif
#ifdef ATMEGA328PB
  *PRR1 = _BV(PRTWI1) | _BV(PRPTC) | _BV(PRTIM4) | _BV(PRSPI1) | _BV(PRTIM3);
  
  PRR |= _BV(PRUSART1);

  //TODO: Port E pullup.
#endif
#ifndef ALS_WIND
  power_twi_disable();
#endif // !ALS_WIND

#ifndef SOLAR_PWM
  power_timer0_disable();
#endif
  power_timer1_disable();

  DIDR0 = 
    _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D) 
#ifdef ATMEGA328PB
    | _BV(ADC6D) | _BV(ADC7D)
#endif
#ifndef ALS_WIND
    | _BV(ADC4D) | _BV(ADC5D)
#endif
    ;
  DIDR1 = _BV(AIN0D) | _BV(AIN1D);
}


void TestBoard()
{
  while (1)
  {
    AWS_DEBUG_PRINTLN();

    for (int i = 0; i < 3; i++)
      PRINT_VARIABLE(millis());
    PRINT_VARIABLE(TimerTwo::testFailedOsc());
    delay(1000);
  }
  while (1);
}

void seedRandom()
{
  unsigned long seed = (unsigned long)analogRead(BATT_PIN); 
#ifdef WIND_DIR_PIN
  seed |= analogRead(WIND_DIR_PIN) << 10 ;
#endif
#ifdef TEMP_SENSE
  seed |= (unsigned long)analogRead(TEMP_SENSE) << 20;
#endif
  randomSeed(seed);
}

void setup() {
  seedRandom();
  
  //Enable the watchdog early to catch initialisation hangs (Side note: This limits initialisation to 8 seconds)
  wdt_enable(WDTO_8S);
  WDTCSR |= (1 << WDIE);

#ifdef DEBUG
  Serial.begin(serialBaud);
#else //Use the serial LEDs as status lights:
  pinMode(LED_PIN0, OUTPUT);
  pinMode(LED_PIN1, OUTPUT);
  signalOff();
#endif // DEBUG
  //delay(50);
  
#ifdef DEBUG_STACK
  if (oldSP >= 0x100)
  {
    BASE_PRINT(F("Previous Stack Pointer: "));
    BASE_PRINTLN(oldSP, HEX);
    BASE_PRINT(F("Previous Stack: "));
    for (int i = 0; i < STACK_DUMP_SIZE; i++)
    {
      BASE_PRINT(oldStack[i], HEX);
      BASE_PRINT(' ');
    }
    BASE_PRINTLN();
  }
#endif
  BASE_PRINTLN(F("Starting..."));

  pinMode(SX_SELECT, OUTPUT);
  digitalWrite(SX_SELECT, HIGH);
#ifdef FLASH_SELECT
  pinMode(FLASH_SELECT, OUTPUT);
  digitalWrite(FLASH_SELECT, HIGH);
#endif

  lastPingMillis = millis();
  PermanentStorage::initialise();

  #ifdef SOLAR_PWM
  PwmSolar::setupPwm();
  #endif

  WeatherProcessing::setupWeatherProcessing();
  BASE_PRINTLN(F("Weather initialised"));
  if (!Flash::flashInit())
  {
    BASE_PRINTLN(F("!! Remote programming failed to initialise !!"));
    SIGNALERROR();
  }
  Database::initDatabase();

  InitMessaging();
  updateIdleState();

  if (oldSP > 0x100)
  {
#ifndef NO_STORAGE
    storeStackTrace();
#endif
    sendStackTrace();
  }
  canarifyStackDump();

  BASE_PRINTLN(F("Messaging Initialised"));
  
  lastStatusMillis = 0;
  testCrystal(true);
}

extern uint8_t __stack;
void sendStackTrace()
{
  MESSAGE_DESTINATION_SOLID<254> msg(false);
  msg.appendByte('S');
  msg.appendByte(stationID);
  msg.appendByte(0x00);
	msg.appendT(oldSP);
  byte size = STACK_DUMP_SIZE;
  unsigned short oldStackSize = (unsigned short)&__stack - oldSP;
  if (size > oldStackSize)
    size = oldStackSize;
  msg.append((byte*)&oldStack, size);
}

void storeStackTrace()
{
  byte buffer[STACK_DUMP_SIZE + 2];
  byte size = STACK_DUMP_SIZE;
  unsigned short oldStackSize = (unsigned short)&__stack - oldSP;
  if (size > oldStackSize)
    size = oldStackSize;
  *((unsigned short*)buffer) = oldSP;
  memcpy(buffer + 2, (void*)&oldStack, size);
  Database::storeData('S', stationID, buffer, size + 2);
}

void sendNoPingMessage()
{
  BASE_PRINTLN(F("Ping timeout message!"));
  MESSAGE_DESTINATION_SOLID<20> msg(false);
  msg.appendByte('K');
  msg.appendByte(stationID);
  msg.appendByte(MessageHandling::getUniqueID());
	msg.append(F("PTimeout"), 8);
}

void sendCrystalMessage(XtalInfo& result)
{
  BASE_PRINTLN(F("Crystal Failed!"));
  MESSAGE_DESTINATION_SOLID<20> msg(false);
  msg.appendByte('K');
  msg.appendByte(stationID);
  msg.appendByte(MessageHandling::getUniqueID());
  msg.appendByte('X');
  msg.appendByte(result.failed);
  msg.appendByte(result.test1);
  msg.appendByte(result.test2);
}

void testCrystal(bool sendAnyway)
{
#ifdef CRYSTAL_FREQ
  XtalInfo xtalRes = TimerTwo::testFailedOsc();
  if (xtalRes.failed || sendAnyway)
    sendCrystalMessage(xtalRes);
#endif
}

void loop() {
  //BASE(auto loopMicros = micros());
  
  if (random(1000) == 1)
  {
    testCrystal(false);
  }
  static bool noPingSent = false;

  if (initMessagingRequired)
  {
    InitMessaging();
    updateIdleState();
    initMessagingRequired = false;
  }

  MessageHandling::readMessages();
#ifndef NO_STORAGE
  Database::doProcessing();
#endif
  
  #ifdef DEBUG
  messageDebugAction();
  #endif



  if (doDeepSleep)
    WeatherProcessing::updateBatterySavings(WeatherProcessing::readBattery(), false);
  else
  {
#if !defined(DEBUG_NO_WEATHER) || !defined(DEBUG)
    WeatherProcessing::processWeather();
#endif
    noInterrupts();
    bool localWeatherRequired = WeatherProcessing::weatherRequired;
    WeatherProcessing::weatherRequired = false;
    interrupts();
    if (localWeatherRequired)
    {
      MessageHandling::sendWeatherMessage();
      WX_PRINTLN(F("Weather message sent."));
    }

    if (millis() - lastStatusMillis > millisBetweenStatus)
    {
      MessageHandling::sendStatusMessage();
    }
  }

  #ifdef SOLAR_PWM
  PwmSolar::doPwmLoop();
  #endif
  if (millis() - lastPingMillis < maxMillisBetweenPings)
  {
    noPingSent = false;
    wdt_reset();
  }
  else if (!noPingSent)
  {
    sendNoPingMessage();
    noPingSent = true;
  }
  if (StackCount() == 0)
    while(1);  
  #if 0 //DEBUG
  loopMicros = micros() - loopMicros;
  if (loopMicros > 20000)
    PRINT_VARIABLE(loopMicros);
  #endif
  sleep(ADC_OFF);
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

#ifndef DEBUG
//Export signalError here.
extern inline void signalError(byte count, unsigned long delay_ms);
#endif

void yield()
{
  //We leave the ADC_ON to hopefully get a less pronounced effect as the LDO goes from low current to running current.
  //Only sleep if we're running fast enough that MCU power consumption might be significant.
  // (Also, if we sleep when our clock rate is low, it might be quite a while before timer2 ticks - long enough to set off the WDT)
  //sleep(ADC_ON);

  #ifdef SOLAR_PWM
  if (solarSleepEnabled == SleepModes::disabled)
    PwmSolar::doPwmLoop();
  #endif

}

void sleep(adc_t adc_state)
{
  SleepModes sleepMode = min(solarSleepEnabled, dbSleepEnabled);
  //If we've disabled sleep for some reason...
  if (sleepMode == SleepModes::disabled)
    return;
  // Or interrupts aren't enabled (= no wakeup)
  if (bit_is_clear(SREG, SREG_I))
    return;
#ifdef DEBUG
  // Or there's data in the serial buffer
  if (bit_is_set(UCSR0B, UDRIE0) || bit_is_clear(UCSR0A, TXC0))
    return;
#endif
#ifdef CRYSTAL_FREQ
  timer2_t timer2State = TIMER2_ON;
#else
  timer2_t timer2State = (doDeepSleep && adc_state == ADC_OFF) ? TIMER2_OFF : TIMER2_ON;
#endif
  if (timer2State == TIMER2_OFF)
    wdt_dontRestart = true;
  else
  {
    // Even though we do the ASSR check after we wake up from sleep, 
    // there is a small chance the ISR could fire while we're awake.
    // This is just some paranoia to ensure we don't get a second interrupt.
    while (TCNT2 == 0xFF || TCNT2 <= 0x01); 
  }

  if (sleepMode == SleepModes::powerSave)
    LowPower.powerSave(SLEEP_FOREVER, //Low power library messes with our watchdog timer, so we lie and tell it to sleep forever.
                      adc_state,
                      BOD_ON,
                      timer2State
                      );
  else if (sleepMode == SleepModes::idle)
  {
    // note that these *_ON just tell LowPower not to mess with the PRR, they don't actually turn things on. 
    // TODO: Test what happens if we turn off SPI & TWI.
    LowPower.idle(SLEEP_FOREVER,
                  adc_state,
                  timer2State,
                  TIMER1_ON, 
                  TIMER0_ON, 
                  SPI_OFF,
                  USART0_ON, 
                  TWI_OFF);
  }
  // Ensure that any calls to millis will work properly, 
  // and that we won't have problems returning to sleep.
  // OCR2B isn't used, this just ensures we wait at least one TOSC cycle.
  #ifdef CRYSTAL_FREQ
  OCR2B++;
  while (ASSR & _BV(OCR2BUB));
  #endif

  // The re-enable isn't guarded to make it harder for runaway code to disable the watchdog timer
  wdt_dontRestart = false;
  if (timer2State == TIMER2_OFF)
  {
    // If there was no wind tick or message received,
    // then we might have been woken by the watchdog timer going off. 
    // In that case we need to re-enable the interrupt.
    WDTCSR |= (1 << WDIE);
  }
}