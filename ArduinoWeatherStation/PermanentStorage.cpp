#include "PermanentStorage.h"
#include "ArduinoWeatherStation.h"
#include <util/crc16.h>

bool PermanentStorage::_initialised = false;

//Just copied this out of the example documentation 
unsigned short PermanentStorage::calcCRC(bool includeCRC)
{
  unsigned short crc = 0xffff;

  size_t max = sizeof(PermanentVariables);
  if (!includeCRC)
    max -= 2;

	for (size_t i = 0; i < max; i++)
	  crc = _crc_ccitt_update(crc, eeprom_read_byte((byte*)i));

	return crc;
}

void PermanentStorage::setCRC()
{
  short crc = calcCRC(false);
  eeprom_write_block(&crc, (void*)offsetof(PermanentVariables, crc), sizeof(short));
}

#define PRINT_VARIABLE(a) do { \
  AWS_DEBUG_PRINT(F(#a ": ")); \
  AWS_DEBUG_PRINTLN(a); \
  } while (0)
void PermanentStorage::initialise()
{
  bool initialised;
  GET_PERMANENT_S(initialised);
  AWS_DEBUG_PRINT(F("Initialised value: "));
  AWS_DEBUG_PRINTLN(initialised);
  if (!initialised || !checkCRC())
  {
    AWS_DEBUG_PRINTLN(F("Initialising Default Parameters"));
    initialised = true;
    const long shortInterval = 4000 - 90 * (stationID - '1');
    const long longInterval = 4000 - 90 * (stationID - '1'); //longInterval == shortInterval because it turns out the transmit is negligble draw.
    const float batteryThreshold = 12.0;
    const bool demandRelay = false;
    const byte emptyBuffer[permanentArraySize] = { 0 };

    const float frequency = 425;
    const short txPower = 10;
    const float bandwidth = 62.5;
    const byte spreadingFactor = 7; //Maybe we can communicate with the RF96?
    const byte csmaP = 100; //40% chance to transmit
    const unsigned long csmaTimeslot = 4000; // 4ms

    SET_PERMANENT_S(shortInterval);
    SET_PERMANENT_S(longInterval);
    SET_PERMANENT_S(batteryThreshold);
    SET_PERMANENT2(emptyBuffer, stationsToRelayCommands);
    SET_PERMANENT2(emptyBuffer, stationsToRelayWeather);
    SET_PERMANENT_S(initialised);

    SET_PERMANENT_S(frequency);
    SET_PERMANENT_S(txPower);
    SET_PERMANENT_S(bandwidth);
    SET_PERMANENT_S(spreadingFactor);
    SET_PERMANENT_S(csmaTimeslot);
    setCRC();
  }
#if DEBUG
  else
  {
    long shortInterval = 4000 - 90 * (stationID - '1');
    long longInterval = 4000 - 90 * (stationID - '1'); //longInterval == shortInterval because it turns out the transmit is negligble draw.
    float batteryThreshold = 12.0;
    bool demandRelay = false;
    byte buffer[permanentArraySize] = { 0 };
    unsigned short crc;
    GET_PERMANENT_S(shortInterval);
    GET_PERMANENT_S(longInterval);
    GET_PERMANENT_S(batteryThreshold);
    GET_PERMANENT2(buffer, stationsToRelayWeather);
    GET_PERMANENT_S(initialised);

    AWS_DEBUG_PRINTLN(F("Using saved parameters"));
    PRINT_VARIABLE(initialised);
    PRINT_VARIABLE(shortInterval);
    PRINT_VARIABLE(longInterval);
    PRINT_VARIABLE(batteryThreshold);
    GET_PERMANENT2(buffer, stationsToRelayCommands);
    AWS_DEBUG_PRINT(F("stationsToRelayCommands:"));
    for (int i = 0; i < permanentArraySize; i++)
    {
      AWS_DEBUG_PRINT(buffer[i]);
      AWS_DEBUG_PRINT(F(", "));
    }
    AWS_DEBUG_PRINTLN();
    GET_PERMANENT2(buffer, stationsToRelayWeather);
    AWS_DEBUG_PRINT(F("stationsToRelayWeather:"));
    for (int i = 0; i < permanentArraySize; i++)
    {
      AWS_DEBUG_PRINT(buffer[i]);
      AWS_DEBUG_PRINT(F(", "));
    }
    AWS_DEBUG_PRINTLN();
    GET_PERMANENT_S(crc);
    AWS_DEBUG_PRINT(F("crc: "));
    AWS_DEBUG_PRINTLN(crc, HEX);

    float frequency, bandwidth;
    short txPower;
    byte spreadingFactor, csmaP;
    unsigned long csmaTimeslot;
    GET_PERMANENT_S(frequency);
    GET_PERMANENT_S(bandwidth);
    GET_PERMANENT_S(txPower);
    GET_PERMANENT_S(spreadingFactor);
    GET_PERMANENT_S(csmaP);
    GET_PERMANENT_S(csmaTimeslot);
    PRINT_VARIABLE(frequency);
    PRINT_VARIABLE(bandwidth);
    PRINT_VARIABLE(txPower);
    PRINT_VARIABLE(spreadingFactor);
    PRINT_VARIABLE(csmaP);
    PRINT_VARIABLE(csmaTimeslot);
  }
#endif
  _initialised = true;
}