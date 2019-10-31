#include "ArduinoWeatherStation.h"
#include "Messaging.h"
#include "MessageHandling.h"
#include "WeatherProcessing.h"
#include "Callsign.h"

extern void handleCommandMessage(byte* message, size_t readByteCount);

bool haveRelayed(byte msgType, byte msgStatID, byte msgUniqueID);
void recordHeardStation(byte msgStatID);
bool shouldRelay(byte msgType, byte msgStatID, byte msgUniqueID);
void recordWeatherForRelay(byte* message, int readByteCount);
void relayMessage(byte msgType, byte* msgBuffer, size_t& readByteCount, int bufferSize);
void recordMessageRelay(byte msgType, byte msgStatID, byte msgUniqueID);

byte recentlySeenStations[recentArraySize][5];
byte recentlyRelayedMessages[recentArraySize][3];
byte recentlyHandledCommands[recentArraySize];
byte curUniqueID = 0;

byte stationsToRelayCommands[recentArraySize];
byte stationsToRelayWeather[recentArraySize]; //20 bytes

byte weatherRelayBuffer[weatherRelayBufferSize];
byte weatherRelayLength = 0;

void readMessages()
{
  const int bufferSize = 255;
  byte msgBuffer[bufferSize]; 
  byte* incomingBuffer = msgBuffer;
  //Serial.println("reading messages...");
  static bool ledWasOn = false;
  while (Serial.available() > 0)
  {
    //Debugging:
    //digitalWrite(LED_BUILTIN, ledWasOn ? LOW : HIGH);
    ledWasOn = !ledWasOn;
    
    //sendTestMessage();
    
    size_t readByteCount = readMessage(incomingBuffer, bufferSize);

    if (readByteCount > 0)
    {
      //Loopback debugging:
      #if 0
      Serial.write(0xC0);
      Serial.write(0x00);
      Serial.write('!');
      Serial.write(incomingBuffer, readByteCount);
      #endif
      byte msgByte0 = incomingBuffer[0] & 0x7F;
      //Determine the originating or destination station:
      byte msgType;
      byte msgStatID;
      byte msgUniqueID;
      if (msgByte0 == 'C' || msgByte0 == 'K' || msgByte0 == 'W' || msgByte0 == 'R')
      {
        msgType = msgByte0;
        msgStatID = incomingBuffer[1];
        msgUniqueID = incomingBuffer[2];
      }
      else
      {
        continue;
      }
        
      //If it's one of our messages relayed back to us, ignore it:
      if (msgType != 'C' && msgStatID == stationId)
        continue;

      //If it's a command addressed to us (or global), handle it:
      if (msgType == 'C' && (msgStatID == stationId || msgStatID == 0))
        handleCommandMessage(incomingBuffer, readByteCount);

      //Record the weather stations we hear:
      if (msgType == 'W')
        recordHeardStation(msgStatID);

      //Otherwise, relay it if necessary:
      bool relayRequired = 
        msgStatID != stationId 
        &&
        ((incomingBuffer[0] & 0x80)
         ||
         shouldRelay(msgType, msgStatID, msgUniqueID));
      if (relayRequired && !haveRelayed(msgType, msgStatID, msgUniqueID))
      {
        if (msgType == 'W')
          recordWeatherForRelay(incomingBuffer, readByteCount);
        else 
          relayMessage(msgType, msgBuffer, readByteCount, bufferSize);
        recordMessageRelay(msgType, msgStatID, msgUniqueID);
      }   
    }
  }
}
bool shouldRelay(byte msgType, byte msgStatID, byte msgUniqueID)
{
  //Check if the source or destination is in our relay list:
  bool ret = false;
  if (msgType == 'C' || msgType == 'K' || msgType == 'R')
  {
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
    if (!recentlyRelayedMessages[i][0])
    {
      break;
    }
    if (recentlyRelayedMessages[i][0] == msgType &&
        recentlyRelayedMessages[i][1] == msgStatID &&
        recentlyRelayedMessages[i][2] == msgUniqueID)
    {
      return true;
    }
  }
  return false;
}

void recordMessageRelay(byte msgType, byte msgStatID, byte msgUniqueID)
{  
  memmove(recentlyRelayedMessages + 1, recentlyRelayedMessages, (recentArraySize - 1) * 3);
  recentlyRelayedMessages[0][0] = msgType;
  recentlyRelayedMessages[0][1] = msgStatID;
  recentlyRelayedMessages[0][2] = msgUniqueID;
}

byte getUniqueID()
{
  //Seems wasteful to send a unique ID that requires escaping
  if (curUniqueID == FEND || curUniqueID == FESC)
    curUniqueID++;
  return curUniqueID++;
}

void relayMessage(byte msgType, byte* msgBuffer, size_t& readByteCount, int bufferSize)
{
  MessageDestination relay;
  relay.append(msgBuffer, readByteCount);
  //sendMessage(msgBuffer, readByteCount, bufferSize);
}

