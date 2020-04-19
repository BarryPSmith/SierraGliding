#include "../TimerTwo.h"
#include "../LoraMessaging.h"
#include "WeatherProcessing.h"
#include "../ArduinoWeatherStation.h"
#include "../PermanentStorage.h"
#include "Wind.h"
#include <avr/boot.h>

#if (defined(DAVIS_WIND) && defined(ARGENTDATA_WIND)) || \
    (defined(DAVIS_WIND) && defined(ALS_WIND)) || \
    (defined(ARGENTDATA_WIND) && defined(ALS_WIND))
#error Multiple wind systems defined
#endif
#if !defined(DAVIS_WIND) && !defined(ARGENTDATA_WIND) && !defined(ALS_WIND)
#error No wind system defined
#endif

#define DEBUG_PWM
#ifdef DEBUG_PWM
namespace PwmSolar
{
  extern byte solarPwmValue;
}
#endif

namespace WeatherProcessing
{
  volatile bool weatherRequired = true;
  unsigned volatile long tickCounts = 0;
  unsigned long requiredTicks = 100;

  short internalTemperature;
  float externalTemperature;

  #ifdef WIND_DIR_AVERAGING
  float curWindX = 0, curWindY = 0;
  volatile bool sampleWind = false;
  #endif



  void countWind();
  void timer2Tick();
  void updateBatterySavings(unsigned short batteryVoltage_mv, bool startup);
  void setTimerInterval();
  void setupWindCounter();
  byte getExternalTemperature();
  byte getInternalTemperature();
  byte getCurWindDirection();

  volatile int windCounts = 0;

  //We use unsigned long for these because they are involved in 32-bit calculations.
  constexpr unsigned long mV_Ref = REF_MV;
  constexpr unsigned long BattVNumerator = BATTV_NUM;
  constexpr unsigned long BattVDenominator = BATTV_DEN;
  constexpr unsigned long MaxBatt_mV = 7500;
  
  static byte simpleMessagesSent = 255;
  constexpr byte complexMessageFrequency 
  #if 0 //defined(DEBUG_PWM)
    = 1;
  #elif defined(DEBUG)
    = 3;
  #else
    = 10;
  #endif

  unsigned long lastWindCountMillis;
  constexpr byte minWindInterval = 3; //Debounce. 330 kph = broken station;
  constexpr byte windSampleTicks = 100 / TimerTwo::MillisPerTick;

  byte getWindDirection()
  {
#if WIND_DIR_AVERAGING
    WX_PRINTVAR(curWindX);
    WX_PRINTVAR(curWindY);
    if (curWindX != 0 || curWindY != 0)
    {
      auto ret = atan2ToByte(curWindX, curWindY);
      curWindX = curWindY = 0;
      return ret;
    }
    else
      return getCurWindDirection();
#else
    return getCurWindDirection();
#endif
  }

  byte __attribute__ ((noinline)) atan2ToByte(float x, float y)
  {
    //These two are equivalent, just the implemented one has only one mult operation and only one add operation.
    //(byte)((atan2(-curWindX, -curWindY) / (2 * PI) + 0.5) * 255 + 0.5);
    return (byte)(atan2(-x, -y) * (255 / (2 * PI)) + (0.5* 255 + 0.5));
  }

  inline uint16_t getWindSpeed_x8()
  {
    noInterrupts();
    int localCounts = windCounts;
    windCounts = 0;
    interrupts();
  #if defined(ARGENTDATA_WIND) || defined(ALS_WIND)
    return (8UL * 2400 * localCounts) / weatherInterval;
  #elif defined(DAVIS_WIND)
    return (8UL * 3600 * localCounts) / weatherInterval;
  #else
    #error Cannot get Wind Speed
  #endif
  }

  uint8_t getWindSpeedByte(const uint16_t windSpeed_x8)
  {
    //Apply simple staged compression to the wsByte to allow accurate low wind while still capturing high wind.
    if (windSpeed_x8 <= 400) //.5 km/h resolution to 50km/h. Max 100
      return (byte)(windSpeed_x8 / 4); //windspeed = byte * 0.5
    else if (windSpeed_x8 < 1000) // 1 km/h resultion to 125km/h. Max 175
      return (byte)(windSpeed_x8 / 8 + 50); //windspeed = byte - 50
    else if (windSpeed_x8 < 285 * 8) // 2 km/h resolution to 285km/h. Max 255
      return (byte)(windSpeed_x8 / 16 + 113); //windspeed = (byte - 113) * 2
    else //We're just going to report 285km/h. It'll be a long search for the station.
      return 255;
  }

