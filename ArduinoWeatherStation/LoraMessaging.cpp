//This files implements messaging using RadioLib and a SX1262
#include "LoraMessaging.h"
#include <Arduino.h>
#include "Callsign.h"
#include "lib/RadioLib/src/Radiolib.h"
#include "CSMA.h"
#include "ArduinoWeatherStation.h"
#include "PermanentStorage.h"

extern inline int16_t lora_check(const int16_t result, const __FlashStringHelper* msg);

//Hardware pins:
Module mod(SX_SELECT, SX_DIO1, SX_BUSY);
#ifndef MOTEINO_96
SX1262 lora = &mod;
CSMAWrapper<SX1262> csma = &lora;
#else
SX1278 lora = &mod;
#endif

#ifdef MOTEINO_96
static volatile bool packetWaiting;
void rxDone() { packetWaiting = true; }
#endif

void updateIdleState()
{
#ifndef MOTEINO_96
  //If we need to relay weather from anyone, we want to listen continuously.
  //We also need to listen continuously if we're set to relay commands: Although commands are sent with a long preamble, acknowledgement is not.
  byte stationsToRelayWeather[permanentArraySize];
  GET_PERMANENT(stationsToRelayWeather);

  byte stationsToRelayCommands[permanentArraySize];
  GET_PERMANENT(stationsToRelayCommands);
  
  uint16_t outboundPreambleLength;
  GET_PERMANENT_S(outboundPreambleLength);
  csma._senderPremableLength = outboundPreambleLength;
  IdleStates newState;
#if defined(MODEM)
  newState = IdleStates::ContinuousReceive;
#else
  bool mustRelay = stationsToRelayWeather[0] || stationsToRelayCommands[0];
  newState = mustRelay ? IdleStates::ContinuousReceive : IdleStates::IntermittentReceive;
#endif
  LORA_CHECK(csma.setIdleState(newState));

  // TODO: We might want to put the unit to sleep entirely if the battery reaches a critical level.
  // (Although we should probably instead work on reducing current draw so it never reaches that level.)
#else //MOTEINO_96
  lora.startReceive();
#endif
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

  
  MessageDestination::s_prependCallsign = false;
  MessageSource::s_discardCallsign = false;


  SPI.usingInterrupt(digitalPinToInterrupt(SX_DIO1));
  
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
      AWS_DEBUG_PRINT(F("Status: "));
      AWS_DEBUG_PRINTLN(status);
      state = LORA_CHECK(lora.standby(SX126X_STANDBY_RC));
      delay(1000);
    }
#endif
    

#ifdef MOTEINO_96
    float frequency = frequency_i / 1.0E6;
    float bandwidth = bandwidth_i / 10.0;

    state = LORA_CHECK(lora.begin(
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
    LORA_CHECK(lora.startReceive());
  }
#else //!MOTEINO_96
#ifdef USE_FP
    float frequency = frequency_i / 1000000.0;
    float bandwidth = bandwidth_i / 10.0;
    state = LORA_CHECK(lora.begin(
      frequency,
      bandwidth,
      spreadingFactor,
      LORA_CR,
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
#endif //!USE_FP
    if (state != ERR_NONE)
    {
      SIGNALERROR(4, 150);
      AWS_DEBUG_PRINTLN(F("==========="));
      delay(500);
    }
  }
  LORA_CHECK(lora.setDio2AsRfSwitch());
  csma.initBuffer();
  csma.setPByte(csmaP);
  csma.setTimeSlot(csmaTimeslot);
  updateIdleState();
  //LORA_CHECK(lora.setIdleState(IdleStates::ContinuousReceive));
#endif //!MOTEINO_96
}

bool HandleMessageCommand(MessageSource& src)
{
#ifndef NO_COMMANDS
  byte descByte;
  if (src.readByte(descByte))
    return false;

  auto state = lora.standby(SX126X_STANDBY_RC);
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
      }
    }
    break;
  default:
    state = ERR_UNKNOWN;
  }
  updateIdleState();
  //LORA_CHECK(csma.enterIdleState());
  return state == ERR_NONE;
#else // !MOTEINO_96
  return false;
#endif 
}

void appendMessageStatistics(MessageDestination& msg)
{
  msg.appendT(csma._crcErrorRate);
  msg.appendT(csma._droppedPacketRate);
  msg.appendT(csma._averageDelayTime);
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
  _currentLocation = -1;
}

MESSAGE_RESULT LoraMessageDestination::finishAndSend()
{
  if (_currentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;

  uint16_t preambleLength = 8;
  if (_isOutbound)
  {
    uint16_t outboundPreambleLength;
    GET_PERMANENT_S(outboundPreambleLength);
    preambleLength = outboundPreambleLength;
  }


#ifndef DEBUG 
  //If we're not in debug there is no visual indication of a message being sent at the station
  //So we light up the TX light on the board:
  digitalWrite(LED_PIN0, LED_ON);
#endif //DEBUG

  auto beforeTxMicros = micros();
  auto state = LORA_CHECK(csma.transmit(_outgoingBuffer, _currentLocation, preambleLength));
  PRINT_VARIABLE(micros() - beforeTxMicros);

#ifndef DEBUG
  if (state == ERR_NONE)
  {
    digitalWrite(LED_PIN0, LED_OFF);
  }
  else //Flash the TX/RX LEDs to indicate an error condition:
  {
    signalError();

    //If a message failed to send, try to re-initialise:
    InitMessaging();
  }
#endif //DEBUG

  bool ret = state == ERR_NONE;

  _currentLocation = -1;
  if (ret)
    return MESSAGE_OK;
  else
    return MESSAGE_ERROR;
  
}

MESSAGE_RESULT LoraMessageDestination::appendByte(const byte data)
{
  if (_currentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  else if (_currentLocation >= maxPacketSize)
  {
    return MESSAGE_BUFFER_OVERRUN;
  }

  _outgoingBuffer[_currentLocation++] = data;

  return MESSAGE_OK;
}

bool LoraMessageSource::beginMessage()
{
  LORA_CHECK(csma.dequeueMessage(&_incomingBuffer, &_incomingMessageSize));
  //we don't use state to determine whether to handle the message
  if (_incomingBuffer == 0)
    return false;
  
  if (s_discardCallsign)
  {
    if (_incomingMessageSize <= 6)
      return false;
    _currentLocation = 6;
    _length = _incomingMessageSize - 6;
  }
  else
  {
    _currentLocation = 0;
    _length = _incomingMessageSize;
  }

  return true;
}

MESSAGE_RESULT LoraMessageSource::endMessage()
{
  if (_currentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  _currentLocation = -1;
  return MESSAGE_END;
}

MESSAGE_RESULT LoraMessageSource::readByte(byte& dest)
{
  if (_currentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  if (_currentLocation >= _incomingMessageSize)
  {
    _currentLocation = -1;
    return MESSAGE_END;
  }
  else
  {
    dest = _incomingBuffer[_currentLocation++];
    return MESSAGE_OK;
  }
}

MESSAGE_RESULT LoraMessageSource::seek(const byte newPosition)
{
  if (newPosition >= _incomingMessageSize)
  {
    _currentLocation == -1;
    return MESSAGE_END;
  } 
  else
  {
    _currentLocation = newPosition;
    return MESSAGE_OK;
  }
}

#ifdef DEBUG
void messageDebugAction()
{
}
#endif