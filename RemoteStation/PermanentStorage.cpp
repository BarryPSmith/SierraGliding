#include "PermanentStorage.h"
#include "ArduinoWeatherStation.h"
#include <util/crc16.h>

#ifdef DEBUG_PARAMETERS
#define PARAMETER_PRINTLN AWS_DEBUG_PRINTLN
#define PARAMETER_PRINTVAR PRINT_VARIABLE
#define PARAMETER_DEBUG AWS_DEBUG
#else
#define PARAMETER_PRINTLN(...) do {} while (0)
#define PARAMETER_PRINTVAR(...) do {} while (0)
#define PARAMETER_DEBUG(...) do {} while (0)
#endif

char stationID = defaultStationID;

bool PermanentStorage::_initialised = false;

//Just copied this out of the example documentation 
unsigned short PermanentStorage::calcCRC(size_t max)
{
  unsigned short crc = 0xffff;

	for (size_t i = 0; i < max; i++)
	  crc = _crc_ccitt_update(crc, eeprom_read_byte((byte*)i));

	return crc;
}

void PermanentStorage::setCRC()
{
  short crc = calcCRC(sizeof(PermanentVariables) - 2);
  eeprom_write_block(&crc, (void*)offsetof(PermanentVariables, crc), sizeof(short));
}

const PermanentVariables defaultVars PROGMEM =
{
  .initialised = true,
  .stationID = defaultStationID,
  .shortInterval = 4000,// - 90 * (stationID - '1');
  .longInterval = 30000,// - 90 * (stationID - '1'); //longInterval == shortInterval because it turns out the transmit is negligble draw.
  .batteryThreshold_mV = 3800,
  .batteryEmergencyThresh_mV = 3700,
  .demandRelay = false,
  //.stationsToRelayCommands = { 0 },
  //.stationsToRelayWeather = { 0 },

  .frequency_i = defaultFreq,
  .bandwidth_i = defaultBw,
  .txPower = -9,
  .spreadingFactor = 5,
  .csmaP = 100, //40% chance to transmit
  .csmaTimeslot = 10000, // 10ms
  .outboundPreambleLength = 384, // allow end nodes to spend most of their time asleep.
        

  // These default tsOffset / tsGain values correspond to the datasheet example values on page 215.
#ifdef ATMEGA328PB
  .tsOffset = 32,
  .tsGain = 128,
#else
  .tsOffset = -75,
  .tsGain = 164,
#endif
  .wdCalib1 = 0,
  .wdCalib2 = 1023,

  .chargeVoltage_mV = 4050,
  .chargeResponseRate = 40,
  .safeFreezingChargeLevel_mV = 3750,
  .safeFreezingPwm = 85,
  .recordNonRelayedMessages = false,
  .inboundPreambleLength = 8,
#ifdef MODEM
  .boostedRx = true,
#else
  .boostedRx = false
#endif
};

void PermanentStorage::initialise()
{
  bool initialised;
  GET_PERMANENT_S(initialised);

  // If possible we use the stored stationID regardless of initialised or CRC check - this helps keep stationID constant across upgrades..
  GET_PERMANENT_S(stationID);
  if (stationID < 1 || stationID & 0x80)
  {
    PARAMETER_PRINTLN(F("Using default station ID"));
    stationID = defaultStationID;
    initialised = false;
    //SET_PERMANENT_S(stationID);
  }
  bool completeCrc = false;
  if (initialised)
  {
    //When upgrading from 2.4 to 2.5, we don't want to clear the entire memory. Just set the inbound preamble length and recalculate the CRC:
    if (checkCRC(sizeof(PermanentVariables) - 3))
    {
      unsigned short inboundPreambleLength = 8;
      SET_PERMANENT_S(inboundPreambleLength);
      bool boostedRx =
#ifdef MODEM
        true;
#else
        false;
#endif
      SET_PERMANENT_S(boostedRx);
      setCRC();
      completeCrc = true;
    }
    completeCrc = checkCRC(sizeof(PermanentVariables));
  }
  if (completeCrc)
  {
    #if DEBUG && defined(DEBUG_PARAMETERS)
    AWS_DEBUG_PRINTLN(F("Using saved parameters"));

    PermanentVariables vars;
    getBytes(0, sizeof(vars), &vars);
    PRINT_VARIABLE(vars.frequency_i);
    PRINT_VARIABLE(vars.bandwidth_i);
    PRINT_VARIABLE(vars.txPower);
    PRINT_VARIABLE(vars.spreadingFactor);
    PRINT_VARIABLE(vars.csmaP);
    PRINT_VARIABLE(vars.csmaTimeslot);
    PRINT_VARIABLE(vars.outboundPreambleLength);
    PRINT_VARIABLE(vars.wdCalib1);
    PRINT_VARIABLE(vars.wdCalib2);
    PRINT_VARIABLE(vars.chargeVoltage_mV);
    PRINT_VARIABLE(vars.chargeResponseRate);
    PRINT_VARIABLE(vars.safeFreezingChargeLevel_mV);
    PRINT_VARIABLE(vars.safeFreezingPwm);

    PRINT_VARIABLE(vars.stationID);
    PRINT_VARIABLE(vars.initialised);
    PRINT_VARIABLE(vars.shortInterval);
    PRINT_VARIABLE(vars.longInterval);
    PRINT_VARIABLE(vars.batteryThreshold_mV);
    PRINT_VARIABLE(vars.batteryEmergencyThresh_mV);
    PRINT_VARIABLE(vars.tsOffset);
    PRINT_VARIABLE(vars.tsGain);
    PRINT_VARIABLE(vars.boostedRx);
    #endif
  }
  else
  {
    PARAMETER_PRINTLN(F("Initialising Default Parameters"));
    PermanentVariables vars;
    memcpy_P(&vars, &defaultVars, sizeof(vars));
    vars.stationID = stationID;
    setBytes(0, sizeof(vars), &vars);
    byte messageTypesToRecord[messageTypeArraySize] = { 'S', 'K', 0 };
    SET_PERMANENT(messageTypesToRecord);
    setCRC();
  }

  _initialised = true;
}