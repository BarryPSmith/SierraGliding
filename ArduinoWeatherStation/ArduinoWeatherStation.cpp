//32kb program memory. 2kb dynamic memory
#define DEBUG_Speed 0
#define DEBUG_Direction 0

#include <spi.h>
#include "lib/RadioLib/src/Radiolib.h"

#include "ArduinoWeatherStation.h"
#include "WeatherProcessing.h"
#include "MessageHandling.h"
#include "SleepyTime.h"

//Setup Variables
unsigned long shortInterval = 4000 - 90 * (stationID - '1'); //Send interval when battery voltage is high. We make sure each station has a different interval to avoid them both transmitting simultaneously for extended periods.
unsigned long longInterval = 4000; //Send interval when battery voltage is low
float batteryThreshold = 12.0;
float batteryHysterisis = 0.05;
bool demandRelay = false;

unsigned long weatherInterval = 6000; //Current weather interval.
unsigned long overrideStartMillis = 0;
unsigned long overrideDuration = 0;
bool overrideShort = false;


//Recent Memory
unsigned long lastStatusMillis;
unsigned long lastWeatherMillis;

void setup();
void loop();
void disableSPIRadios();

int main()
{
  randomSeed(digitalRead(A0));

  init();

  setup();
  while (1)
  {
    loop();
    if (serialEventRun) serialEventRun();
  }
  return 0;
}

void setup() {
  Serial.begin(tncBaud);
  delay(50);

  AWS_DEBUG_PRINTLN("Serial Begun");

  disableSPIRadios();

  AWS_DEBUG_PRINTLN("Radios Disabled");

  ZeroMessagingArrays();
  

  /*memcpy(callSign, statusMessage + 2, 6);
  memcpy(statusMessageTemplate, statusMessage + 8, statusMessageLength);
  statusMessage[8 + statusMessageLength] = stationID;
  statusMessage[0] = FEND;
  statusMessage[1] = 0x00;
  statusMessage[22] = FEND;*/
  
  sendStatusMessage();

  lastStatusMillis = millis();

  setupWeatherProcessing();
  setupSleepy();
}

void loop() {
  readMessages();

  if (millis() - lastWeatherMillis > weatherInterval)
  {
    lastWeatherMillis = millis();
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
void disableSPIRadios()
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

void sendTestMessage()
{
  /*byte data[] = {0xC0, 0x00, 0x96, 0x9C, 0x6C, 0x88, 0xAA, 0x86, 0xE0, 0x96, 0x9C, 0x6C, 0x88, 0xAA, 0x86, 0xE1, 0x03, 0xF0, 0x48, 0x45, 0x4C, 0x4C, 0x4F, 0xC0 };
  Serial.write(data, sizeof(data));*/
}
