//32kb program memory. 2kb dynamic memory
#define DEBUG_Speed 0
#define DEBUG_Direction 0

#include <spi.h>
#include <TimerOne.h>
#include <LowPower.h>
#include "lib/RadioLib/src/Radiolib.h"
#include "ArduinoWeatherStation.h"
#include "WeatherProcessing.h"
#include "MessageHandling.h"
#include "SleepyTime.h"
#include "TimerTwo.h"

//Setup Variables //1910
unsigned long shortInterval = 4000 - 90 * (stationID - '1'); //Send interval when battery voltage is high. We make sure each station has a different interval to avoid them both transmitting simultaneously for extended periods.
unsigned long longInterval = 30000; //Send interval when battery voltage is low
float batteryThreshold = 12.0;
float batteryHysterisis = 0.05;
bool demandRelay = false;

constexpr unsigned long weatherTolerance = 100;
unsigned long weatherInterval = 2000; //Current weather interval.
unsigned long overrideStartMillis = 0;
unsigned long overrideDuration = 0;
bool overrideShort = false;

#ifdef DEBUG
extern volatile bool windTicked;
#endif // DEBUG


//Recent Memory
unsigned long lastStatusMillis = 0;

void setup();
void loop();
void disableRFM69();
void sleepUntilNextWeather();

int main()
{
  //Arduino library initialisaton:
  init();
  TimerTwo::initialise();
  
  randomSeed(digitalRead(A0));


  setup();
  while (1)
  {
    loop();
    if (serialEventRun) serialEventRun();
  }
  return 0;
}



void setup() {
  //gotta do this first to get our timers running:
  setupWeatherProcessing();
#ifdef DEBUG
  Serial.begin(tncBaud);
#endif // DEBUG
  AWS_DEBUG_PRINTLN(F("Starting..."));
  delay(50);

#ifdef DEBUG
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
#endif

  disableRFM69();

  AWS_DEBUG_PRINTLN(F("Radios Disabled"));

  InitMessaging();

  AWS_DEBUG_PRINTLN(F("Messaging Initialised"));

  ZeroMessagingArrays();
    
  sendStatusMessage();

  lastStatusMillis = millis();
}

void loop() {

  readMessages();
  
  noInterrupts();
  bool localWeatherRequired = weatherRequired;
  weatherRequired = false;
#ifdef DEBUG
  bool localWindTicked = windTicked;
  windTicked = false;
#endif
  interrupts();

#ifdef DEBUG
  if (localWindTicked)
    AWS_DEBUG_PRINTLN(F("Wind Tick"));
#endif

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
}

//A test board has a RFM69 on it for reading Davis weather stations.
// But some testing revealed the davis station was faulty.
//So we put it both to sleep.
void disableRFM69()
{
  SPI.begin();

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
}

void sleepUntilNextWeather()
{
#if DEBUG
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

#ifdef DEBUG
void AwsRecordState(int i)
{
  digitalWrite(5, i & 1 ? 1 : 0);
  digitalWrite(6, i & 2 ? 1 : 0);
  digitalWrite(7, i & 4 ? 1 : 0);
  digitalWrite(8, i & 8 ? 1 : 0);
}
#endif