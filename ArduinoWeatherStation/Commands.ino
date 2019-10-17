void handleCommandMessage(byte* message, size_t readByteCount)
{
  byte uniqueId = message[2];
  int i;
  //Check if we've already handled this command:
  for (i = 0; i < recentArraySize; i++)
  {
    if (!recentlyHandledCommands[i])
      break;
    if (recentlyHandledCommands[i] == uniqueId)
      return;
  }
  if (i == recentArraySize)
    i--;
  //Remember that we've handled this command:
  memmove(recentlyHandledCommands + 1, recentlyHandledCommands, i);
  recentlyHandledCommands[0] = uniqueId;

  //Delay for testing.
  //TODO: Replace delay with a queue of acknowledgements
  delay(3000);

  byte command = message[3];
  bool handled = false;
  bool acknowledged = false;
  switch (command)
  {
    case 'R': //Relay command
      handled = handleRelayCommand(message, readByteCount);
      break;

    case 'I': //Interval command
      handled = handleIntervalCommand(message, readByteCount);
    break;

    case 'B': //Battery threshold command
      handled = handleThresholdCommand(message, readByteCount);
    break;

    case 'Q': //Query
      handled = handleQueryCommand(message, readByteCount);
    break;

    case 'D': //Debug Query
      handled = handleDebugCommand(message, readByteCount);
    break;

    case 'O': //Override Interval
      handled = handleOverrideCommand(message, readByteCount);

    #if DEBUG_Speed
    case 'S':
      handled = true;
      speedDebugging = !speedDebugging;
      acknowledgeMessage(message);
      break;
    case 'W':
      handled = true;
      countWind();
      acknowledgeMessage(message);
      break;
    #endif
    #if DEBUG_Direction
    case 'E':
      handled = true;
      directionDebugging = !directionDebugging;
      acknowledgeMessage(message);
    #endif

    default:
    //Test code: Just acknowledge it.
    break;
  }
  if (!handled)
  {
    byte msgBuffer[40];
    byte* responseBuffer = msgBuffer + messageOffset;
    responseBuffer[0] = 'K';
    if (message[0] & 0x80)
      responseBuffer[0] = 'K' | 0x80;
    responseBuffer[1] = stationId;
    responseBuffer[2] = uniqueId;
    responseBuffer[3] = 7;
    memcpy(responseBuffer + 4, "IGNORED", 7);
    size_t msgLength = 11;
    sendMessage(msgBuffer, msgLength, 40);
  }
}

//Relay command: C(ID)(UID)R(dataLength)((+|-)(C|W)(RelayID))*
bool handleRelayCommand(byte* message, byte readByteCount)
{
  byte dataLength = message[4];
  if (dataLength + 4 < readByteCount || dataLength == 0)
    return false;
  byte* commandData = message + 5;
  for (int i = 0; i + 2 < dataLength; i+=3)
  {
    bool isAdd = commandData[i] == '+';
    bool isRemove = commandData[i] == '-';
    bool isWeather = commandData[i + 1] == 'W';
    bool isCmd = commandData[i + 1] = 'C';
    byte statID = commandData[i + 2];
    if ((!isWeather && !isCmd) ||
        (!isAdd && !isRemove) ||
         statID == 0)
      return false;

    byte* list = isWeather ? stationsToRelayWeather : stationsToRelayCommands;
    if (isAdd)
    {
      //Check if it already exists, and check if we have space to add it
      int i;
      for (i = 0; i < recentArraySize; i++)
      {
        if (!list[i] || list[i] == statID)
          break;
      }
      //If it doesn't exist and we have no room, error out:
      if (i == recentArraySize)
        return false;
      //add it (or overwrite itself, no biggie)
      list[i] = statID;
    }
    else
    {
      bool removed = false;
      for (int i = 0; i < recentArraySize; i++)
      {
        if (!list[i])
          break;
        if (list[i] == statID)
        {
          memmove(list + i, list + i + 1, recentArraySize - i - 1);
          removed = true;
        }
      }
      if (removed)
        list[i - 1] = 0;
    }
  }
  acknowledgeMessage(message);
  return true;
}

