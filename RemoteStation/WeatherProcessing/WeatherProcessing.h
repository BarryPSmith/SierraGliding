#pragma once
#include "../LoraMessaging.h"
#include "../ArduinoWeatherStation.h"

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
  void createWeatherData(MESSAGE_DESTINATION_SOLID<254>& message);
  bool handleWeatherCommand(MessageSource& src);
  unsigned short readBattery();
  void updateBatterySavings(unsigned short batteryVoltage_mV, bool initial);
  //ALS only:
#ifdef ALS_WIND
  bool writeAlsEeprom();
#ifdef ALS_FIELD_STRENGTH
  extern unsigned long curFieldSquared;
  extern short curSampleCount;
#endif
#endif

  extern volatile bool weatherRequired;
  extern volatile unsigned short windCounts;

  extern short internalTemperature_x2;
  extern float externalTemperature;
}
