#pragma once
#include "../LoraMessaging.h"

//#define DEBUG_WEATHER
#ifdef DEBUG_WEATHER
#define WEATHER_PRINT AWS_DEBUG_PRINT
#define WEATHER_PRINTLN AWS_DEBUG_PRINTLN
#define WEATHER_PRINTVAR PRINT_VARIABLE
#else
#define WEATHER_PRINT(...)  do { } while (0)
#define WEATHER_PRINTLN(...)  do { } while (0)
#define WEATHER_PRINTVAR(...)  do { } while (0)
#endif

namespace WeatherProcessing
{
  void setupWeatherProcessing();
  void processWeather();
  void createWeatherData(MessageDestination& message);
  bool handleWeatherCommand(MessageSource& src);
  extern volatile bool weatherRequired;

  extern short internalTemperature;
  extern float externalTemperature;
}
