#pragma once
#include "LoraMessaging.h"
void setupWeatherProcessing();
void createWeatherData(MessageDestination& message);
void setupWindCounter();

//const int voltagePin = A2;
//const int windDirPin = A0;

// Station 1 (Mt Tom)=2
// Station 2 (Flynns)=3
//const int windSpdPin = 3; 

#if DEBUG_Speed
bool speedDebugging = true;
void sendSpeedDebugMessage();
#endif
#if DEBUG_Direction
bool directionDebugging = true;
bool debugThisRound = true;
#endif
byte GetWindDirection(int);
