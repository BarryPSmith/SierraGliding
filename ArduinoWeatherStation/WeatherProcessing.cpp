#include "TimerTwo.h"
#include "LoraMessaging.h"
#include "WeatherProcessing.h"
#include "ArduinoWeatherStation.h"
#include "PermanentStorage.h"
#include <avr/boot.h>

#define DAVIS_WIND
//#define ARGENTDATA_WIND

#if defined(DAVIS_WIND) && defined(ARGENTDATA_WIND)
#error Multiple wind systems defined
#endif

volatile bool weatherRequired = true;
unsigned volatile long tickCounts = 0;
unsigned long requiredTicks = 1;

void countWind();
void timer2Tick();
#ifdef DAVIS_WIND
byte getWindDirectionDavis();
#elif defined(ARGENTDATA_WIND)
byte getWindDirectionArgentData();
#else
#error No Wind System Selected
#endif
void updateSendInterval(unsigned short batteryVoltage_mv, bool startup);
void updateCanSleep(unsigned short batteryVoltage_mV);
void setTimerInterval();
void setupWindCounter();
byte getExternalTemperature();
byte getInternalTemperature();

volatile int windCounts = 0;

//We use unsigned long for these because they are involved in 32-bit calculations.
constexpr unsigned long mV_Ref = REF_MV;
constexpr unsigned long BattVNumerator = BATTV_NUM;
constexpr unsigned long BattVDenominator = BATTV_DEN;
constexpr unsigned long MaxBatt_mV = 7500;
constexpr int voltagePin = BATT_PIN;
constexpr int windDirPin = WIND_DIR_PIN;
constexpr int windSpdPin = WIND_SPD_PIN;

#if DEBUG_Speed
const size_t speedTickLength = 200;
int speedTicks[200];
byte curSpeedLocation = 0;
#endif
byte getWindDirection()
{
#ifdef DAVIS_WIND
  return getWindDirectionDavis();
#elif ARGENTDATA_WIND
  return getWindDirectionArgentData();
#endif
}

//Get the wind direction for a davis vane.
//Davis vane is a potentiometer sweep. 0 and full resistance correspond to 'north'.
//We connect the ends of pot to +VCC and GND. So we just measure the centre pin and it linearly correlates with angle.
//We're going to ignore the dead zone for now. Maybe test for it later. But given we intend to replace it all with ultrasonics, probably don't waste time.
byte getWindDirectionDavis()
{
  int wdVoltage = analogRead(windDirPin);
  AWS_DEBUG_PRINT(F("wdVoltage: "));
  AWS_DEBUG_PRINTLN(wdVoltage);
  return wdVoltage / 4;
}

//Get the wind direction for an argent data wind vane (8 distinct directions, sometimes landing between them).
//This assumes a 4kOhm resistor in series to form a voltage divider
byte getWindDirectionArgentData()
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
  int wdVoltage = analogRead(windDirPin);
  
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
    return 114;  // 9 (SSW)
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
    return 64; // E (NW)
  return 192;   // C (W)
}

inline uint16_t getWindSpeed_x8()
{
  noInterrupts();
  int localCounts = windCounts;
  windCounts = 0;
  interrupts();
#ifdef ARGENTDATA_WIND
  return (8 * 2400 * localCounts) / weatherInterval;
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
  else //We're just going to report 285km/h. It'll be a long search for the station. And the mountain.
    return 255;
}

static byte simpleMessagesSent = 255;
constexpr byte complexMessageFrequency = 10;

