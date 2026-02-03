#pragma once

namespace WeatherProcessing
{
  byte getCurWindDirection();
  void doSampleWind();
  byte atan2ToByte(float x, float y);

#if defined(DAVIS_WIND) || defined(ALS_WIND)
  void calibrateWindDirection();
#else
  static inline void calibrateWindDirection() {}
#endif

#ifdef ALS_WIND
  void initWind();
#else
  static inline void initWind() {}
#endif
#ifdef WS80_WIND

  //16 bytes
  struct WS80_Reading {
  public:
    uint16_t light; // 2
    uint16_t battery_mv; // 2
    float temp; // 4
    uint8_t humidity; // 1
    uint16_t wind_avg_x10; // 2
    uint16_t wind_max_x10; // 2
    uint8_t uv_index_x10; // 1
    uint8_t wind_dir; // 1
  };

  extern WS80_Reading lastReading;
#endif
}