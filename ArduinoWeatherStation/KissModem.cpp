#include "revid.h"
#include "KissMessaging.h"
#include "Messaging.h"
#include <spi.h>
#include "PermanentStorage.h"

#define SERIALBAUD 38400


void sendMaxPowerCommand();
extern void *__brkval;
int main()
{
  init();

  randomSeed(analogRead(A0));

  pinMode(A4, OUTPUT);
  digitalWrite(A4, LOW);
        delay(1);
        digitalWrite(A4, HIGH);
        delay(1);
  digitalWrite(A4, LOW);
        delay(1);
        digitalWrite(A4, HIGH);
        delay(1);
  digitalWrite(A4, LOW);

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
  /*Serial.println(F("Lora Parameters:"));
  Serial.print(F("Frequency: "));
  Serial.println(LORA_FREQ);
  Serial.print(F("Bandwidth: "));
  Serial.println(LORA_BW);
  Serial.print(F("Spreading Factor: "));
  Serial.println(LORA_SF);*/
  Serial.print(F("Coding Rate: "));
  Serial.println(LORA_CR);
  Serial.println();
  Serial.println(F("Starting..."));
   
  MessageDestination::prependCallsign = false;
  MessageSource::discardCallsign = false;

  
  
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
    if (/*Serial.available() > 0 &&*/ kissSrc.beginMessage())
    {
      LoraMessageDestination dst;
      if (dst.appendData(kissSrc, maxPacketSize) != MESSAGE_END)
        dst.abort();
      else
        dst.finishAndSend();
    }

    //if (serialEventRun) serialEventRun();
  }

  return 0;
}

void sendMaxPowerCommand()
{
  LoraMessageDestination dest;
  dest.append(F("KN6DUC"), 6);
  dest.appendT('C'); //Command
  dest.appendByte(0); //Station ID (0 = send to all)
  dest.appendByte(0); //Unique ID (0 = Always execute and reset recently heard commands)
  dest.append(F("MP"), 3); //Modem -> Power
  dest.appendT<short>(22); //Maximum power. Don't know why we made it int, but there you go.
  auto res = dest.finishAndSend();
  if (res)
  {
    Serial.print(F("Error sending maxPower message: "));
    Serial.println(res);
  }
}