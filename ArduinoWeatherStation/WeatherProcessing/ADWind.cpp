#ifdef ARGENTDATA_WIND
#include <Arduino.h>
#include "WeatherProcessing.h"
namespace WeatherProcessing
{
  //Get the wind direction for an argent data wind vane (8 distinct directions, sometimes landing between them).
  //This assumes a 4kOhm resistor in series to form a voltage divider
  byte getCurWindDirection()
  {
    //Ver 2: We're going to use the full range of a byte for wind. 0 = N, 255 =~ 1 degree west.
    //N  0
    //NNE 16
    //NE 32
    //ENE 48
    //E  64
    //ESE 80
    //SE 96
    //SSE 112
    //S  128
    //SSW 144
    //SW 160
    //WSW 176
    //W  192
    //WNW 208
    //NW 224
    //NNW 240
    int wdVoltage = analogRead(WIND_DIR_PIN);
#ifdef INVERSE_AD_WIND
    wdVoltage = 1023 - wdVoltage;
#endif
    //WX_PRINTVAR(wdVoltage);
    if (wdVoltage < 168)
      return 80;  // 5 (ESE)
    if (wdVoltage < 195)
      return 48;  // 3 (ENE)
    if (wdVoltage < 236)
      return 64;  // 4 (E)
    if (wdVoltage < 315)
      return 112;  // 7 (SSE)
    if (wdVoltage < 406)
      return 96;  // 6 (SE)
    if (wdVoltage < 477)
      return 144;  // 9 (SSW)
    if (wdVoltage < 570)
      return 128;  // 8 (S)
    if (wdVoltage < 662)
      return 16;  // 1 (NNE)
    if (wdVoltage < 742)
      return 32;  // 2 (NE)
    if (wdVoltage < 808)
      return 176; // B (WSW)
    if (wdVoltage < 842)
      return 160; // A (SW)
    if (wdVoltage < 889)
      return 240; // F (NNW)
    if (wdVoltage < 923)
      return 0;  // 0 (N)
    if (wdVoltage < 949)
      return 208; // D (WNW)
    if (wdVoltage < 977)
      return 224; // E (NW)
    return 192;   // C (W)
  }
}
#endif