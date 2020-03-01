#pragma once
#include <Arduino.h>
#include "MessagingCommon.h"

#define outputPower 10

constexpr auto LORA_CR = 5;
constexpr auto maxPacketSize = 255;

void InitMessaging();
bool handleMessageCommand(MessageSource& src, byte* desc = nullptr);
void appendMessageStatistics(MessageDestination& msg);

#ifdef DEBUG
void messageDebugAction();
#endif

class LoraMessageSource : public MessageSource
{
  public:
    LoraMessageSource();
    LoraMessageSource& operator=(const LoraMessageSource) =delete;
    bool beginMessage() override;
    MESSAGE_RESULT endMessage() override;
    MESSAGE_RESULT readByte(byte& dest) override;

    MESSAGE_RESULT seek(const byte newPosition);

  private:
#ifdef MOTEINO_96
    byte _incomingBuffer[255];
#else
    byte* _incomingBuffer;
#endif
    byte _incomingMessageSize;
};

class LoraMessageDestination : public MessageDestination
{
  public:
    LoraMessageDestination(bool isOutbound);
    ~LoraMessageDestination();
    LoraMessageDestination& operator=(const LoraMessageDestination) =delete;
    MESSAGE_RESULT appendByte(const byte data) override;
    MESSAGE_RESULT finishAndSend() override;
    void abort();
    
    static constexpr byte outgoingBufferSize = 255;
  private:
    byte _outgoingBuffer[outgoingBufferSize];
    bool _isOutbound;
};