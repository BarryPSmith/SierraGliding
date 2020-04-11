#include "PermanentStorage.h"
#include "ArduinoWeatherStation.h"
#include <util/crc16.h>
//#define DEBUG_PARAMETERS

char stationID = defaultStationID;

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

  // If possible we use the stored stationID regardless of initialised or CRC check - this helps keep stationID constant across upgrades..
  GET_PERMANENT_S(stationID);
  if (stationID < 1 || stationID & 0x80)
  {
    AWS_DEBUG_PRINTLN("Using default station ID");
    stationID = defaultStationID;
    SET_PERMANENT_S(stationID);
  }

  if (!initialised || !checkCRC())
  {
    AWS_DEBUG_PRINTLN(F("Initialising Default Parameters"));

    PermanentVariables vars
    {
    .initialised = true,
    .stationID = stationID,
    .shortInterval = 4000,// - 90 * (stationID - '1');
    .longInterval = 4000,// - 90 * (stationID - '1'); //longInterval == shortInterval because it turns out the transmit is negligble draw.
    .batteryThreshold_mV = 4000,
    .batteryUpperThresh_mV = 4700,
    .demandRelay = false,
    .stationsToRelayCommands = { 0 },
    .stationsToRelayWeather = { 0 },

    .frequency_i = 425000000,
    .bandwidth_i = 625,
    .txPower = -9,
    .spreadingFactor = 5,
    .csmaP = 100, //40% chance to transmit
    .csmaTimeslot = 4000, // 4ms
    .outboundPreambleLength = 128, // allow end nodes to spend most of their time asleep.

    // These default tsOffset / tsGain values correspond to the datasheet example values on page 215.
    .tsOffset = -75,
    .tsGain = 164,
    .wdCalibMin = 0,
    .wdCalibMax = 1023,

    .chargeVoltage_mV = 4000,
    .chargeResponseRate = 256,
    .safeFreezingChargeLevel_mV = 3700,
    .safeFreezingPwm = 255
    };
    setBytes(0, sizeof(vars), &vars);

    
    setCRC();
  }
  else
  {
#if DEBUG && defined(DEBUG_PARAMETERS)
    AWS_DEBUG_PRINTLN(F("Using saved parameters"));

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
    unsigned short chargeVoltage_mV, chargeResponseRate, safeFreezingChargeLevel_mV;
    byte safeFreezingPwm;

    GET_PERMANENT_S(shortInterval);
    GET_PERMANENT_S(longInterval);
    GET_PERMANENT_S(batteryThreshold_mV);
    GET_PERMANENT_S(batteryUpperThresh_mV);
    GET_PERMANENT_S(tsOffset);
    GET_PERMANENT_S(tsGain);
    GET_PERMANENT_S(initialised);

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

    GET_PERMANENT_S(chargeVoltage_mV);
    GET_PERMANENT_S(chargeResponseRate);
    GET_PERMANENT_S(safeFreezingChargeLevel_mV);
    GET_PERMANENT_S(safeFreezingPwm);
    PRINT_VARIABLE(chargeVoltage_mV);
    PRINT_VARIABLE(chargeResponseRate);
    PRINT_VARIABLE(safeFreezingChargeLevel_mV);
    PRINT_VARIABLE(safeFreezingPwm);
#endif
  }
  _initialised = true;
}