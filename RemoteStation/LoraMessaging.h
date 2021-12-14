#pragma once
#include <Arduino.h>
#include "MessagingCommon.h"
#include "lib/RadioLib/src/Radiolib.h"
#include "CSMA.h"
#include "Callsign.h"
#include "PermanentStorage.h"

#define outputPower 10

constexpr auto LORA_CR = 5;
constexpr auto maxPacketSize = 254;

void InitMessaging();
bool handleMessageCommand(MessageSource& src, byte* desc = nullptr);
void appendMessageStatistics(MessageDestination& msg);
void updateIdleState(); 
void sleepRadio();

extern SX1262 lora;
extern CSMAWrapper<SX1262> csma;
extern bool initMessagingRequired;

#ifdef DEBUG
void messageDebugAction();
#endif

class LoraMessageSource : public MessageSource
{
  public:
    LoraMessageSource();
    ~LoraMessageSource();
    LoraMessageSource& operator=(const LoraMessageSource) =delete;
    bool beginMessage() override;
    MESSAGE_RESULT endMessage() override;
    MESSAGE_RESULT readByte(byte& dest) override;
    MESSAGE_RESULT accessBytes(byte** dest, byte bytesToRead) override;

    MESSAGE_RESULT seek(const byte newPosition) override;

#ifdef GET_CRC_FAILURES
    bool _crcMismatch;
#endif

  private:
    byte* _incomingBuffer;
    //byte _incomingMessageSize;
};

extern bool delayRequired;

class LoraMessageDestination final : public MessageDestination
{
  public:
    LoraMessageDestination(bool isOutbound,
      byte* buffer, uint8_t bufferSize, bool prependX = true)
    {
      _isOutbound = isOutbound;
      _outgoingBuffer = buffer;
      outgoingBufferSize = bufferSize;

      if (s_prependCallsign)
        append((byte*)callSign, 6);
      else if (prependX)
      {
        *buffer = 'X';
        _currentLocation = 1;
      }
    }

    /*LoraMessageDestination(bool isOutbound,
      byte* buffer, uint8_t bufferSize)
    {
      _isOutbound = isOutbound;
      _outgoingBuffer = buffer;
      outgoingBufferSize = bufferSize;

      *buffer = 'X';
      _currentLocation = 1;
    }*/

    __attribute__((noinline)) LoraMessageDestination(bool isOutbound,
      byte* buffer, uint8_t bufferSize,
      byte type, byte uniqueID);

    ~LoraMessageDestination()
    {
      finishAndSend();
    }
    LoraMessageDestination& operator=(const LoraMessageDestination) =delete;
    MESSAGE_RESULT appendByte(const byte data) override
    {
      if (_currentLocation == 255)
        return MESSAGE_NOT_IN_MESSAGE;
      else if (_currentLocation >= outgoingBufferSize)
      {
        return MESSAGE_BUFFER_OVERRUN;
      }

      _outgoingBuffer[_currentLocation++] = data;

      return MESSAGE_OK;
    }

    #define appendByte2 appendByte
    /*void appendByte2(const byte data)
    {
      _outgoingBuffer[_currentLocation++] = data;
    }*/

    void appendByteUnchecked(const byte data)
    {
      _outgoingBuffer[_currentLocation++] = data;
    }
    MESSAGE_RESULT finishAndSend() override
    {
      if (_currentLocation == 255)
        return MESSAGE_NOT_IN_MESSAGE;

      uint16_t preambleLength = 8;
      if (_isOutbound)
      {
        uint16_t outboundPreambleLength;
        GET_PERMANENT_S(outboundPreambleLength);
        preambleLength = outboundPreambleLength;
      }
      else
      {
        uint16_t inboundPreambleLength = 8;
        if (batteryMode == BatteryMode::Normal)
          GET_PERMANENT_S(inboundPreambleLength);
        preambleLength = inboundPreambleLength;
      }


    #if !defined(DEBUG) && !defined(MODEM)
      //If we're not in debug there is no visual indication of a message being sent at the station
      //So we light up the TX light on the board:
      digitalWrite(LED_PIN0, LED_ON);
    #endif //DEBUG

      TX_DEBUG(auto beforeTxMicros = micros());
      if (delayRequired)
        delayMicroseconds(lora._tcxoDelay * random(1,5));
      auto state = LORA_CHECK(csma.transmit(_outgoingBuffer, _currentLocation, preambleLength));
      TX_PRINTVAR(micros() - beforeTxMicros);

      if (state == ERR_NONE)
      {
    #if !defined(DEBUG) && !defined(MODEM)
        digitalWrite(LED_PIN0, LED_OFF);
    #endif
      }
      else //Flash the TX/RX LEDs to indicate an error condition:
      {
    #if !defined(DEBUG) && !defined(MODEM)
        signalError();
    #endif
        // If a message failed to send, try to re-initialise:
        // We use a global flag to do this on the next loop rather than immediately,
        // to keep the stack usage down
        initMessagingRequired = true;
      }

      bool ret = state == ERR_NONE;

      _currentLocation = 255;
      if (ret)
        return MESSAGE_OK;
      else
        return MESSAGE_ERROR;
    }
    void abort()
    {
      _currentLocation = 255;
    }
    MESSAGE_RESULT getBuffer(byte** buffer, byte bytesToAdd)
    {
      if (bytesToAdd + _currentLocation >= maxPacketSize)
        return MESSAGE_BUFFER_OVERRUN;
      *buffer = _outgoingBuffer + _currentLocation;
      _currentLocation += bytesToAdd;
      return MESSAGE_OK;
    }
    
    // static constexpr byte outgoingBufferSize = 254;
    // delayRequired is used when we respond to messages.
    // It allows the sender to get back into receive mode (but then again that's already taken care of by delayCSMA)
    // and to avoid collisions if multiple stations try to transmit simultaneously (although multi-addressed messages generally aren't replied to).
    // static bool delayRequired;
  private:
    byte* _outgoingBuffer;
    bool _isOutbound;
    uint8_t outgoingBufferSize;
};