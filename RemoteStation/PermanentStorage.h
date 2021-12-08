#pragma once

#include <stddef.h>
#include <avr/eeprom.h>
#include "ArduinoWeatherStation.h"

#define GET_PERMANENT2(buffer, member) \
    PermanentStorage::getBytes((void*)offsetof(PermanentVariables, member), \
    sizeof(PermanentVariables::member), \
    buffer)

#define GET_PERMANENT_S(member)  do { \
    static_assert(sizeof(member) == sizeof(PermanentVariables::member), "GET_PERMANENT_S Incorrect size on " #member); \
    GET_PERMANENT2(&member, member); \
    } while (0)

#define GET_PERMANENT(member) do { \
    static_assert(sizeof(member) == sizeof(PermanentVariables::member), "GET_PERMANENT Incorrect size on " #member); \
    GET_PERMANENT2(member, member); \
    } while (0)

#define SET_PERMANENT2(buffer, member) \
    PermanentStorage::setBytes((void*)offsetof(PermanentVariables, member), \
    sizeof(PermanentVariables::member), \
    buffer)

#define SET_PERMANENT_S(member) do { \
    static_assert(sizeof(member) == sizeof(PermanentVariables::member), "SET_PEREMANENT_S Incorrect size on " #member); \
    SET_PERMANENT2(&member, member); \
    } while (0)

#define SET_PERMANENT(member) do { \
    static_assert(sizeof(member) == sizeof(PermanentVariables::member), "SET_PERMANENT Incorrect size on " #member); \
    SET_PERMANENT2(&member, member); \
    } while (0)

inline constexpr int permanentArraySize = 15;
inline constexpr int messageTypeArraySize = 7;

//Implements EEPROM storage of permanent setup variables.
//In its own class so we can change processors more easily
typedef struct PermanentVariables
{
  bool initialised; //1
  char stationID;
  unsigned long shortInterval, //4
                longInterval;  //4
  unsigned short batteryThreshold_mV; //2
  unsigned short batteryEmergencyThresh_mV; //2
  bool demandRelay; //1
  byte stationsToRelayCommands[permanentArraySize]; //20
  byte stationsToRelayWeather[permanentArraySize]; //20
   
  //LoRa parameters:
  uint32_t frequency_i;
  uint16_t bandwidth_i;
  short txPower; //1
  byte spreadingFactor; //1
  byte csmaP;
  unsigned long csmaTimeslot;
  unsigned short outboundPreambleLength;

  signed char tsOffset;
  byte tsGain;
  short wdCalib1, 
    wdCalib2;

  unsigned short chargeVoltage_mV;
  unsigned short chargeResponseRate;
  unsigned short safeFreezingChargeLevel_mV;
  byte safeFreezingPwm;

  byte messageTypesToRecord[messageTypeArraySize]; // V2.2 had a CRC here
  bool recordNonRelayedMessages;
  unsigned short inboundPreambleLength; //2.4 had a CRC here
  bool boostedRx;
  bool stasisRequested;
  short crc;
} PermanentVariables;

class PermanentStorage
{
public:
  static void initialise();

  static unsigned short calcCRC(size_t max);
  static inline bool checkCRC(size_t max)
  {
    auto crc = calcCRC(max);
    return crc == 0;
  }
  static void setCRC();
#ifdef DEBUG
  static inline void dump()
  {
    byte mem[255];
    eeprom_read_block(mem, (void*)0, 255);
    for (int i = 0; i < 255; i++)
    {
      AWS_DEBUG_PRINT(i);
      AWS_DEBUG_PRINT(F("\t:\t"));
      PRINT_VARIABLE_HEX(mem[i]);
    }
  }
#endif
  static inline void getBytes(const void* address, size_t size, void* buffer)
  {
    eeprom_read_block(buffer, address, size);
  }
  static inline void setBytes(void* address, size_t size, const void* buffer)
  {
    eeprom_write_block(buffer, address, size);
    if (_initialised)
    {
      setCRC();
    }
  }

private:
  static bool _initialised;
};