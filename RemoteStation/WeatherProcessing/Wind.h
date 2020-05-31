#pragma once

namespace WeatherProcessing
{
  byte getCurWindDirection();
  void doSampleWind();
  byte atan2ToByte(float x, float y);

#ifdef DAVIS_WIND
  void calibrateWindDirection();
#else
  static inline void calibrateWindDirection() {}
#endif

#ifdef ALS_WIND
  void initWind();
#else
  static inline void initWind() {}
#endif
}