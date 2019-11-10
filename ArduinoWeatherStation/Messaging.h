#pragma once
#include <Arduino.h>
//KISS special characters
const byte FEND = 0xC0;
const byte FESC = 0xDB;
const byte TFEND = 0xDC;
const byte TFESC = 0xDD;

enum MESSAGE_RESULT {
  MESSAGE_OK = 0,
  MESSAGE_END = 1,
  MESSAGE_TIMEOUT = 2,
  MESSAGE_NOT_IN_MESSAGE = 3,
};

//We're going to get rid of this, but for now...
//size_t readMessage(byte* msgBuffer, size_t bufferSize);
class MessageSource;
class MessageDestination
{  
  int m_iCurrentLocation = 0;
  Stream* m_pStream;
  
  public:
    const size_t maxPacketSize = 250;
    const size_t minPacketSize = 10; //15 for the argent data radioshield (station 1). 10 for the mobilinkd TNC (station 2).
    
    MessageDestination(Stream* dest);
    ~MessageDestination();
    
    MessageDestination(const MessageDestination&) =delete;
    MessageDestination& operator=(const MessageDestination) =delete;
  
    MESSAGE_RESULT appendByte(const byte data);
    MESSAGE_RESULT appendInt(const int data);
    template<class T>
    MESSAGE_RESULT appendT(T data)
    {
      return append((byte*)&data, sizeof(T));
    }
    MESSAGE_RESULT append(const byte* data, size_t dataLen);
    MESSAGE_RESULT appendData(MessageSource& source, size_t maxBytes);

    MESSAGE_RESULT finishAndSend();

    size_t getCurrentLocation();
};

class MessageSource
{
  private:
    int m_iCurrentLocation = -1;
    Stream* m_pStream;
    bool readByteRaw(byte& dest)
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
        data = m_pStream->read();
      }
      dest = data;
      return true;
    }

  public:
    MessageSource(Stream* dest)
    {
      m_pStream = dest;
    }
    ~MessageSource()
    {
      //Ensure that we clear the serial buffer:
      endMessage();
    }

    MessageSource(const MessageSource&) = delete;
    MessageSource& operator=(const MessageSource) = delete;

    //Reads everything from the serial port, blocking the thread until a timeout occurs or 0xC0 0x00 [CALLSIGN] is encountered
    //Returns true if a message is encountered, after reading and discarding the callsign.
    //Returns false if a timeout occurs.
    bool beginMessage()
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
      //Read the callsign. Discard it for now, but we might want to filter on it later.
      for (int i = 0; i < 6; i++)
      {
        if (!readByteRaw(data))
          return false;
      }
      m_iCurrentLocation = 0;
      return true;
    }

    //If in a message, reads everything from the serial port, blocking the thread until a timout occurs or 0xC0.
    //If not in a message, returns immediately
    MESSAGE_RESULT endMessage()
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

    //Reads the next byte
    MESSAGE_RESULT readByte(byte& dest)
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
    template<class T>
    MESSAGE_RESULT read(T& dest)
    {
      return readBytes((byte*)&dest, sizeof(T));
    }
    MESSAGE_RESULT readBytes(byte* dest, size_t dataLen)
    {
      for (int i = 0; i < dataLen; i++)
      {
        auto result = readByte(dest[i]);
        if (result != MESSAGE_OK)
          return result;
      }
      return MESSAGE_OK;
    }

    size_t getCurrentLocation()
    {
      return m_iCurrentLocation;
    }
};
