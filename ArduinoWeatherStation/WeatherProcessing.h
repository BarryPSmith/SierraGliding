#pragma once
#include "LoraMessaging.h"
void setupWeatherProcessing();
void createWeatherData(MessageDestination& message);
byte getNextWeatherMessageSize();

#if DEBUG_Speed
bool speedDebugging = true;
void sendSpeedDebugMessage();
#endif
#if DEBUG_Direction
bool directionDebugging = true;
bool debugThisRound = true;
#endif
