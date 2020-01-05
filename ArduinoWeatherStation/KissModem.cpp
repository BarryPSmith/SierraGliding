#include "revid.h"
#include "KissMessaging.h"
#include "LoraMessaging.h"
#include <spi.h>
#include "PermanentStorage.h"
#include "StackCanary.h"
#include <avr/wdt.h>

#define SERIALBAUD 38400

int main()
{
  init();

  randomSeed(analogRead(A0));
  
  Serial.begin(SERIALBAUD);
  delay(10);

  if (oldSP >= 0x100)
  {
    Serial.print(F("Previous Stack Pointer: "));
    Serial.println(oldSP, HEX);
    Serial.print(F("Previous Stack: "));
    for (int i = 0; i < STACK_DUMP_SIZE; i++)
    {
      Serial.print(oldStack[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
  }

  //Enable the watchdog early to catch initialisation hangs (Side note: This limits initialisation to 8 seconds)
  wdt_enable(WDTO_8S);
  WDTCSR |= (1 << WDIE);

  SPI.begin();
  delay(10);
  Serial.println(XSTR(RADIO_TYPE));
  
  PermanentStorage::initialise();

  InitMessaging();
  
  Serial.print(F("Arduino LoRa modem. Version "));
  Serial.println(REV_ID);
  Serial.println();
  Serial.print(F("Coding Rate: "));
  Serial.println(LORA_CR);
  Serial.println();
  Serial.println(F("Starting..."));
   
  MessageDestination::s_prependCallsign = false;
  MessageSource::s_discardCallsign = false;

  int i =0;
  while (1)
  {
    LoraMessageSource loraSrc;
    if (loraSrc.beginMessage())
    {
      KissMessageDestination dst;
      dst.appendData(loraSrc, maxPacketSize);
      if (dst.finishAndSend() == MESSAGE_OK)
      {
        wdt_reset();
        watchdogLoops = 0;
      }
    }
    
    KissMessageSource kissSrc;
    if (Serial.available() > 0 && kissSrc.beginMessage())
    {
      if (kissSrc.getMessageType() == 0x00)
      {
        LoraMessageDestination dst(true);
        auto result = dst.appendData(kissSrc, maxPacketSize);
        if (result != MESSAGE_END)
          dst.abort();
        else
          dst.finishAndSend();
      }
      else if (kissSrc.getMessageType() == 0x06)
      {
        if (HandleMessageCommand(kissSrc))
          Serial.println(F("Command SUCCESS"));
        else
          Serial.println(F("Command FAILURE"));
      }
    }

    //if (serialEventRun) serialEventRun();
  }

  return 0;
}