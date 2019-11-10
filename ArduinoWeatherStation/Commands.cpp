#include "Messaging.h"
#include "MessageHandling.h"
#include "ArduinoWeatherStation.h"

bool handleRelayCommand(MessageSource& msg, bool demandRelay, byte uniqueID, bool isSpecific);
bool handleIntervalCommand(MessageSource& msg, bool demandRelay, byte uniqueID, bool isSpecific);
bool handleThresholdCommand(MessageSource& msg, bool demandRelay, byte uniqueID, bool isSpecific);
bool handleQueryCommand(bool demandRelay, byte uniqueID);
bool handleDebugCommand(bool demandRelay, byte uniqueID);
bool handleOverrideCommand(MessageSource& msg, bool demandRelay, byte uniqueID, bool isSpecific);
void acknowledgeMessage(byte uniqueID, bool isSpecific, bool demandRelay);

extern Stream* pStream;

void handleCommandMessage(MessageSource& msg, bool demandRelay, byte uniqueID, bool isSpecific)
{
  int i;
  //Check if we've already handled this command:
  for (i = 0; i < recentArraySize; i++)
  {
    if (!recentlyHandledCommands[i])
      break;
    if (recentlyHandledCommands[i] == uniqueID)
      return;
  }
  if (i == recentArraySize)
    i--;
  //Remember that we've handled this command:
  memmove(recentlyHandledCommands + 1, recentlyHandledCommands, i);
  recentlyHandledCommands[0] = uniqueID;

  //Can't delay with streaming messages. Perhaps a short delay before committing our response.

  byte command;
  if (msg.readByte(command))
    return;

  bool handled = false;
  bool acknowledged = false;
  switch (command)
  {
    case 'R': //Relay command
      handled = handleRelayCommand(msg, demandRelay, uniqueID, isSpecific);
      break;

    case 'I': //Interval command
      handled = handleIntervalCommand(msg, demandRelay, uniqueID, isSpecific);
    break;

    case 'B': //Battery threshold command
      handled = handleThresholdCommand(msg, demandRelay, uniqueID, isSpecific);
    break;

    case 'Q': //Query
      handled = handleQueryCommand(demandRelay, uniqueID);
    break;

    case 'D': //Debug Query
      handled = handleDebugCommand(demandRelay, uniqueID);
    break;

    case 'O': //Override Interval
      handled = handleOverrideCommand(msg, demandRelay, uniqueID, isSpecific);

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
    MessageDestination response(pStream);
    if (demandRelay)
      response.appendByte('K' | 0x80);
    else
      response.appendByte('K');
    response.appendByte(stationID);
    response.appendByte(uniqueID);
    response.appendByte(7);
    response.append("IGNORED", 7);
    /*
    byte msgBuffer[40];
    byte* responseBuffer = msgBuffer + messageOffset;
    responseBuffer[0] = 'K';
    if (message[0] & 0x80)
      responseBuffer[0] = 'K' | 0x80;
    responseBuffer[1] = stationID;
    responseBuffer[2] = uniqueID;
    responseBuffer[3] = 7;
    memcpy(responseBuffer + 4, "IGNORED", 7);
    size_t msgLength = 11;
    sendMessage(msgBuffer, msgLength, 40);
    */
  }
}

