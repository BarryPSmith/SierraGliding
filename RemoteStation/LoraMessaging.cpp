//This files implements messaging using RadioLib and a SX1262
#include "LoraMessaging.h"
#include <Arduino.h>
#include "Callsign.h"
#include "lib/RadioLib/src/Radiolib.h"
#include "CSMA.h"
#include "ArduinoWeatherStation.h"
#include "PermanentStorage.h"

extern inline int16_t lora_check(const int16_t result, const __FlashStringHelper* msg);

bool delayRequired = false;
bool initMessagingRequired = false;

//Hardware pins:
Module mod(SX_SELECT, SX_DIO1, SX_BUSY);
SX1262 lora = &mod;
CSMAWrapper<SX1262> csma = &lora;

void sleepRadio()
{
  csma.setIdleState(IdleStates::Sleep);
}

void updateIdleState()
{
  //If we need to relay weather from anyone, we want to listen continuously.
  //We also need to listen continuously if we're set to relay commands: Although commands are sent with a long preamble, acknowledgement is not.

  uint16_t outboundPreambleLength;
  GET_PERMANENT_S(outboundPreambleLength);
  if (batteryMode == BatteryMode::DeepSleep)
    outboundPreambleLength *= 8;
  csma._senderPremableLength = outboundPreambleLength;
  IdleStates newState;
#if defined(MODEM)
  newState = IdleStates::ContinuousReceive;
#else
  bool continuousReceive = false;
  if (batteryMode == BatteryMode::Normal)
  {
    for (int i = 0; i < permanentArraySize; i++)
    {
      byte b;
      PermanentStorage::getBytes((void*)(offsetof(PermanentVariables, stationsToRelayWeather) + i), 1, &b);
      if (!b)
        PermanentStorage::getBytes((void*)(offsetof(PermanentVariables, stationsToRelayCommands) + i), 1, &b);
      if (b)
      {
        continuousReceive = true;
        break;
      }
    }
  }
  newState = continuousReceive ? IdleStates::ContinuousReceive : IdleStates::IntermittentReceive;
#endif
  LORA_CHECK(csma.setIdleState(newState));

  // TODO: We might want to put the unit to sleep entirely if the battery reaches a critical level.
  // (Although we should probably instead work on reducing current draw so it never reaches that level.)
}

void InitMessaging()
{

#if defined(SX_RESET) && SX_RESET >= 0
  pinMode(SX_RESET, OUTPUT);
  pinMode(SX_SELECT, OUTPUT);
#endif
#ifdef SX_SWITCH
  pinMode(SX_SWITCH, OUTPUT);
  digitalWrite(SX_SWITCH, HIGH);
  // If we really wanted to save power, we could turn the switch off when in sleep.
  // Best way to do that would be to update Radiolib
  // But the datasheet suggests the switch draws 20uA. We're a long way from that being important.
#endif

  SPI.usingInterrupt(digitalPinToInterrupt(SX_DIO1));
  
  unsigned long frequency_i;
  unsigned short bandwidth_i;
  GET_PERMANENT_S(frequency_i);
  GET_PERMANENT_S(bandwidth_i);
  if (frequency_i < MIN_FREQ || frequency_i > MAX_FREQ)
  {
    AWS_DEBUG_PRINTLN(F("Freq OOR. Using Default"));
    frequency_i = defaultFreq;
    bandwidth_i = defaultBw;
    SET_PERMANENT_S(frequency_i);
    SET_PERMANENT_S(bandwidth_i);
  }

  short txPower;
  byte spreadingFactor, csmaP;
  unsigned long csmaTimeslot;
  byte codingRate;
  GET_PERMANENT_S(txPower);
  GET_PERMANENT_S(spreadingFactor);
  GET_PERMANENT_S(csmaP);
  GET_PERMANENT_S(csmaTimeslot);
  GET_PERMANENT_S(codingRate);
  
  int state = ERR_UNKNOWN;
  
  SPI.begin();
  while (state != ERR_NONE)
  {
#if defined(SX_RESET)
    digitalWrite(SX_SELECT, HIGH);
    digitalWrite(SX_RESET, LOW);
    delay(100);
    digitalWrite(SX_RESET, HIGH);
    delay(100);
    while (state != ERR_NONE)
    {
      uint8_t status;
      lora.getStatus(&status);
#ifdef DETAILED_LORA_CHECK
      AWS_DEBUG_PRINT(F("Status: "));
      AWS_DEBUG_PRINTLN(status);
#endif
      state = LORA_CHECK(lora.standby(SX126X_STANDBY_RC));
      delay(1000);
    }
#endif
    

#ifdef USE_FP
    float frequency = frequency_i / 1000000.0;
    float bandwidth = bandwidth_i / 10.0;
    state = LORA_CHECK(lora.begin(
      frequency,
      bandwidth,
      spreadingFactor,
      codingRate,
      SX126X_SYNC_WORD_PRIVATE,
      txPower,
      140, //currentLimit
      8, //preambleLength
      1.7 //TCXO voltage
    ));
#else //!USE_FP
    state = LORA_CHECK(lora.begin_i(frequency_i,
      bandwidth_i,
      spreadingFactor,
      LORA_CR,
      SX126X_SYNC_WORD_PRIVATE,
      txPower,
      (uint8_t)(140 / 2.5), //current limit
      0xFFFF, //Preamble length
#ifdef SX_TCXOV_X10
      SX_TCXOV_X10
#else
      0 //TCXO voltage
#endif
    ));
#ifdef SX_TCXO_STARTUP_US
    LORA_CHECK(lora.setTCXO_i(SX_TCXOV_X10, SX_TCXO_STARTUP_US));
#endif
#endif //!USE_FP
    if (state != ERR_NONE)
    {
      SIGNALERROR(RADIO_STARTUP_ERROR, 50);
#ifdef DETAILED_LORA_CHECK
      AWS_DEBUG_PRINTLN(F("==========="));
#endif
      delay(500);
    }
  }
  LORA_CHECK(lora.setDio2AsRfSwitch());
  csma.initBuffer();
  csma.setPByte(csmaP);
  csma.setTimeSlot(csmaTimeslot);

  bool boostedRx;
  GET_PERMANENT_S(boostedRx);
  LORA_CHECK(lora.setRxGain(boostedRx));
}

