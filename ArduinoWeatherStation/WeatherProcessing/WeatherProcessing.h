#pragma once
#include "../LoraMessaging.h"
#include "../ArduinoWeatherStation.h"

#define DEBUG_WEATHER
#ifdef DEBUG_WEATHER
#define WX_PRINT AWS_DEBUG_PRINT
#define WX_PRINTLN AWS_DEBUG_PRINTLN
#define WX_PRINTVAR PRINT_VARIABLE
#define WX_DEBUG AWS_DEBUG
#else
#define WX_PRINT(...)  do { } while (0)
#define WX_PRINTLN(...)  do { } while (0)
#define WX_PRINTVAR(...)  do { } while (0)
#define WX_DEBUG(...) do { } while (0)
#endif

namespace WeatherProcessing
{
  void setupWeatherProcessing();
  void processWeather();
  void createWeatherData(MessageDestination& message);
  bool handleWeatherCommand(MessageSource& src);
  unsigned short readBattery();
  void updateBatterySavings(unsigned short batteryVoltage_mV, bool initial);
  //ALS only:
#ifdef ALS_WIND
  bool writeAlsEeprom();
#endif

  extern volatile bool weatherRequired;

  extern short internalTemperature;
  extern float externalTemperature;
}
