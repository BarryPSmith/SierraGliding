
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
  bool shouldRecord(byte msgType, bool relayRequired,
    MessageSource& msg);
  bool recordWeatherForRelay(MessageSource& message, byte msgStatID, byte msgUniqueID);
  void relayMessage(MessageSource& message, byte msgType, byte msgFirstByte, byte msgStatID, byte msgUniqueID);
  void recordMessageRelay(byte msgType, byte msgStatID, byte msgUniqueID);
  void checkPing(MessageSource& message);
  void readMessage(LoraMessageSource& msg);

  //These arrays use 320 bytes.
  RecentlySeenStation recentlySeenStations[permanentArraySize]; //100
  RecentlyRelayedMessage recentlyRelayedMessages[permanentArraySize]; //60
  byte recentlyRelayedMessageIndex = 0;
  byte curUniqueID = 0;

  constexpr int weatherRelayBufferSize = 100;
  constexpr int headerSize = 4; //'XR1#'
  byte weatherRelayBufferBase[weatherRelayBufferSize + headerSize];//104
  byte* const weatherRelayBuffer = weatherRelayBufferBase + headerSize;
  byte weatherRelayLength = 0;

  void readMessages()
  {
    LoraMessageSource msg;
    delayRequired = true;
    while (msg.beginMessage())
    {
      readMessage(msg);
      msg.doneWithMessage();
    }
    if (msg._lastBeginError == REENTRY_NOT_SUPPORTED)
      csma.clearBuffer();
    delayRequired = false;
  }

  void readMessage(LoraMessageSource& msg)
  {
    if (!MessageSource::s_discardCallsign)
    {
      byte zerothByte;
      //Filter: First byte must be X if we're discarding callsign.
      if (msg.readByte(zerothByte) || zerothByte != 'X')
        return;
    }
    byte msgFirstByte;
    if (msg.readByte(msgFirstByte))
      return; //= incomingBuffer[0] & 0x7F;
    //Determine the originating or destination station:
    byte msgType = msgFirstByte & 0x7F;
    byte msgStatID;
    byte msgUniqueID;
    if (msgType == 'C' || msgType == 'K' || msgType == 'W' || msgType == 'R'
      || msgType == 'P' || msgType == 'S' || msgType == 'Q')
    {
      if (msg.readByte(msgStatID) ||
        msg.readByte(msgUniqueID))
      {
        MSGPROC_PRINTLN(F("Unable to read message header."));
        SIGNALERROR(MESSAGING_CANNOT_READ_HEADER);
        return;
      }
    }
    else
    {
      MSGPROC_PRINT(F("Unknown message type: "));
      MSGPROC_PRINTLN(msgType);
      SIGNALERROR(MESSAGING_UNKNOWN_TYPE);
      return;
    }

    byte afterHeader = msg.getCurrentLocation();

    MSGPROC_PRINT(F("Message Received. Type: "));
    MSGPROC_PRINT((char)msgType);
    MSGPROC_PRINT(F(", stationID: "));
    MSGPROC_PRINT((char)msgStatID);
    MSGPROC_PRINT(F(", ID: "));
    MSGPROC_PRINTLN(msgUniqueID);

    if (msgType == 'P' && Commands::checkCommandUID(msgUniqueID))
      checkPing(msg);

    //If it's one of our messages relayed back to us, ignore it:
    if (msgType != 'C' && msgStatID == stationID)
      return;

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
        return;
      }

      bool relayed = false;
      if (msgType == 'W')
      {
        // Record for sending with the next weather packet.
        // If it's too long to be recorded, change it to be a Q packet and send it on.
        if (recordWeatherForRelay(msg, msgStatID, msgUniqueID))
          relayed = true;
        else
          msgFirstByte = 'Q';
      }
      if (!relayed)
        relayMessage(msg, msgType, msgFirstByte, msgStatID, msgUniqueID);

      recordMessageRelay(msgType, msgStatID, msgUniqueID);
    }
#ifndef NO_STORAGE
    if (shouldRecord(msgType, relayRequired, msg))
    {
      msg.seek(afterHeader);
      Database::storeMessage(msgType, msgStatID, msg);
    }
