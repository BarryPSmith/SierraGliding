#pragma once
void sendStatusMessage();
void readMessages();
void sendWeatherMessage();

constexpr int recentArraySize = 20;

//We keep track of recently seen stations to allow network debugging / optimisation
struct RecentlySeenStation
{
  byte id;
  unsigned long millis; 
};

struct RecentlyRelayedMessage
{
  char type;
  byte msgID;
  byte stationID;
};

extern RecentlySeenStation recentlySeenStations[recentArraySize]; //100 bytes
//We keep track of recently relayed messages to avoid relaying the same message multiple times
extern RecentlyRelayedMessage recentlyRelayedMessages[recentArraySize]; //3 bytes per record: msg type, stationID, uniqueID. 60 bytes
//We keep track of recently handled commands to avoid executing the same command multiple times
extern byte recentlyHandledCommands[recentArraySize];
//Every message is given a unique ID. This is so other stations can keep track of them
extern byte curUniqueID;

const int weatherRelayBufferSize = 100;
extern byte weatherRelayBuffer[weatherRelayBufferSize];
extern byte weatherRelayLength;
