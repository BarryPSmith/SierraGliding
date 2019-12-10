//This files implements messaging using RadioLib and a SX1262
#include "LoraMessaging.h"
#include <Arduino.h>
#include "Callsign.h"
#include "lib/RadioLib/src/Radiolib.h"
#include "CSMA.h"
#include "ArduinoWeatherStation.h"
#include "PermanentStorage.h"

//I hope the compiler is smart enough to optimise away all of the 'msg's.
static inline int16_t lora_check(const int16_t result, const __FlashStringHelper* msg) 
{
  if (result != ERR_NONE && result != NO_PACKET_AVAILABLE)
  {
    AWS_DEBUG_PRINT(msg);
    AWS_DEBUG_PRINTLN(result);
  }
  return result;
}
#define LORA_CHECK(A) lora_check(A, F("FAILED " #A ": "))

//Hardware pins:
Module mod(SX_SELECT, SX_DIO1, SX_BUSY);

//These use 100 bytes (Seems alot)
RADIO_TYPE lora = &mod; //28 bytes
bool initialised = false; //72 bytes

#ifdef MOTEINO_96
static volatile bool packetWaiting;
void rxDone() { packetWaiting = true; }
#endif

void updateIdleState()
{
#ifndef MOTEINO_96
  //If we need to relay weather from anyone, we want to listen continuously.
  //We don't need to use continuous listen for commands because they're sent with a long preamble.
  byte stationsToRelayWeather[permanentArraySize];
  GET_PERMANENT(stationsToRelayWeather);
  
  uint16_t outboundPreambleLength;
  GET_PERMANENT_S(outboundPreambleLength);
  lora._senderPremableLength = outboundPreambleLength;
  IdleStates newState;
#if defined(MODEM)
  newState = IdleStates::ContinuousReceive;
#else
  newState = stationsToRelayWeather[0] ? IdleStates::ContinuousReceive : IdleStates::IntermittentReceive;
#endif
  LORA_CHECK(lora.setIdleState(newState));

  // TODO: We might want to put the unit to sleep entirely if the battery reaches a critical level.
  // (Although we should probably instead work on reducing current draw so it never reaches that level.)
#else //MOTEINO_96
  lora.startReceive();
#endif
}

void InitMessaging()
{
  if (initialised)
    return;
  
  MessageDestination::s_prependCallsign = false;
  MessageSource::s_discardCallsign = false;


  SPI.usingInterrupt(digitalPinToInterrupt(SX_DIO1));
  
  AWS_DEBUG_PRINT(F("Mod size: "));
  AWS_DEBUG_PRINTLN(sizeof(mod));
  AWS_DEBUG_PRINT(F("Lora size: "));
  AWS_DEBUG_PRINTLN(sizeof(lora));
  AWS_DEBUG_PRINT(F("Radio type: "));
  AWS_DEBUG_PRINTLN(F(XSTR(RADIO_TYPE)));
  // Might have to tweak these parameters to get long distance communications.
  // but testing suggests we can transmit 23km at -10 dBm. at +10 dBm we've got a fair bit of headroom.
  unsigned long frequency_i;
  unsigned short bandwidth_i;
  GET_PERMANENT_S(frequency_i);
  GET_PERMANENT_S(bandwidth_i);
  short txPower;
  byte spreadingFactor, csmaP;
  unsigned long csmaTimeslot;
  GET_PERMANENT_S(txPower);
  GET_PERMANENT_S(spreadingFactor);
  GET_PERMANENT_S(csmaP);
  GET_PERMANENT_S(csmaTimeslot);
#ifdef MOTEINO_96
  float frequency = frequency_i / 1.0E6;
  float bandwidth = bandwidth_i / 10.0;
  LORA_CHECK(lora.begin(
    frequency, 
    bandwidth, 
    spreadingFactor,
    LORA_CR, 
    SX127X_SYNC_WORD,
    txPower,
    100, //currentLimit
    8, //preambleLength
    0 //gain
    ));
  lora.setDio0Action(rxDone);
  LORA_CHECK(lora.startReceive(RX_TIMEOUT));
#else //!MOTEINO_96
#ifdef USE_FP
  float frequency = frequency_i / 1000000.0;
  float bandwidth = bandwidth_i / 10.0;
  LORA_CHECK(lora.begin(
    frequency,
    bandwidth,
    spreadingFactor,
    LORA_CR,
    SX126X_SYNC_WORD_PRIVATE,
    txPower,
    140, //currentLimit
    8, //preambleLength
    0 //TCXO voltage
  ));
#else //!USE_FP
  LORA_CHECK(lora.begin_i(frequency_i,
    bandwidth_i,
    spreadingFactor,
    LORA_CR,
    SX126X_SYNC_WORD_PRIVATE,
    txPower,
    (uint8_t)(140 / 2.5), //current limit
    0xFFFF, //Preamble length
    0)); //TCXO voltage
#endif //!USE_FP
  LORA_CHECK(lora.setDio2AsRfSwitch());
  lora.enableIsChannelBusy();
  lora.initBuffer();
  lora.setPByte(csmaP);
  lora.setTimeSlot(csmaTimeslot);
  updateIdleState();
  //LORA_CHECK(lora.setIdleState(IdleStates::ContinuousReceive));
#endif //!MOTEINO_96

  
  initialised = true;
}

