#include "Messaging.h"

bool MessageSource::discardCallsign = true;
bool MessageDestination::prependCallsign = true;

MessageDestination::MessageDestination() {}

MESSAGE_RESULT MessageDestination::appendData(MessageSource& source, size_t maxBytes)
{
  for (int i = 0; i < maxBytes; i++)
  {
    byte b;
    auto ret = source.readByte(b);
    if (ret != MESSAGE_OK)
      return ret;
    ret = appendByte(b);
    if (ret != MESSAGE_OK)
      return ret;
  }
  return MESSAGE_OK;
}

size_t MessageDestination::getCurrentLocation()
{
  return m_iCurrentLocation;
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
MESSAGE_RESULT MessageDestination::append(const char* data, size_t dataLen)
{
  return append((byte*)data, dataLen);
}

MESSAGE_RESULT MessageSource::readBytes(byte* dest, size_t dataLen)
{
  for (int i = 0; i < dataLen; i++)
  {
    auto result = readByte(dest[i]);
    if (result != MESSAGE_OK)
      return result;
  }
  return MESSAGE_OK;
}

size_t MessageSource::getCurrentLocation()
{
  return m_iCurrentLocation;
}

MessageSource::MessageSource() {}
