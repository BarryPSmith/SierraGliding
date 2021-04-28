#include <Arduino.h>
#include "PermanentStorage.h"
#include "ArduinoWeatherStation.h"
#include "PWMSolar.h"
#include "WeatherProcessing/WeatherProcessing.h"

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

#define MAX_PWM_BASE_FREQ 4000000

namespace PwmSolar
{
#define SOLAR_PWM_PIN 5 // Pin 5 is OCR0B

  //Old board:
  //Battery sense has 33kOhm resistor and 10nF capacitor
  //Time constant is 0.33 milliseconds.
  //New board:
  //110kOhm, 10nF => Tau = 1.1ms
  //So... if our update interval is ~4 time constants,
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

#if defined(DEBUG_PWM)
  short debug_desired;
  bool debug_applyLimits;
  short debug_change;
  unsigned short debug_curMillis;
#endif

  enum CurrentSensorState { Off, Startup, Running };
  CurrentSensorState currentState = CurrentSensorState::Off;

  unsigned long lastPwmMicros = 0;
  // On startup, have the solar fully open
  // It's a generally good idea anyway,
  // but especially important because there's a problem with our PWM circuit 
  // where it latches closed if the battery voltage is low.
  byte solarPwmValue = 0xFF;
  

  short curCurrent_mA_x6;
  unsigned short lastCurrentCheckMillis;

  int getNewPwmValue(bool* canIdle);
  byte getMaxPwm(unsigned short batteryVoltage_mV);
  unsigned short getDesiredBatteryVoltage();
  void turnOffSolar();
  void setSolarFull();
  void ensurePwmActive(bool canIdle);
  byte readCurrentAndCalcMaxPwm(bool applyLimits);
#ifdef CURRENT_SENSE_PWR
  void stopCurrentSensor();
  void startupCurrentSensor();
#endif
#ifdef CURRENT_SENSE
  short readCurrent_x6();
#endif

#if defined(SOLAR_IMPEDANCE_SWITCHING) && (F_CPU < MAX_PWM_BASE_FREQ)
  constexpr byte minPwm = 8;
#else
  constexpr byte minPwm = 1;
#endif

  void setupPwm()
  {
    GET_PERMANENT_S(chargeResponseRate);
    GET_PERMANENT_S(safeFreezingChargeLevel_mV);
    GET_PERMANENT_S(safeFreezingPwm);
    GET_PERMANENT_S(chargeVoltage_mV);
#ifndef SOLAR_IMPEDANCE_SWITCHING
    pinMode(SOLAR_PWM_PIN, OUTPUT);
#endif
#ifdef CURRENT_SENSE
    pinMode(CURRENT_SENSE, INPUT);
#endif
#ifdef CURRENT_SENSE_PWR
    pinMode(CURRENT_SENSE_PWR, OUTPUT);
#endif
  }

  void doPwmLoop()
  {
    if (micros() - lastPwmMicros < PwmUpdateInterval_uS)
      return;
#ifdef DEBUG_SOLAR
    loopCount++;
#endif
    lastPwmMicros = micros();
    
    bool canIdle;
    short newValue = getNewPwmValue(&canIdle);
    if (newValue < minPwm)
    {
      turnOffSolar();
      // We set the 'desired' pwm value, even though we aren't using it.
      // Our algorithm may have it hovering around minPwm,
      // or may take several iterations to get it to a value greater than minPwm.
      solarPwmValue = newValue;
    }
    else if (newValue >= 255)
    {
      setSolarFull();
      solarPwmValue = 255;
    }
    else
    {
      ensurePwmActive(canIdle);
      solarPwmValue = OCR0B = newValue;
    }
  }

  int getNewPwmValue(bool* canIdle)
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