  void createWeatherData(MessageDestination& message)
  {
  #ifdef DEBUG
    unsigned long entryMicros = micros();
  #endif // DEBUG

  #ifdef WIND_PWR_PIN
    digitalWrite(WIND_PWR_PIN, HIGH);
    delayMicroseconds(1000); //use delayuS instead of standard to avoid putting the device to sleep.
  #endif

    bool isComplex = simpleMessagesSent >= complexMessageFrequency - 1;

    byte length = isComplex ? 5 : 2;
#ifdef DEBUG_PWM
    if (isComplex)
      length++;
#endif
    
    message.appendByte(length);

    //Message format is W(StationID)(UniqueID)(DirHex)(Spd * 2)(Voltage)
  
    auto localCounts = windCounts;
    uint16_t windSpeed_x8 = getWindSpeed_x8();

    //Update the send interval only after we calculate windSpeed, because windSpeed is dependent on weatherInterval
    unsigned short batt_mV = readBattery();
    updateBatterySavings(batt_mV, false);
  
    byte windDirection = getWindDirection();
    message.appendByte(windDirection);
    byte wsByte = getWindSpeedByte(windSpeed_x8);

    message.appendByte(wsByte);
    
    byte batteryByte, externalTempByte, internalTempByte;
    if (isComplex)
    {
      batteryByte = (byte)(255UL * batt_mV / MaxBatt_mV);
      message.appendByte(batteryByte);

      externalTempByte = getExternalTemperature();
      message.appendByte(externalTempByte);

      internalTempByte = getInternalTemperature();
      message.appendByte(internalTempByte);

#ifdef DEBUG_PWM
      message.appendByte(PwmSolar::solarPwmValue);
#endif
    
      simpleMessagesSent = 0;
    }
    else
      simpleMessagesSent++;
    #ifdef WIND_PWR_PIN
    digitalWrite(WIND_PWR_PIN, LOW);
    #endif
  #ifdef DEBUG
    unsigned long weatherPrepMicros = micros() - entryMicros;
    //WX_PRINTVAR(weatherPrepMicros);
  #endif
  #if 1
    WX_PRINTLN(F("  ======================"));
    WX_PRINT(F("batt_mV: "));
    WX_PRINTLN(batt_mV);
    WX_PRINT(F("Wind Counts: "));
    WX_PRINTLN(localCounts);
    WX_PRINT(F("windDirection byte: "));
    WX_PRINTLN(windDirection);
    WX_PRINT(F("windSpeed byte: "));
    WX_PRINTLN(wsByte);
    if (isComplex)
    {
      WX_PRINTVAR(batteryByte);
      WX_PRINTVAR(externalTempByte);
      WX_PRINTVAR(internalTempByte);
    }
    WX_PRINTLN(F("  ======================"));
  #endif
  }

  unsigned short readBattery()
  {
    int batteryVoltageReading = analogRead(BATT_PIN);
    return mV_Ref * BattVNumerator * batteryVoltageReading / (BattVDenominator  * 1023);
  }

  void updateBatterySavings(unsigned short batteryVoltage_mV, bool initial)
  {
    unsigned long oldInterval = weatherInterval;
    bool overrideActive = millis() - overrideStartMillis < overrideDuration;
    unsigned short batteryThreshold_mV, batteryEmergencyThresh_mV;
    GET_PERMANENT_S(batteryThreshold_mV);
    GET_PERMANENT_S(batteryEmergencyThresh_mV);
    if (!initial && batteryVoltage_mV < batteryEmergencyThresh_mV)
    {
      WX_PRINTLN(F("Entering Deep Sleep"));
      doDeepSleep = true;
    }
    else 
    {
      WX_PRINTLN(F("No Deep Sleep"));
      doDeepSleep = false;
      if (initial || 
        (!overrideActive && batteryVoltage_mV > batteryThreshold_mV + batteryHysterisis_mV) ||
        (overrideActive && overrideShort))
      {
        GET_PERMANENT2(&weatherInterval, shortInterval);
        continuousReceiveEnabled = true;
      } else if ((!overrideActive && batteryVoltage_mV < batteryThreshold_mV - batteryHysterisis_mV) ||
          (overrideActive && !overrideShort))
      {
        GET_PERMANENT2(&weatherInterval, longInterval);
        continuousReceiveEnabled = false;
      }
    }
    //Ensure that once we get out of override, we won't accidently go back into it due to millis wraparound.
    if (!overrideActive)
      overrideDuration = 0;
  
    setTimerInterval();
  }