void recordWeatherForRelay(byte* message, int readByteCount)
{
  //Incoming Weather message:
  //0 W
  //1 Station ID - Take
  //2 Unique ID - Take
  //3 Data Length - Use but don't record specifically
  //4 Data - Take
  //assert(message[0] & 0x7F == 'W');

  //Relayed:
  //(Sending Station ID)(Message Unique ID)(Message Data)

  //E.g. if we append a single packet, we will append 5 bytes:
  //(1) (0x01) (WD) (WS) (Batt)
  //if we append a packet that already has a single relay, we will append 10 bytes:
  //(1) (0x01) (WD1) (WS1) (Batt1) (2) (0x01) (WD2) (WS2) (Batt2)
  
  //Note: We must be careful when setting up the network that there are not multiple paths for weather messages to get relayed
  //it won't necessarily cause problems, but it will use unnecessary bandwidth
  
  byte dataSize = message[3];
  if (dataSize + 2 > readByteCount)
    return; //We've got an invaild message
  if (weatherRelayLength + dataSize + 2 > weatherRelayBufferSize)
  {
    //Prepend R + StationID and send what we have, then write this message to the now empty buffer
    MessageDestination message;
    message.appendByte(stationId);
    message.appendByte('R');
    message.appendByte(weatherRelayLength);
    message.append(weatherRelayBuffer, weatherRelayLength);
    weatherRelayLength = 0;
    /*
    weatherRelayBuffer[-1] = stationId;
    weatherRelayBuffer[-2] = 'R';
    size_t messageSize = 2 + weatherRelayLength;
    sendMessage(weatherRelayBuffer - 2 - messageOffset, messageSize, weatherMessageBufferSize - weatherRelayOffset + 2);
    weatherRelayLength = 0;
    */
  }
  //Record 
  weatherRelayBuffer[weatherRelayLength] = message[1]; //Sending StationID
  weatherRelayBuffer[weatherRelayLength + 1] = message[2]; //UniqueID
  memcpy(weatherRelayBuffer + weatherRelayLength + 2, message + 4, dataSize);
  
  weatherRelayLength += dataSize + 2;
}

void recordHeardStation(byte msgStatID)
{  
  int i = 0;
  for (; i < recentArraySize; i++)
  {
    if (!recentlySeenStations[i][0])
    {
      break;
    }
    if (i < recentArraySize - 1 && recentlySeenStations[i][0]==msgStatID)
    {
      memmove(recentlySeenStations + i, recentlySeenStations + i +1, 5 * (recentArraySize - i - 1));
      i--;
    }
  }
  if (i == recentArraySize)
    i--;
  memmove(recentlySeenStations + 1, recentlySeenStations, i * 5);
  
  unsigned long curMillis = millis();
  recentlySeenStations[0][0] = msgStatID;
  memcpy(recentlySeenStations[0] + 1, &curMillis, 4);
}

void sendWeatherMessage()
{
  //If it's just our message, it will be:
  //W (Station ID) (Unique ID) (3) (WS) (WD) (Batt)
  //If it's a single relay, it would be:
  //W (Station ID) (Unique ID) (8) (WS) (WD) (Batt) : (StationR) (UidR) (WsR) (WdR) (BattR)
  //If there are multiple relay messages included, each has an extra 5 bytes.
  
  MessageDestination message;
  message.appendByte('W');
  message.appendByte(stationId);
  message.appendByte(getUniqueID());
  message.appendByte(3 + weatherRelayLength);
  createWeatherData(message);
  message.append(weatherRelayBuffer, weatherRelayLength);
  weatherRelayLength = 0;
  message.finishAndSend();
  
  /*byte* weatherMessage = weatherMessageBuffer + messageOffset;
  weatherMessage[0] = 'W'; //Message type
  weatherMessage[1] = stationId; //Station ID
  weatherMessage[2] = getUniqueID();
  weatherMessage[3] = 3 + weatherRelayLength; //Data length
  size_t msgSize = createWeatherData(weatherMessage + 4); //Our data
  //assert(msgSize == 3);
  msgSize = singleStationWeatherMessageSize + weatherRelayLength; //the relay data has been built in place in the weather buffer.
  sendMessage(weatherMessageBuffer, msgSize, weatherMessageBufferSize);
  weatherRelayLength = 0;*/
}

void sendStatusMessage()
{
  /*sendMessage(statusMessageTemplate, 19, 19, 0);
  Serial.print("!");
  Serial.print(millis()); 
  Serial.print(" ");
  Serial.print(lastStatusMillis + millisBetweenStatus);*/
  lastStatusMillis = millis();
}

void ZeroMessagingArrays()
{
  memset(stationsToRelayCommands, 0, recentArraySize);
  memset(stationsToRelayWeather, 0, recentArraySize);
  memset(recentlySeenStations, 0, recentArraySize * 5);
  memset(recentlyRelayedMessages, 0, recentArraySize * 3);
  memset(recentlyHandledCommands, 0, recentArraySize);
}
