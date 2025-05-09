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
// deep sleep disables timer2, and allows the MCU to turn off everything.
// That greatly drops sleep current, but the unit cannot do its weather reporting.
// It'll still respond to messages.
// It'll even relay messages but weather messages will only be relayed when the buffer is filled
BatteryMode batteryMode = BatteryMode::Normal;
unsigned short batteryReading_mV;
bool stasisRequested;
unsigned short lastErrorSeconds;
short lastErrorCode;
bool silentSignal = false;

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

void updateBatterySavings();
void enterDeepSleep();
void enterNormalMode();
void enterBatterySave();
void enterStasis();

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

  //TestBoard();

  setup();

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
#if defined(CLOCK_DIVIDER)// && CLOCK_DIVIDER != 1
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

extern SX1262 lora;
void TestBoard()
{
#if 0
  wdt_disable();
  Serial.begin(serialBaud);
  for (int i = 0; i < 256; i++)
  {
    byte b;
    PermanentStorage::getBytes((void*)i, 1, &b);
    Serial.write(b);
  }
  while (1);
#endif
}

void seedRandom()
{
  unsigned short seed = analogRead(BATT_PIN); 
#ifdef WIND_DIR_PIN
  seed ^= analogRead(WIND_DIR_PIN) << 3 ;
#endif
#ifdef TEMP_SENSE
  seed ^= analogRead(TEMP_SENSE) << 6;
#endif
  srand(seed);
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
  GET_PERMANENT_S(stasisRequested);

  #ifdef SOLAR_PWM
  PwmSolar::setupPwm();
  #endif

  if (!Flash::flashInit())
  {
    BASE_PRINTLN(F("!! Remote programming failed to initialise !!"));
    SIGNALERROR(REMOTE_PROGRAM_INITALISATION_FAILURE);
  }
  Database::initDatabase();

  InitMessaging();

  WeatherProcessing::readBattery();
  enterNormalMode();
  WeatherProcessing::setupWeatherProcessing();  
  BASE_PRINTLN(F("Weather initialised"));

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
__attribute__((noinline))
void sendStackTrace()
{
  byte buffer[254];
  LoraMessageDestination msg(false, buffer, sizeof(buffer), 'S', 0x00);
	msg.appendT(oldSP);
  byte size = STACK_DUMP_SIZE;
  unsigned short oldStackSize = (unsigned short)&__stack - oldSP;
  if (size > oldStackSize)
    size = oldStackSize;
  msg.append((byte*)&oldStack, size);
}

__attribute__((noinline))
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
  byte buffer[20];
  LoraMessageDestination msg(false, buffer, sizeof(buffer), 'K', MessageHandling::getUniqueID());
	msg.append(F("PTimeout"), 8);
}

void sendCrystalMessage(XtalInfo& result)
{
  BASE_PRINTLN(F("Crystal Failed!"));
  byte buffer[20];
  LoraMessageDestination msg(false, buffer, sizeof(buffer), 'K', MessageHandling::getUniqueID());
  msg.appendByte2('X');
  msg.appendByte2(result.failed);
  msg.appendByte2(result.test1);
  msg.appendByte2(result.test2);
}

void testCrystal(bool sendAnyway)
{
#ifdef CRYSTAL_FREQ
  XtalInfo xtalRes = TimerTwo::testFailedOsc();
  if (xtalRes.failed || sendAnyway)
    sendCrystalMessage(xtalRes);
#endif
}
__attribute__((noinline))
void loop() {
  //BASE(auto loopMicros = micros());
  static byte counter = 0;
  constexpr byte cystalTestInterval = 50;

  if (++counter == cystalTestInterval)
  {
    if ((rand() & 0x0F) == 1)
    {
      testCrystal(false);
    }
    counter = 0;
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



  if (batteryMode == BatteryMode::DeepSleep)
  {
#ifdef CRYSTAL_FREQ
    noInterrupts();
    bool localWeatherRequired = WeatherProcessing::weatherRequired;
    WeatherProcessing::weatherRequired = false;
    interrupts();
    if (localWeatherRequired)
#endif
    {
      // We periodically read the battery to see if we should come out of deep sleep:
      WeatherProcessing::readBattery();
    }
  }
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
      BASE_PRINTLN(F("Weather message sent."));
    }

    if (millis() - lastStatusMillis > millisBetweenStatus)
    {
      MessageHandling::sendStatusMessage();
    }
#ifdef SOLAR_PWM
    PwmSolar::doPwmLoop();
#endif
  }
  updateBatterySavings();

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
  if (stasisRequested)
    enterStasis();
}

void restart()
{
  // Just allow the watchdog timer to timeout. Signal error as we're going down.
  while (1)
  {
    AWS_DEBUG_PRINT(F("RESTARTING!")); //By including this in the loop we will flash the LEDs in debug mode too.
    delay(500);
    SIGNALERROR(RESTARTING);
  }
}

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
  {
    return;
  }
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
  timer2_t timer2State = (batteryMode == BatteryMode::DeepSleep && adc_state == ADC_OFF) ? TIMER2_OFF : TIMER2_ON;
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
  {
    LowPower.powerSave(SLEEP_FOREVER, //Low power library messes with our watchdog timer, so we lie and tell it to sleep forever.
      adc_state,
      BOD_ON,
      timer2State
    );
  }
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

