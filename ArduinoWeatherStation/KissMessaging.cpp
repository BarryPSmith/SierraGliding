#include "KissMessaging.h"
#include <Arduino.h>
#include "Callsign.h"

const size_t minPacketSize = 0; //15 for the argent data radioshield (station 1). 10 for the mobilinkd TNC (station 2). 0 when we're talking to the computer.

//This isn't... standard. Is there a better way to do it that doesn't involve virtual functions?
//Ugh, come back to this to get rid of it. We're not that concerned about performance that we can't have virtual functions.
#define STREAM Serial

KissMessageDestination::KissMessageDestination()
{
  STREAM.write(FEND);
  STREAM.write((byte)0x00);

  if (prependCallsign)
    append((byte*)callSign, 6);
}

KissMessageDestination::~KissMessageDestination()
{
  finishAndSend();
}

MESSAGE_RESULT KissMessageDestination::finishAndSend()
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;

  while(m_iCurrentLocation < minPacketSize)
    appendByte(0);
    
  STREAM.write(FEND);
  m_iCurrentLocation = -1;
  return MESSAGE_END;
}
    
MESSAGE_RESULT KissMessageDestination::appendByte(const byte data)
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  if (data == FEND)
  {
    STREAM.write(FESC);
    STREAM.write(TFEND);
  }
  else if (data == FESC)
  {
    STREAM.write(FESC);
    STREAM.write(TFESC);
  }
  else
    STREAM.write(data);
  m_iCurrentLocation++;
  return MESSAGE_OK;
}

KissMessageSource::~KissMessageSource()
{
  endMessage();
}

bool KissMessageSource::readByteRaw(byte& dest)
{
  unsigned long startMillis = millis();
  const unsigned long timeout = 1000;
  int data = -1;
  while (data == -1)
  {
    if (millis() - startMillis > timeout)
    {
      m_iCurrentLocation = -1;
      return false;
    }
    data = STREAM.read();
  }
  dest = data;
  return true;
}

bool KissMessageSource::beginMessage()
{
  byte data;
  bool matchedFend = false;
  //Wait for FEND 0x00:
  while (true)
  {
    if (!readByteRaw(data))
      return false;
    if (data == FEND)
      matchedFend = true;
    else if (matchedFend && data == 0x00)
      break;
    else
      matchedFend = false;
  }
  if (discardCallsign)
  {
    //Read the callsign. Discard it for now, but we might want to filter on it later.
    for (int i = 0; i < 6; i++)
    {
      if (!readByteRaw(data))
        return false;
    }
  }
  m_iCurrentLocation = 0;
  return true;
}

MESSAGE_RESULT KissMessageSource::endMessage()
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  byte data;
  while (true)
  {
    if (!readByteRaw(data))
      return MESSAGE_TIMEOUT;
    if (data == FEND)
      return MESSAGE_END;
  }
}

MESSAGE_RESULT KissMessageSource::readByte(byte& dest)
{
  {
    if (m_iCurrentLocation < 0)
      return MESSAGE_NOT_IN_MESSAGE;
    if (!readByteRaw(dest))
      return MESSAGE_TIMEOUT;
    if (dest == FEND)
    {
      m_iCurrentLocation = -1;
      return MESSAGE_END;
    }

    m_iCurrentLocation++;
    if (dest != FESC)
      return MESSAGE_OK;
    if (!readByteRaw(dest))
      return MESSAGE_TIMEOUT;
    switch (dest)
    {
    case TFEND: dest = FEND; break;
    case TFESC: dest = FESC; break;
    case FEND:
      m_iCurrentLocation = -1;
      return MESSAGE_END;
    }

    return MESSAGE_OK;
  }
}