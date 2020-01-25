#pragma once
#include <Arduino.h>
#include "revid.h"

#ifdef DEBUG
void AwsRecordState(int i);
#define AWS_RECORD_STATE(i) AwsRecordState(i);

#define AWS_DEBUG_PRINT(...) do { \
                    Serial.print(__VA_ARGS__); \
                    } while (0)
#define AWS_DEBUG_PRINTLN(...) do { \
                    Serial.println(__VA_ARGS__);  \
                    } while (0)
#else
#define AWS_DEBUG_PRINT(...) do { } while (0)
#define AWS_DEBUG_PRINTLN(...) do { } while (0)
#define AWS_RECORD_STATE(A)
#endif

#define MESSAGE_DESTINATION_SOLID LoraMessageDestination
#define MESSAGE_SOURCE_SOLID LoraMessageSource

#define ver F("2.0." REV_ID) //Wind station Version 2.0
#define STATUS_MESSAGE F(" SierraGliding Weather Station. github.com/BarryPSmith/SierraGliding for source. SierraGliding.us for location. This station identified by first three bytes=XW")

//Station Specific Constants
#ifdef STATION_ID
#define STR(A) #A
#define XSTR(A) STR(A)
inline constexpr char stationID = STATION_ID;
#pragma message ("StationID: " XSTR(STATION_ID))
#endif

//Global Constants
inline constexpr unsigned long tncBaud = 38400;
inline constexpr unsigned long millisBetweenStatus = 600000; //We send our status messages every ten minutes.
inline constexpr unsigned long maxMillisBetweenPings = 1300000; //If we don't receive a ping in just under 20 minutes, we restart.
inline constexpr unsigned short batteryHysterisis_mV = 50;

extern unsigned long weatherInterval; //Current weather interval.
extern unsigned long overrideStartMillis;
extern unsigned long overrideDuration;
extern bool overrideShort;
extern bool sleepEnabled;

//Recent Memory
extern unsigned long lastStatusMillis;

extern volatile bool weatherRequired;

extern unsigned long lastPingMillis;

#ifndef DEBUG
inline void signalError(const byte count = 5, const unsigned long delay_ms = 70)
{
  bool zeroHigh = true;
  for (byte i = 0; i < count; i++)
  {
    delay(delay_ms);
    if (zeroHigh)
    {
      digitalWrite(0, LOW);
      digitalWrite(1, HIGH);
    }
    else
    {
      digitalWrite(0, HIGH);
      digitalWrite(1, LOW);
    }
    zeroHigh = !zeroHigh;
  }
  delay(delay_ms);
  digitalWrite(0, LOW);
  digitalWrite(1, LOW);
}
#define SIGNALERROR(...) signalError(__VA_ARGS__)
#else
#define SIGNALERROR(...) do {} while (0)
#endif // !DEBUG