void createWeatherData(MessageDestination& message)
{
#ifdef WIND_PWR_PIN
  digitalWrite(WIND_PWR_PIN, HIGH);
  delay(1);
#endif

  bool isComplex = simpleMessagesSent >= complexMessageFrequency - 1;
  
  //Message format is W(StationID)(UniqueID)(DirHex)(Spd * 2)(Voltage)
  int batteryVoltageReading = analogRead(voltagePin);
  unsigned long batt_mV = mV_Ref * BattVNumerator * batteryVoltageReading / (BattVDenominator  * 1023);
  AWS_DEBUG_PRINTLN(F("  ======================"));
  AWS_DEBUG_PRINT(F("batteryVoltageReading: "));
  AWS_DEBUG_PRINTLN(batteryVoltageReading);
  AWS_DEBUG_PRINT(F("batt_mV: "));
  AWS_DEBUG_PRINTLN(batt_mV);
  AWS_DEBUG_PRINT(F("Wind Counts: "));
  AWS_DEBUG_PRINTLN(windCounts);
  uint16_t windSpeed_x8 = getWindSpeed_x8();

  //Update the send interval only after we calculate windSpeed, because windSpeed is dependent on weatherInterval
  updateSendInterval(batt_mV, false);
  updateCanSleep(batt_mV);
  
  byte windDirection = getWindDirection();
  AWS_DEBUG_PRINT(F("windDirection byte: "));
  AWS_DEBUG_PRINTLN(windDirection);
  message.appendByte(windDirection);
  byte wsByte = getWindSpeedByte(windSpeed_x8);

  AWS_DEBUG_PRINT(F("windSpeed byte: "));
  AWS_DEBUG_PRINTLN(wsByte);
  message.appendByte(wsByte);
    
  if (isComplex)
  {
    byte batteryByte = (byte)(255UL * batt_mV / MaxBatt_mV);
    message.appendByte(batteryByte);
    AWS_DEBUG_PRINT(F("batteryByte: "));
    AWS_DEBUG_PRINTLN(batteryByte, HEX);

    byte externalTempByte = getExternalTemperature();
    message.appendByte(externalTempByte);
    AWS_DEBUG_PRINT(F("externalTempByte: "));
    AWS_DEBUG_PRINTLN(externalTempByte, HEX);

    byte internalTempByte = getInternalTemperature();
    message.appendByte(internalTempByte);
    AWS_DEBUG_PRINT(F("internalTempByte: "));
    AWS_DEBUG_PRINTLN(internalTempByte, HEX);
    
    simpleMessagesSent = 0;
  }
  else
    simpleMessagesSent++;
  
  AWS_DEBUG_PRINTLN(F("  ======================"));
  #ifdef WIND_PWR_PIN
  digitalWrite(WIND_PWR_PIN, LOW);
  #endif
}

void updateCanSleep(unsigned short batteryVoltage_mV)
{
  // This is a lazy attempt at battery charge regulation:
  // If the voltage is too high, increase current consumption by not sleeping.
  unsigned short batteryUpperThresh_mV;
  GET_PERMANENT_S(batteryUpperThresh_mV);
  sleepEnabled = batteryVoltage_mV < batteryUpperThresh_mV;
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
  AWS_DEBUG_PRINT(F("weather interval: "));
  AWS_DEBUG_PRINTLN(weatherInterval);
  requiredTicks = weatherInterval / TimerTwo::MillisPerTick;
  if (requiredTicks < 1)
  {
    AWS_DEBUG_PRINTLN(F("Required ticks is zero!"));
    requiredTicks = 1;
  }
  AWS_DEBUG_PRINT(F("requiredTicks: "));
  AWS_DEBUG_PRINTLN(requiredTicks);
  weatherInterval = requiredTicks * TimerTwo::MillisPerTick;
}

#if DEBUG_Speed
void circularMemCpy(void* dest, void* src0, byte srcOffset, byte srcSize, byte amount);

