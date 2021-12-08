#include "../TimerTwo.h"
#include "../LoraMessaging.h"
#include "WeatherProcessing.h"
#include "../ArduinoWeatherStation.h"
#include "../PermanentStorage.h"
#include "Wind.h"
#include <avr/boot.h>
#include "../PWMSolar.h"

//#define DEBUG_IT

#if (defined(DAVIS_WIND) && defined(ARGENTDATA_WIND)) || \
    (defined(DAVIS_WIND) && defined(ALS_WIND)) || \
    (defined(ARGENTDATA_WIND) && defined(ALS_WIND))
#error Multiple wind systems defined
#endif
#if !defined(DAVIS_WIND) && !defined(ARGENTDATA_WIND) && !defined(ALS_WIND)
#error No wind system defined
#endif


namespace WeatherProcessing
{
  volatile bool weatherRequired = true;
  unsigned volatile long tickCounts = 0;
  unsigned long requiredTicks = 0xFFFFFF;

  short internalTemperature_x2;
  float externalTemperature;

  #ifdef WIND_DIR_AVERAGING
  float curWindX = 0, curWindY = 0;
  volatile bool sampleWind = false;
  #endif



  void countWind();
  void timer2Tick();
  void setTimerInterval();
  void setupWindCounter();
  byte getExternalTemperature();
  byte getInternalTemperature(short& reading);
  byte getCurWindDirection();

  volatile unsigned short windCounts = 0;
  volatile unsigned short minInterval = 0xFFFF;
  bool hasTicked = false;

  //We use unsigned long for these because they are involved in 32-bit calculations.
  constexpr unsigned long mV_Ref = REF_MV;
  constexpr unsigned long BattVNumerator = BATTV_NUM;
  constexpr unsigned long BattVDenominator = BATTV_DEN;
  constexpr unsigned long MaxBatt_mV = 7500;
  
  static byte simpleMessagesSent = 255;
  constexpr byte complexMessageFrequency 
  #if defined(DEBUG_PWM)
    = 1;
  #elif defined(DEBUG)
    = 3;
  #else
    = 10;
  #endif