bool handleMessageCommand(MessageSource& src, byte* desc)
{
  byte descByte;
  if (src.readByte(descByte))
    return false;
  else if (desc)
    *desc = descByte;

  auto state = lora.standby(SX126X_STANDBY_RC);
  if (state != ERR_NONE)
    return false;
  
  switch (descByte) {
  case 'P':
  {
    short txPower;
    if (src.read(txPower))
    {
      state = ERR_UNKNOWN;
      break;
    }
    state = LORA_CHECK(lora.setOutputPower(txPower));
    if (state == ERR_NONE)
      SET_PERMANENT_S(txPower);
    break;
  }
  case 'C':
  {
    byte csmaP;
    if (src.read(csmaP))
    {
      state = ERR_UNKNOWN;
      break;
    }
    csma.setPByte(csmaP);
    SET_PERMANENT_S(csmaP);
    state == ERR_NONE;
    break;
  }
  case 'T':
  {
    uint32_t csmaTimeslot;
    if (src.read(csmaTimeslot))
    {
      state = ERR_UNKNOWN;
      break;
    }
    csma.setTimeSlot(csmaTimeslot);
    SET_PERMANENT_S(csmaTimeslot);
    state = ERR_NONE;
    break;
  }
  case 'F':
  {
    uint32_t frequency_i;
    if (src.read(frequency_i))
    {
      state = ERR_UNKNOWN;
      break;
    }
#ifdef USE_FP
    float frequency = frequency_i / 1000000.0;
    state = LORA_CHECK(lora.setFrequency(frequency));
#else
    state = LORA_CHECK(lora.setFrequency_i(frequency_i));
#endif
    if (state == ERR_NONE)
      SET_PERMANENT_S(frequency_i);
    break;
  }
  case 'B':
  {
    uint16_t bandwidth_i;
    if (src.read(bandwidth_i))
    {
      state = ERR_UNKNOWN;
      break;
    }
#ifdef USE_FP
    float bandwidth = bandwidth_i / 10.0;
    state = LORA_CHECK(lora.setBandwidth(bandwidth));
#else
    state = LORA_CHECK(lora.setBandwidth_i(bandwidth_i));
#endif
    if (state == ERR_NONE)
      SET_PERMANENT_S(bandwidth_i);
    break;
  }
  case 'S':
  {
    byte spreadingFactor;
    if (src.read(spreadingFactor))
    {
      state = ERR_UNKNOWN;
      break;
    }
    state = LORA_CHECK(lora.setSpreadingFactor(spreadingFactor));
    if (state == ERR_NONE)
      SET_PERMANENT_S(spreadingFactor);
    break;
  }
  case 'O':
  {
    uint16_t outboundPreambleLength;

    if (src.read(outboundPreambleLength))
      state = ERR_UNKNOWN;
    else
    {
      state = ERR_NONE;
      SET_PERMANENT_S(outboundPreambleLength);
      csma._senderPremableLength = outboundPreambleLength;
    }
    break;
  }
  case 'I':
    uint16_t inboundPreambleLength;
    if (src.read(inboundPreambleLength))
      state = ERR_UNKNOWN;
    else
    {
      state = ERR_NONE;
      SET_PERMANENT_S(inboundPreambleLength);
    }
    break;
  case 'R':
    bool boostedRx;
    if (src.read(boostedRx))
      state = ERR_UNKNOWN;
    else
    {
      state = LORA_CHECK(lora.setRxGain(boostedRx));
      if (state == ERR_NONE)
        SET_PERMANENT_S(boostedRx);
    }
    break;
  case 'E':
    byte codingRate;
    if (src.read(codingRate))
      state = ERR_UNKNOWN;
    else
    {
      state = LORA_CHECK(lora.setCodingRate(codingRate));
      if (state == ERR_NONE)
        SET_PERMANENT_S(codingRate);
    }
    break;
  default:
    state = ERR_UNKNOWN;
  }
  //updateIdleState();
  LORA_CHECK(csma.enterIdleState());
  return state == ERR_NONE;
}

