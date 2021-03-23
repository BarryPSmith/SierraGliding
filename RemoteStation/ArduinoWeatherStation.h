#pragma once
#include <Arduino.h>
#include "revid.h"

#define STR(A) #A
#define XSTR(A) STR(A)

#ifdef DEBUG
#define AWS_DEBUG_PRINT(...) do { \
                    Serial.print(__VA_ARGS__); \
                    } while (0)
#define AWS_DEBUG_PRINTLN(...) do { \
                    Serial.println(__VA_ARGS__);  \
                    } while (0)
#define AWS_DEBUG_WRITE(...) Serial.write(__VA_ARGS__)
#define AWS_DEBUG(...) __VA_ARGS__
#else
#define AWS_DEBUG_PRINT(...) do { } while (0)
#define AWS_DEBUG_PRINTLN(...) do { } while (0)
#define AWS_DEBUG(...) do { } while (0)
#define AWS_DEBUG_WRITE(...) do { } while (0)
#endif

#define PRINT_VARIABLE(a) do { \
  AWS_DEBUG_PRINT(F(#a ": ")); \
  AWS_DEBUG_PRINTLN(a); \
  } while (0)

#define PRINT_VARIABLE_HEX(a) do { \
  AWS_DEBUG_PRINT(F(#a ": ")); \
  AWS_DEBUG_PRINTLN(a, HEX); \
  } while (0)

//#define DEBUG_COMMANDS
//#define DEBUG_TX
//#define DEBUG_RX
//#define DEBUG_DATABASE
//#define DEBUG_MSGPROC
//#define DEBUG_PARAMETERS
//#define DEBUG_SOLAR
//#define DEBUG_PROGRAMMING
//#define DEBUG_IT
//#define DEBUG_PWM
//#define DEBUG_WEATHER
//#define DETAILED_LORA_CHECK
//#define DEBUG_STACK
#define DEBUG_BASE
//#define DEBUG_FLASH
#define DEBUG_NO_WEATHER //This is us desperate to get program space.
#ifdef DEBUG
#undef SOLAR_PWM
#undef CRYSTAL_FREQ
#endif

#ifdef DEBUG_TX
#define TX_PRINT AWS_DEBUG_PRINT
#define TX_PRINTLN AWS_DEBUG_PRINTLN
#define TX_PRINTVAR PRINT_VARIABLE
#define TX_DEBUG AWS_DEBUG
#else
#define TX_PRINT(...) do { } while (0)
#define TX_PRINTLN(...) do { } while (0)
#define TX_PRINTVAR(...) do { } while (0)
#define TX_DEBUG(...) do { } while (0)
#endif

#ifdef DEBUG_RX
#define RX_PRINT AWS_DEBUG_PRINT
#define RX_PRINTLN AWS_DEBUG_PRINTLN
#define RX_PRINTVAR PRINT_VARIABLE
#define RX_DEBUG AWS_DEBUG
#else
#define RX_PRINT(...) do { } while (0)
#define RX_PRINTLN(...) do { } while (0)
#define RX_PRINTVAR(...) do { } while (0)
#define RX_DEBUG(...) do { } while (0)
#endif

#define MESSAGE_DESTINATION_SOLID LoraMessageDestination
#define MESSAGE_SOURCE_SOLID LoraMessageSource

#define ver_str "2.4." REV_ID "." XSTR(BOARD)
#define ASW_VER F(ver_str)
#define ver_size (sizeof(ver_str) - 1)

/*extern const PROGMEM char verPM[] = "2.2." REV_ID;
inline constexpr byte verS = sizeof(verPM);
#define ver reinterpret_cast<__FlashStringHelper *>(verPM);*/

/*inline constexpr char verC[] = "2.2." REV_ID;
inline constexpr byte verS = sizeof(verC);
inline constexpr __FlashStringHelper verF = F(verC);*/
#define STATUS_MESSAGE F(" SierraGliding Weather Station. github.com/BarryPSmith/SierraGliding for source. SierraGliding.us for location. This station identified by first three bytes=XW")

//Station Specific Constants
#ifdef STATION_ID
inline constexpr char defaultStationID = STATION_ID;
#else
inline constexpr char defaultStationID = 'Z';
#endif
extern char stationID;

//Global Constants
inline constexpr unsigned long serialBaud = 
#if defined(SERIAL_BAUD)
  SERIAL_BAUD;
#else
  38400;
#endif
inline constexpr unsigned long millisBetweenStatus = 600000; //We send our status messages every ten minutes.
inline constexpr unsigned long maxMillisBetweenPings = 1300000; //If we don't receive a ping in just under 20 minutes, we restart.
inline constexpr unsigned short batteryHysterisis_mV = 50;

extern unsigned long weatherInterval; //Current weather interval.
/*extern unsigned long overrideStartMillis;
extern unsigned long overrideDuration;
extern bool overrideShort;*/
enum SleepModes { disabled = 0, idle = 1, powerSave = 2 };
extern SleepModes solarSleepEnabled;
extern SleepModes dbSleepEnabled;
extern bool continuousReceiveEnabled;
extern bool doDeepSleep;

//Recent Memory
extern unsigned long lastStatusMillis;

extern unsigned long lastPingMillis;

#if !defined(DEBUG) && !defined(MODEM)
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

#ifdef ATMEGA328PB
  const byte PRUSART1 = 4;
  extern volatile byte* PRR1;
  const byte PRTWI1 = 5;
  const byte PRPTC = 4;
  const byte PRTIM4 = 3;
  const byte PRSPI1 = 2;
  const byte PRTIM3 = 0;
  const byte ADC6D = 6;
  const byte ADC7D = 7;
#endif