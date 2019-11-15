#include "revid.h"
#include "KissMessaging.h"
#include "Messaging.h"
#include <spi.h>

#define SERIALBAUD 38400

int main()
{
  init();

  Serial.begin(SERIALBAUD);
  delay(10);
  SPI.begin();
  delay(10);
  
  InitMessaging();

  Serial.print("Arduino LoRa modem. Version ");
  Serial.println(REV_ID);
  Serial.println();
  Serial.println("Lora Parameters:");
  Serial.print("Frequency: ");
  Serial.println(LORA_FREQ);
  Serial.print("Bandwidth: ");
  Serial.println(LORA_BW);
  Serial.print("Spreading Factor: ");
  Serial.println(LORA_SF);
  Serial.print("Coding Rate: ");
  Serial.println(LORA_CR);
  Serial.println();
  Serial.println("Starting...");

  MessageDestination::prependCallsign = false;
  MessageSource::discardCallsign = false;

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
      Serial.println("KISS Message Received.");
      LoraMessageDestination dst;
      dst.appendData(kissSrc, maxPacketSize);
    }

    if (serialEventRun) serialEventRun();
  }

  return 0;
}