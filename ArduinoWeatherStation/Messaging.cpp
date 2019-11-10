#include "Messaging.h"
#include <Arduino.h>
#include "Callsign.h"
//int MessageSource::_iCurrentLocation = -1;

MessageDestination::MessageDestination(Stream* dest)
{
  m_pStream = dest;

  m_pStream->write(FEND);
  m_pStream->write((byte)0x00);

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

  while(m_iCurrentLocation < minPacketSize)
    appendByte(0);
    
  m_pStream->write(FEND);
  m_iCurrentLocation = -1;
  return MESSAGE_END;
}
    
MESSAGE_RESULT MessageDestination::appendByte(const byte data)
{
  if (m_iCurrentLocation < 0)
    return MESSAGE_NOT_IN_MESSAGE;
  if (data == FEND)
  {
    m_pStream->write(FESC);
    m_pStream->write(TFEND);
  }
  else if (data == FESC)
  {
    m_pStream->write(FESC);
    m_pStream->write(TFESC);
  }
  else
    m_pStream->write(data);
  m_iCurrentLocation++;
  return MESSAGE_OK;
}
MESSAGE_RESULT MessageDestination::appendInt(const int data)
{
  return append((byte*)&data, sizeof(int));
}

MESSAGE_RESULT MessageDestination::append(const byte* data, size_t dataLen)
{
  for (int i = 0; i < dataLen; i++)
  {
    auto ret = appendByte(data[i]);
    if (ret)
      return ret;
  }
  return MESSAGE_OK;
}

MESSAGE_RESULT MessageDestination::appendData(MessageSource& source, size_t maxBytes)
{
  for (int i = 0; i < maxBytes; i++)
  {
    byte b;
    auto ret = appendByte(source.readByte(b));
    if (ret != MESSAGE_OK)
      return ret;
  }
  return MESSAGE_OK;
}

size_t MessageDestination::getCurrentLocation()
{
  return m_iCurrentLocation;
}
