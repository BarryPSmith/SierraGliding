#include "TimerTwo.h"
#include "LoraMessaging.h"
#include "MessageHandling.h"
#include "ArduinoWeatherStation.h"
#include "PermanentStorage.h"
#include "WeatherProcessing/WeatherProcessing.h"
#include "RemoteProgramming.h"
#include "PWMSolar.h"
#include "Flash.h"
#include "Database.h"

//#define DEBUG_COMMANDS
#ifdef DEBUG_COMMANDS
#define COMMAND_PRINT AWS_DEBUG_PRINT
#define COMMAND_PRINTLN AWS_DEBUG_PRINTLN
#define COMMAND_PRINTVAR PRINT_VARIABLE
#else
#define COMMAND_PRINT(...)  do { } while (0)
#define COMMAND_PRINTLN(...)  do { } while (0)
#define COMMAND_PRINTVAR(...)  do { } while (0)
#endif

extern uint8_t _end;

namespace Commands
{
  //We keep track of recently handled commands to avoid executing the same command multiple times
  byte recentlyHandledCommands[permanentArraySize];
  byte curCmdIndex = 0;

  bool handleRelayCommand(MessageSource& msg);
  bool handleIntervalCommand(MessageSource& msg);
  bool handleThresholdCommand(MessageSource& msg);
  bool handleChargingCommand(MessageSource& msg);
  bool handleIDCommand(MessageSource& msg, byte uniqueID);
  bool handleQueryCommand(MessageSource& msg, byte uniqueID);
  void handleQueryConfigCommand(MessageDestination& response);
  void handleQueryVolatileCommand(MessageDestination& response);
  bool handleOverrideCommand(MessageSource& msg);
  void acknowledgeMessage(byte uniqueID, bool isSpecific, byte msgType);

  void handleCommandMessage(MessageSource& msg, byte uniqueID, bool isSpecific)
  {
    static bool acknowledge = true;

    //Check if we've already handled this command:
    if (uniqueID != 0)
    {
      for (int i = 0; i < permanentArraySize; i++)
      {
        if (recentlyHandledCommands[i] == uniqueID)
          return;
      }
      //Remember that we've handled this command:
      recentlyHandledCommands[curCmdIndex++] = uniqueID;
      if (curCmdIndex >= permanentArraySize)
        curCmdIndex = 0;
    }

    //Can't delay with streaming messages. Perhaps a short delay before committing our response.

    byte command;
    bool handled = false;
    bool ackRequired = true;

    if (msg.readByte(command) == MESSAGE_OK)
    {
      COMMAND_PRINT(F("Command Received: "));
      COMMAND_PRINTLN((char)command);

      switch (command)
      {
        case 'A':
          handled = (msg.read(acknowledge) == MESSAGE_OK);
          break;

        case 'R': //Relay command
          handled = handleRelayCommand(msg);
          break;

        case 'I': //Interval command
          handled = handleIntervalCommand(msg);
        break;

        case 'B': //Battery threshold command
          handled = handleThresholdCommand(msg);
        break;

        case 'C':
          handled = handleChargingCommand(msg);
        break;

        case 'Q': //Query
          handled = handleQueryCommand(msg, uniqueID);
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
          handled = RemoteProgramming::handleProgrammingCommand(msg, uniqueID, &ackRequired);
          break;

        case 'U': // Change ID
          handled = handleIDCommand(msg, uniqueID);
          break;

        case 'F': // Restart
          acknowledgeMessage(uniqueID, isSpecific, command);
          while (1);

        case 'G': // Flash (Gordon)
          handled = Flash::handleFlashCommand(msg, uniqueID, &ackRequired);
          break;

#ifndef  NO_STORAGE
        case 'D':
          handled = Database::handleDatabaseCommand(msg, uniqueID, &ackRequired);
          break;
#endif // ! NO_STORAGE

        default:
        //do nothing. handled = false, so we send "IGNORED"
        break;
      }
    }
    if (handled && ackRequired && acknowledge)
      acknowledgeMessage(uniqueID, isSpecific, command);

    if (!handled)
    {
      MESSAGE_DESTINATION_SOLID<20> response(false);
      response.appendByte('K');
      response.appendByte(stationID);
      response.appendByte(uniqueID);
      response.appendByte(command);
      response.append(F("IGNORED"), 7);
    }
  }