    *canIdle = true;
    if (newValue <= 0)
      return 0;
    else if (newValue >= maxPwm)
      return maxPwm;
    else
    {
      *canIdle = false;
      return newValue;
    }
  }

  /*
  * Current PWM behaviour:
  * Check voltage. Adjust PWM as necessary within limits.
  * 
  * In presence of current sensor:
  * Check voltage. Determine new desired PWM level based on existing level.
  * Check current. Determine new desired PWM level based on existing level.
  * Take whichever gives the lowest PWM level to exceed neither voltage nor current.
  * 
  * Current sensor logic:
  * If temp sufficient: No current limit => No need to run current sensor
  * If current limit in place.
  *  - Is current-based PWM full right now?
  *   + Yes: Do not check current until check interval has passed
  *   + No: Check current immediately.
  * */

  byte readCurrentAndCalcMaxPwm(bool applyLimits)
  {
#ifndef CURRENT_SENSE
    return applyLimits ? safeFreezingPwm : 255;
#else
    //We can use shorts for all these millis values because the intervals are much less than 65 seconds.
    static unsigned short powerUpMillis;
    unsigned short curMillis = millis();
#if defined(DEBUG_PWM)
    debug_curMillis = curMillis;
#endif
    // R = 47kOhm, C = 10nF, Tau = 470uS, f0 = 330Hz
    // According to datasheet chip startup time is ~100uS.
    // For 2% accuracy, allow settling time 4 time constants => 2ms
    constexpr unsigned short powerUpInterval_us = 2000;
    constexpr unsigned short currentCheckInterval_ms = 5000;
    if (currentState == CurrentSensorState::Off)
    {
      if (curMillis - lastCurrentCheckMillis > currentCheckInterval_ms)
      {
        startupCurrentSensor();
        powerUpMillis = curMillis;
        // Better to delay than to loop - we might sleep for 250ms
        // and leave the current sensor on the whole time (at ~70uA draw).
        delayMicroseconds(powerUpInterval_us);
      }
      else
        return 255;
    }
    /*if (currentState == CurrentSensorState::Startup)
    {
      if (curMillis - powerUpMillis > powerUpInterval_ms)
        currentState = CurrentSensorState::Running;
      else
        return 255;
    }*/
    // If we get here, the sensor is running.
    curCurrent_mA_x6 = readCurrent_x6();
    lastCurrentCheckMillis = curMillis;
    short diff = ((short)safeFreezingPwm * 6 - curCurrent_mA_x6);
    short change = diff * (long)chargeResponseRate / 256; //diff can be as large as 6 * 255 = 1530. To avoid an overflow, cast chargeResponseRate to long.
    short desired = solarPwmValue + change;
#if defined(DEBUG_PWM)
    debug_applyLimits = applyLimits;
    debug_desired = desired;
    debug_change = change;
#endif
    if (!applyLimits || desired >= 255) {
      // If there is no limit due to the current, then shut the sensor down for a while to save power.
      stopCurrentSensor();
      return 255;
    }
    if (desired < 0)
      desired = 0;
    return desired;
#endif
  }

  void startupCurrentSensor()
  {
#ifdef CURRENT_SENSE_PWR
    digitalWrite(CURRENT_SENSE_PWR, HIGH);
    currentState = CurrentSensorState::Startup;
#endif
  }

  void stopCurrentSensor()
  {
#ifdef CURRENT_SENSE_PWR
    digitalWrite(CURRENT_SENSE_PWR, LOW);
    currentState = CurrentSensorState::Off;
#endif
  }

#ifdef CURRENT_SENSE
  short readCurrent_x6()
  {
    int reading = analogRead(CURRENT_SENSE);
    constexpr unsigned long denominator = (CURRENT_SENSE_GAIN * 1023);
    return ((unsigned long)reading * REF_MV * 6) / denominator;
  }
