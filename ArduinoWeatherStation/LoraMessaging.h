#pragma once
#include <Arduino.h>
#include "MessagingCommon.h"


#if defined(MOTEINO_96)
#define SX_SELECT 10
#define SX_DIO1 2
#define SX_DIO2 3
#define SX_BUSY -1
#define RADIO_TYPE SX1278
#define RX_TIMEOUT
#define NO_COMMANDS
#else
/*
#define SX_SELECT 9
#define SX_DIO1 2
#define SX_BUSY 4
*/
#define RADIO_TYPE CSMAWrapper<SX1262>
#define RX_TIMEOUT SX126X_RX_TIMEOUT_INF
#endif
#define outputPower 10

#define LORA_CR 5

#define maxPacketSize 255

void InitMessaging();
bool HandleMessageCommand(MessageSource& src);
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