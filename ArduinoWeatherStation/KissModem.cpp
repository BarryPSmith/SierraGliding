#include "revid.h"
#include "KissMessaging.h"
#include "Messaging.h"
#include <spi.h>

#define SERIALBAUD 38400

void sendMaxPowerCommand();

int main()
{
  init();

  Serial.begin(SERIALBAUD);
  delay(10);
  SPI.begin();
  delay(10);
  Serial.println(XSTR(RADIO_TYPE));
  
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

  
  
  sendMaxPowerCommand();
  Serial.println("Max Power command sent");

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
      sendMaxPowerCommand();
      LoraMessageDestination dst;
      dst.appendData(kissSrc, maxPacketSize);
    }

    if (serialEventRun) serialEventRun();
  }

  return 0;
}

void sendMaxPowerCommand()
{
  LoraMessageDestination dest;
  dest.append("KN6DUC", 6);
  dest.appendT('C'); //Command
  dest.appendByte(0); //Station ID (0 = send to all)
  dest.appendByte(0); //Unique ID (0 = Always execute and reset recently heard commands)
  dest.append("MP", 3); //Modem -> Power
  dest.appendT<short>(22); //Maximum power. Don't know why we made it int, but there you go.
  auto res = dest.finishAndSend();
  if (res)
  {
    Serial.print("Error sending maxPower message: ");
    Serial.println(res);
  }
}