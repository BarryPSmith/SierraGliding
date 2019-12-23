#include "revid.h"
#include "KissMessaging.h"
#include "LoraMessaging.h"
#include <spi.h>
#include "PermanentStorage.h"
#include "StackCanary.h"

#define SERIALBAUD 38400

int main()
{
  init();

  randomSeed(analogRead(A0));
  
  Serial.begin(SERIALBAUD);
  delay(10);

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

  
  
  //sendMaxPowerCommand();
  //Serial.println(F("Max Power command sent"));

  int i =0;
  while (1)
  {
    LoraMessageSource loraSrc;
    if (loraSrc.beginMessage())
    {
      KissMessageDestination dst;
      dst.appendData(loraSrc, maxPacketSize);
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