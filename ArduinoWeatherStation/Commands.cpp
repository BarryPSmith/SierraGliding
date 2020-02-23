#include "TimerTwo.h"
#include "LoraMessaging.h"
#include "MessageHandling.h"
#include "ArduinoWeatherStation.h"
#include "PermanentStorage.h"
#include "WeatherProcessing.h"
#include "RemoteProgramming.h"

namespace Commands
{
  //We keep track of recently handled commands to avoid executing the same command multiple times
  byte recentlyHandledCommands[MessageHandling::recentArraySize];

  bool handleRelayCommand(MessageSource& msg);
  bool handleIntervalCommand(MessageSource& msg);
  bool handleThresholdCommand(MessageSource& msg);
  bool handleIDCommand(MessageSource& msg, bool demandRelay, byte uniqueID);
  bool handleQueryCommand(MessageSource& msg, bool demandRelay, byte uniqueID);
  void handleQueryConfigCommand(MessageDestination& response);
  void handleQueryVolatileCommand(MessageDestination& response);
  bool handleOverrideCommand(MessageSource& msg);
  void acknowledgeMessage(byte uniqueID, bool isSpecific, bool demandRelay);

  void handleCommandMessage(MessageSource& msg, bool demandRelay, byte uniqueID, bool isSpecific)
  {
    int i;
    //Check if we've already handled this command:
    for (i = 0; i < MessageHandling::recentArraySize; i++)
    {
      if (!recentlyHandledCommands[i])
        break;
      if (recentlyHandledCommands[i] == uniqueID)
        return;
    }
    if (i == MessageHandling::recentArraySize)
      i--;
    //Remember that we've handled this command:
    memmove(recentlyHandledCommands + 1, recentlyHandledCommands, i);
    recentlyHandledCommands[0] = uniqueID;

    //Can't delay with streaming messages. Perhaps a short delay before committing our response.

    byte command;
    bool handled = false;
    bool ackRequired = true;

    if (msg.readByte(command) == MESSAGE_OK)
    {
      AWS_DEBUG_PRINT(F("Command Received: "));
      AWS_DEBUG_PRINTLN((char)command);

      switch (command)
      {
        case 'R': //Relay command
          handled = handleRelayCommand(msg);
          break;

        case 'I': //Interval command
          handled = handleIntervalCommand(msg);
        break;

        case 'B': //Battery threshold command
          handled = handleThresholdCommand(msg);
        break;

        case 'Q': //Query
          handled = handleQueryCommand(msg, demandRelay, uniqueID);
          ackRequired = false;
        break;

        case 'O': //Override Interval
          handled = handleOverrideCommand(msg);
          break;

        case 'M':
          handled = handleMessageCommand(msg);
          break;

        case 'W':
          handled = WeatherProcessing::handleWeatherCommand(msg);
          break;

        case 'P':
          handled = RemoteProgramming::handleProgrammingCommand(msg, uniqueID, demandRelay, &ackRequired);
          break;

        case 'U': // Change ID
          handled = handleIDCommand(msg, demandRelay, uniqueID);
          break;

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
        //do nothing. handled = false, so we send "IGNORED"
        break;
      }
    }
    if (handled && ackRequired)
      acknowledgeMessage(uniqueID, isSpecific, demandRelay);

    if (!handled)
    {
      MESSAGE_DESTINATION_SOLID response(false);
      if (demandRelay)
        response.appendByte('K' | 0x80);
      else
        response.appendByte('K');
      response.appendByte(stationID);
      response.appendByte(uniqueID);
      response.append(F("IGNORED"), 7);
    }
  }

