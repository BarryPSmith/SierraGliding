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
#define SX_SELECT 9
#define SX_DIO1 2
//#define SX_DIO2 -1
#define SX_BUSY 4
#define RADIO_TYPE CSMAWrapper<SX1262>
#define RX_TIMEOUT SX126X_RX_TIMEOUT_INF
#endif
#define outputPower 10

/*#define LORA_FREQ 425.0
#define LORA_BW 62.5
#define LORA_SF 6*/
#define LORA_CR 5

#define maxPacketSize 255

void InitMessaging();
bool HandleMessageCommand(MessageSource& src);
#ifdef DEBUG
int scanChannel();
#endif

class LoraMessageSource : public MessageSource
{
  public:
    LoraMessageSource();
    LoraMessageSource& operator=(const LoraMessageSource) =delete;
    bool beginMessage() override;
    MESSAGE_RESULT endMessage() override;
    MESSAGE_RESULT readByte(byte& dest) override;
};

class LoraMessageDestination : public MessageDestination
{
  public:
    LoraMessageDestination();
    ~LoraMessageDestination();
    LoraMessageDestination& operator=(const LoraMessageDestination) =delete;
    MESSAGE_RESULT appendByte(const byte data) override;
    MESSAGE_RESULT finishAndSend() override;
    void abort();
};