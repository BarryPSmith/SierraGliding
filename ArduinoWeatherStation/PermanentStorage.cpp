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

void PermanentStorage::initialise()
{
  bool initialised;
  GET_PERMANENT_S(initialised);
  if (!initialised || !checkCRC())
  {
    AWS_DEBUG_PRINTLN(F("Initialising Default Parameters"));
    initialised = true;
    const long shortInterval = 4000;// - 90 * (stationID - '1');
    const long longInterval = 4000;// - 90 * (stationID - '1'); //longInterval == shortInterval because it turns out the transmit is negligble draw.
    const unsigned short batteryThreshold_mV = 4000;
    const unsigned short batteryUpperThresh_mV = 4700;
    // These default tsOffset / tsGain values correspond to the datasheet example values on page 215.
    signed char tsOffset = -75;
    byte tsGain = 164;
    int wdCalibMin = 0;
    int wdCalibMax = 1023;
    const bool demandRelay = false;
    const byte emptyBuffer[permanentArraySize] = { 0 };

    const uint32_t frequency_i = 425000000;
    const uint16_t bandwidth_i = 625;
    const short txPower = -9;
    const byte spreadingFactor = 5; //Maybe we can communicate with the RF96?
    const byte csmaP = 100; //40% chance to transmit
    const unsigned long csmaTimeslot = 4000; // 4ms
    const unsigned short outboundPreambleLength = 128; // allow end nodes to spend most of their time asleep.

    SET_PERMANENT_S(stationID);
    SET_PERMANENT_S(shortInterval);
    SET_PERMANENT_S(longInterval);
    SET_PERMANENT_S(batteryThreshold_mV);
    SET_PERMANENT_S(batteryUpperThresh_mV);
    SET_PERMANENT2(emptyBuffer, stationsToRelayCommands);
    SET_PERMANENT2(emptyBuffer, stationsToRelayWeather);
    SET_PERMANENT_S(initialised);

    SET_PERMANENT_S(frequency_i);
    SET_PERMANENT_S(txPower);
    SET_PERMANENT_S(bandwidth_i);
    SET_PERMANENT_S(spreadingFactor);
    SET_PERMANENT_S(csmaTimeslot);
    SET_PERMANENT_S(outboundPreambleLength);
    
    SET_PERMANENT_S(tsOffset);
    SET_PERMANENT_S(tsGain);
    SET_PERMANENT_S(wdCalibMin);
    SET_PERMANENT_S(wdCalibMax);
    setCRC();
  }
  else
  {
    GET_PERMANENT_S(stationID);
#if DEBUG
    long shortInterval;
    long longInterval;
    unsigned short batteryThreshold_mV,
      batteryUpperThresh_mV;
    bool demandRelay = false;
    byte buffer[permanentArraySize] = { 0 };
    unsigned short crc;
    signed char tsOffset;
    byte tsGain;
    int wdCalibMin, wdCalibMax;

    GET_PERMANENT_S(shortInterval);
    GET_PERMANENT_S(longInterval);
    GET_PERMANENT_S(batteryThreshold_mV);
    GET_PERMANENT_S(batteryUpperThresh_mV);
    GET_PERMANENT_S(tsOffset);
    GET_PERMANENT_S(tsGain);
    GET_PERMANENT2(buffer, stationsToRelayWeather);
    GET_PERMANENT_S(initialised);

    AWS_DEBUG_PRINTLN(F("Using saved parameters"));
    PRINT_VARIABLE(stationID);
    PRINT_VARIABLE(initialised);
    PRINT_VARIABLE(shortInterval);
    PRINT_VARIABLE(longInterval);
    PRINT_VARIABLE(batteryThreshold_mV);
    PRINT_VARIABLE(batteryUpperThresh_mV);
    PRINT_VARIABLE(tsOffset);
    PRINT_VARIABLE(tsGain);
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

    uint32_t frequency_i;
    uint16_t bandwidth_i;
    short txPower;
    byte spreadingFactor, csmaP;
    unsigned long csmaTimeslot;
    unsigned short outboundPreambleLength;
    GET_PERMANENT_S(frequency_i);
    GET_PERMANENT_S(bandwidth_i);
    GET_PERMANENT_S(txPower);
    GET_PERMANENT_S(spreadingFactor);
    GET_PERMANENT_S(csmaP);
    GET_PERMANENT_S(csmaTimeslot);
    GET_PERMANENT_S(outboundPreambleLength);
    PRINT_VARIABLE(frequency_i);
    PRINT_VARIABLE(bandwidth_i);
    PRINT_VARIABLE(txPower);
    PRINT_VARIABLE(spreadingFactor);
    PRINT_VARIABLE(csmaP);
    PRINT_VARIABLE(csmaTimeslot);
    PRINT_VARIABLE(outboundPreambleLength);

    GET_PERMANENT_S(wdCalibMin);
    GET_PERMANENT_S(wdCalibMax);
    PRINT_VARIABLE(wdCalibMin);
    PRINT_VARIABLE(wdCalibMax);
#endif
  }
  _initialised = true;
}