  //Relay command: C(ID)(UID)R(dataLength)((+|-)(C|W)(RelayID))*
  bool handleRelayCommand(MessageSource& msg)
  {
    byte types[] = { 'W', 'C', 'R' };
    for (byte curType : types)
    {
      byte list[permanentArraySize];
      byte max;
      switch (curType)
      {
      case 'W':
        PermanentStorage::getBytes((void*)offsetof(PermanentVariables, stationsToRelayWeather), permanentArraySize, list);
        max = permanentArraySize;
        break;
      case 'C':
        PermanentStorage::getBytes((void*)offsetof(PermanentVariables, stationsToRelayCommands), permanentArraySize, list);
        max = permanentArraySize;
        break;
      case 'R':
        PermanentStorage::getBytes((void*)offsetof(PermanentVariables, messageTypesToRecord), messageTypeArraySize, list);
        max = messageTypeArraySize;
        break;
      }
      byte opByte;
      bool changed = false;
      while (msg.readByte(opByte) == MESSAGE_OK)
      {
        byte typeByte;
        if (msg.readByte(typeByte))
        {
          COMMAND_PRINTLN(F("Relay: no type"));
          return false;
        }
        bool isAdd = opByte == '+';
        if (!isAdd && opByte != '-')
        {
          COMMAND_PRINTLN(F("Relay: mismatch"));
          return false;
        }
        byte statID;
        if (msg.readByte(statID))
        {
          COMMAND_PRINTLN(F("Relay: no station"));
          return false;
        }
        if (statID == 0)
        {
          COMMAND_PRINTLN(F("Relay: zero station"));
          return false;
        }

        if (typeByte != curType)
          continue;
        changed = true;

        if (isAdd)
        {
          //Check if it already exists, and check if we have space to add it
          int i;
          for (i = 0; i < permanentArraySize; i++)
          {
            if (!list[i] || list[i] == statID)
              break;
          }
          //If it doesn't exist and we have no room, error out:
          if (i == permanentArraySize)
          {
            return false;
            COMMAND_PRINTLN(F("Relay: no space"));
          }
          //add it (or overwrite itself, no biggie)
          list[i] = statID;
        }
        else
        {
          for (int i = 0; i < permanentArraySize; i++)
            if (list[i] == statID)
              list[i] = 0;
        }
      }
      if (changed)
        switch (curType)
        {
        case 'W':
          PermanentStorage::setBytes((void*)offsetof(PermanentVariables, stationsToRelayWeather), permanentArraySize, list);
          max = permanentArraySize;
          break;
        case 'C':
          PermanentStorage::setBytes((void*)offsetof(PermanentVariables, stationsToRelayCommands), permanentArraySize, list);
          max = permanentArraySize;
          break;
        case 'R':
          PermanentStorage::setBytes((void*)offsetof(PermanentVariables, messageTypesToRecord), messageTypeArraySize, list);
          max = messageTypeArraySize;
          break;
        }
    }
    updateIdleState();
    return true;
  }

  //Interval command: C(ID)(UID)I(length)(new short interval)(new long interval)
  bool handleIntervalCommand(MessageSource& msg)
  {
    unsigned long shortInterval, longInterval;
    
    if (msg.read(shortInterval))
      return false;
    if (msg.read(longInterval))
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

    constexpr unsigned long multiplier = 1000;

    overrideDuration = val * multiplier;
    overrideStartMillis = millis();
    return true;
  }
  
  //Threshold command: C(ID)(UID)B(new threshold)
  bool handleThresholdCommand(MessageSource& msg)
  {
    unsigned short batteryThreshold_mV,
      batteryEmergencyThresh_mV;
    if (msg.read(batteryThreshold_mV) ||
        msg.read(batteryEmergencyThresh_mV))
      return false;
    SET_PERMANENT_S(batteryThreshold_mV);
    SET_PERMANENT_S(batteryEmergencyThresh_mV);
    return true;
  }

  bool handleChargingCommand(MessageSource& msg)
  {
    unsigned short chargeVoltage_mV;
    unsigned short chargeResponseRate;
    unsigned short safeFreezingChargeLevel_mV;
    byte safeFreezingPwm;
    if (msg.read(chargeVoltage_mV))
      return false;
    if (msg.read(chargeResponseRate))
      return false;
    if (msg.read(safeFreezingChargeLevel_mV))
      return false;
    if (msg.read(safeFreezingPwm))
      return false;

    SET_PERMANENT_S(chargeVoltage_mV);
    SET_PERMANENT_S(chargeResponseRate);
    SET_PERMANENT_S(safeFreezingChargeLevel_mV);
    SET_PERMANENT_S(safeFreezingPwm);

    PwmSolar::chargeVoltage_mV = chargeVoltage_mV;
    PwmSolar::chargeResponseRate = chargeResponseRate;
    PwmSolar::safeFreezingChargeLevel_mV = safeFreezingChargeLevel_mV;
    PwmSolar::safeFreezingPwm = safeFreezingPwm;
    
    return true;
  }