  void setTimerInterval()
  {
    WX_PRINT(F("weather interval: "));
    WX_PRINTLN(weatherInterval);
    requiredTicks = weatherInterval / TimerTwo::MillisPerTick;
    if (requiredTicks < 1)
    {
      WX_PRINTLN(F("Required ticks is zero!"));
      requiredTicks = 1;
    }
    WX_PRINT(F("requiredTicks: "));
    WX_PRINTLN(requiredTicks);
    weatherInterval = requiredTicks * TimerTwo::MillisPerTick;
  }

  void countWind()
  {
    // no counting in deep sleep / debounce the signal:
    if (doDeepSleep || millis() - lastWindCountMillis < minWindInterval)
      return;
    #ifdef WIND_DIR_AVERAGING
    sampleWind = true;
    #endif
    lastWindCountMillis = millis();
    windCounts++;
  }

  void processWeather()
  {
    #ifdef WIND_DIR_AVERAGING
    cli();
    bool localSample = sampleWind;
    sampleWind = false;
    sei();
    if (localSample)
      doSampleWind();
    #endif
  }

#ifndef ALS_WIND
  void doSampleWind()
  {
      #ifdef WIND_PWR_PIN
        digitalWrite(WIND_PWR_PIN, HIGH);
        delayMicroseconds(1000); //use delayuS instead of standard to avoid putting the device to sleep.
      #endif
      byte wd = getCurWindDirection();
      #ifdef WIND_PWR_PIN
        digitalWrite(WIND_PWR_PIN, LOW);
      #endif
      curWindX += sin(2 * PI * wd / 255);
      curWindY += cos(2 * PI * wd / 255);
#if 0
      WX_PRINTVAR(windCounts);
      WX_PRINTVAR(wd);
      WX_PRINTVAR(curWindX);
      WX_PRINTVAR(curWindY);
#endif
  }
#endif // !ALS_WIND

  void setupWindCounter()
  {
    windCounts = 0;
    //pinMode or digitalWrite should give the same results:
    pinMode(WIND_SPD_PIN, INPUT_PULLUP);
    //digitalWrite(windSpdPin, HIGH);
    attachInterrupt(digitalPinToInterrupt(WIND_SPD_PIN), countWind, RISING);
  }

  void setupWeatherProcessing()
  {
    initWind();
    updateBatterySavings(0, true);
    TimerTwo::attachInterrupt(&timer2Tick);
    setupWindCounter();
  #ifdef WIND_PWR_PIN
    pinMode(WIND_PWR_PIN, OUTPUT);
    digitalWrite(WIND_PWR_PIN, LOW);
  #endif
  }

