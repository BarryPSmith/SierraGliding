#include <Arduino.h>
#include "PermanentStorage.h"
#include "ArduinoWeatherStation.h"
#include "PWMSolar.h"
#include "WeatherProcessing/WeatherProcessing.h"

#define DEBUG_SOLAR
#ifdef DEBUG_SOLAR
byte loopCount;
constexpr byte printFreq = 1;
#define SOLAR_PRINT(...) do { if (loopCount % printFreq == 0) AWS_DEBUG_PRINT(__VA_ARGS__); } while (0)
#define SOLAR_PRINTLN(...) do { if (loopCount % printFreq == 0) AWS_DEBUG_PRINTLN(__VA_ARGS__); }  while (0)
#define SOLAR_PRINTVAR(...) do { if (loopCount % printFreq == 0) PRINT_VARIABLE(__VA_ARGS__); } while (0)
#else
#define SOLAR_PRINT(...)  do { } while (0)
#define SOLAR_PRINTLN(...)  do { } while (0)
#define SOLAR_PRINTVAR(...)  do { } while (0)
#endif

namespace PwmSolar
{
#define SOLAR_PWM_PIN 5 // Pin 5 is OCR0B

  //Battery sense has 33kOhm resistor and 10nF capacitor
  //Time constant is 2 milliseconds.
  //So... if our update interval is two time constants,
  //the battery voltage we read will be close to the actual battery voltage.
  constexpr unsigned int PwmUpdateInterval_uS = 4000;

  constexpr unsigned long mV_Ref = REF_MV;
  constexpr unsigned long BattVNumerator = BATTV_NUM;
  constexpr unsigned long BattVDenominator = BATTV_DEN;


  // This factor determines how quickly our PWM reacts.
  unsigned short chargeResponseRate;
  unsigned short safeFreezingChargeLevel_mV;
  byte safeFreezingPwm;
  unsigned short chargeVoltage_mV;

  unsigned long lastPwmMicros = 0;
  byte solarPwmValue = 0;

  int getNewPwmValue();
  byte getMaxPwm(unsigned short batteryVoltage_mV);
  unsigned short getDesiredBatteryVoltage();
  void turnOffSolar();
  void setSolarFull();
  void ensurePwmActive();

  void setupPwm()
  {
    GET_PERMANENT_S(chargeResponseRate);
    GET_PERMANENT_S(safeFreezingChargeLevel_mV);
    GET_PERMANENT_S(safeFreezingPwm);
    GET_PERMANENT_S(chargeVoltage_mV);
    pinMode(SOLAR_PWM_PIN, OUTPUT);
  }

  void doPwmLoop()
  {
    if (micros() - lastPwmMicros < PwmUpdateInterval_uS)
      return;
#ifdef DEBUG_SOLAR
    loopCount++;
#endif
    lastPwmMicros = micros();
    
    int newValue = getNewPwmValue();
    if (newValue <= 0)
    {
      turnOffSolar();
      solarPwmValue = 0;
    }
    else if (newValue >= 255)
    {
      setSolarFull();
      solarPwmValue = 255;
    }
    else
    {
      ensurePwmActive();
      solarPwmValue = OCR0B = newValue;
    }
  }

  int getNewPwmValue()
  {
    int batteryVoltageReading = analogRead(BATT_PIN);
    int batteryVoltage_mV = mV_Ref * BattVNumerator * batteryVoltageReading / (BattVDenominator  * 1023);
    
    int desiredVoltage_mV = getDesiredBatteryVoltage();

    int maxPwm = getMaxPwm(batteryVoltage_mV);

    int change = (long)(desiredVoltage_mV - batteryVoltage_mV) * chargeResponseRate / 256;

    int newValue = solarPwmValue + change;

    SOLAR_PRINTVAR(batteryVoltage_mV);
    SOLAR_PRINTVAR(solarPwmValue);
    SOLAR_PRINTVAR(change);

    if (newValue <= 0)
      return 0;
    else if (newValue >= maxPwm)
      return maxPwm;
    else
      return newValue;
  }

  //In cold temperatures with lithium ion, we must perform current and voltage limitation. maxPwm limits the maximum chart current.
  //Internet sources suggest 0.02C charge rate is acceptable in very cold temperatures.
  //So we can calculate the maximum PWM because we know the maximum current from the panel and the battery capacity.
  byte getMaxPwm(unsigned short batteryVoltage_mV)
  {
    //Undefined stuff will should be defined in permanent storage.
    if (batteryVoltage_mV < safeFreezingChargeLevel_mV) //If the battery voltage is less than 3.7V, we're unlikely to damage it with any current we throw at it.
      return 255;
    else if (WeatherProcessing::internalTemperature < 0 ||
             (WeatherProcessing::internalTemperature < 5 && WeatherProcessing::externalTemperature < 5)) //The battery might be colder than the MCU - thermal capacity, inaccurate measurement, etc.
      return safeFreezingPwm;
    else
      return 255;
  }

  unsigned short getDesiredBatteryVoltage()
  {
    // Get the desired voltage from our permanent variables. Probably 4.0V to ensure we don't overcharge the batteries because we CBF putting in a timer.
    // If our voltage every drops below some absolute minimum (2.5V?), we will should update that variable to a safe 'non-charging' level (3.7V?)
    return chargeVoltage_mV;
  }

  void turnOffSolar()
  {
    // Take control of the pin back.
    // write it HIGH (which will turn the switch off)
    // Record that sleep is allowed
    digitalWrite(SOLAR_PWM_PIN, HIGH);
    sleepEnabled = true;
  }

  void setSolarFull()
  {
    // Take control of the pin back
    // write it LOW (which will turn the switch on)
    // Record that sleep is allowed
    digitalWrite(SOLAR_PWM_PIN, LOW);
    sleepEnabled = true;
  }

  void ensurePwmActive()
  {
    // Set up the necessary registers
    // Ensure the MCU doesn't go to sleep.
    TIMSK0 = 0; //No interrupts
    TCCR0A = _BV(COM0B1) | _BV(COM0B0) | _BV(WGM01) | _BV(WGM00); //Fast PWM inverting on output B
    TCCR0B = _BV(CS00); // No prescaler: Operate at full clock frequency (PWM at 31.25kHz)
    sleepEnabled = false;
  }
}