bool HandleMessageCommand(MessageSource& src)
{
#ifndef NO_COMMANDS
  byte descByte;
  if (src.readByte(descByte))
    return false;

  auto state = lora.standby();
  if (state != ERR_NONE)
    return false;
  
  switch(descByte) {
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
      lora.setPByte(csmaP);
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
      lora.setTimeSlot(csmaTimeslot);
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
      }
    }
    break;
  default:
    state = ERR_UNKNOWN;
  }
  LORA_CHECK(lora.enterIdleState());
  return state == ERR_NONE;
#else // !MOTEINO_96
  return false;
#endif 
}

void appendMessageStatistics(MessageDestination& msg)
{
#ifndef MOTEINO_96
  msg.appendT(lora._crcErrorRate);
  msg.appendT(lora._droppedPacketRate);
  msg.appendT(lora._averageDelayTime);
#endif
}

LoraMessageSource::LoraMessageSource() : MessageSource()
{}

LoraMessageDestination::~LoraMessageDestination()
{
  finishAndSend();
}

LoraMessageDestination::LoraMessageDestination(bool isOutbound)
{
  if (s_prependCallsign)
    append((byte*)callSign, 6);
  else
    appendByte('X');
  
  // TODO: Check if this interrupts a currently-in-progress receive.
  _isOutbound = isOutbound;
}

void LoraMessageDestination::abort()
{
  m_iCurrentLocation = -1;
}

MESSAGE_RESULT LoraMessageDestination::finishAndSend()
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;

  uint16_t preambleLength = 8;
  if (_isOutbound)
  {
    uint16_t outboundPreambleLength;
    GET_PERMANENT_S(outboundPreambleLength);
    preambleLength = outboundPreambleLength;
  }

  
#ifdef MOTEINO_96
  LORA_CHECK(lora.setPreambleLength(preambleLength));
  auto state = LORA_CHECK(lora.transmit(_outgoingBuffer, m_iCurrentLocation));
  LORA_CHECK(lora.setPreambleLength(0xFFFF));
  LORA_CHECK(lora.startReceive());
#else
  auto state = LORA_CHECK(lora.transmit(_outgoingBuffer, m_iCurrentLocation, preambleLength));
#endif
  bool ret = state == ERR_NONE;

  m_iCurrentLocation = -1;
  if (ret)
    return MESSAGE_OK;
  else
    return MESSAGE_ERROR;
  
}

MESSAGE_RESULT LoraMessageDestination::appendByte(const byte data)
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  else if (m_iCurrentLocation >= maxPacketSize)
  {
    return MESSAGE_BUFFER_OVERRUN;
  }

  _outgoingBuffer[m_iCurrentLocation++] = data;

  return MESSAGE_OK;
}

bool LoraMessageSource::beginMessage()
{
  #if defined(MOTEINO_96)
  if (!packetWaiting)
    return false;
  packetWaiting = false;

  _incomingMessageSize = lora.getPacketLength(false);
  auto state = LORA_CHECK(lora.readData(_incomingBuffer, _incomingMessageSize));
  LORA_CHECK(lora.startReceive());
  if (state != ERR_NONE)
    return false;
  #else
  LORA_CHECK(lora.dequeueMessage(&_incomingBuffer, &_incomingMessageSize));
  //we don't use state to determine whether to handle the message
  if (_incomingBuffer == 0)
    return false;
  #endif

  
  
  if (s_discardCallsign)
  {
    if (_incomingMessageSize <= 6)
      return false;
    m_iCurrentLocation = 6;
  }
  else
  {
    m_iCurrentLocation = 0;
  }

  return true;
}

MESSAGE_RESULT LoraMessageSource::endMessage()
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  m_iCurrentLocation = -1;
  return MESSAGE_END;
}

MESSAGE_RESULT LoraMessageSource::readByte(byte& dest)
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  if (m_iCurrentLocation >= _incomingMessageSize)
  {
    m_iCurrentLocation = -1;
    return MESSAGE_END;
  }
  else
  {
    dest = _incomingBuffer[m_iCurrentLocation++];
    return MESSAGE_OK;
  }
}

#ifdef DEBUG
void messageDebugAction()
{
#if 0
  AWS_DEBUG_PRINT(millis());
  AWS_DEBUG_PRINT('\t');
  AWS_DEBUG_PRINT(lora._lastPreambleDetMicros);
  AWS_DEBUG_PRINT('\t');
  AWS_DEBUG_PRINT(lora._curStatus);
  AWS_DEBUG_PRINT('\t');
  AWS_DEBUG_PRINT(lora._maybeReceiving);
  AWS_DEBUG_PRINT('\t');
  AWS_DEBUG_PRINT(lora._entryMicros);
  AWS_DEBUG_PRINT('\t');
  AWS_DEBUG_PRINT(lora._isrState);
  AWS_DEBUG_PRINT('\t');
  AWS_DEBUG_PRINT(lora._isrIrqStatus);
  AWS_DEBUG_PRINTLN();
#endif
}
#endif