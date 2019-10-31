#pragma once
#include <Arduino.h>
//KISS special characters
const byte FEND = 0xC0;
const byte FESC = 0xDB;
const byte TFEND = 0xDC;
const byte TFESC = 0xDD;

//We're going to get rid of this, but for now...
size_t readMessage(byte* msgBuffer, size_t bufferSize);

class MessageDestination
{  
  int m_iCurrentLocation = 0;
  
  public:
    const size_t maxPacketSize = 250;
    const size_t minPacketSize = 15;
    
    MessageDestination();
    ~MessageDestination();
    
    MessageDestination(const MessageDestination&) =delete;
    MessageDestination& operator=(const MessageDestination) =delete;
  
    void appendByte(byte data);
    void appendInt(int data);
    template<class T>
    void appendT(T data)
    {
      append((byte*)&data, sizeof(T));
    }
    void append(byte* data, size_t dataLen);

    void finishAndSend();

    size_t getCurrentLocation();
};

enum MESSAGE_RESULT {
  MESSAGE_OK = 0,
  MESSAGE_END = 1,
  MESSAGE_TIMEOUT = 2,
  MESSAGE_NOT_IN_MESSAGE = 3,
  MESSAGE_BUFFER_FULL = 4
};

class MessageSource
{
  private:
    static int _iCurrentLocation;
    static bool readByteRaw(byte& dest)
    {
      unsigned long startMillis = millis();
      const unsigned long timeout = 1000;
      int data = -1;
      while (data == -1)
      {
        if (millis() - startMillis > timeout)
        {
          _iCurrentLocation = -1;
          return false;
        }
        data = Serial.read();
      }
      dest = data;
      return true;
    }

  public:
    MessageSource() =delete;

    //Reads everything from the serial port, blocking the thread until a timeout occurs or 0xC0 0x00 [CALLSIGN] is encountered
    //Returns true if a message is encountered, after reading and discarding the callsign.
    //Returns false if a timeout occurs.
    static bool beginMessage()
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
      _iCurrentLocation = 0;
      return true;
    }
    //If in a message, reads everything from the serial port, blocking the thread until a timout occurs or 0xC0.
    //If not in a message, returns immediately
    static MESSAGE_RESULT endMessage()
    {
      if (_iCurrentLocation < 0)
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
    static MESSAGE_RESULT readByte(byte& dest)
    {
      if (_iCurrentLocation < 0)
        return MESSAGE_NOT_IN_MESSAGE;
      if (!readByteRaw(dest))
        return MESSAGE_TIMEOUT;
      if (dest == FEND)
      {
        _iCurrentLocation = -1;
        return MESSAGE_END;
      }
      
      _iCurrentLocation++;
      if (dest != FESC)
        return MESSAGE_OK;
      if (!readByteRaw(dest))
        return MESSAGE_TIMEOUT;
      switch (dest)
      {
        case TFEND: dest = FEND; break;
        case TFESC: dest = FESC; break;
        case FEND:
          _iCurrentLocation = -1;
          return MESSAGE_END;
      }
      
      return MESSAGE_OK;
    }
    template<class T>
    static MESSAGE_RESULT read(T& dest)
    {
      return readBytes((byte*)&dest, sizeof(T));
    }
    static MESSAGE_RESULT readBytes(byte* dest, size_t dataLen)
    {
      for (int i = 0; i < dataLen; i++)
      {
        auto result = readByte(dest[i]);
        if (result != MESSAGE_OK)
          return result;
      }
      return MESSAGE_OK;
    }

    static size_t getCurrentLocation()
    {
      return _iCurrentLocation;
    }
};
