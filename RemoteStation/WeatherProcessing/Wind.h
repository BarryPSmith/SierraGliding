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

  // CHECK: Is the compiler smart enough to not create this struct if it's never used?
  struct WS80_Reading {
    //16 bytes
  public:
    uint16_t light; // 2
    uint16_t battery_mv; // 2
    float temp; // 4
    uint8_t humidity; // 1
    uint16_t wind_avg_x50; // 2
    uint16_t wind_max_x50; // 2
    uint8_t uv_index_x10; // 1
    uint8_t wind_dir; // 1
  };
  extern WS80_Reading lastReading;
}