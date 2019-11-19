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


//Recent Memory
unsigned long lastStatusMillis = 0;

void setup();
void loop();
void disableRFM69();
void sleepUntilNextWeather();

int main()
{
  //Arduino library initialisaton:
  //AFAICT, Init is only for the millis / micros timer
  //init();
  sei();

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
  Serial.begin(tncBaud);
  Serial.println("Starting...");
  delay(50);

  AWS_DEBUG_PRINTLN("Serial Begun");

#ifdef DEBUG
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
#endif

  disableRFM69();

  AWS_DEBUG_PRINTLN("Radios Disabled");

  InitMessaging();

  AWS_DEBUG_PRINTLN("Messaging Initialised");

  ZeroMessagingArrays();
    
  sendStatusMessage();

  lastStatusMillis = Timer1.millis();
}

void loop() {

  readMessages();
  bool localWeatherRequired;
  noInterrupts();
  localWeatherRequired = weatherRequired;
  weatherRequired = false;
  interrupts();
  if (localWeatherRequired)
  {
    AWS_DEBUG_PRINTLN("I'm going to send a weather message.");
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
    AWS_DEBUG_PRINTLN("Weather message sent.");
  }
  
  if (Timer1.millis() - lastStatusMillis > millisBetweenStatus)
  {
    sendStatusMessage();
  }

  AWS_DEBUG_PRINTLN("I'm tired. Goodnight.");
  sleepUntilNextWeather();
  AWS_DEBUG_PRINTLN("I'm Awake!");
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
      AWS_DEBUG_PRINT("Unable to sleep RFM69: ");
      AWS_DEBUG_PRINTLN(rfmSleep);
    }
    else
      AWS_DEBUG_PRINTLN("RFM Asleep.");
  }
}

void sleepUntilNextWeather()
{
  //Flush the serial or we risk writing garbage or being woken by a send complete message.
  Serial.flush();
  LowPower.idle(SLEEP_FOREVER,
                ADC_OFF,
                TIMER2_OFF,
                TIMER1_ON,
                TIMER0_OFF,
                SPI_ON,
                USART0_ON,
                TWI_OFF);
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