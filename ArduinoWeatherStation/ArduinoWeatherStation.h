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

#define PRINT_VARIABLE(a) do { \
  AWS_DEBUG_PRINT(F(#a ": ")); \
  AWS_DEBUG_PRINTLN(a); \
  } while (0)

#define DEBUG_TX
#ifdef DEBUG_TX
#define TX_PRINT AWS_DEBUG_PRINT
#define TX_PRINTLN AWS_DEBUG_PRINTLN
#define TX_PRINTVAR PRINT_VARIABLE
#else
#define TX_PRINT
#define TX_PRINTLN
#define TX_PRINTVAR
#endif

#define MESSAGE_DESTINATION_SOLID LoraMessageDestination
#define MESSAGE_SOURCE_SOLID LoraMessageSource

#define ver F("2.1." REV_ID) //Wind station Version 2.0
#define STATUS_MESSAGE F(" SierraGliding Weather Station. github.com/BarryPSmith/SierraGliding for source. SierraGliding.us for location. This station identified by first three bytes=XW")

//Station Specific Constants
#ifdef STATION_ID
#define STR(A) #A
#define XSTR(A) STR(A)
inline constexpr char defaultStationID = STATION_ID;
#else
inline constexpr char defaultStationID = 'Z';
#endif
extern char stationID;

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

extern unsigned long lastPingMillis;

#ifndef DEBUG
#define LED_PIN0 0
#define LED_PIN1 1
#define LED_OFF HIGH
#define LED_ON LOW
inline void signalOff()
{
  digitalWrite(LED_PIN0, LED_OFF);
  digitalWrite(LED_PIN1, LED_OFF);
}

inline void signalError(const byte count = 5, const unsigned long delay_ms = 70)
{
  bool zeroHigh = true;
  for (byte i = 0; i < count; i++)
  {
    delay(delay_ms);
    if (zeroHigh)
    {
      digitalWrite(LED_PIN0, LED_OFF);
      digitalWrite(LED_PIN1, LED_ON);
    }
    else
    {
      digitalWrite(LED_PIN0, LED_ON);
      digitalWrite(LED_PIN1, LED_OFF);
    }
    zeroHigh = !zeroHigh;
  }
  delay(delay_ms);
  signalOff();
}
#define SIGNALERROR(...) signalError(__VA_ARGS__)
#else
#define SIGNALERROR(...) do {} while (0)
#endif // !DEBUG