  //Relay command: C(ID)(UID)R(dataLength)((+|-)(C|W)(RelayID))*
  bool handleRelayCommand(MessageSource& msg)
  {
    byte dataLength;
    if (msg.readByte(dataLength))
      return false;
    if (dataLength == 0)
      return false;
    byte stationsToRelayWeather[permanentArraySize], 
         stationsToRelayCommands[permanentArraySize];
    GET_PERMANENT(stationsToRelayWeather);
    GET_PERMANENT(stationsToRelayCommands);
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
        for (i = 0; i < MessageHandling::recentArraySize; i++)
        {
          if (!list[i] || list[i] == statID)
            break;
        }
        //If it doesn't exist and we have no room, error out:
        if (i == MessageHandling::recentArraySize)
          return false;
        //add it (or overwrite itself, no biggie)
        list[i] = statID;
      }
      else
      {
        bool removed = false;
        for (int i = 0; i < MessageHandling::recentArraySize; i++)
        {
          if (!list[i])
            break;
          if (list[i] == statID)
          {
            memmove(list + i, list + i + 1, MessageHandling::recentArraySize - i - 1);
            removed = true;
          }
        }
        if (removed)
          list[i - 1] = 0;
      }
    }
    SET_PERMANENT(stationsToRelayWeather);
    SET_PERMANENT(stationsToRelayCommands);
    return true;
  }

  //Interval command: C(ID)(UID)I(length)(new short interval)(new long interval)
  bool handleIntervalCommand(MessageSource& msg)
  {
    byte dataLength;
    if (msg.readByte(dataLength) || dataLength == 0)
      return false;

    unsigned long shortInterval, longInterval;
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

    SET_PERMANENT_S(shortInterval);
    SET_PERMANENT_S(longInterval);
    return true;
  }

  //Override command: C(ID)(UID)O(L|S)(Duration)(S|M|H)
  bool handleOverrideCommand(MessageSource& msg)
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
    return true;
  }
  
  //Threshold command: C(ID)(UID)B(new threshold)
  bool handleThresholdCommand(MessageSource& msg)
  {
    unsigned short batteryThreshold_mV;
    if (msg.read(batteryThreshold_mV))
      return false;
    SET_PERMANENT_S(batteryThreshold_mV);
    return true;
  }

  //Query command: C(ID)(UID)Q
  bool handleQueryCommand(MessageSource& msg, bool responseDemandRelay, byte uniqueID)
  {
    //Reponse is limited to 255 bytes. Beware buffer overrun.
    byte queryType;
    if (msg.readByte(queryType) != MESSAGE_OK)
      queryType = 'C';
  
    MESSAGE_DESTINATION_SOLID response(false);
    if (responseDemandRelay)
      response.appendByte('K' | 0x80);
    else
      response.appendByte('K');
    response.appendByte(stationID);
    response.appendByte(uniqueID);

    //Version (a few bytes)
    response.appendByte('V');
    response.append(ver, strlen_P((const char*)ver));
    response.appendByte(0);

    switch (queryType)
    {
    case 'C':
      handleQueryConfigCommand(response);
      break;
    case 'V':
      handleQueryVolatileCommand(response);
      break;
    default:
      response.abort();
      return false;
    }
    return true;
  }

  void handleQueryConfigCommand(MessageDestination& response)
  {
    // Just throw the whole config at them.
    PermanentVariables variables;
    PermanentStorage::getBytes(0, sizeof(PermanentVariables), &variables);
    response.appendT(variables);
  }

  void handleQueryVolatileCommand(MessageDestination& response)
  { 
    response.appendByte('M');
    unsigned long curMillis = millis();
    response.appendT(curMillis);
    response.appendT(lastPingMillis);
    response.appendT(overrideDuration);
    if (overrideDuration)
    {
      response.appendT(overrideStartMillis);
      response.appendT(overrideShort);
    }
    response.appendByte(0);
  
    //Recently seen stations, max 102 bytes
    response.appendByte('S');
    for (int i = 0; i < MessageHandling::recentArraySize; i++)
    {
      if (!MessageHandling::recentlySeenStations[i].id)
      {
        break;
      }
      response.appendT(MessageHandling::recentlySeenStations[i]);
    }
    response.appendByte(0);

    //recently handled commands, max 22 bytes
    response.appendByte('C');
    for (int i = 0; i < MessageHandling::recentArraySize; i++)
    {
      if (!recentlyHandledCommands[i])
        break;
      response.appendByte(recentlyHandledCommands[i]);
    }
    response.appendByte(0);

    //recently relayed messages, max 62 bytes
    response.appendByte('R');
    for (int i = 0; i < MessageHandling::recentArraySize; i++)
    {
      if (!MessageHandling::recentlyRelayedMessages[i].type)
        break;
      response.appendT(MessageHandling::recentlyRelayedMessages[i]);
    }
    response.appendByte(0);

    //message statistics:
    response.appendByte('M');
    appendMessageStatistics(response);
  }

  void notifyNewID(bool demandRelay, byte uniqueID, byte newID)
  {
    MESSAGE_DESTINATION_SOLID reply(false);
      reply.appendByte('K' | (demandRelay ? 0x80 : 0));
      reply.appendByte(stationID);
      reply.appendByte(uniqueID);
      reply.append(F("NewID"), 5);
      reply.appendByte(newID);
      reply.finishAndSend();
  }

  bool handleIDCommand(MessageSource& msg, bool demandRelay, byte uniqueID)
  {
    byte subCommand;
    if (msg.readByte(subCommand))
      return false;
    byte newID;
    switch (subCommand)
    {
    case 'R':
      bool duplicate;
      do
      {
        duplicate = false;
        newID = random(1, 255);
        for (int i = 0; i < MessageHandling::recentArraySize; i++)
        {
          if (!MessageHandling::recentlySeenStations[i].id)
            break;
          if (MessageHandling::recentlySeenStations[i].id == newID)
          {
            duplicate = true;
            break;
          }
        }
      } while (duplicate);
      break;
    case 'S':
      if (msg.readByte(newID) || newID == 0)
        return false;
      break;
    default:
      return false;
    }
    // Reply with the new ID before we change our station ID
    // that should automatically get routed back to the base station
    // (which the new ID might not)
    notifyNewID(demandRelay, uniqueID, newID);
    stationID = newID;
    SET_PERMANENT_S(stationID);
    return true;
  }

  void acknowledgeMessage(byte uniqueID, bool isSpecific, bool demandRelay)
  {
    //We won't acknowledge relayed commands with destination station = 0. If destination station = 0, we execute every time we receive it. We're likely to see multiple copies of these commands.
    //We'd flood the network if we acknowledge all of them, particularly because we need to demand relay on the response.
    if (demandRelay && !isSpecific)
      return;
  
    MESSAGE_DESTINATION_SOLID response(false);
    if (demandRelay)
      response.appendByte('K' | 0x80);
    else
      response.appendByte('K');
    response.appendByte(stationID);
    response.appendByte(uniqueID);
    response.append(F("OK"), 2);
  }
}