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

const int permanentArraySize = 20;

//Implements EEPROM storage of permanent setup variables.
//In its own class so we can change processors more easily
typedef struct PermanentVariables
{
  bool initialised; //1
  unsigned long shortInterval, //4
                longInterval;  //4
  unsigned short batteryThreshold_mV; //2
  unsigned short batteryUpperThresh_mV; //2
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

  short crc; //2
} PermanentVariables;

class PermanentStorage
{
public:
  static void initialise();

  static unsigned short calcCRC(bool includeCRC);
  static inline bool checkCRC()
  {
    auto crc = calcCRC(true);
    return crc == 0;
  }
  static void setCRC();

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