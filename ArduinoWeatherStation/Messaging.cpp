#include "Messaging.h"
#include <Arduino.h>
#include "Callsign.h"
#include "lib/RadioLib/src/Radiolib.h"

constexpr size_t maxPacketSize = 255;
//This files implements messaging using RadioLib and a SX1262

//Hardware pins:
#define SX_SELECT 9
#define SX_DIO1 2
#define SX_BUSY 4
Module mod(SX_SELECT, SX_DIO1, -1, SX_BUSY);
SX1262 lora = &mod;
bool initialised = false;

//TODO: Put a streaming interface into RadioLib to avoid these buffers.
byte[maxPacketSize] incommingBuffer;
byte[maxPacketSize] outgoingBuffer;
byte incomingBufferSize;

void Initialise()
{
  if (initialised)
    return;
  //Might have to tweak these parameters to get long distance communications.
  lora.begin(
    425.0, //Frequency
    125.0, //Bandwidth
    5,     //Spreading Factor
    5      //Coding rate
  );
}

MessageDestination::MessageDestination()
{
  append((byte*)callsign, 6);
}

MessageDestination::~MessageDestination()
{
  finishAndSend();
}

MESSAGE_RESULT MessageDestination::finishAndSend()
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;

  lora.transmit(outgoingBuffer, m_iCurrentLocation);
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
}

bool MessageSource::readByteRaw(byte& dest)
{
  //Not used in this interface
  return false;
}

bool MessageSource::beginMessage()
{
  if (lora.receive(incomingBuffer, maxPacketSize))
    return false;

  incommingBufferSize = lora.getPacketLength(false);

  //Discard the callsign:
  if (incomingBufferSize <= 6)
    return false;
  m_iCurrentLocation = 6;
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
  if (m_iCurrentLocation >= incommingBufferSize)
  {
    m_iCurrentLocation = -1;
    return MESSAGE_END;
  }
  dest = incommingBuffer[m_iCurrentLocation++];
  return MESSAGE_OK;
}
