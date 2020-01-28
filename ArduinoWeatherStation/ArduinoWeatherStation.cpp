//32kb program memory. 2kb dynamic memory
#define DEBUG_Speed 0
#define DEBUG_Direction 0

#include <avr/wdt.h>
#include <spi.h>
//#include <TimerOne.h>
#include <LowPower.h>
#include <avr/power.h>
#include "lib/RadioLib/src/Radiolib.h"
#include "ArduinoWeatherStation.h"
#include "WeatherProcessing.h"
#include "MessageHandling.h"
#include "TimerTwo.h"
#include "PermanentStorage.h"
#include "StackCanary.h"

unsigned long weatherInterval = 2000; //Current weather interval.
unsigned long overrideStartMillis;
unsigned long overrideDuration;
bool overrideShort;
bool sleepEnabled = true;

#ifdef DEBUG
extern volatile bool windTicked;
#endif // DEBUG

//Recent Memory
unsigned long lastStatusMillis = 0;
unsigned long lastPingMillis = 0;


void setup();
void loop();
void disableRFM69();
void sleep(adc_t adc_state);
void restart();

void TestBoard();
void savePower();

int main()
{
  //Arduino library initialisaton:
  init();
  TimerTwo::initialise();
  //Disabling savePower() until we can test it:
  //savePower();
  //TestBoard();

  setup();

  while (1)
  {
    loop();
    if (serialEventRun) serialEventRun();
  }
  return 0;
}

void savePower()
{
  constexpr int PinCount = 20;
  bool pinsInUse[PinCount];
  memset(pinsInUse, 0, PinCount);
      pinsInUse[BATT_PIN] 
    = pinsInUse[WIND_DIR_PIN]
    = pinsInUse[WIND_SPD_PIN]
    = pinsInUse[SX_BUSY]
    = pinsInUse[SX_DIO1]
    = pinsInUse[SX_SELECT]
#ifdef SX_RESET
    = pinsInUse[SX_RESET]
#endif
    = true;
    
  for (int i = 0; i < PinCount; i++)
  {
    if (!pinsInUse[i])
      pinMode(i, INPUT_PULLUP);
  }

  power_twi_disable();
#ifndef DEBUG
  power_usart0_disable();
#endif
  power_timer0_disable();
  power_timer1_disable();

  DIDR0 = _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D) | _BV(ADC4D) | _BV(ADC5D);
  DIDR1 = _BV(AIN0D) | _BV(AIN1D);
}

extern SX1262 lora;
#include "CSMA.h"
extern CSMAWrapper<SX1262> csma;
void TestBoard()
{
  //sleep the radio:
  InitMessaging();
  csma.setIdleState(IdleStates::IntermittentReceive);
  //lora.sleep();

  while (1)
  {
    //32 ms per loop. This should give us about 2 seconds of sleep:
    for (int i = 0; i < 60; i++)
      LowPower.powerSave(SLEEP_FOREVER, //we're actually going to wake up on our next timer2 or wind tick.
                       ADC_OFF,
                       BOD_OFF,
                       TIMER2_ON);
    delay(1000);
  }
}

void setup() {
  randomSeed(analogRead(A0));
  
  //Enable the watchdog early to catch initialisation hangs (Side note: This limits initialisation to 8 seconds)
  wdt_enable(WDTO_8S);
  WDTCSR |= (1 << WDIE);

  WeatherProcessing::setupWeatherProcessing();
#ifdef DEBUG
  Serial.begin(tncBaud);
#else //Use the serial LEDs as status lights:
  pinMode(LED_PIN0, OUTPUT);
  pinMode(LED_PIN1, OUTPUT);
  signalOff();
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
  
  MessageHandling::sendStatusMessage();

  lastStatusMillis = millis();
}

void loop() {
  MessageHandling::readMessages();
  
  noInterrupts();
  bool localWeatherRequired = WeatherProcessing::weatherRequired;
  WeatherProcessing::weatherRequired = false;
  interrupts();

  #ifdef DEBUG
  messageDebugAction();
  #endif
  
  if (localWeatherRequired)
  {
    MessageHandling::sendWeatherMessage();
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
    MessageHandling::sendStatusMessage();
  }
  
  if (millis() - lastPingMillis > maxMillisBetweenPings)
    restart();

  wdt_reset();
  sleep(ADC_OFF);
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

void yield()
{
  //We leave the ADC_ON to hopefully get a less pronounced effect as the LDO goes from low current to running current.
  sleep(ADC_ON);
}

void sleep(adc_t adc_state)
{
  //If we've disabled sleep for some reason...
  if (!sleepEnabled)
    return;
  // Or interrupts aren't enabled (= no wakeup)
  if (bit_is_clear(SREG, SREG_I))
    return;
#ifdef DEBUG
  // Or there's data in the serial buffer
  if (bit_is_set(UCSR0B, UDRIE0) || bit_is_clear(UCSR0A, TXC0))
    return;
#endif
  //return;
  LowPower.powerSave(SLEEP_FOREVER, //we're actually going to wake up on our next timer2 or wind tick.
                    adc_state,
                    BOD_OFF,
                    TIMER2_ON);
}