#include <TimerOne.h>
#include "Messaging.h"
#include "WeatherProcessing.h"
#include "ArduinoWeatherStation.h"

#define DAVIS_WIND
//#define ARGENTDATA_WIND

#if defined(DAVIS_WIND) && defined(ARGENTDATA_WIND)
#error Multiple wind systems defined
#endif

volatile bool weatherRequired = true;
//int overrun:
//4 second interval, * 65536 = 2.5E5 seconds (several days)
unsigned int tickCounts = 0;
unsigned volatile int requiredTicks = 1;

void countWind();
void timer1Tick();
#ifdef DAVIS_WIND
byte getWindDirectionDavis();
#elif defined(ARGENTDATA_WIND)
byte getWindDirectionArgentData();
#else
#error No Wind System Selected
#endif
void updateSendInterval(float batteryVoltage);
void setTimerInterval();

volatile int windCounts = 0;

const float V_Ref = 3.8;
const float BattVDivider = 49.1/10;
const float MinBattV = 7.5;
const float MaxBattV = 15.0;

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

inline float getWindSpeed()
{
  noInterrupts();
  int localCounts = windCounts;
  windCounts = 0;
  interrupts();
#ifdef ARGENTDATA_WIND
  return (2400.0 * localCounts) / weatherInterval;
#elif defined(DAVIS_WIND)
  return (1600.0 * localCounts) / weatherInterval;
#endif

}

void createWeatherData(MessageDestination& message)
{
  //Message format is W(StationID)(UniqueID)(DirHex)(Spd * 2)(Voltage)
  int batteryVoltageReading = analogRead(voltagePin);
  float battV = V_Ref * batteryVoltageReading / 1023 * BattVDivider;

  float windSpeed = getWindSpeed();

  //Update the send interval only after we calculate windSpeed, because windSpeed is dependent on weatherInterval
  updateSendInterval(battV);
  
  byte windDirection = getWindDirection();
  message.appendByte(windDirection);
  message.appendByte((byte)(windSpeed * 2));

  //Lead Acid
  //Expected voltage range: 10 - 15V
  //Divide by 3, gives 0.33 - 5V
  //Binary values 674 - 1024 (range: 350)
  message.appendByte((byte)(255 * (battV - MinBattV) / (MaxBattV - MinBattV) + 0.5));
}

void updateSendInterval(float batteryVoltage)
{
  //This is always called shortly after Timer1 resets. We shouldn't expect major issues
  unsigned long oldInterval = weatherInterval;
  bool overrideActive = Timer1.millis() - overrideStartMillis < overrideDuration;
  if ((!overrideActive && batteryVoltage < batteryThreshold - 2) ||
      (overrideActive && !overrideShort))
  {
    weatherInterval = longInterval;
  }
  else if ((!overrideActive && batteryVoltage > batteryThreshold + 2) ||
    (overrideShort && overrideActive))
  {
    weatherInterval = shortInterval;
  }
  //Ensure that once we get out of override, we won't accidently go back into it due to Timer1.millis wraparound.
  if (!overrideActive)
    overrideDuration = 0;

  if (oldInterval != weatherInterval)
    setTimerInterval();
}

void setTimerInterval()
{
  unsigned long microInterval = weatherInterval * 1000;
  auto sreg = SREG;
  noInterrupts();
  requiredTicks = (microInterval - 1) / Timer1.MaxMicros + 1;
  SREG = sreg;
  Timer1.setPeriod(microInterval / requiredTicks);
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
constexpr unsigned long minWindInterval = 6; //160 MPH = broken station;
void countWind()
{
  #if DEBUG_Speed
  speedTicks[curSpeedLocation++] = Timer1.millis();
  curSpeedLocation = curSpeedLocation % 200;
  #endif
  // debounce the signal:
  if (Timer1.millis() - lastWindCountMillis < minWindInterval)
    return;
  lastWindCountMillis = Timer1.millis();
  windCounts++;
}

void setupWindCounter()
{
  windCounts = 0;
  digitalWrite(windSpdPin, HIGH);
  attachInterrupt(digitalPinToInterrupt(windSpdPin), countWind, RISING);
}

#if DEBUG_Direction
void sendDirectionDebug()
{
  static unsigned long lastDirectionDebug = 0;
  //Ensure we don't flood our messaging capabilities. We're sending 500 bytes, this should take about 5 seconds...
  if (Timer1.millis() - lastDirectionDebug < 5000)
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
  unsigned long lastMillis = Timer1.millis();
  int i = 0;
  while (i < dataLen)
  {
    if (Timer1.millis() - lastMillis < interval)
      continue;
    data[i++] = analogRead(windDirPin);
  }
  size_t messageLen = dataLen * 2 + 3;
  sendMessage(msgBuffer, messageLen, bufferSize);
  lastDirectionDebug = Timer1.millis();
}
#endif

void setupWeatherProcessing()
{
  Timer1.initialize(1000000);
  setTimerInterval();
  Timer1.start();
  Timer1.attachInterrupt(&timer1Tick);
  setupWindCounter();
}

void timer1Tick()
{
  if (++tickCounts >= requiredTicks)
  {
    tickCounts = 0;
    weatherRequired = true;
  }
}