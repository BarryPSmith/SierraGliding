
//#include <TimerOne.h>
#include "ArduinoWeatherStation.h"
#include "LoraMessaging.h"
#include "MessageHandling.h"
#include "WeatherProcessing/WeatherProcessing.h"
#include "Callsign.h"
#include "PermanentStorage.h"
#include <string.h>
#include "Commands.h"
#include "TimerTwo.h"
#include "Database.h"

//#define DEBUG_MSGPROC
#ifdef DEBUG_MSGPROC
#define MSGPROC_PRINT AWS_DEBUG_PRINT
#define MSGPROC_PRINTLN AWS_DEBUG_PRINTLN
#define MSGPROC_PRINTVAR PRINT_VARIABLE
#else
#define MSGPROC_PRINT(...)  do { } while (0)
#define MSGPROC_PRINTLN(...)  do { } while (0)
#define MSGPROC_PRINTVAR(...)  do { } while (0)
#endif

//Placement new operator.
void* operator new(size_t size, void* ptr)
{
  return ptr;
}

namespace MessageHandling
{
  bool haveRelayed(byte msgType, byte msgStatID, byte msgUniqueID);
  void recordHeardStation(byte msgStatID);
  bool shouldRelay(byte msgType, byte msgStatID, byte msgUniqueID);
  bool shouldRecord(byte msgType, bool relayRequired);
  void recordWeatherForRelay(MessageSource& message, byte msgStatID, byte msgUniqueID);
  void relayMessage(MessageSource& message, byte msgType, byte msgFirstByte, byte msgStatID, byte msgUniqueID);
  void recordMessageRelay(byte msgType, byte msgStatID, byte msgUniqueID);
  void checkPing(MessageSource& message);

  //These arrays use 320 bytes.
  RecentlySeenStation recentlySeenStations[permanentArraySize]; //100
  RecentlyRelayedMessage recentlyRelayedMessages[permanentArraySize]; //60
  byte recentlyRelayedMessageIndex = 0;
  byte curUniqueID = 0;

  byte weatherRelayBuffer[weatherRelayBufferSize]; //100
  byte weatherRelayLength = 0;

  void readMessages()
  {
    static bool ledWasOn = false;
    MESSAGE_SOURCE_SOLID msg;
    /*MESSAGE_DESTINATION_SOLID::*/delayRequired = true;
    while (msg.beginMessage())
    {
      if (!MessageSource::s_discardCallsign)
      {
        byte zerothByte;
        //Filter: First byte must be X if we're discarding callsign.
        if (msg.readByte(zerothByte) || zerothByte != 'X')
          continue;
      }
      byte msgFirstByte;
      if (msg.readByte(msgFirstByte))
        continue; //= incomingBuffer[0] & 0x7F;
      //Determine the originating or destination station:
      byte msgType = msgFirstByte & 0x7F;
      byte msgStatID;
      byte msgUniqueID;
      if (msgType == 'C' || msgType == 'K' || msgType == 'W' || msgType == 'R'
        || msgType == 'P' || msgType == 'S')
      {
        if (msg.readByte(msgStatID) ||
          msg.readByte(msgUniqueID))
        {
          MSGPROC_PRINTLN(F("Unable to read message header."));
          SIGNALERROR();
          continue;
        }
      }
      else
      {
        MSGPROC_PRINT(F("Unknown message type: "));
        MSGPROC_PRINTLN(msgType);
        SIGNALERROR();
        continue;
      }

      byte afterHeader = msg.getCurrentLocation();

      MSGPROC_PRINT(F("Message Received. Type: "));
      MSGPROC_PRINT((char)msgType);
      MSGPROC_PRINT(F(", stationID: "));
      MSGPROC_PRINT((char)msgStatID);
      MSGPROC_PRINT(F(", ID: "));
      MSGPROC_PRINTLN(msgUniqueID);

      if (msgType == 'P')
        checkPing(msg);
        
      //If it's one of our messages relayed back to us, ignore it:
      if (msgType != 'C' && msgStatID == stationID)
        continue;

      //If it's a command addressed to us (or global), handle it:
      if (msgType == 'C' && (msgStatID == stationID || msgStatID == 0))
        Commands::handleCommandMessage(msg, msgUniqueID, msgStatID != 0);

      //Record the weather stations we hear:
      if (msgType == 'W')
        recordHeardStation(msgStatID);

      //Otherwise, relay it if necessary:
      bool relayRequired = 
        msgStatID != stationID 
        &&
        (shouldRelay(msgType, msgStatID, msgUniqueID));
      if (relayRequired && !haveRelayed(msgType, msgStatID, msgUniqueID))
      {
        if (msg.seek(afterHeader))
        {
          MSGPROC_PRINTLN(F("Unable to seek to afterHeader"));
          continue;
        }

        if (msgType == 'W')
          recordWeatherForRelay(msg, msgStatID, msgUniqueID);
        else 
          relayMessage(msg, msgType, msgFirstByte, msgStatID, msgUniqueID);

        recordMessageRelay(msgType, msgStatID, msgUniqueID);
      }
#ifndef NO_STORAGE
      if (shouldRecord(msgType, relayRequired))
      {
        msg.seek(afterHeader);
        Database::storeMessage(msgType, msgStatID, msg);
      }
#endif // !NO_STORAGE
    }
    /*MESSAGE_DESTINATION_SOLID::*/delayRequired = false;
  }

