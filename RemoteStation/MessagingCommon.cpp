#include "LoraMessaging.h"

const bool MessageSource::s_discardCallsign = false;
const bool MessageDestination::s_prependCallsign = false;

MessageDestination::MessageDestination() {}

MESSAGE_RESULT MessageDestination::appendData(MessageSource& source, byte maxBytes)
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
  return _currentLocation;
}

MESSAGE_RESULT MessageDestination::append(const byte* data, byte dataLen)
{
  for (int i = 0; i < dataLen; i++)
  {
    auto ret = appendByte(data[i]);
    if (ret)
      return ret;
  }
  return MESSAGE_OK;
}
MESSAGE_RESULT MessageDestination::append(const char* data, byte dataLen)
{
  return append((byte*)data, dataLen);
}
MESSAGE_RESULT MessageDestination::append(const __FlashStringHelper* data, byte dataLen)
{
  auto str = reinterpret_cast<const char*>(data);
  for (int i = 0; i < dataLen; i++)
  {
    byte cur = pgm_read_byte(str + i);
    auto ret = appendByte(cur);
    if (ret)
      return ret;
  }
  return MESSAGE_OK;
}

MESSAGE_RESULT MessageSource::readBytes(byte* dest, byte dataLen)
{
  for (int i = 0; i < dataLen; i++)
  {
    auto result = readByte(dest[i]);
    if (result != MESSAGE_OK)
      return result;
  }
  return MESSAGE_OK;
}

byte MessageSource::getCurrentLocation()
{
  return _currentLocation;
}

MessageSource::MessageSource() {}

byte MessageSource::getMessageLength()
{
  return _length;
}