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

//Station Specific Constants
const char stationID= '2';

//Global Constants
const unsigned long tncBaud = 38400;
const unsigned long millisBetweenStatus = 600000; //We send our status messages this often. Note that status message stuff is currently commented out.


extern float batteryHysterisis; //This should be a DEFINE

extern unsigned long weatherInterval; //Current weather interval.
extern unsigned long overrideStartMillis;
extern unsigned long overrideDuration;
extern bool overrideShort;

//Recent Memory
extern unsigned long lastStatusMillis;

extern volatile bool weatherRequired;