void sendSpeedDebugMessage()
{
  static byte lastSpeedLocation = 0;
  byte msgBuffer[250];
  byte localSpeedLocation = curSpeedLocation;

  byte totalDataPoints = (int)(localSpeedLocation - lastSpeedLocation) % speedTickLength;
  byte firstMessageSize = totalDataPoints;
  if (firstMessageSize > 100)
    firstMessageSize = 100;
  msgBuffer[messageOffset] = 1;
  msgBuffer[messageOffset + 1] = localSpeedLocation;
  circularMemCpy( msgBuffer + messageOffset + 2, 
                  speedTicks, 
                  lastSpeedLocation * sizeof(int), 
                  speedTickLength * sizeof(int), 
                  firstMessageSize * sizeof(int));
  size_t msgSize = 2 + firstMessageSize * sizeof(int);
  sendMessage(msgBuffer, msgSize, 250);

  if (firstMessageSize < totalDataPoints)
  {
    byte secondMessageSize = totalDataPoints - firstMessageSize;
    msgBuffer[messageOffset] = 2;
    circularMemCpy(msgBuffer + messageOffset,
                  speedTicks, 
                  (lastSpeedLocation + firstMessageSize) * sizeof(int), 
                  speedTickLength * sizeof(int),
                  secondMessageSize * sizeof(int));
    msgSize = 1 + secondMessageSize * sizeof(int);
    sendMessage(msgBuffer, msgSize, 250);
  }
  lastSpeedLocation = localSpeedLocation;
}

void circularMemCpy(void* dest, void* src0, size_t srcOffset, size_t srcSize, size_t amount)
{
  if (amount > srcSize)
    amount = srcSize;
  if (srcOffset + amount < srcSize)
    memcpy(dest, src0 + srcOffset, amount);
  else
  {
    byte firstSize = srcSize - srcOffset;
    memcpy(dest, src0 + srcOffset, firstSize);
    byte secondSize = amount - firstSize;
    memcpy(dest + firstSize, src0, secondSize);
  }
}
#endif

unsigned long lastWindCountMillis;
constexpr unsigned long minWindInterval = 3; //Debounce. 330 kph = broken station;
void countWind()
{
  #if DEBUG_Speed
  speedTicks[curSpeedLocation++] = millis();
  curSpeedLocation = curSpeedLocation % 200;
  #endif
  // debounce the signal:
  if (millis() - lastWindCountMillis < minWindInterval)
    return;
  lastWindCountMillis = millis();
  windCounts++;
}

void setupWindCounter()
{
  windCounts = 0;
  //pinMode or digitalWrite should give the same results:
  pinMode(windSpdPin, INPUT_PULLUP);
  //digitalWrite(windSpdPin, HIGH);
  attachInterrupt(digitalPinToInterrupt(windSpdPin), countWind, RISING);
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
    data[i++] = analogRead(windDirPin);
  }
  size_t messageLen = dataLen * 2 + 3;
  sendMessage(msgBuffer, messageLen, bufferSize);
  lastDirectionDebug = millis();
}
#endif

void setupWeatherProcessing()
{
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
  const signed char tsOffset = boot_signature_byte_get(2);
  const byte tsGain = boot_signature_byte_get(3);

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

  int temp_x2_offset = (((int)ADC - (273 + 100 - tsOffset)) * 256) / tsGain + 25;
  temp_x2_offset += 64;
  if (temp_x2_offset < 0)
    temp_x2_offset = 0;
  if (temp_x2_offset > 255)
    temp_x2_offset = 255;
  return temp_x2_offset;
}

byte getExternalTemperature()
{
#ifndef TEMP_SENSE
  return 0;
#else
  auto reading = analogRead(TEMP_SENSE);
  // V = Vref * R1 / (R1 + R2)
  // V / (Vref * R1) = 1 / (R1 + R2)
  // R1 * Vref / V = R1 + R2
  // 0 = (1 - Vref / V) R1 + R2
  // R1 = R2 / (Vref / V - 1)
  // R2 / R1 = Vref / V - 1
  // Vref / V = 1023 / reading
  float r2_r1 = 1023.0 / reading - 1;
  float B = 3892;
  float T0 = 273.15 + 25;
  float T = 1/(1/T0 + B * log(r2_r1));
  T -= 273.15;
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
}