  bool shouldRecord(byte msgType, bool relayRequired)
  {
    if (!relayRequired)
    {
      bool recordNonRelayedMessages;
      GET_PERMANENT_S(recordNonRelayedMessages);
      if (!recordNonRelayedMessages)
        return false;
    }
    byte messageTypesToRecord[messageTypeArraySize];
    GET_PERMANENT(messageTypesToRecord);
    for (int i = 0; i < messageTypeArraySize; i++)
      if (messageTypesToRecord[i] == msgType)
        return true;
    return false;
  }

  bool shouldRelay(byte msgType, byte msgStatID, byte msgUniqueID)
  {
    //Check if the source or destination is in our relay list:
    if (msgType == 'C' || msgType == 'K' || msgType == 'R' || msgType == 'P' || msgType == 'S')
    {
      byte stationsToRelayCommands[permanentArraySize];
      GET_PERMANENT(stationsToRelayCommands);
      
      for (int i = 0; i < permanentArraySize; i++)
      {
        if ((msgStatID == 0) != (stationsToRelayCommands[i] == msgStatID))
        {
          return true;
        }
      }
    }
    else if (msgType == 'W')
    {
      byte stationsToRelayWeather[permanentArraySize];
      GET_PERMANENT(stationsToRelayWeather);
      for (int i = 0; i < permanentArraySize; i++)
      {
        if ((msgStatID == 0) != (stationsToRelayWeather[i] == msgStatID))
        {
          return true;
        }
      }
    }
    return false;
  }

  bool haveRelayed(byte msgType, byte msgStatID, byte msgUniqueID)
  {
      //Check if we've already relayed this message (more appropriate for command relays than weather)
    for (int i = 0; i < permanentArraySize; i++)
    {
      if (recentlyRelayedMessages[i].type == msgType &&
          recentlyRelayedMessages[i].stationID == msgStatID &&
          recentlyRelayedMessages[i].msgID == msgUniqueID)
      {
        return true;
      }
    }
    return false;
  }

  void recordMessageRelay(byte msgType, byte msgStatID, byte msgUniqueID)
  { 
    recentlyRelayedMessages[recentlyRelayedMessageIndex].type = msgType;
    recentlyRelayedMessages[recentlyRelayedMessageIndex].stationID = msgStatID;
    recentlyRelayedMessages[recentlyRelayedMessageIndex].msgID = msgUniqueID;
    if (++recentlyRelayedMessageIndex >= permanentArraySize)
      recentlyRelayedMessageIndex = 0;
  }

  byte getUniqueID()
  {
    return curUniqueID++;
  }

  void relayMessage(MessageSource& msg, byte msgType, byte msgFirstByte, byte msgStatID, byte msgUniqueID)
  {
    //Outbound messages are 'C' or 'P'
    MESSAGE_DESTINATION_SOLID<254> relay(msgType == 'C' || msgType == 'P');
    relay.appendByte(msgFirstByte);
    relay.appendByte(msgStatID);
    relay.appendByte(msgUniqueID);
    relay.appendData(msg, 254);
  }