//Appends 8 bytes 
void appendMessageStatistics(MessageDestination& msg)
{
  msg.appendT(csma._crcErrorRate); //2 bytes
  msg.appendT(csma._droppedPacketRate); //2 bytes
  msg.appendT(csma._averageDelayTime / csma.averagingPeriod); //4 bytes
}

LoraMessageSource::LoraMessageSource() : MessageSource()
{}


void LoraMessageSource::doneWithMessage()
{
  csma.doneWithBuffer();
}

bool LoraMessageSource::beginMessage()
{
  _lastBeginError = LORA_CHECK(csma.dequeueMessage(&_incomingBuffer, &_length,
    &_timestamp
#ifdef GET_CRC_FAILURES
    , &_crcMismatch
#endif
  ));
  // The result of dequeueMessage is actually the result of readIfPossible
  // But regardless of what that function returned, we might have read 
  // something out of the buffer.
  // So we don't use state to determine whether to handle the message
  if (_length == 0)
    return false;
  
  if (s_discardCallsign)
  {
    if (_length <= 6)
      return false;
    _currentLocation = 6;
  }
  else
  {
    _currentLocation = 0;
  }

  return true;
}

MESSAGE_RESULT LoraMessageSource::endMessage()
{
  if (_currentLocation == 255)
    return MESSAGE_NOT_IN_MESSAGE;
  _currentLocation = 255;
  return MESSAGE_END;
}

MESSAGE_RESULT LoraMessageSource::readByte(byte& dest)
{
  if (_currentLocation == 255)
    return MESSAGE_NOT_IN_MESSAGE;
  if (_currentLocation >= _length)
  {
    _currentLocation = 255;
    return MESSAGE_END;
  }
  else
  {
    dest = _incomingBuffer[_currentLocation++];
    return MESSAGE_OK;
  }
}

MESSAGE_RESULT LoraMessageSource::accessBytes(byte** dest, byte bytesToRead)
{
  if (_currentLocation == 255)
    return MESSAGE_NOT_IN_MESSAGE;
  if (_currentLocation >= _length)
  {
    _currentLocation = 255;
    return MESSAGE_END;
  }
  byte endLocation = _currentLocation + bytesToRead;
  if (endLocation < _currentLocation || endLocation > _length)
  {
    return MESSAGE_BUFFER_OVERRUN;
  }
  *dest = _incomingBuffer + _currentLocation;
  _currentLocation += bytesToRead;
  return MESSAGE_OK;
}

MESSAGE_RESULT LoraMessageSource::seek(const byte newPosition)
{
  if (newPosition >= _length)
  {
    _currentLocation = 255;
    return MESSAGE_END;
  } 
  else
  {
    _currentLocation = newPosition;
    return MESSAGE_OK;
  }
}

LoraMessageDestination::LoraMessageDestination(bool isOutbound,
  byte* buffer, uint8_t bufferSize,
  byte type, byte uniqueID)
{
  _isOutbound = isOutbound;
  _outgoingBuffer = buffer;
  outgoingBufferSize = bufferSize;

  buffer[0] = 'X';
  buffer[1] = type;
  buffer[2] = stationID;
  buffer[3] = uniqueID;
  _currentLocation = 4;
}

#ifdef DEBUG
void messageDebugAction()
{
}
#endif