//Interval command: C(ID)(UID)I(length)(new short interval)(new long interval)
bool handleIntervalCommand(byte* message, byte readByteCount)
{
  byte dataLength = message[4];
  if (dataLength > readByteCount - 4 || dataLength == 0)
    return false;
  byte* commandData = message + 5;
  if (dataLength == 2)
  {
    shortInterval = commandData[0] * 1000;
    longInterval = commandData[1] * 1000;
  }
  else if (dataLength == 4)
  {
    shortInterval = ((uint16_t*)commandData)[0] * 1000;
    longInterval = ((uint16_t*)commandData)[1] * 1000;
  }
  else if (dataLength == 8)
  {
    shortInterval = ((uint32_t*)commandData)[0];
    longInterval = ((uint32_t*)commandData)[1];
  }
  else
    return false;
  acknowledgeMessage(message);
  return true;
}

//Override command: C(ID)(UID)O(L|S)(Duration)(S|M|H)
bool handleOverrideCommand(byte* message, byte readByteCount)
{
  if (readByteCount < 6)
    return false;
  switch (message[4])
  {
    case 'L':
      overrideShort = false;
      break;
    case 'S':
      overrideShort = true;
      break;
    default:
      return false;
  }
  unsigned long multiplier;
  if (readByteCount >= 7)
    {
      switch (message[7])
      {
        case 'H':
          multiplier = 3600000;
        case 'M':
          multiplier = 60000;
        case 'S':
        default:
          multiplier = 1000;
      }
    }
  else
    multiplier = 1000;
    
  overrideDuration = *(int*)(message + 5) * multiplier;
  overrideStartMillis = millis();
  acknowledgeMessage(message);
  return true;
}

//Threshold command: C(ID)(UID)B(new threshold)
bool handleThresholdCommand(byte* message, byte readByteCount)
{
  if (readByteCount < 6)
    return false;
  batteryThreshold = *(int*)(message + 4);
  acknowledgeMessage(message);
  return true;
}