  //Query command: C(ID)(UID)Q
  bool handleQueryCommand(MessageSource& msg, byte uniqueID)
  {
    //Reponse is limited to 255 bytes. Beware buffer overrun.
    byte queryType;
    if (msg.readByte(queryType) != MESSAGE_OK)
      queryType = 'C';
  
    MESSAGE_DESTINATION_SOLID<254> response(false);
    byte headerStart[6];
    

      headerStart[0] = ('K');
    headerStart[1] = (stationID);
    headerStart[2] = (uniqueID);
    headerStart[3] = 'Q';
    headerStart[4] = queryType;

    //Version (a few bytes)
    headerStart[5] = ('V');
    response.append(headerStart, sizeof(headerStart)); //+6 = 6
    response.append(ASW_VER, ver_size); //+11 = 17
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
#if 0 //This takes too much memory
    PermanentVariables variables;
    PermanentStorage::getBytes(0, sizeof(PermanentVariables), &variables);
    response.appendT(variables);
#else
    for (byte i = 0; i < sizeof(PermanentVariables); i++)
    {
      byte b;
      PermanentStorage::getBytes((void*)(int)i, 1, &b);
      response.appendByte(b);
    }
#endif
  }

  void handleQueryVolatileCommand(MessageDestination& response)
  { 
    //So far used 17 bytes. 237 bytes remain
    response.appendByte('M'); //+1 = 1
    unsigned long curMillis = millis();
    response.appendT(curMillis); //+4 = 5
    response.appendT(lastPingMillis); //+4 = 9
    response.appendT(overrideDuration); //+4 = 13
    if (overrideDuration)
    {
      response.appendT(overrideStartMillis); //+4 = 17
      response.appendT(overrideShort); //1 = 18
    }
    response.appendByte(0); //+1 = 19
  
    //Recently seen stations, +102 bytes = 121
    response.appendByte('S');
    for (int i = 0; i < permanentArraySize; i++)
    {
      response.appendT(MessageHandling::recentlySeenStations[i]);
    }
    response.appendByte(0);

    //recently handled commands, +22 bytes = 143
    response.appendByte('C');
    for (int i = 0; i < permanentArraySize; i++)
    {
      response.appendByte(recentlyHandledCommands[i]);
    }
    response.appendByte(0);

    //recently relayed messages, +62 bytes = 205
    response.appendByte('R');
    for (int i = 0; i < permanentArraySize; i++)
    {
      response.appendT(MessageHandling::recentlyRelayedMessages[i]);
    }
    response.appendByte(0);

    //message statistics:
    response.appendByte('M'); //+1 = 206
    appendMessageStatistics(response); //+8 = 214
    response.appendByte('S'); //+1 = 215
    response.appendT(TimerTwo::seconds()); //+4 = 219
    response.appendT(StackCount()); //+2 = 221
    const uint8_t *p = &_end;
    response.appendT(SP - (unsigned short)p); //+2 = 223
  }

  void notifyNewID(byte uniqueID, byte newID)
  {
    MESSAGE_DESTINATION_SOLID<20> reply(false);
      reply.appendByte('K');
      reply.appendByte(stationID);
      reply.appendByte(uniqueID);
      reply.append(F("UNewID"), 6);
      reply.appendByte(newID);
      reply.finishAndSend();
  }

  bool handleIDCommand(MessageSource& msg, byte uniqueID)
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
        newID = random(1, 127);
        for (int i = 0; i < permanentArraySize; i++)
        {
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
    notifyNewID(uniqueID, newID);
    stationID = newID;
    SET_PERMANENT_S(stationID);
    return true;
  }

  void acknowledgeMessage(byte uniqueID, bool isSpecific, byte commandType)
  {
    //Demand relay has been removed.
    //We won't acknowledge relayed commands with destination station = 0. If destination station = 0, we execute every time we receive it. We're likely to see multiple copies of these commands.
    //We'd flood the network if we acknowledge all of them, particularly because we need to demand relay on the response.
    //if (!isSpecific)
      //return;
  
    MESSAGE_DESTINATION_SOLID<20> response(false);
    response.appendByte('K');
    response.appendByte(stationID);
    response.appendByte(uniqueID);
    response.appendByte(commandType);
    response.append(F("OK"), 2);
  }
}