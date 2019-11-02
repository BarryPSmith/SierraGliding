#include "Messaging.h"
#include <Arduino.h>
#include "Callsign.h"
//int MessageSource::_iCurrentLocation = -1;

MessageDestination::MessageDestination(Stream* dest)
{
  m_pStream = dest;

  m_pStream->write(FEND);
  m_pStream->write(0x00b);

  append((byte*)callSign, 6);
}

MessageDestination::~MessageDestination()
{
  finishAndSend();
}

void MessageDestination::finishAndSend()
{
  if (m_iCurrentLocation < 0)
    return;

  while(m_iCurrentLocation < minPacketSize)
    appendByte(0);
    
  m_pStream->write(FEND);
  m_iCurrentLocation = -1;
}
    
void MessageDestination::appendByte(const byte data)
{
  if (m_iCurrentLocation < 0)
    return;
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
}
void MessageDestination::appendInt(const int data)
{
  append((byte*)&data, sizeof(int));
}

void MessageDestination::append(const byte* data, size_t dataLen)
{
  for (int i = 0; i < dataLen; i++)
    appendByte(data[i]);
}

size_t MessageDestination::getCurrentLocation()
{
  return m_iCurrentLocation;
}
