volatile int windCounts = 0;

#if DEBUG_Speed
const size_t speedTickLength = 200;
int speedTicks[200];
byte curSpeedLocation = 0;
#endif

byte GetWindDirection(int wdVoltage)
{
  if (wdVoltage < 168)
    return 5;  // 5 (ESE)
  if (wdVoltage < 195)
    return 3;  // 3 (ENE)
  if (wdVoltage < 236)
    return 4;  // 4 (E)
  if (wdVoltage < 315)
    return 7;  // 7 (SSE)
  if (wdVoltage < 406)
    return 6;  // 6 (SE)
  if (wdVoltage < 477)
    return 9;  // 9 (SSW)
  if (wdVoltage < 570)
    return 8;  // 8 (S)
  if (wdVoltage < 662)
    return 1;  // 1 (NNE)
  if (wdVoltage < 742)
    return 2;  // 2 (NE)
  if (wdVoltage < 808)
    return 11; // B (WSW)
  if (wdVoltage < 842)
    return 10; // A (SW)
  if (wdVoltage < 889)
    return 15; // F (NNW)
  if (wdVoltage < 923)
    return 0;  // 0 (N)
  if (wdVoltage < 949)
    return 13; // D (WNW)
  if (wdVoltage < 977)
    return 14; // E (NW)
  return 12;   // C (W)
}

size_t createWeatherData(byte* msgBuffer)
{
  //Message format is W(StationID)(UniqueID)(DirHex)(Spd * 2)(Voltage)
  int batteryVoltage = analogRead(voltagePin);
  updateSendInterval(batteryVoltage);
  int dirVoltage = analogRead(windDirPin);
  noInterrupts();
  int localCounts = windCounts;
  windCounts = 0; 
  interrupts();
  float windSpeed = (2400.0 * localCounts) / weatherInterval;
  byte windDirection = GetWindDirection(dirVoltage);

  if (windDirection < 10)
    msgBuffer[0] = (byte)('0' + windDirection);
  else
    msgBuffer[0] = (byte)('A' + windDirection - 10);
  msgBuffer[1] = (byte)(windSpeed * 2);

  //Lead Acid
  //Expected voltage range: 10 - 15V
  //Divide by 3, gives 0.33 - 5V
  //Binary values 674 - 1024 (range: 350)
  msgBuffer[2] = (batteryVoltage - 512) / 2;
  return 3;
}

void updateSendInterval(int batteryVoltage)
{
  if (batteryVoltage < batteryThreshold)
    weatherInterval = longInterval;
  else
    weatherInterval = shortInterval;
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

void countWind()
{
  #if DEBUG_Speed
  speedTicks[curSpeedLocation++] = millis();
  curSpeedLocation = curSpeedLocation % 200;
  #endif
  windCounts++;
}

void setupWindCounter()
{
  windCounts = 0;
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
  msg[1] = stationId;
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