void updateBatterySavings()
{
  unsigned short batteryThreshold_mV, batteryEmergencyThresh_mV;
  GET_PERMANENT_S(batteryThreshold_mV);
  GET_PERMANENT_S(batteryEmergencyThresh_mV);
  if (batteryReading_mV < batteryEmergencyThresh_mV)
  {
    if (batteryMode != BatteryMode::DeepSleep)
      enterDeepSleep();
  }
  else if ((batteryReading_mV < batteryThreshold_mV - batteryHysterisis_mV)
    || batteryMode == BatteryMode::DeepSleep)
  {
    if (batteryMode != BatteryMode::Save)
      enterBatterySave();
  }
  else if (batteryReading_mV > batteryThreshold_mV + batteryHysterisis_mV)
  {
    if (batteryMode != BatteryMode::Normal)
      enterNormalMode();
  }
}

void enterDeepSleep()
{
  BASE_PRINTLN(F("Entering Deep Sleep"));
  batteryMode = BatteryMode::DeepSleep;
  WeatherProcessing::enterDeepSleep();
  updateIdleState();
#ifdef SOLAR_PWM
  PwmSolar::setSolarFull();
  PwmSolar::stopCurrentSensor();
#endif
}

void enterBatterySave()
{
  batteryMode = BatteryMode::Save;
  WeatherProcessing::enterBatterySave();
  updateIdleState();
}

void enterNormalMode()
{
  batteryMode = BatteryMode::Normal;
  WeatherProcessing::enterNormalMode();
  updateIdleState();
}

void enterStasis()
{
  batteryMode = BatteryMode::Stasis;
  WeatherProcessing::enterDeepSleep();
  sleepRadio();
  TimerTwo::slowDown();
#ifdef SOLAR_PWM
  PwmSolar::setSolarFull();
  PwmSolar::stopCurrentSensor();
#endif
  byte lastSeconds = TimerTwo::seconds();
  wdt_disable();
  WeatherProcessing::windCounts = 0;

#ifdef CRYSTAL_FREQ
  timer2_t timer2State = TimerTwo::_crystalFailed ? TIMER2_OFF : TIMER2_ON;
#else
  timer2_t timer2State = TIMER2_OFF;
#endif
#ifdef SX_SWITCH
  digitalWrite(SX_SWITCH, LOW);
#endif
  while (1)
  {
    byte theseSeconds = TimerTwo::seconds();
    if (theseSeconds != lastSeconds)
    {
      lastSeconds = theseSeconds;
      WeatherProcessing::windCounts = 0;
    }
    if (WeatherProcessing::windCounts > 10)
      break;    

    LowPower.powerSave(SLEEP_FOREVER,
        ADC_OFF,
        BOD_OFF,
        timer2State
      );
  }
  WeatherProcessing::windCounts = 0;
#ifdef SX_SWITCH
  digitalWrite(SX_SWITCH, HIGH);
#endif
  stasisRequested = false;
  SET_PERMANENT_S(stasisRequested);
  unsigned long exitSeconds = TimerTwo::seconds();
  TimerTwo::initialise();
  wdt_enable(WDTO_8S);
  enterNormalMode();
}

void signalError(uint16_t errorCode, const byte delay_ms)
{
  // Don't bother with CRC mismatch, it's an expected condition;
  // it wastes battery to flash and masks potentially more interesting faults.
  if (errorCode == ERR_CRC_MISMATCH)
    return;
  lastErrorSeconds = TimerTwo::seconds();
  lastErrorCode = errorCode;
#ifdef DARK
  return;
#endif
  if (silentSignal)
    return;
#ifndef DEBUG
  static_assert(LED_PIN0 == 0);
  static_assert(LED_PIN1 == 1);
  static_assert(LED_ON == LOW);
  static_assert(LED_OFF == HIGH);
  auto delay2 = delay_ms / 4;
  PORTD &= ~(_BV(PD0) | _BV(PD1));
  //digitalWrite(LED_PIN0, LED_ON);
  //digitalWrite(LED_PIN1, LED_ON);
  delay(delay2);
  PORTD |= _BV(PD1);//digitalWrite(LED_PIN1, LED_OFF);
  delay(delay2);
  PORTD &= ~_BV(PD1); //digitalWrite(LED_PIN1, LED_ON);
  delay(delay2);
  PORTD |= _BV(PD1); //digitalWrite(LED_PIN1, LED_OFF);
  delay(delay2);
  //Shorten it to 11 bits (=-1024 to + 1023)
  for (byte i = 0; i < 11; i++)
  {
    if (i & 1)
      PORTD |= _BV(PD0);
    else
      PORTD &= ~_BV(PD0);
    //digitalWrite(LED_PIN0, (i & 1) ? LED_OFF : LED_ON);
    if (errorCode & 1)
      PORTD &= ~_BV(PD1);
    else
      PORTD |= _BV(PD1);
    //digitalWrite(LED_PIN1, (errorCode & 1) ? LED_ON : LED_OFF);
    errorCode = errorCode >> 1;
    delay(delay_ms);
  }
  // Write the sign bit
  //digitalWrite(LED_PIN0, LED_OFF);
  //digitalWrite(LED_PIN1, errorCode & (0x800 >> 11) ? LED_ON : LED_OFF);
  if (errorCode & (0x800 >> 11))
    PORTD &= ~_BV(PD1);
  else
    PORTD |= _BV(PD1);
  PORTD |= _BV(PD0);
  delay(delay_ms);
  signalOff();
#endif
}