
//#include <TimerOne.h>
#include "ArduinoWeatherStation.h"
#include "LoraMessaging.h"
#include "MessageHandling.h"
#include "WeatherProcessing.h"
#include "Callsign.h"
#include "PermanentStorage.h"
#include <string.h>
#include "Commands.h"


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
  void recordWeatherForRelay(MessageSource& message, byte msgStatID, byte msgUniqueID);
  void relayMessage(MessageSource& message, byte msgType, byte msgFirstByte, byte msgStatID, byte msgUniqueID);
  void recordMessageRelay(byte msgType, byte msgStatID, byte msgUniqueID);
  void checkPing(MessageSource& message);

  //These arrays use 320 bytes.
  RecentlySeenStation recentlySeenStations[recentArraySize]; //100
  RecentlyRelayedMessage recentlyRelayedMessages[recentArraySize]; //60
  byte curUniqueID = 0;

  byte weatherRelayBuffer[weatherRelayBufferSize]; //100
  byte weatherRelayLength = 0;

  void readMessages()
  {
    static bool ledWasOn = false;
    MESSAGE_SOURCE_SOLID msg;
    while (msg.beginMessage())
    {
      AWS_DEBUG_PRINTLN(F("Got one!"));
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
      bool relayDemanded = msgFirstByte & 0x80;
      //Determine the originating or destination station:
      byte msgType = msgFirstByte & 0x7F;
      byte msgStatID;
      byte msgUniqueID;
      if (msgType == 'C' || msgType == 'K' || msgType == 'W' || msgType == 'R'
        || msgType == 'P')
      {
        if (msg.readByte(msgStatID) ||
          msg.readByte(msgUniqueID))
        {
          AWS_DEBUG_PRINTLN(F("Unable to read message header."));
          SIGNALERROR();
          continue;
        }
      }
      else
      {
        AWS_DEBUG_PRINT(F("Unknown message type: "));
        AWS_DEBUG_PRINTLN(msgType);
        SIGNALERROR();
        continue;
      }

      int afterHeader = msg.getCurrentLocation();

      AWS_DEBUG_PRINT(F("Message Received. Type: "));
      AWS_DEBUG_PRINT((char)msgType);
      AWS_DEBUG_PRINT(F(", stationID: "));
      AWS_DEBUG_PRINT((char)msgStatID);
      AWS_DEBUG_PRINT(F(", ID: "));
      AWS_DEBUG_PRINTLN(msgUniqueID);

      if (msgType == 'P')
        checkPing(msg);
        
      //If it's one of our messages relayed back to us, ignore it:
      if (msgType != 'C' && msgStatID == stationID)
        continue;

      //If it's a command addressed to us (or global), handle it:
      if (msgType == 'C' && (msgStatID == stationID || msgStatID == 0))
        Commands::handleCommandMessage(msg, relayDemanded, msgUniqueID, msgStatID != 0);

      //Record the weather stations we hear:
      if (msgType == 'W')
        recordHeardStation(msgStatID);

      //Otherwise, relay it if necessary:
      bool relayRequired = 
        msgStatID != stationID 
        &&
        (relayDemanded
          ||
          shouldRelay(msgType, msgStatID, msgUniqueID));
      if (relayRequired && !haveRelayed(msgType, msgStatID, msgUniqueID))
      {
        if (msg.seek(afterHeader))
        {
          AWS_DEBUG_PRINTLN(F("Unable to seek to afterHeader"));
          continue;
        }

        if (msgType == 'W')
          recordWeatherForRelay(msg, msgStatID, msgUniqueID);
        else 
          relayMessage(msg, msgType, msgFirstByte, msgStatID, msgUniqueID);

        recordMessageRelay(msgType, msgStatID, msgUniqueID);
      }   
    }
  }

  bool shouldRelay(byte msgType, byte msgStatID, byte msgUniqueID)
  {
    //Check if the source or destination is in our relay list:
    bool ret = false;
    if (msgType == 'C' || msgType == 'K' || msgType == 'R')
    {
      byte stationsToRelayCommands[permanentArraySize];
      GET_PERMANENT(stationsToRelayCommands);

      for (int i = 0; i < recentArraySize; i++)
      {
        if (!stationsToRelayCommands[i])
          break;
        if (stationsToRelayCommands[i] == msgStatID)
        {
          ret = true;
          break;
        }
      }
    }
    else if (msgType == 'W')
    {
      byte stationsToRelayWeather[permanentArraySize];
      GET_PERMANENT(stationsToRelayWeather);
      for (int i = 0; i < recentArraySize; i++)
      {
        if (!stationsToRelayWeather[i])
          break;
        if (stationsToRelayWeather[i] == msgStatID)
        {
          ret = true;
          break;
        }
      }
    }
    return ret;
  }

  bool haveRelayed(byte msgType, byte msgStatID, byte msgUniqueID)
  {
      //Check if we've already relayed this message (more appropriate for command relays than weather)
    for (int i = 0; i < recentArraySize; i++)
    {
      if (!recentlyRelayedMessages[i].type)
      {
        break;
      }
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
    memmove(recentlyRelayedMessages + 1, recentlyRelayedMessages, (recentArraySize - 1) * 3);
    recentlyRelayedMessages[0].type = msgType;
    recentlyRelayedMessages[0].stationID = msgStatID;
    recentlyRelayedMessages[0].msgID = msgUniqueID;
  }

  byte getUniqueID()
  {
    return curUniqueID++;
  }

  void relayMessage(MessageSource& msg, byte msgType, byte msgFirstByte, byte msgStatID, byte msgUniqueID)
  {
    //Outbound messages are 'C'
    MESSAGE_DESTINATION_SOLID relay(msgType == 'C');
    relay.appendByte(msgFirstByte);
    relay.appendByte(msgStatID);
    relay.appendByte(msgUniqueID);
    relay.appendData(msg, 5000);
  }

  void recordWeatherForRelay(MessageSource& msg, byte msgStatID, byte msgUniqueID)
  { 
    //Note: We must be careful when setting up the network that there are not multiple paths for weather messages to get relayed
    //it won't necessarily cause problems, but it will use unnecessary bandwidth
  
    byte dataSize = msg.getMessageLength() - 2; //2 for the 'XW'

    //If we can't fit it in the relay buffer,
    //we have to make sure to read the incomming message as we're sending out bytes,
    //or our buffers might overflow.
    bool overflow = weatherRelayLength + dataSize + 2 > weatherRelayBufferSize;
    byte buffer[sizeof(MESSAGE_DESTINATION_SOLID)];
    MESSAGE_DESTINATION_SOLID* msgDump = overflow ? new (buffer) MESSAGE_DESTINATION_SOLID(false) : 0;
    size_t offset = overflow ? 0 : weatherRelayLength;
    bool sourceFaulted = false;
    if (overflow)
    {
      msgDump->appendByte('R');
      msgDump->appendByte(stationID);
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
    weatherRelayLength = sourceFaulted ? 0 : dataSize + 2;
    
  }

  void recordHeardStation(byte msgStatID)
  {  
    int i = 0;
    for (; i < recentArraySize; i++)
    {
      if (!recentlySeenStations[i].id)
      {
        break;
      }
      if (i < recentArraySize - 1 && recentlySeenStations[i].id ==msgStatID)
      {
        memmove(recentlySeenStations + i, 
                recentlySeenStations + i + 1, 
                sizeof(RecentlySeenStation) * (recentArraySize - i - 1));
        i--;
      }
    }
    if (i == recentArraySize)
      i--;
    memmove(recentlySeenStations + 1, recentlySeenStations, sizeof(RecentlySeenStation) * i);
  
    recentlySeenStations[0].id = msgStatID;
    recentlySeenStations[0].millis = millis();
  }

  void sendWeatherMessage()
  {
    //If it's just our message, it will be:
    //W (Station ID) (Unique ID) (3) (WS) (WD) (Batt)
    //If it's a single relay, it would be:
    //W (Station ID) (Unique ID) (8) (WS) (WD) (Batt) : (StationR) (UidR) (WsR) (WdR) (BattR)
    //If there are multiple relay messages included, each has an extra 5 bytes.
  
    MESSAGE_DESTINATION_SOLID message(false);
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
    bool wasPrependCallsign = MessageDestination::s_prependCallsign;
    MessageDestination::s_prependCallsign = true;
    MESSAGE_DESTINATION_SOLID msg(false);
    //msg.append(callSign, strlen(callSign));
    msg.append(STATUS_MESSAGE, strlen_P((const char*)STATUS_MESSAGE));
    msg.appendByte(stationID);
    msg.finishAndSend();
    MessageDestination::s_prependCallsign = wasPrependCallsign;
    lastStatusMillis = millis();
  }

  void checkPing(MessageSource& msg)
  {
    byte callSignBuffer[sizeof(callSign) - 1];
    if (msg.readBytes(callSignBuffer, sizeof(callSignBuffer)) != MESSAGE_OK)
    {
      AWS_DEBUG_PRINTLN(F("Ping: can't read callsign"));
      SIGNALERROR();
    }
    else if (memcmp(callSignBuffer, callSign, sizeof(callSignBuffer)) != 0)
    {
      AWS_DEBUG_PRINT(F("Ping: callsign mismatch: "));
      for (int i = 0; i < sizeof(callSignBuffer); i++)
        AWS_DEBUG_PRINTLN((char)callSignBuffer[i]);
      SIGNALERROR();
    }
    else
    {
      AWS_DEBUG_PRINTLN(F("Ping Successful"));
      lastPingMillis = millis();
    }
  }
}