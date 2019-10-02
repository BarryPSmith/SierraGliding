#pragma once
const char* ver = "1.0"; //Wind station Version 1.0
const char* callSign = "KN6DUC";
//const char* statusMessageTemplate = "KN6DUC WindStation1";
//const int statusMessageLength = 13;

//Station Specific Constants
const char stationId = '1';

//Global Constants
const int voltagePin = A2;
const int windDirPin = A0;
const int windSpdPin = 2;
const int recentArraySize = 20; //We use 14 bytes for each of these. 280 bytes.
const unsigned long millisBetweenStatus = 600000; //We send our status messages this often. Note that status message stuff is currently commented out.
const unsigned long wakePeriod = 50; //We won't go to sleep if our next weather send is due before this time

const int messageOffset = 8; //Start FEND, 0x00, Callsign
const int minMessageSize = 18;// Radioshield will not transmit shorter than 15 bytes. We also need FEND 0x00 ... FEND
const int extraMessageBytes = 9; //Minimum number of bytes needed to write FEND 0x00 CALLSIGN [...] FEND

//KISS special characters
const byte FEND = 0xC0;
const byte FESC = 0xDB;
const byte TFEND = 0xDC;
const byte TFESC = 0xDD;

//Setup Variables
unsigned long shortInterval = 4000; //Send interval when battery voltage is high
unsigned long longInterval = 30000; //Send interval when battery voltage is low
int batteryThreshold = 804; //Battery voltage to switch between short and long interval. 784 = 11.5V: Resulted in station running out of power for one hour. 804 = 11.8V
unsigned long weatherInterval = 6000; //Current weather interval.
bool demandRelay = false;



//Recent Memory
//We keep track of recently seen stations to allow network debugging / optimisation
byte recentlySeenStations[recentArraySize][5]; //5 bytes per record: 1 byte for station ID, 4 bytes for millis() when seen. 100 bytes
//We keep track of recently relayed messages to avoid relaying the same message multiple times
byte recentlyRelayedMessages[recentArraySize][3]; //3 bytes per record: msg type, stationId, uniqueID. 60 bytes
//We keep track of recently handled commands to avoid executing the same command multiple times
byte recentlyHandledCommands[recentArraySize];
//Every message is given a unique ID. This is so other stations can keep track of them
byte curUniqueID = 0;
unsigned long lastStatusMillis;
unsigned long lastWeatherMillis;

byte stationsToRelayCommands[recentArraySize];
byte stationsToRelayWeather[recentArraySize]; //20 bytes

const int weatherMessageBufferSize = 255;
const int singleStationWeatherMessageSize = 7; // "W#ULDDD"
const int weatherRelayOffset = messageOffset + singleStationWeatherMessageSize; 
const int maxRelaySize = weatherMessageBufferSize - weatherRelayOffset - extraMessageBytes;
byte weatherMessageBuffer[weatherMessageBufferSize];
byte weatherRelayLength = 0;

#if DEBUG_Speed
bool speedDebugging = true;
void sendSpeedDebugMessage();
#endif
#if DEBUG_Direction
bool directionDebugging = true;
bool debugThisRound = true;
#endif
byte GetWindDirection(int);
void setupWindCounter();
void CountWind();
size_t createWeatherData(byte* msgBuffer);

void sendMessage(byte* msgBuffer, size_t& messageLength, size_t bufferLength);