//Query command: C(ID)(UID)Q
bool handleQueryCommand(byte* message, byte readByteCount)
{
  //Response is currently at 204 bytes. Beware buffer overrun.
  
  const int bufferSize = 300;
  byte uniqueId = message[2];
  byte messageBuffer[bufferSize];
  byte* responseBuffer = messageBuffer + messageOffset;
  responseBuffer[0] = 'K';
  if (message[0] & 0x80)
    responseBuffer[0] = 'K' | 0x80;
  responseBuffer[1] = stationId;
  responseBuffer[2] = uniqueId;
  size_t curLoc = 3;

  //Version (4 bytes)
  responseBuffer[curLoc++] = 'V';
  memcpy(responseBuffer + curLoc, ver, 3);
  curLoc += 3;
  responseBuffer[curLoc++] = 0;

  //Setup Variables (31 bytes)
  responseBuffer[curLoc++] = demandRelay;
  responseBuffer[curLoc++] = 'I';
  memcpy(responseBuffer + curLoc, &weatherInterval, sizeof(unsigned long));
  curLoc += sizeof(unsigned long);
  memcpy(responseBuffer + curLoc, &shortInterval, sizeof(unsigned long));
  curLoc += sizeof(unsigned long);
  memcpy(responseBuffer + curLoc, &longInterval, sizeof(unsigned long));
  curLoc += sizeof(unsigned long);
  memcpy(responseBuffer + curLoc, &batteryThreshold, sizeof(int));
  curLoc += sizeof(int);
  responseBuffer[curLoc++] = 'M';
  unsigned long curMillis = millis();
  memcpy(responseBuffer + curLoc, &curMillis, sizeof(unsigned long));
  curLoc += sizeof(unsigned long);
  memcpy(responseBuffer + curLoc, &overrideDuration, sizeof(unsigned long));
  curLoc += sizeof(unsigned long);
  if (overrideDuration)
  {
    memcpy(responseBuffer + curLoc, &overrideStartMillis, sizeof(unsigned long));
    curLoc += sizeof(unsigned long);
    memcpy(responseBuffer + curLoc, &overrideShort, sizeof(bool));
    curLoc += sizeof(bool);
  }
  responseBuffer[curLoc++] = 0;

  //Stations to relay, max 42 bytes:
  responseBuffer[curLoc++] = 'R';
  for (int i = 0; i < recentArraySize; i++)
  {
    if (!stationsToRelayCommands[i])
      break;
    responseBuffer[curLoc++] = stationsToRelayCommands[i];
  }
  responseBuffer[curLoc++] = 0;
  for (int i = 0; i < recentArraySize; i++)
  {
    if (!stationsToRelayWeather[i])
      break;
    responseBuffer[curLoc++] = stationsToRelayWeather[i];
  }
  responseBuffer[curLoc++] = 0;

  //Recently seen stations, max 102 bytes
  responseBuffer[curLoc++] = 'S';
  for (int i = 0; i < recentArraySize; i++)
  {
    if (!recentlySeenStations[i][0])
    {
      break;
    }
    memcpy(responseBuffer + curLoc, &recentlySeenStations[i][0], 5);
    curLoc += 5;
  }
  responseBuffer[curLoc++] = 0;

  //recently handled commands, max 22 bytes
  responseBuffer[curLoc++] = 'C';
  for (int i = 0; i < recentArraySize; i++)
  {
    if (!recentlyHandledCommands[i])
      break;
    responseBuffer[curLoc++] = recentlyHandledCommands[i];
  }
  responseBuffer[curLoc++] = 0;
  sendMessage(messageBuffer, curLoc, bufferSize);
  return true;
}

//Debug command: C(ID)(UID)D
bool handleDebugCommand(byte* message, byte readByteCount)
{
  const int bufferSize = 300;
  byte uniqueId = message[2];
  byte messageBuffer[bufferSize];
  byte* responseBuffer = messageBuffer + messageOffset;
  responseBuffer[0] = 'K';
  if (message[0] & 0x80)
    responseBuffer[0] = 'K' | 0x80;
  responseBuffer[1] = stationId;
  responseBuffer[2] = uniqueId;
  size_t curLoc = 3;

  //Version (4 bytes)
  memcpy(responseBuffer + curLoc, ver, 3);
  curLoc += 3;
  responseBuffer[curLoc++] = 0;
  
   //recently relayed messages, max 61 bytes
  for (int i = 0; i < recentArraySize; i++)
  {
    if (!recentlyRelayedMessages[i][0])
      break;
    memcpy(responseBuffer + curLoc, &recentlyRelayedMessages[i][0], 3);
    curLoc += 3;
  }
  responseBuffer[curLoc++] = 0;
  sendMessage(messageBuffer, curLoc, bufferSize);
  return true;
}

void acknowledgeMessage(byte* message)
{
  //We won't acknowledge relayed commands with destination station = 0. If destination station = 0, we execute every time we receive it. We're likely to see multiple copies of these commands.
  //We'd flood the network if we acknowledge all of them, particularly because we need to demand relay on the response.
  if ((message[0] & 0x80) && message[1] == 0)
    return;
  
  byte uniqueId = message[2];
  byte msgBuffer[40];
  byte* responseBuffer = msgBuffer + messageOffset;
  responseBuffer[0] = 'K';
  if (message[0] & 0x80)
    responseBuffer[0] = 'K' | 0x80;
  responseBuffer[1] = stationId;
  responseBuffer[2] = uniqueId;
  memcpy(responseBuffer + 3, "OK", 2);
  size_t msgLength = 5;
  sendMessage(msgBuffer, msgLength, 40);
}