#endif

  //In cold temperatures with lithium ion, we must perform current and voltage limitation. maxPwm limits the maximum chart current.
  //Internet sources suggest 0.02C charge rate is acceptable in very cold temperatures.
  //So we can calculate the maximum PWM because we know the maximum current from the panel and the battery capacity.
  byte getMaxPwm(unsigned short batteryVoltage_mV)
  {
    bool applyLimits = false;
    if (WeatherProcessing::internalTemperature_x2 > 100)
    {
      stopCurrentSensor();
      curCurrent_mA_x6 = 0;
      return 0;
    }
    //If the battery voltage is less than 3.7V, we're unlikely to damage it with any current we throw at it.
    if (batteryVoltage_mV > safeFreezingChargeLevel_mV &&
        (WeatherProcessing::internalTemperature_x2 < 0 ||
        (WeatherProcessing::internalTemperature_x2 < 4 && WeatherProcessing::externalTemperature < 2))) //The battery might be colder than the MCU - thermal capacity, inaccurate measurement, etc.
      applyLimits = true;
    return readCurrentAndCalcMaxPwm(applyLimits);
  }

  unsigned short getDesiredBatteryVoltage()
  {
    // Get the desired voltage from our permanent variables. Probably 4.0V to ensure we don't overcharge the batteries because we CBF putting in a timer.
    // If our voltage every drops below some absolute minimum (2.5V?), we will should update that variable to a safe 'non-charging' level (3.7V?)
    return chargeVoltage_mV;
  }

  void turnOffSolar()
  {
#ifdef SOLAR_IMPEDANCE_SWITCHING
    //Disable Timer0 interrupts:
    TIMSK0 = 0;
    TIFR0 = _BV(TOV0) | _BV(OCF0B);
    //Set pin5 to output, value HIGH
    static_assert(SOLAR_PWM_PIN == 5);
    PORTD |= _BV(PORTD5);
    DDRD |= _BV(DDD5);
#else
    // Take control of the pin back.
    // write it HIGH (which will turn the switch off)
#ifdef SOLAR_INVERSE
    digitalWrite(SOLAR_PWM_PIN, LOW);
#else
    digitalWrite(SOLAR_PWM_PIN, HIGH);
#endif
#endif
    // Record that sleep is allowed
    solarSleepEnabled = SleepModes::powerSave;
  }

  void setSolarFull()
  {
#ifdef SOLAR_IMPEDANCE_SWITCHING
    //Disable Timer0 interrupts:
    TIMSK0 = 0;
    TIFR0 = _BV(TOV0) | _BV(OCF0B);
    //Set pin5 to high impedance
    static_assert(SOLAR_PWM_PIN == 5);
    PORTD &= ~_BV(PORTD5);
    DDRD &= ~_BV(DDD5);
#else
    // Take control of the pin back
    // write it LOW (which will turn the switch on)
#ifdef SOLAR_INVERSE
    digitalWrite(SOLAR_PWM_PIN, HIGH);
#else
    digitalWrite(SOLAR_PWM_PIN, LOW);
#endif
#endif
    // Record that sleep is allowed
    solarSleepEnabled = SleepModes::powerSave;
  }

  void ensurePwmActive(bool canIdle)
  {
    // Set up the necessary registers
    // Ensure the MCU doesn't go to sleep.
#ifdef SOLAR_IMPEDANCE_SWITCHING
    TIMSK0 = _BV(OCIE0B) | _BV(TOIE0);
#else
    TIMSK0 = 0; //No interrupts
#endif
#ifndef SOLAR_INVERSE
    TCCR0A = _BV(COM0B1) | _BV(COM0B0) | _BV(WGM01) | _BV(WGM00); //Fast PWM inverting on output B
#else
    TCCR0A = _BV(COM0B1) | _BV(WGM01) | _BV(WGM00); //Fast PWM non-inverting on output B
#endif
#if F_CPU < MAX_PWM_BASE_FREQ
    TCCR0B = _BV(CS00); // No prescaler: Operate at full clock frequency (PWM at 3.9kHz for 1MHz)
#else
    TCCR0B = _BV(CS01); // 8x prescaler: PWM at 3.9kHz for 8MHz
#endif
    solarSleepEnabled = canIdle ? SleepModes::idle : SleepModes::disabled;
  }

#ifdef SOLAR_IMPEDANCE_SWITCHING
  ISR(TIMER0_COMPB_vect, ISR_NAKED)
  {
    static_assert(SOLAR_PWM_PIN == 5);
    PORTD |= _BV(PORTD5);
    DDRD |= _BV(DDD5);
    reti();
  }

  ISR(TIMER0_OVF_vect, ISR_NAKED)
  {
    static_assert(SOLAR_PWM_PIN == 5);
    PORTD &= ~_BV(PORTD5);
    DDRD &= ~_BV(DDD5);
    reti();
  }
#endif
}