//Relay command: C(ID)(UID)R(dataLength)((+|-)(C|W)(RelayID))*
bool handleRelayCommand(MessageSource& msg, bool demandRelay, byte uniqueID, bool isSpecific)
{
  byte dataLength;
  if (msg.readByte(dataLength))
    return false;
  if (dataLength == 0)
    return false;
  for (int i = 0; i + 2 < dataLength; i+=3)
  {
    byte opByte;
    byte typeByte;
    auto obr = msg.readByte(opByte);
    if (obr)
      return false;
    if (msg.readByte(typeByte))
      return false;
    bool isAdd = opByte == '+';
    bool isWeather = typeByte == 'W';
    if ((!isAdd && opByte != '-') ||
      (!isWeather && typeByte != 'C'))
      return false;
    byte statID;
    if (msg.readByte(statID))
      return false;
    if (statID == 0)
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
  acknowledgeMessage(uniqueID, isSpecific, demandRelay);
  return true;
}

//Interval command: C(ID)(UID)I(length)(new short interval)(new long interval)
bool handleIntervalCommand(MessageSource& msg, bool demandRelay, byte uniqueID, bool isSpecific)
{
  byte dataLength;
  if (msg.readByte(dataLength) || dataLength == 0)
    return false;

  if (dataLength == 2)
  {
    byte tmp;
    if (msg.readByte(tmp))
      return false;
    shortInterval = tmp * 1000;
    if (msg.readByte(tmp))
      return false;
    longInterval = tmp * 1000;
  }
  else if (dataLength == 4)
  {
    uint16_t tmp;
    if (msg.read(tmp))
      return false;
    shortInterval = tmp * 1000;
    if (msg.read(tmp))
      return false;
    longInterval = tmp * 1000;
  }
  else if (dataLength == 8)
  {
    //Use a temporary variable for in case we encounter an error halfway through reading.
    unsigned long tmp;
    if (msg.read(tmp))
      return false;
    shortInterval = tmp;
    if (msg.read(tmp))
      return false;
    longInterval = tmp;
  }
  else
    return false;
  acknowledgeMessage(uniqueID, isSpecific, demandRelay);
  return true;
}

//Override command: C(ID)(UID)O(L|S)(Duration)(S|M|H)
bool handleOverrideCommand(MessageSource& msg, bool demandRelay, byte uniqueID, bool isSpecific)
{
  byte destByte;
  if (msg.readByte(destByte))
    return false;
  switch (destByte)
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
  unsigned int val;
  if (msg.read(val))
    return false;

  unsigned long multiplier = 1000;
  byte multByte;
  if (!msg.readByte(multByte))
  {
    switch (multByte)
    {
    case 'H':
      multiplier = 3600000;
    case 'M':
      multiplier = 60000;
    }
  }

  overrideDuration = val * multiplier;
  overrideStartMillis = millis();
  acknowledgeMessage(uniqueID, isSpecific, demandRelay);
  return true;
}

//Threshold command: C(ID)(UID)B(new threshold)
bool handleThresholdCommand(MessageSource& msg, bool demandRelay, byte uniqueID, bool isSpecific)
{
  int tmp;
  if (msg.read(tmp))
    return false;
  batteryThreshold = tmp;
  acknowledgeMessage(uniqueID, isSpecific, demandRelay);
  return true;
}

//Query command: C(ID)(UID)Q
bool handleQueryCommand(bool demandRelay, byte uniqueID)
{
  //Response is currently at 204 bytes. Beware buffer overrun.
  
  //const int bufferSize = 300;
  MessageDestination response(pStream);
  //byte messageBuffer[bufferSize];
  //byte* responseBuffer = messageBuffer + messageOffset;
  if (demandRelay)
    response.appendByte('K' | 0x80);
  else
    response.appendByte('K');
  response.appendByte(stationID);
  response.appendByte(uniqueID);

  //Version (4 bytes)
  response.appendByte('V');
  response.append(ver, 3);
  response.appendByte(0);
  
  //Setup Variables (31 bytes)
  response.appendByte(demandRelay);
  response.appendByte('I');
  response.appendT(weatherInterval);
  response.appendT(shortInterval);
  response.appendT(longInterval);
  response.appendT(batteryThreshold);
  response.appendByte('M');
  unsigned long curMillis = millis();
  response.appendT(curMillis);
  response.appendT(overrideDuration);
  if (overrideDuration)
  {
    response.appendT(overrideStartMillis);
    response.appendT(overrideShort);
  }
  response.appendByte(0);

  //Stations to relay, max 42 bytes:
  response.appendByte('R');
  for (int i = 0; i < recentArraySize; i++)
  {
    if (!stationsToRelayCommands[i])
      break;
    response.appendByte(stationsToRelayCommands[i]);
  }
  response.appendByte(0);
  for (int i = 0; i < recentArraySize; i++)
  {
    if (!stationsToRelayWeather[i])
      break;
    response.appendByte(stationsToRelayWeather[i]);
  }
  response.appendByte(0);

  //Recently seen stations, max 102 bytes
  response.appendByte('S');
  for (int i = 0; i < recentArraySize; i++)
  {
    if (!recentlySeenStations[i][0])
    {
      break;
    }
    response.append(&recentlySeenStations[i][0], 5);
  }
  response.appendByte(0);

  //recently handled commands, max 22 bytes
  response.appendByte('C');
  for (int i = 0; i < recentArraySize; i++)
  {
    if (!recentlyHandledCommands[i])
      break;
    response.appendByte(recentlyHandledCommands[i]);
  }
  response.appendByte(0);
  return true;
}

//Debug command: C(ID)(UID)D
bool handleDebugCommand(bool demandRelay, byte uniqueID)
{
  //byte messageBuffer[bufferSize];
  //byte* responseBuffer = messageBuffer + messageOffset;
  MessageDestination response(pStream);
  if (demandRelay)
    response.appendByte('K' | 0x80);
  else
    response.appendByte('K');
  response.appendByte(stationID);
  response.appendByte(uniqueID);

  //Version (a few bytes)
  response.append(ver, strlen(ver));
  response.appendByte(0);
  
   //recently relayed messages, max 61 bytes
  for (int i = 0; i < recentArraySize; i++)
  {
    if (!recentlyRelayedMessages[i][0])
      break;
    response.append(&recentlyRelayedMessages[i][0], 3);
  }
  response.appendByte(0);
  return true;
}

void acknowledgeMessage(byte uniqueID, bool isSpecific, bool demandRelay)
{
  //We won't acknowledge relayed commands with destination station = 0. If destination station = 0, we execute every time we receive it. We're likely to see multiple copies of these commands.
  //We'd flood the network if we acknowledge all of them, particularly because we need to demand relay on the response.
  if (demandRelay && !isSpecific)
    return;
  
  MessageDestination response(pStream);
  if (demandRelay)
    response.appendByte('K' | 0x80);
  else
    response.appendByte('K');
  response.appendByte(stationID);
  response.appendByte(uniqueID);
  response.append("OK", 2);
}
