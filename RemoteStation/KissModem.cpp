#include "revid.h"
#include "KissMessaging.h"
#include "LoraMessaging.h"
#include <spi.h>
#include "PermanentStorage.h"
#include "StackCanary.h"
#include <avr/wdt.h>

#define SERIALBAUD 38400

//Testing stuff:
#if 0
void printState()
  {
    Serial.print(digitalRead(SX_RESET));
    Serial.print('\t');
    Serial.print(digitalRead(SX_SELECT));
    Serial.print('\t');
    Serial.print(digitalRead(SX_BUSY));

    Serial.println();
  }
void TestBoard()
{
  wdt_disable();
  Serial.begin(38400);
  SPISettings spiSettings = SPISettings(2000000, MSBFIRST, SPI_MODE0);
  SPI.begin();
  
  // put your setup code here, to run once:
  pinMode(SX_RESET, OUTPUT);
  pinMode(SX_SELECT, OUTPUT);
  pinMode(SX_BUSY, INPUT);
  pinMode(SX_DIO1, INPUT);

  Serial.println("Reset\tSelect\tBusy");

  digitalWrite(SX_SELECT, HIGH);
  digitalWrite(SX_RESET, LOW);
  for (int i = 0; i < 10; i++)
  {
    printState();
    delay(100);
  }
  digitalWrite(SX_RESET, HIGH);
  for (int i = 0; i < 10; i++)
  {
    printState();
    delay(100);
  }
  digitalWrite(SX_SELECT, LOW);
  for (int i = 0; i < 10; i++)
  {
    printState();
    delay(1);
  }
  digitalWrite(SX_SELECT, HIGH);
  for (int i = 0; i < 10; i++)
  {
    printState();
    delay(1);
  }

  digitalWrite(SX_SELECT, LOW);
  while (digitalRead(SX_BUSY))
  { 
    Serial.print('a');
  }
  Serial.println();
  SPI.beginTransaction(spiSettings);
  auto ret1 = SPI.transfer(0xC0);
  auto ret2 = SPI.transfer(0x00);
  digitalWrite(SX_SELECT, HIGH);
  SPI.endTransaction();
  Serial.println("Results:");
  Serial.println(ret1, HEX);
  Serial.println(ret2, HEX);
  while(1);
}
#endif

unsigned long lastFailMillis = 0;
BatteryMode batteryMode = BatteryMode::Normal;
bool stasisRequested = false;

int main()
{
  //Enable the watchdog early to catch initialisation hangs (Side note: This limits initialisation to 8 seconds)
  wdt_enable(WDTO_8S);
  //wdt_disable();
  WDTCSR |= (1 << WDIE);


  init();

  //TestBoard();

  randomSeed(analogRead(A0));
  
  Serial.begin(SERIALBAUD);
  delay(10);

  Serial.print(F("oldSP: "));
  Serial.println(oldSP, HEX);
  Serial.print(F("MCUSR_Mirror: "));
  Serial.println(MCUSR_Mirror, HEX);
  if (oldSP >= 0x100)
  {
    Serial.print(F("Previous Stack Pointer: "));
    Serial.println(oldSP, HEX);
    Serial.print(F("Previous Stack: "));
    for (int i = 0; i < STACK_DUMP_SIZE; i++)
    {
      Serial.print((&oldStack)[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
  }

  SPI.begin();
  delay(10);

  PermanentStorage::initialise();
  InitMessaging();
  updateIdleState();
  
  Serial.print(F("Arduino LoRa modem. Version "));
  Serial.println(REV_ID);
  Serial.println();
  Serial.print(F("Coding Rate: "));
  Serial.println(LORA_CR);
  Serial.println();
  Serial.println(F("Starting..."));

  unsigned long lastMessage = 0;
  const unsigned long messageInterval = 6000;

  while (1)
  {
    LoraMessageSource loraSrc;
    while (loraSrc.beginMessage())
    {
      auto rssi = lora.getRSSI();
      auto snr = lora.getSNR();
      bool corrupt = false;
#ifdef GET_CRC_FAILURES
      corrupt = loraSrc._crcMismatch;
#else
      static_assert(false); //Because GET_CRC_FAILURES should be defined if MODEM is defined.
#endif
      KissMessageDestination dst(corrupt);
      dst.appendData(loraSrc, maxPacketSize);
      if (dst.finishAndSend() == MESSAGE_END)
      {
        wdt_reset();
#ifdef WATCHDOG_LOOPS
        watchdogLoops = 0;
#endif
        lastMessage = millis();
      }
      dst.finishAndSend();

      KissMessageDestination stats(false);
      stats.appendByte('X');
      stats.appendByte(0x07); //Message type
      stats.appendByte(0x00); //Station ID
      stats.appendByte(0x00); //Unique ID
      stats.appendT(rssi);
      stats.appendT(snr);
      stats.finishAndSend();
    }
    
    auto thisMillis = millis();
    if (thisMillis - lastMessage > messageInterval &&
        thisMillis - lastFailMillis > messageInterval)
    {
      LORA_CHECK(lora.standby(SX126X_STANDBY_RC));
      LORA_CHECK(csma.enterIdleState());
      lastMessage = millis();
      AWS_DEBUG_PRINTLN(F("No messages, jiggled Radio."));
    }
    
    KissMessageSource kissSrc;
    if (Serial.available() > 0 && kissSrc.beginMessage())
    {
      if (kissSrc.getMessageType() == 0x00)
      {
        byte buffer[254];
        LoraMessageDestination dst(true, buffer, sizeof(buffer));
        auto result = dst.appendData(kissSrc, maxPacketSize);
        if (result != MESSAGE_END)
          dst.abort();
        else
          dst.finishAndSend();
      }
      else if (kissSrc.getMessageType() == 0x06)
      {
        byte desc;
        if (handleMessageCommand(kissSrc, &desc))
          Serial.println(F("Command SUCCESS"));
        else if (desc == 'I')
        {
          KissMessageDestination reply(false);
          reply.appendByte('X');
          reply.appendByte(0x06); //Message type
          reply.appendByte(0x00); //Station ID
          reply.appendByte(0x00); //Unique ID
          appendMessageStatistics(reply);
          reply.finishAndSend();
        }
        else
          Serial.println(F("Command FAILURE"));
      }
    }
  }

  return 0;
}

void yield()
{
  static bool preventRecursion = false;
  if (preventRecursion)
    return;
  preventRecursion = true;
  LORA_CHECK(csma.readIfPossible());
  preventRecursion = false;
}