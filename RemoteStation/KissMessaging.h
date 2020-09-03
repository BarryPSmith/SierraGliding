#include "MessagingCommon.h"

class KissMessageSource : public MessageSource
{
  private:
    bool readByteRaw(byte& dest);
    uint8_t _messageType;

  public:
    ~KissMessageSource();
    bool beginMessage() override;
    MESSAGE_RESULT endMessage() override;
    MESSAGE_RESULT readByte(byte& dest) override;
    MESSAGE_RESULT seek(const byte dest) override;
    inline uint8_t getMessageType() { return _messageType; }
};

class KissMessageDestination : public MessageDestination
{
  private:

  public:
    KissMessageDestination();
    ~KissMessageDestination();
    MESSAGE_RESULT finishAndSend() override;
    MESSAGE_RESULT appendByte(const byte data) override;
    MESSAGE_RESULT readBytes(byte** buffer, byte len) override;
};