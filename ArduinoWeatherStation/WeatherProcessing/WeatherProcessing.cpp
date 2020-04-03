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
  void updateSendInterval(unsigned short batteryVoltage_mv, bool startup);
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
  #ifdef DEBUG
    = 3;
  #else
    = 10;
  #endif

  byte getWindDirection()
  {
#if WIND_DIR_AVERAGING
    WEATHER_PRINTVAR(curWindX);
    WEATHER_PRINTVAR(curWindY);
    if (curWindX != 0 || curWindY != 0)
    {
      auto ret = (byte)((atan2(-curWindX, -curWindY) / (2 * PI) + 0.5) * 255 + 0.5);
      curWindX = curWindY = 0;
      return ret;
    }
    else
      return getCurWindDirection();
#else
    return getCurWindDirection();
#endif
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
    message.appendByte(length);

    //Message format is W(StationID)(UniqueID)(DirHex)(Spd * 2)(Voltage)
    int batteryVoltageReading = analogRead(BATT_PIN);
    unsigned long batt_mV = mV_Ref * BattVNumerator * batteryVoltageReading / (BattVDenominator  * 1023);
  
    auto localCounts = windCounts;
    uint16_t windSpeed_x8 = getWindSpeed_x8();

    //Update the send interval only after we calculate windSpeed, because windSpeed is dependent on weatherInterval
    updateSendInterval(batt_mV, false);
  
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
    
      simpleMessagesSent = 0;
    }
    else
      simpleMessagesSent++;
    #ifdef WIND_PWR_PIN
    digitalWrite(WIND_PWR_PIN, LOW);
    #endif
  #ifdef DEBUG
    unsigned long weatherPrepMicros = micros() - entryMicros;
    //WEATHER_PRINTVAR(weatherPrepMicros);
  #endif
  #if 1
    WEATHER_PRINTLN(F("  ======================"));
    WEATHER_PRINT(F("batteryVoltageReading: "));
    WEATHER_PRINTLN(batteryVoltageReading);
    WEATHER_PRINT(F("batt_mV: "));
    WEATHER_PRINTLN(batt_mV);
    WEATHER_PRINT(F("Wind Counts: "));
    WEATHER_PRINTLN(localCounts);
    WEATHER_PRINT(F("windDirection byte: "));
    WEATHER_PRINTLN(windDirection);
    WEATHER_PRINT(F("windSpeed byte: "));
    WEATHER_PRINTLN(wsByte);
    if (isComplex)
    {
      WEATHER_PRINTVAR(batteryByte);
      WEATHER_PRINTVAR(externalTempByte);
      WEATHER_PRINTVAR(internalTempByte);
    }
    WEATHER_PRINTLN(F("  ======================"));
  #endif
  }

  void updateSendInterval(unsigned short batteryVoltage_mV, bool initial)
  {
    unsigned long oldInterval = weatherInterval;
    bool overrideActive = millis() - overrideStartMillis < overrideDuration;
    unsigned short batteryThreshold_mV;
    GET_PERMANENT_S(batteryThreshold_mV);
    if (initial || 
      (!overrideActive && batteryVoltage_mV > batteryThreshold_mV + batteryHysterisis_mV) ||
      (overrideActive && overrideShort))
    {
      GET_PERMANENT2(&weatherInterval, shortInterval);
    } else if ((!overrideActive && batteryVoltage_mV < batteryThreshold_mV - batteryHysterisis_mV) ||
        (overrideActive && !overrideShort))
    {
      GET_PERMANENT2(&weatherInterval, longInterval);
    }
    //Ensure that once we get out of override, we won't accidently go back into it due to millis wraparound.
    if (!overrideActive)
      overrideDuration = 0;
  
    setTimerInterval();
  }

  void setTimerInterval()
  {
    WEATHER_PRINT(F("weather interval: "));
    WEATHER_PRINTLN(weatherInterval);
    requiredTicks = weatherInterval / TimerTwo::MillisPerTick;
    if (requiredTicks < 1)
    {
      WEATHER_PRINTLN(F("Required ticks is zero!"));
      requiredTicks = 1;
    }
    WEATHER_PRINT(F("requiredTicks: "));
    WEATHER_PRINTLN(requiredTicks);
    weatherInterval = requiredTicks * TimerTwo::MillisPerTick;
  }

  unsigned long lastWindCountMillis;
  constexpr unsigned long minWindInterval = 3; //Debounce. 330 kph = broken station;
  void countWind()
  {
    // debounce the signal:
    if (millis() - lastWindCountMillis < minWindInterval)
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
      WEATHER_PRINTVAR(windCounts);
      WEATHER_PRINTVAR(wd);
      WEATHER_PRINTVAR(curWindX);
      WEATHER_PRINTVAR(curWindY);
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

  #if DEBUG_Direction
  void sendDirectionDebug()
  {
    static unsigned long lastDirectionDebug = 0;
    //Ensure we don't flood our messaging capabilities. We're sending 500 bytes, this should take about 5 seconds...
    if (millis() - lastDirectionDebug < 5000)
      return;
    if (!directionDebugging)
    {
      sleepUntilNextWeather();
      return;
    }
    static byte interval = 1;
    const size_t bufferSize = 250;
    byte msgBuffer[bufferSize];
    byte* msg = msgBuffer + messageOffset;
    msg[0] = 'D';
    msg[1] = stationID;
    msg[2] = getUniqueID();
    int* data = (int*)(msg + 3);
    size_t dataLen = (bufferSize - messageOffset - 3 - 15) / sizeof(int); //-15 to allow extra bytes for escaping
    unsigned long lastMillis = millis();
    int i = 0;
    while (i < dataLen)
    {
      if (millis() - lastMillis < interval)
        continue;
      data[i++] = analogRead(WIND_DIR_PIN);
    }
    size_t messageLen = dataLen * 2 + 3;
    sendMessage(msgBuffer, messageLen, bufferSize);
    lastDirectionDebug = millis();
  }
  #endif

  void setupWeatherProcessing()
  {
    initWind();
    updateSendInterval(0, true);
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
    WEATHER_PRINT("Weather reading: ");
    WEATHER_PRINTLN(ADC);
    WEATHER_PRINTVAR(tsOffset);
    WEATHER_PRINTVAR(tsGain);
#endif

    int temp_x2 = (((long)ADC + tsOffset - (273 + 100)) * 256) / tsGain + 100 * 2;
    internalTemperature = temp_x2 / 2;
    int temp_x2_offset = temp_x2 + 64;
    if (temp_x2_offset < 0)
      temp_x2_offset = 0;
    if (temp_x2_offset > 255)
      temp_x2_offset = 255;
    return temp_x2_offset;
  }

  byte getExternalTemperature()
  {
  #ifdef ALS_TEMP
    T = externalTemperature;
  #elif defined(TEMP_SENSE)
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

    T -= 273.15;
    
    externalTemperature = T;
  #else
    return 0;
  #endif

    //We send temperature as a byte, ranging from -32 to 95 C (1 LSB = half a degree)
    if (T < -32)
      T = -32;
    if (T > 95)
      T = 95;
    return (T + 0.5 + 32) * 2;
  }

  void timer2Tick()
  {
    if (++tickCounts >= requiredTicks)
    {
      tickCounts = 0;
      weatherRequired = true;
    }
  }

  bool handleWeatherCommand(MessageSource& src)
  {
    byte commandType;
    if (src.readByte(commandType))
      return false;

    byte newValue, tsOffset, tsGain;
    if (commandType == 'O' || commandType == 'G' &&
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