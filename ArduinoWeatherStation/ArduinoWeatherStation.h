#pragma once
#include "revid.h"

static const char* ver = "1.1." REV_ID; //Wind station Version 1.1
//const char* statusMessageTemplate = "KN6DUC WindStation1";
//const int statusMessageLength = 13;

//Station Specific Constants
const char stationId = '2';

//Global Constants
const unsigned long tncBaud = 38400;
const unsigned long millisBetweenStatus = 600000; //We send our status messages this often. Note that status message stuff is currently commented out.
const unsigned long wakePeriod = 50; //We won't go to sleep if our next weather send is due before this time

//const int messageOffset = 8; //Start FEND, 0x00, Callsign
const int minMessageSize = 18;// Radioshield will not transmit shorter than 15 bytes. We also need FEND 0x00 ... FEND
const int extraMessageBytes = 9; //Minimum number of bytes needed to write FEND 0x00 CALLSIGN [...] FEND

//Setup Variables
extern unsigned long shortInterval; //Send interval when battery voltage is high
extern unsigned long longInterval; //Send interval when battery voltage is low
extern float batteryThreshold;
extern float batteryHysterisis;
extern bool demandRelay;

extern unsigned long weatherInterval; //Current weather interval.
extern unsigned long overrideStartMillis;
extern unsigned long overrideDuration;
extern bool overrideShort;


//Recent Memory
extern unsigned long lastStatusMillis;
extern unsigned long lastWeatherMillis;

//void sendMessage(byte* msgBuffer, size_t& messageLength, size_t bufferLength);
