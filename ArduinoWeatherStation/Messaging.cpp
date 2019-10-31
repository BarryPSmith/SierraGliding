#include "Messaging.h"
#include <Arduino.h>
#include "Callsign.h"
//int MessageSource::_iCurrentLocation = -1;

MessageDestination::MessageDestination()
{
  Serial.write(FEND);
  Serial.write(0x00);

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
    
  Serial.write(FEND);
  m_iCurrentLocation = -1;
}
    
void MessageDestination::appendByte(byte data)
{
  if (m_iCurrentLocation < 0)
    return;
  if (data == FEND)
  {
    Serial.write(FESC);
    Serial.write(TFEND);
  }
  else if (data == FESC)
  {
    Serial.write(FESC);
    Serial.write(TFESC);
  }
  else
    Serial.write(data);
  m_iCurrentLocation++;
}
void MessageDestination::appendInt(int data)
{
  append((byte*)&data, sizeof(int));
}

void MessageDestination::append(byte* data, size_t dataLen)
{
  for (int i = 0; i < dataLen; i++)
    appendByte(data + i);
}

size_t MessageDestination::getCurrentLocation()
{
  return m_iCurrentLocation;
}