  byte getInternalTemperature()
  {
  #ifdef NO_INTERNAL_TEMP
    return 0;
  #endif
    signed char tsOffset;// = boot_signature_byte_get(3);
    byte tsGain;// = boot_signature_byte_get(5);
    // The documentation around factory values of tsOffset and tsGain is bullshit. So we're going to have to manually calculate them...
    GET_PERMANENT_S(tsOffset);
    GET_PERMANENT_S(tsGain);

    auto oldMux = ADMUX;
    // Set 1.1V internal reference, set to channel 8 (internal temperature reference)
    ADMUX = _BV(REFS0) | _BV(REFS1) | _BV(MUX3);
    // Wait a short while to ensure the ADC reference capacitor is stable:
    // We can reduce this to 5us, see the last post in https://forum.arduino.cc/index.php?topic=22922.0
    // (We only have a 10nF capacitor on Aref, not 100nF like the arduino, so we're probably OK with just 1ms delay here.)
    delay(10);
    // Start the conversion
    ADCSRA = ADCSRA | _BV(ADSC);
    // Wait for conversion
    while (!(ADCSRA & _BV(ADIF)));

    // Restore the old reference, allow the reference cap to settle
    ADMUX = oldMux;
    delay(1);

#if 0
    WX_PRINT("Weather reading: ");
    WX_PRINTLN(ADC);
    WX_PRINTVAR(tsOffset);
    WX_PRINTVAR(tsGain);
#endif

    //The point at which tsGain has no effect is at 100C.
    int temp_x2 = (((long)ADC + tsOffset - (273 + 100)) * 256) / tsGain + 100 * 2;
    // T = ADC * 128 / tsGain + (tsOffset - 373) * 128 / tsGain + 100
    internalTemperature = temp_x2 / 2;
    int temp_x2_offset = temp_x2 + 64;
    if (temp_x2_offset < 0)
      temp_x2_offset = 0;
    if (temp_x2_offset > 255)
      temp_x2_offset = 255;
    return temp_x2_offset;

    // To calibrate internal temperature: Place the external temperature probe in the case with the main board.
    // Let it run for a while, measuring a variety of temperatures.
    // Plot External Temp vs Internal Temp. Get a line of best fit in the form ET = A * IT + B
    // New constants are:
    // tsGain = tsGain(orig) / A
    // tsOffset = tsOffset(orig) + tsGain(orig) / (128 * A) * (100 * (A - 1) + B)
  }

  byte getExternalTemperature()
  {
  #ifdef ALS_TEMP
  #pragma message("Using ALS Temperature")
    float T = externalTemperature;
  #elif defined(TEMP_SENSE)
  #pragma message("Using NTC Thermistor")
    auto tempReading = analogRead(TEMP_SENSE);
    // V = Vref * R1 / (R1 + R2)
    // V / (Vref * R1) = 1 / (R1 + R2)
    // R1 * Vref / V = R1 + R2
    // 0 = (1 - Vref / V) R1 + R2
    // R1 = R2 / (Vref / V - 1)
    // R2 / R1 = Vref / V - 1
    // Vref / V = 1023 / reading
    float r2_r1 = 1023.0 / tempReading - 1;
    constexpr float invB = 1/3892.0;
    constexpr float T0 = 273.15 + 25;
    float T = 1/(1/T0 - invB * log(r2_r1));

    //If tempReading = 1000, r2_r1 is v. small
    //then log(r2_r1) = large negative
    //then T = 1/(C - large negative) = 1/(C + large positive) = small!
    //So at low temperatures we want large reading. Thermistor between sense and GND.

    T -= 273.15;
    
    externalTemperature = T;

    //WX_PRINTVAR(tempReading);
  #else
    #pragma message("No temperature")
    return 0;
  #endif
#if defined(ALS_TEMP) || defined(TEMP_SENSE)
    //We send temperature as a byte, ranging from -32 to 95 C (1 LSB = half a degree)
    if (T < -32)
      T = -32;
    if (T > 95)
      T = 95;
    return (T + 0.5 + 32) * 2;
#endif
  }

  void timer2Tick()
  {
    if (++tickCounts >= requiredTicks)
    {
      tickCounts = 0;
      weatherRequired = true;
    }
    if (windSampleTicks == 0 || tickCounts % windSampleTicks == 0)
      sampleWind = true;
  }

  bool handleWeatherCommand(MessageSource& src)
  {
    byte commandType;
    if (src.readByte(commandType))
      return false;

    byte newValue, tsOffset, tsGain;
    if ((commandType == 'O' || commandType == 'G') &&
        src.readByte(newValue))
      return false;

    switch (commandType)
    {
    case 'C': // WC - perform wind calibration
      calibrateWindDirection();
      return true;
    case 'O': // WO - set temp offset
      tsOffset = newValue;
      SET_PERMANENT(tsOffset);
      return true;
    case 'G': // WG - set temp gain
      tsGain = newValue;
      SET_PERMANENT(tsGain);
      return true;
    default:
      return false;
    }
  }
}