#pragma once
#include <Arduino.h>
#include "MessagingCommon.h"
#include "lib/RadioLib/src/Radiolib.h"
#include "CSMA.h"

#define outputPower 10

constexpr auto LORA_CR = 5;
constexpr auto maxPacketSize = 255;

void InitMessaging();
bool handleMessageCommand(MessageSource& src, byte* desc = nullptr);
void appendMessageStatistics(MessageDestination& msg);
void updateIdleState();

extern SX1262 lora;
extern CSMAWrapper<SX1262> csma;

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
    byte* _incomingBuffer;
    //byte _incomingMessageSize;
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
    // delayRequired is used when we respond to messages.
    // It allows the sender to get back into receive mode (but then again that's already taken care of by delayCSMA)
    // and to avoid collisions if multiple stations try to transmit simultaneously (although multi-addressed messages generally aren't replied to).
    static bool delayRequired; 
  private:
    byte _outgoingBuffer[outgoingBufferSize];
    bool _isOutbound;
};