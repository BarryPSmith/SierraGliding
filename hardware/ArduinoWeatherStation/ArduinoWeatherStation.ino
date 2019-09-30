//32kb program memory. 2kb dynamic memory
#define DEBUG 1
#define DEBUG_Speed 0
#define DEBUG_Direction 0

#include "ArduinoWeatherStation.h"

void setup() {
  //Zero the arrays:
  memset(stationsToRelayCommands, 0, recentArraySize);
  memset(stationsToRelayWeather, 0, recentArraySize);
  memset(recentlySeenStations, 0, recentArraySize * 5);
  memset(recentlyRelayedMessages, 0, recentArraySize * 3);
  memset(recentlyHandledCommands, 0, recentArraySize);

  
  pinMode(LED_BUILTIN, OUTPUT);

  /*memcpy(callSign, statusMessage + 2, 6);
  memcpy(statusMessageTemplate, statusMessage + 8, statusMessageLength);
  statusMessage[8 + statusMessageLength] = stationId;
  statusMessage[0] = FEND;
  statusMessage[1] = 0x00;
  statusMessage[22] = FEND;*/
  
  Serial.begin(4800);
  delay(50);
  sendStatusMessage();

  lastStatusMillis = millis();

  setupWindCounter();
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

void sendTestMessage()
{
  byte data[] = {0xC0, 0x00, 0x96, 0x9C, 0x6C, 0x88, 0xAA, 0x86, 0xE0, 0x96, 0x9C, 0x6C, 0x88, 0xAA, 0x86, 0xE1, 0x03, 0xF0, 0x48, 0x45, 0x4C, 0x4C, 0x4F, 0xC0 };
  Serial.write(data, sizeof(data));
}
