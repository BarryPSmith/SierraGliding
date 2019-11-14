//This files implements messaging using RadioLib and a SX1262
#include "Messaging.h"
#include <Arduino.h>
#include "Callsign.h"
#include "lib/RadioLib/src/Radiolib.h"
#include "CSMA.h"

#define maxPacketSize 255
#define outputPower 10

//Hardware pins:
#define SX_SELECT 9
#define SX_DIO1 2
#define SX_BUSY 4
Module mod(SX_SELECT, SX_DIO1, -1, SX_BUSY);

CSMAWrapper<SX1262> lora = &mod;
bool initialised = false;

//TODO: Put a streaming interface into RadioLib to avoid these buffers.
byte incomingBuffer[maxPacketSize];
byte outgoingBuffer[maxPacketSize];
byte incomingBufferSize;

static bool packetWaiting = false;
static void rxDone();

void InitMessaging()
{
  if (initialised)
    return;
  // Might have to tweak these parameters to get long distance communications.
  // but testing suggests we can transmit 23km at -10 dBm. at +10 dBm we've got a fair bit of headroom.
  lora.begin(
    425.0, //Frequency
    62.5,  //Bandwidth
    5,     //Spreading Factor
    5      //Coding rate
  );
  lora.setOutputPower(outputPower);
  lora.setCurrentLimit(140);
  lora.setRxDoneAction(rxDone);
  lora.startReceive(SX126X_RX_TIMEOUT_INF);
}

static void rxDone()
{
  packetWaiting = true;
}

bool HandleMessageCommand(MessageSource src)
{
  byte descByte;
  if (src.readByte(descByte))
    return false;

  switch(descByte) {
  case 'P':
    {
      int newPower;
      if (src.read(newPower))
        return false;
      if (lora.standby())
        return false;
      bool ret = lora.setOutputPower(newPower) == ERR_NONE;
      lora.startReceive(SX126X_RX_TIMEOUT_INF);
      return ret;
    }
  case 'C':
    {
      byte newP;
      if (src.read(newP))
        return false;
      return lora.setPByte(newP);
    }
  case 'T':
    {
      uint32_t newTimeSlot;
      if (src.read(newTimeSlot))
        return false;
      lora.setTimeSlot(newTimeSlot);
      return true;
    }
  }
}

MessageDestination::MessageDestination()
{
  append((byte*)callSign, 6);
}

MessageDestination::~MessageDestination()
{
  finishAndSend();
}

MESSAGE_RESULT MessageDestination::finishAndSend()
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;

  bool ret = lora.transmit(outgoingBuffer, m_iCurrentLocation) == ERR_NONE;
  lora.startReceive(SX126X_RX_TIMEOUT_INF);
  if (ret)
    return MESSAGE_OK;
  else
    return MESSAGE_ERROR;
  
}

MESSAGE_RESULT MessageDestination::appendByte(const byte data)
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

bool MessageSource::readByteRaw(byte& dest)
{
  //Not used in this interface
  return false;
}

bool MessageSource::beginMessage()
{
  if (!packetWaiting)
    return false;
  packetWaiting = false;

  if (lora.readData(incomingBuffer, maxPacketSize))
    return false;

  incomingBufferSize = lora.getPacketLength(false);

  //Discard the callsign:
  if (incomingBufferSize <= 6)
    return false;
  m_iCurrentLocation = 6;

  return true;
}

MESSAGE_RESULT MessageSource::endMessage()
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  m_iCurrentLocation = -1;
  return MESSAGE_END;
}

MESSAGE_RESULT MessageSource::readByte(byte& dest)
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  if (m_iCurrentLocation >= incomingBufferSize)
  {
    m_iCurrentLocation = -1;
    return MESSAGE_END;
  }
  dest = incomingBuffer[m_iCurrentLocation++];
  return MESSAGE_OK;
}