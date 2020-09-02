#pragma once
#include <Arduino.h>

#define STR(A) #A
#define XSTR(A) STR(A)

enum MESSAGE_RESULT {
  MESSAGE_OK = 0,
  MESSAGE_END = 1,
  MESSAGE_TIMEOUT = 2,
  MESSAGE_NOT_IN_MESSAGE = 3,
  MESSAGE_BUFFER_OVERRUN = 4,
  MESSAGE_ERROR = -1
};

//KISS special characters
const byte FEND = 0xC0;
const byte FESC = 0xDB;
const byte TFEND = 0xDC;
const byte TFESC = 0xDD;

class MessageSource;

class MessageDestination
{  
  protected:
    byte _currentLocation = 0;
  
  public:
    const static bool s_prependCallsign;

    MessageDestination();
    
    MessageDestination(const MessageDestination&) =delete;
  
    virtual MESSAGE_RESULT appendByte(const byte data) = 0;

    virtual MESSAGE_RESULT finishAndSend() = 0;
    template<class T>
    MESSAGE_RESULT appendT(T data)
    {
      return append((byte*)&data, sizeof(T));
    }

    MESSAGE_RESULT append(const byte* data, byte dataLen);
    MESSAGE_RESULT append(const __FlashStringHelper* data, byte dataLen);
    MESSAGE_RESULT append(const char* data, byte dataLen);

    MESSAGE_RESULT appendData(MessageSource& source, byte maxBytes);
    
    size_t getCurrentLocation();
};

class MessageSource
{
  protected:
    byte _currentLocation = 255;
    byte _length;

  public:
    const static bool s_discardCallsign;

    MessageSource();

    MessageSource(const MessageSource&) = delete;

    //Returns true if a message is encountered, after reading and discarding the callsign.
    //Returns false if a timeout occurs.
    virtual bool beginMessage() = 0;

    //If in a message, reads everything from the serial port, blocking the thread until a timout occurs or 0xC0.
    //If not in a message, returns immediately
    virtual MESSAGE_RESULT endMessage() = 0;

    //Reads the next byte
    virtual MESSAGE_RESULT readByte(byte& dest) = 0;

    template<class T>
    MESSAGE_RESULT read(T& dest)
    {
      return readBytes((byte*)&dest, sizeof(T));
    }
    MESSAGE_RESULT readBytes(byte* dest, byte dataLen);

    virtual MESSAGE_RESULT readBytes(byte** dest, byte dataLen) = 0;
    
    byte getMessageLength();
    byte getCurrentLocation();
};