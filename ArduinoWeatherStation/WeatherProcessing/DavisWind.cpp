#ifdef DAVIS_WIND
#include "WeatherProcessing.h"
#include "../PermanentStorage.h"

namespace WeatherProcessing
{
  void signalWindCalibration(unsigned long durationRemaining);
  //Get the wind direction for a davis vane.
  //Davis vane is a potentiometer sweep. 0 and full resistance correspond to 'north'.
  //We connect the ends of pot to +VCC and GND. So we just measure the centre pin and it linearly correlates with angle.
  //We're going to ignore the dead zone for now. Maybe test for it later. But given we intend to replace it all with ultrasonics, probably don't waste time.  
  byte getCurWindDirection()
  {
    int maxVoltage = 1023;
    int minVoltage = 0;
    int wdCalibMin, wdCalibMax;
    int pwrVoltage = 1023;
    GET_PERMANENT_S(wdCalibMin);
    GET_PERMANENT_S(wdCalibMax);
  #ifdef WIND_PWR_PIN
    pwrVoltage = analogRead(WIND_PWR_PIN);
    WX_PRINTVAR(pwrVoltage);
  #endif
    maxVoltage = ((long)wdCalibMax * pwrVoltage) / 1023;
    minVoltage = ((long)wdCalibMin * pwrVoltage) / 1023;
    int voltageDiff = maxVoltage - minVoltage;

    int wdVoltage = analogRead(WIND_DIR_PIN);
    WX_PRINT(F("wdVoltage: "));
    WX_PRINTLN(wdVoltage);

    int scaled = ((long)(wdVoltage - minVoltage + 2) * 255) / voltageDiff; //(The +2 is to make it do nearest rounding. Approximate voltageDiff ~1023)
    if (scaled < 0)
      scaled = 0;
    if (scaled > 255)
      scaled = 255;

    return scaled;
  }

  void calibrateWindDirection()
  {
#ifdef WIND_PWR_PIN
    digitalWrite(WIND_PWR_PIN, HIGH);
    delayMicroseconds(1000);
#endif
    //This must be long enough for an accurate calibration, but not so long that the watchdog kicks in.
    constexpr unsigned long calibrationDuration = 6000;
    unsigned long entryMillis = millis();
    int minValue = 1023;
    int maxValue = 0;
    int pwrVoltage = 1023;
    while (millis() - entryMillis < calibrationDuration)
    {
      #ifdef WIND_PWR_PIN
        pwrVoltage = analogRead(WIND_PWR_PIN);
        if (pwrVoltage == 0)
        {
          WX_PRINTLN(F("Power voltage is zero!"));
          SIGNALERROR();
          continue;
        }
      #endif
      int wdVoltageScaled = (analogRead(WIND_DIR_PIN) * 1023L) / pwrVoltage;
      if (wdVoltageScaled < minValue)
        minValue = wdVoltageScaled;
      if (wdVoltageScaled > maxValue)
        maxValue = wdVoltageScaled;

      signalWindCalibration(calibrationDuration - (millis() - entryMillis));
    }
#ifdef WIND_PWR_PIN
    digitalWrite(WIND_PWR_PIN, LOW);
#endif
#ifndef DEBUG
    signalOff();
#endif
    if (maxValue > minValue + 100)
    {
      SET_PERMANENT2(&minValue, wdCalibMin);
      SET_PERMANENT2(&maxValue, wdCalibMax);
    }
    else
    {
      SIGNALERROR();
      WX_PRINTLN(F("Calibration failed!"));
    }
    WX_PRINTLN(F("Calibration Result: "));
    WX_PRINTVAR(minValue);
    WX_PRINTVAR(maxValue);
  }

  void signalWindCalibration(unsigned long durationRemaining)
  {
    WX_PRINT(F("Calibration Time Remaining: "));
    WX_PRINTLN(durationRemaining);
    if (durationRemaining < 0)
      return;
  #ifndef DEBUG
    digitalWrite(LED_PIN0, durationRemaining % 2000 < 1000);
    unsigned long flashDuration = (durationRemaining / 1000) * 250 + 250;
    digitalWrite(LED_PIN1, durationRemaining % flashDuration > flashDuration / 2);
  #endif
  }
}
#endif