#endif // !NO_STORAGE
  }

  bool shouldRecord(byte msgType, bool relayRequired,
    MessageSource& msg)
  {
    if (!relayRequired)
    {
      bool recordNonRelayedMessages;
      GET_PERMANENT_S(recordNonRelayedMessages);
      if (!recordNonRelayedMessages)
        return false;
    }
    byte messageTypesToRecord[messageTypeArraySize];
#ifdef DONT_RECORD_ACKS
    if (msgType == 'K' && msg.getMessageLength() <= 20)
    {
      byte* buffer;
      msg.seek(msg.getMessageLength() - 2);
      msg.accessBytes(&buffer, 2);
      if (buffer[0] == 'O' && buffer[1] == 'K')
        return false;
      if (msg.getMessageLength >= 7)
      {
        byte comp[] = {'I', 'G', 'N', 'O', 'R', 'E', 'D'};
        msg.seek(msg.getMessageLength() - sizeof(comp));
        msg.accessBytes(&buffer, sizeof(comp));
        if (memcmp(buffer, comp, sizeof(comp)) == 0)
          return false;
      }
    }
#endif
    GET_PERMANENT(messageTypesToRecord);
    for (int i = 0; i < messageTypeArraySize; i++)
      if (messageTypesToRecord[i] == msgType)
        return true;
    return false;
  }

  bool shouldRelay(byte msgType, byte msgStatID, byte msgUniqueID)
  {
    //Check if the source or destination is in our relay list:
    if (msgType == 'C' || msgType == 'K' || msgType == 'R' || msgType == 'P' || msgType == 'S'
      || msgType == 'Q')
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
    byte buffer[254];
    LoraMessageDestination relay(msgType == 'C' || msgType == 'P', buffer, sizeof(buffer));
    relay.appendByte2(msgFirstByte);
    // Note: R's follow the same rules as K messages:
    // They're controlled by the relay command array, not the relay weather array,
    // so we leave the sender station ID
    relay.appendByte2(msgStatID);
    relay.appendByte2(msgUniqueID);
    relay.appendData(msg, 254);
  }

  
  bool __attribute__((noinline)) recordWeatherForRelay(MessageSource& msg, byte msgStatID, byte msgUniqueID)
  { 
    //Note: We must be careful when setting up the network that there are not multiple paths for weather messages to get relayed
    //it won't necessarily cause problems, but it will use unnecessary bandwidth
  
    //Incomming message might be XW (SID) (UID) (Sz) (WD) (WS) - length = 7
    //In that case we need to append (SID) (UID) (Sz) (WD) (WS) - length = 5, 3 more to read (because we've already read SID and UID)
    byte dataSize = msg.getMessageLength() - 2; //2 for the 'XW'
    // If our data is larger than our buffer, we can't do anything with it.
    if (dataSize > weatherRelayBufferSize)
      return false;

    bool overflow = weatherRelayLength + dataSize + 2 > weatherRelayBufferSize;
    if (overflow)
    {
      LoraMessageDestination msgDump(false, weatherRelayBufferBase, sizeof(weatherRelayBufferBase), 'R', getUniqueID());
      byte* notUsed;
      if (msgDump.getBuffer(&notUsed, weatherRelayLength) != MESSAGE_OK)
        msgDump.abort();
      weatherRelayLength = 0;
    }

    byte offset = weatherRelayLength;
    bool sourceFaulted = 
      msg.seek(2) ||
      msg.readBytes(weatherRelayBuffer + offset, dataSize);

    if (sourceFaulted)
      weatherRelayLength = 0;
    else
      weatherRelayLength += dataSize;
    return true;
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
  
    byte buffer[254];
    LoraMessageDestination message(false, buffer, sizeof(buffer), 'W', getUniqueID());

    WeatherProcessing::createWeatherData(message);
    message.append(weatherRelayBuffer, weatherRelayLength);
    weatherRelayLength = 0;
    message.finishAndSend();
  }

  void sendStatusMessage()
  {
    //bool wasPrependCallsign = MessageDestination::s_prependCallsign;
    //MessageDestination::s_prependCallsign = true;
    byte buffer[254];
    LoraMessageDestination msg(false, buffer, sizeof(buffer), false);
    if (!LoraMessageDestination::s_prependCallsign)
      msg.append((byte*)callSign, 6);
    msg.append(STATUS_MESSAGE, strlen_P((const char*)STATUS_MESSAGE));
    msg.appendByte2(stationID);
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
      SIGNALERROR(MESSAGING_CANT_READ_CALLSIGN);
    }
    else if (memcmp(callSignBuffer, callSign, sizeof(callSignBuffer)) != 0)
    {
      MSGPROC_PRINT(F("Ping: callsign mismatch: "));
      for (int i = 0; i < sizeof(callSignBuffer); i++)
        MSGPROC_PRINTLN((char)callSignBuffer[i]);
      SIGNALERROR(MESSAGING_CALLSIGN_MISMATCH);
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