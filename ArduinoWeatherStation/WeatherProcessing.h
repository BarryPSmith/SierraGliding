#pragma once
#include "Messaging.h"
void setupWeatherProcessing();
void createWeatherData(MessageDestination& message);
void setupWindCounter();

const int voltagePin = A2;
const int windDirPin = A0;
const int windSpdPin = 2;

#if DEBUG_Speed
bool speedDebugging = true;
void sendSpeedDebugMessage();
#endif
#if DEBUG_Direction
bool directionDebugging = true;
bool debugThisRound = true;
#endif
byte GetWindDirection(int);