  unsigned long lastWindCountMillis;
  constexpr byte minWindIntervalTest = 3; //Debounce. 330 kph = broken station;
  constexpr byte windSampleShortTicks = 100 / TimerTwo::MillisPerTick;
  constexpr byte windSampleLongTicks = 1000 / TimerTwo::MillisPerTick;
  volatile byte windSampleTicks = windSampleShortTicks;

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
    auto ret = (byte)(atan2(-x, -y) * (255 / (2 * PI)) + (0.5* 255 + 0.5));
#ifdef ALS_WIND
    ret -= 96; //Account for the fact that we put the sensor in 135 degree out.
#endif
    return ret;
  }

  inline uint16_t getWindSpeed_x8()
  {
    noInterrupts();
    short localCounts = windCounts;
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

  inline uint16_t getWindGust_x8()
  {
    noInterrupts();
    unsigned short localInterval = minInterval;
    minInterval = 0xFFFF;
    interrupts();
  #if defined (ARGENTDATA_WIND) || defined(ALS_WIND)
    return ((unsigned short)8 * 2400) / localInterval;
  #elif defined (DAVIS_WIND)
    return ((unsigned short)8 * 3600) / localInterval;
  #else
    #error Cannot get wind gust
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

  void createWeatherData(LoraMessageDestination& message)
  {
  #ifdef DEBUG
    unsigned long entryMicros = micros();
  #endif // DEBUG

  #ifdef WIND_PWR_PIN
    digitalWrite(WIND_PWR_PIN, HIGH);
    delayMicroseconds(1000); //use delayuS instead of standard to avoid putting the device to sleep.
  #endif

    bool isComplex = simpleMessagesSent >= complexMessageFrequency - 1;

    byte length = isComplex ? 8 : 3;

    if (isComplex)
    {
#ifdef DEBUG_PWM
      length += 9;
#endif
#ifdef DEBUG_IT
      length += 2;
#endif
#if defined(ALS_WIND) && defined(ALS_FIELD_STRENGTH)
      length += 4;
#endif
    }
    
    message.appendByte(length);

    //Message format is W(StationID)(UniqueID)(DirHex)(Spd * 2)(Voltage)
  
    WX_DEBUG(auto localCounts = windCounts);
    uint16_t windSpeed_x8 = getWindSpeed_x8();
    byte wsByte = getWindSpeedByte(windSpeed_x8);

    auto localInterval = minInterval;

    uint16_t windGust_x8 = getWindGust_x8();
    byte wgByte = getWindSpeedByte(windGust_x8);

    //Update the send interval only after we calculate windSpeed, because windSpeed is dependent on weatherInterval
    unsigned short batt_mV = readBattery();
    
    byte windDirection = getWindDirection();
    message.appendByte(windDirection);

    message.appendByte(wsByte);
    message.appendByte(wgByte);
    
    byte batteryByte, externalTempByte, internalTempByte;
    if (isComplex)
    {
      batteryByte = (byte)(255UL * batt_mV / MaxBatt_mV);
      message.appendByte(batteryByte);

      externalTempByte = getExternalTemperature();
      message.appendByte(externalTempByte);

      short iTReading;
      internalTempByte = getInternalTemperature(iTReading);
      message.appendByte(internalTempByte);

      message.appendByte(PwmSolar::solarPwmValue);
      message.appendByte(PwmSolar::curCurrent_mA_x6/6);
#ifdef DEBUG_IT
      message.appendT(iTReading);
#endif
#ifdef DEBUG_PWM
      message.appendT(PwmSolar::debug_applyLimits); //1
      message.appendT(PwmSolar::debug_desired); //2
      message.appendT(PwmSolar::debug_change); //2
      message.appendT(PwmSolar::lastCurrentCheckMillis); //2
      message.appendT(PwmSolar::debug_curMillis); //2
#endif
#if defined(ALS_WIND) && defined(ALS_FIELD_STRENGTH)
      message.appendT(curFieldSquared / curSampleCount);
      curFieldSquared = 0;
      curSampleCount = 0;
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
    WX_PRINTVAR(wgByte);
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
    batteryReading_mV = mV_Ref * BattVNumerator * batteryVoltageReading / (BattVDenominator  * 1023);
    return batteryReading_mV;
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
    unsigned long thisMillis = millis();
    unsigned long thisInterval = thisMillis - lastWindCountMillis;
    // no counting in deep sleep / debounce the signal:
    if (batteryMode == BatteryMode::DeepSleep || thisInterval < minWindIntervalTest)
      return;
    lastWindCountMillis = thisMillis;
    if (hasTicked && thisInterval < minInterval)
      minInterval = thisInterval;
    hasTicked = true;
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
    {
      doSampleWind();
    }
    #endif
  }

#if !defined(ALS_WIND) && defined(WIND_DIR_AVERAGING)
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
    static_assert(WIND_SPD_PIN == 2);
    EICRA |= _BV(ISC01) | _BV(ISC00);
    EIMSK |= _BV(INT0);
  }

  ISR(INT0_vect)
  {
    countWind();
  }

  void setupWeatherProcessing()
  {
    weatherRequired = false;
    tickCounts = 0;
    initWind();
    setupWindCounter();
  #ifdef WIND_PWR_PIN
    pinMode(WIND_PWR_PIN, OUTPUT);
    digitalWrite(WIND_PWR_PIN, LOW);
  #endif
  #ifdef TEMP_PWR_PIN
    pinMode(TEMP_PWR_PIN, OUTPUT);
    digitalWrite(TEMP_PWR_PIN, LOW);
  #endif
  }

  void enterDeepSleep()
  {
    GET_PERMANENT2(&weatherInterval, longInterval);
    WeatherProcessing::setTimerInterval();
#ifdef ALS_WIND
    sleepWind();
#endif
  }

  void enterBatterySave()
  {
    windSampleTicks = windSampleLongTicks;
    GET_PERMANENT2(&weatherInterval, longInterval);
    WeatherProcessing::setTimerInterval();
#ifdef ALS_WIND
    setWindLowPower();
#endif
  }

  void enterNormalMode()
  {
    windSampleTicks = windSampleShortTicks;
    GET_PERMANENT2(&weatherInterval, shortInterval);
    WeatherProcessing::setTimerInterval();
#ifdef ALS_WIND
    setWindNormal();
#endif
  }

  byte getInternalTemperature(short& reading)
  {
#ifdef ALS_TEMP
#pragma message("Using ALS Temperature")
    if (internalTemperature_x2 < -64)
      return 0;
    else if (internalTemperature_x2 > 190)
      return 255;
    else
      return internalTemperature_x2 + 64;
#endif
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
    // use DelayMicroseconds here rather than delay because we don't want to yield(), which might do Solar PWM processing.
    delayMicroseconds(10000);
    // Start the conversion
    ADCSRA = ADCSRA | _BV(ADSC);
    // Wait for conversion
    while (!(ADCSRA & _BV(ADIF)));

    // Restore the old reference, allow the reference cap to settle
    reading = ADC;
    ADMUX = oldMux;
    delay(1);

#if 0
    WX_PRINT("Weather reading: ");
    WX_PRINTLN(ADC);
    WX_PRINTVAR(tsOffset);
    WX_PRINTVAR(tsGain);
#endif

    //The point at which tsGain has no effect is at 100C.
    // (ADC + Offset - 173) * 128 / gain + 100
    int temp_x2 = (((long)reading + tsOffset - (273 + 100)) * 256) / tsGain + 100 * 2;
    // T = ADC * 128 / tsGain + (tsOffset - 373) * 128 / tsGain + 100
    internalTemperature_x2 = temp_x2;
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

    // If calibrating using DEBUG_IT:
    // temp = (ADC + tsOffset - 373) * 128 / gain + 100
    // temp = [100 + (tsOffset - 373) * 128 / gain] + ADC * 128 / gain
    // Find linear relationship between temp and ADC, then:
    // For a rationship T = intersect + slope * ADC
    // gain = 128 / slope
    // offset = (intersect - 100) / slope + 373
  }

  byte getExternalTemperature()
  {
#if defined(TEMP_SENSE)
  #pragma message("Using NTC Thermistor")
#ifdef TEMP_PWR_PIN
    digitalWrite(TEMP_PWR_PIN, HIGH);
    delay(3);
#endif
    auto tempReading = analogRead(TEMP_SENSE);
#ifdef TEMP_PWR_PIN
    digitalWrite(TEMP_PWR_PIN, LOW);
#endif
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
#ifdef WIND_DIR_AVERAGING
    if (windSampleTicks == 0 || tickCounts % windSampleTicks == 0)
      sampleWind = true;
#endif
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
#ifdef ALS_WIND
    case 'E':
      return writeAlsEeprom();
#endif
    default:
      return false;
    }
  }
}

void timer2InterruptAction()
{
  WeatherProcessing::timer2Tick();
}