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
  MESSAGE_BUFFER_OVERRUN = 4,
  MESSAGE_ERROR = -1
};

class MessageSource;

void InitMessaging();
bool HandleMessageCommand(MessageSource src);

//We're going to get rid of this, but for now...
class MessageDestination
{  
  int m_iCurrentLocation = 0;
  
  public:    
    MessageDestination();
    ~MessageDestination();
    
    MessageDestination(const MessageDestination&) =delete;
    MessageDestination& operator=(const MessageDestination) =delete;
  
    MESSAGE_RESULT appendByte(const byte data);

    MESSAGE_RESULT finishAndSend();
    template<class T>
    MESSAGE_RESULT appendT(T data)
    {
      return append((byte*)&data, sizeof(T));
    }

    MESSAGE_RESULT append(const byte* data, size_t dataLen);

    MESSAGE_RESULT appendData(MessageSource& source, size_t maxBytes);
    
    size_t getCurrentLocation();
};

class MessageSource
{
  private:
    int m_iCurrentLocation = -1;
    bool readByteRaw(byte& dest);

  public:
    MessageSource();
    ~MessageSource();

    MessageSource(const MessageSource&) = delete;
    MessageSource& operator=(const MessageSource) = delete;

    //Reads everything from the serial port, blocking the thread until a timeout occurs or 0xC0 0x00 [CALLSIGN] is encountered
    //Returns true if a message is encountered, after reading and discarding the callsign.
    //Returns false if a timeout occurs.
    bool beginMessage();

    //If in a message, reads everything from the serial port, blocking the thread until a timout occurs or 0xC0.
    //If not in a message, returns immediately
    MESSAGE_RESULT endMessage();

    //Reads the next byte
    MESSAGE_RESULT readByte(byte& dest);

    template<class T>
    MESSAGE_RESULT read(T& dest)
    {
      return readBytes((byte*)&dest, sizeof(T));
    }
    MESSAGE_RESULT readBytes(byte* dest, size_t dataLen);

    size_t getCurrentLocation();
};