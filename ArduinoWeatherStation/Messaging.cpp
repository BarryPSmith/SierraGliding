//This files implements messaging using RadioLib and a SX1262
#include "Messaging.h"
#include <Arduino.h>
#include "Callsign.h"
#include "lib/RadioLib/src/Radiolib.h"
#include "CSMA.h"
#include "ArduinoWeatherStation.h"
#include "PermanentStorage.h"

#define LORA_CHECK(A) { auto res = A; if(res) { AWS_DEBUG_PRINT(F("FAILED " #A ": ")); AWS_DEBUG_PRINTLN(res); } } while(0)

//Hardware pins:
Module mod(SX_SELECT, SX_DIO1, SX_DIO2, SX_BUSY);

//These use 100 bytes (Seems alot)
RADIO_TYPE lora = &mod; //28 bytes
bool initialised = false; //72 bytes

//TODO: Put a streaming interface into RadioLib to avoid these buffers.
//This uses 510 bytes.
byte incomingBuffer[maxPacketSize];
byte outgoingBuffer[maxPacketSize];
byte incomingBufferSize;

static volatile bool packetWaiting = false;
static void rxDone();


void InitMessaging()
{
  if (initialised)
    return;
  SPI.usingInterrupt(digitalPinToInterrupt(SX_DIO1));

  AWS_DEBUG_PRINT(F("Mod size: "));
  AWS_DEBUG_PRINTLN(sizeof(mod));
  AWS_DEBUG_PRINT(F("Lora size: "));
  AWS_DEBUG_PRINTLN(sizeof(lora));
  AWS_DEBUG_PRINT(F("Radio type: "));
  AWS_DEBUG_PRINTLN(F(XSTR(RADIO_TYPE)));
  // Might have to tweak these parameters to get long distance communications.
  // but testing suggests we can transmit 23km at -10 dBm. at +10 dBm we've got a fair bit of headroom.
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
  LORA_CHECK(lora.begin(
    frequency,
    bandwidth,
    spreadingFactor,
    LORA_CR
  ));
  LORA_CHECK(lora.setOutputPower(txPower));
#if defined(MOTEINO_96)
  lora.setDio0Action(rxDone);
#else
  LORA_CHECK(lora.setCurrentLimit(140));
  LORA_CHECK(lora.setDio2AsRfSwitch());
  lora.enableIsChannelBusy();
  lora.setRxDoneAction(rxDone);
  lora.setPByte(csmaP);
  lora.setTimeSlot(csmaTimeslot);
#endif

  LORA_CHECK(lora.startReceive(RX_TIMEOUT));

  initialised = true;
}

#ifdef DEBUG
int scanChannel()
{
  return lora.scanChannel();
}
#endif

static void rxDone()
{
  packetWaiting = true;
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
      state = lora.setOutputPower(txPower) == ERR_NONE;
      lora.startReceive(SX126X_RX_TIMEOUT_INF);
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
      float frequency;
      if (src.read(frequency))
      {
        state = ERR_UNKNOWN;
        break;
      }      
      state = lora.setFrequency(frequency);
      if (state == ERR_NONE)
        SET_PERMANENT_S(frequency);
      break;
    }
  case 'B':
    {
      float bandwidth;
      if (src.read(bandwidth))
      {
        state = ERR_UNKNOWN;
        break;
      }
      state = lora.setBandwidth(bandwidth);
      if (state == ERR_NONE)
        SET_PERMANENT_S(bandwidth);
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
      state = lora.setSpreadingFactor(spreadingFactor);
      if (state == ERR_NONE)
        SET_PERMANENT_S(spreadingFactor);
      break;
    }
  default:
    state == ERR_UNKNOWN;
  }
  lora.startReceive(SX126X_RX_TIMEOUT_INF);
  return state == ERR_NONE;
#else // !MOTEINO_96
  return false;
#endif 
}



LoraMessageSource::LoraMessageSource() : MessageSource()
{}

LoraMessageDestination::~LoraMessageDestination()
{
  finishAndSend();
}

LoraMessageDestination::LoraMessageDestination()
{
  if (prependCallsign)
    append((byte*)callSign, 6);
}

MESSAGE_RESULT LoraMessageDestination::finishAndSend()
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;

  auto state = lora.transmit(outgoingBuffer, m_iCurrentLocation);
  if (state != ERR_NONE)
  {
    AWS_DEBUG_PRINT(F("Error transmitting message: "));
    AWS_DEBUG_PRINTLN(state);
  }
  bool ret = state == ERR_NONE;
  state = lora.startReceive(RX_TIMEOUT);
  if (state != ERR_NONE)
  {
    AWS_DEBUG_PRINT(F("Error restarting receive: "));
    AWS_DEBUG_PRINTLN(state);
  }

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

  outgoingBuffer[m_iCurrentLocation++] = data;

  return MESSAGE_OK;
}

bool LoraMessageSource::beginMessage()
{
  if (!packetWaiting)
  {
    return false;
  }
  packetWaiting = false;
  
  incomingBufferSize = lora.getPacketLength(false);
  auto state = lora.readData(incomingBuffer, maxPacketSize);
  
  #if 0 //defined(MONTEINO_96)
  auto state2 = lora.startReceive();
  if (state2 != ERR_NONE)
  {
    AWS_DEBUG_PRINTLN(F("startReceive error: "));
    AWS_DEBUG_PRINTLN(state2);
  }
  #endif
  if (state != ERR_NONE)
  {
    AWS_DEBUG_PRINT(F("readData error: "));
    AWS_DEBUG_PRINTLN(state);
    return false;
  }

  if (discardCallsign)
  {
    if (incomingBufferSize <= 6)
      return false;
    m_iCurrentLocation = 6;
  }
  else
    m_iCurrentLocation = 0;

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
  if (m_iCurrentLocation >= incomingBufferSize)
  {
    m_iCurrentLocation = -1;
    return MESSAGE_END;
  }
  else
  {
    dest = incomingBuffer[m_iCurrentLocation++];
    return MESSAGE_OK;
  }
}