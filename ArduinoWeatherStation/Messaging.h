#pragma once
#include <Arduino.h>
#include "MessagingCommon.h"

#define SX_SELECT 9
#define SX_DIO1 2
#define SX_BUSY 4
#define outputPower 10

#define LORA_FREQ 425.0
#define LORA_BW 62.5
#define LORA_CR 5
#define LORA_SF 5

#define maxPacketSize 255

void InitMessaging();
bool HandleMessageCommand(MessageSource& src);

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
};