  void recordWeatherForRelay(MessageSource& msg, byte msgStatID, byte msgUniqueID)
  { 
    //Note: We must be careful when setting up the network that there are not multiple paths for weather messages to get relayed
    //it won't necessarily cause problems, but it will use unnecessary bandwidth
  
    //Incomming message might be XW (SID) (UID) (Sz) (WD) (WS) - length = 7
    //In that case we need to append (SID) (UID) (Sz) (WD) (WS) - length = 5, 3 more to read (because we've already read SID and UID)
    byte dataSize = msg.getMessageLength() - 2; //2 for the 'XW'

    //If we can't fit it in the relay buffer,
    //we have to make sure to read the incomming message as we're sending out bytes,
    //or our buffers might overflow.
    bool overflow = weatherRelayLength + dataSize + 2 > weatherRelayBufferSize;
    byte buffer[sizeof(MESSAGE_DESTINATION_SOLID<254>)];
    MESSAGE_DESTINATION_SOLID<254>* msgDump = overflow ? new (buffer) MESSAGE_DESTINATION_SOLID<254>(false) : 0;
    size_t offset = overflow ? 0 : weatherRelayLength;
    bool sourceFaulted = false;
      
    if (overflow)
    {
      msgDump->appendByte('R');
      msgDump->appendByte(stationID);
      msgDump->appendByte(getUniqueID());
      msgDump->appendByte(weatherRelayBuffer[0]);
      msgDump->appendByte(weatherRelayBuffer[1]);
    }
    weatherRelayBuffer[offset + 0] = msgStatID;
    weatherRelayBuffer[offset + 1] = msgUniqueID;

    for (int i = 2; i < dataSize; i++)
    {
      if (overflow)
        msgDump->appendByte(weatherRelayBuffer[i]);
      sourceFaulted = sourceFaulted ||
        msg.readByte(weatherRelayBuffer[offset + i]);
    }
    if (overflow)
    {
      msgDump->append(weatherRelayBuffer + dataSize, weatherRelayLength - dataSize);
      msgDump->~MESSAGE_DESTINATION_SOLID();
      msgDump = 0; //We're done with it. Ensure we don't accidentally re-use it.
    }
    if (sourceFaulted)
      weatherRelayLength = 0;
    else
      weatherRelayLength += dataSize;
  }

  void recordHeardStation(byte msgStatID)
  { 
    RecentlySeenStation* cur = recentlySeenStations;
    RecentlySeenStation* end = recentlySeenStations + permanentArraySize;
    uint32_t oldest = 0;
    uint32_t curMillis = millis();
    RecentlySeenStation* oldestRecord = recentlySeenStations;
    for (; cur < end; cur++)
    {
      if (cur->id == msgStatID || cur->id == 0)
        break;
      uint32_t diff = curMillis - cur->millis;
      if (diff > oldest)
      {
        oldest = diff;
        oldestRecord = cur;
      }
    }
    if (cur == end)
      cur = oldestRecord;
    cur->id = msgStatID;
    cur->millis = curMillis;
  }

  void sendWeatherMessage()
  {
    //If it's just our message, it will be:
    //W (Station ID) (Unique ID) (3) (WS) (WD) (Batt)
    //If it's a single relay, it would be:
    //W (Station ID) (Unique ID) (8) (WS) (WD) (Batt) : (StationR) (UidR) (WsR) (WdR) (BattR)
    //If there are multiple relay messages included, each has an extra 5 bytes.
  
    MESSAGE_DESTINATION_SOLID<254> message(false);
    message.appendByte('W');
    message.appendByte(stationID);
    message.appendByte(getUniqueID());
    WeatherProcessing::createWeatherData(message);
    message.append(weatherRelayBuffer, weatherRelayLength);
    weatherRelayLength = 0;
    message.finishAndSend();
  }

  void sendStatusMessage()
  {
    //bool wasPrependCallsign = MessageDestination::s_prependCallsign;
    //MessageDestination::s_prependCallsign = true;
    MESSAGE_DESTINATION_SOLID<254> msg(false, false);
    if (!MESSAGE_DESTINATION_SOLID<254>::s_prependCallsign)
      msg.append((byte*)callSign, 6);
    msg.append(STATUS_MESSAGE, strlen_P((const char*)STATUS_MESSAGE));
    msg.appendByte(stationID);
    msg.finishAndSend();
    //MessageDestination::s_prependCallsign = wasPrependCallsign;
    lastStatusMillis = millis();
  }

  void checkPing(MessageSource& msg)
  {
    byte callSignBuffer[sizeof(callSign) - 1];
    if (msg.readBytes(callSignBuffer, sizeof(callSignBuffer)) != MESSAGE_OK)
    {
      MSGPROC_PRINTLN(F("Ping: can't read callsign"));
      SIGNALERROR();
    }
    else if (memcmp(callSignBuffer, callSign, sizeof(callSignBuffer)) != 0)
    {
      MSGPROC_PRINT(F("Ping: callsign mismatch: "));
      for (int i = 0; i < sizeof(callSignBuffer); i++)
        MSGPROC_PRINTLN((char)callSignBuffer[i]);
      SIGNALERROR();
    }
    else
    {
      MSGPROC_PRINTLN(F("Ping Successful"));
      unsigned long seconds;
      if (msg.read(seconds) == MESSAGE_OK)
      {
        MSGPROC_PRINTLN(F("Time set"));
        TimerTwo::setSeconds(seconds);
        MSGPROC_PRINTVAR(seconds);
        MSGPROC_PRINTVAR(TimerTwo::_ticks);
        MSGPROC_PRINTVAR(TimerTwo::_ofTicks);
        MSGPROC_PRINTVAR(TimerTwo::MillisPerTick);
      }
      lastPingMillis = millis();
    }
  }
}