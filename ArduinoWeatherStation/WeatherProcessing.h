#pragma once
#include "LoraMessaging.h"

namespace WeatherProcessing
{
  void setupWeatherProcessing();
  void processWeather();
  void createWeatherData(MessageDestination& message);
  bool handleWeatherCommand(MessageSource& src);
  extern volatile bool weatherRequired;
}
