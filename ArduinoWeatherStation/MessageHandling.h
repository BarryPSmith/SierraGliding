#pragma once
void ZeroMessagingArrays();
void sendStatusMessage();
void readMessages();
void sendWeatherMessage();

const int recentArraySize = 20; //We use 14 bytes for each of these. 280 bytes.

//We keep track of recently seen stations to allow network debugging / optimisation
extern byte recentlySeenStations[recentArraySize][5]; //5 bytes per record: 1 byte for station ID, 4 bytes for millis() when seen. 100 bytes
//We keep track of recently relayed messages to avoid relaying the same message multiple times
extern byte recentlyRelayedMessages[recentArraySize][3]; //3 bytes per record: msg type, stationId, uniqueID. 60 bytes
//We keep track of recently handled commands to avoid executing the same command multiple times
extern byte recentlyHandledCommands[recentArraySize];
//Every message is given a unique ID. This is so other stations can keep track of them
extern byte curUniqueID;

extern byte stationsToRelayCommands[recentArraySize];
extern byte stationsToRelayWeather[recentArraySize]; //20 bytes

const int weatherRelayBufferSize = 100;
extern byte weatherRelayBuffer[weatherRelayBufferSize];
extern byte weatherRelayLength;
