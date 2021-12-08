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

#ifdef DEBUG_COMMANDS
#define COMMAND_PRINT AWS_DEBUG_PRINT
#define COMMAND_PRINTLN AWS_DEBUG_PRINTLN
#define COMMAND_PRINTVAR PRINT_VARIABLE
#else
#define COMMAND_PRINT(...)  do { } while (0)
#define COMMAND_PRINTLN(...)  do { } while (0)
#define COMMAND_PRINTVAR(...)  do { } while (0)
#endif

//#define MessageDestination LoraMessageDestination;

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
  void handleQueryConfigCommand(LoraMessageDestination& response);
  void handleQueryVolatileCommand(LoraMessageDestination& response);
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



    if (msg.getCrc(0xBEEF) == 0)
    {
      msg.trim(2);
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
          /*
          case 'O': //Override Interval
            handled = handleOverrideCommand(msg);
            break;
            */
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
          case 'S':
            stasisRequested = true;
            SET_PERMANENT_S(stasisRequested);
            handled = true;
          default:
          //do nothing. handled = false, so we send "IGNORED"
          break;
        }
      }
      msg.trim(-2);
    }
    if (handled && ackRequired && acknowledge)
      acknowledgeMessage(uniqueID, isSpecific, command);

    if (!handled)
    {
      byte buffer[20];// = { 'X', 'K', stationID, uniqueID, command, 'I', 'G', 'N', 'O', 'R', 'E', 'D' };
      LoraMessageDestination response(false, buffer, sizeof(buffer), 'K', uniqueID);
      response.appendByte2(command);
      response.append(F("IGNORED"), 7);
    }
  }

  //Relay command: C(ID)(UID)R(dataLength)((+|-)(C|W)(RelayID))*
  bool handleRelayCommand(MessageSource& msg)
  {
    byte types[] = { 'W', 'C', 'R' };
    byte loc = msg.getCurrentLocation();
    for (byte curType : types)
    {
      COMMAND_PRINTVAR(curType);
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
      msg.seek(loc);
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
      COMMAND_PRINTVAR(changed);
      if (changed)
        switch (curType)
        {
        case 'W':
          COMMAND_PRINTLN(F("UPDATE W"));
          PermanentStorage::setBytes((void*)offsetof(PermanentVariables, stationsToRelayWeather), permanentArraySize, list);
          max = permanentArraySize;
          break;
        case 'C':
          COMMAND_PRINTLN(F("UPDATE C"));
          PermanentStorage::setBytes((void*)offsetof(PermanentVariables, stationsToRelayCommands), permanentArraySize, list);
          max = permanentArraySize;
          break;
        case 'R':
          COMMAND_PRINTLN(F("UPDATE R"));
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
  //We don't use this so I'm removing it
  /*
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
  */
  
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
  
    byte buffer[254];
    LoraMessageDestination response(false, buffer, sizeof(buffer), 'K', uniqueID);
    byte headerStart[3];
    
    headerStart[0] = 'Q';
    headerStart[1] = queryType;

    //Version (a few bytes)
    headerStart[2] = ('V');
    response.append(headerStart, sizeof(headerStart)); //+6 = 6
    response.append(ASW_VER, ver_size); //+11 = 17
    response.appendByte2(0);

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

  void handleQueryConfigCommand(LoraMessageDestination& response)
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
      response.appendByte2(b);
    }
#endif
  }

#define copyInt(a, b) memcpy(a, &b, sizeof(b));

  void handleQueryVolatileCommand(LoraMessageDestination& response)
  {
    unsigned long curMillis = millis();
#if 1
    byte* buffer;
    const byte messageSize = 33
      + sizeof(MessageHandling::recentlySeenStations)
      + sizeof(recentlyHandledCommands)
      + sizeof(MessageHandling::recentlyRelayedMessages);
    response.getBuffer(&buffer, messageSize);
    *(unsigned long*)buffer = curMillis; //+4 = 4
    buffer += 4;
    *(unsigned long*)buffer = lastPingMillis; //+4 = 8
    buffer += 4;
    memcpy(buffer, MessageHandling::recentlySeenStations, sizeof(MessageHandling::recentlySeenStations));
    buffer += sizeof(MessageHandling::recentlySeenStations);
    memcpy(buffer, recentlyHandledCommands, sizeof(recentlyHandledCommands));
    buffer += sizeof(recentlyHandledCommands);
    memcpy(buffer, MessageHandling::recentlyRelayedMessages, sizeof(MessageHandling::recentlyRelayedMessages));
    buffer += sizeof(MessageHandling::recentlyRelayedMessages);
    *(unsigned short*)buffer = csma._crcErrorRate; // +2 = 10
    buffer += 2;
    *(unsigned short*)buffer = csma._droppedPacketRate; // +2 = 12
    buffer += 2;
    *(unsigned long*)buffer = csma._averageDelayTime; // +4 = 16
    buffer += 4;
    *(unsigned long*)buffer = TimerTwo::seconds(); // +4 = 20
    buffer += 4;
    *(unsigned short*)buffer = StackCount(); // +2 = 22
    buffer += 2;
    const uint8_t* p1 = &_end;
    *(unsigned short*)buffer = SP - (unsigned short)p1; //+2 = 24
    buffer += 2;
    *(unsigned long*)buffer = Database::_curHeaderAddress; //+4 = 28
    buffer += 4;
    *(unsigned long*)buffer = Database::_curWriteAddress; //+4 = 32
    buffer += 4;
    *buffer = Database::_curCycle; //+1 = 33
    return;
#endif
  }

  void notifyNewID(byte uniqueID, byte newID)
  {
    byte buffer[20];
    LoraMessageDestination reply(false, buffer, sizeof(buffer), 'K', uniqueID);
      reply.append(F("UNewID"), 6);
      reply.appendByte2(newID);
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
  
    byte buffer[20];
    LoraMessageDestination response(false, buffer, sizeof(buffer), 'K', uniqueID);
    response.appendByte2(commandType);
    response.append(F("OK"), 2);
  }
}