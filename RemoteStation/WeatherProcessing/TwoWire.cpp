#include <util/twi.h>
#include "TwoWire.h"
#include <Arduino.h>

byte wire_count;

constexpr byte GetWireClock(long desiredSpeed)
{
  //FClock = F_CPU / (16 + 2 * TWBR * Prescaler)
  //TWBR = (F_CPU / FClock - 16) / 2
  auto test = (long)F_CPU / desiredSpeed - 16;
  if (test < 2)
    return 1;
  else if (test < 512)
    return test / 2;
  else
    return 255;
}

byte Wire_read()
{
  bool ack = --wire_count > 0;
  if (ack)
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWEA);
  else
    TWCR = _BV(TWINT) | _BV(TWEN);
  while (!(TWCR & _BV(TWINT)));
  if (ack && TW_STATUS != TW_MR_DATA_ACK)
    return 0xFF;
  else if (!ack && TW_STATUS != TW_MR_DATA_NACK)
    return 0xFF;
  return TWDR;
}

void Wire_endTransmission(bool close)
{
  if (!close)
    return;
  delayMicroseconds(5);
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO); // | _BV(TWEA);
  while (TWCR & _BV(TWSTO));
}

void Wire_requestFrom(byte address, byte count)
{
  //Send START
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTA);
  //Wait for START
  while (!(TWCR & _BV(TWINT)));
  byte status = TW_STATUS;
  if (status != TW_START && status != TW_REP_START)
    return;
  //Set slave address + read
  TWDR = (address << 1) | TW_READ;
  TWCR = _BV(TWINT) | _BV(TWEN);
  //Wait for SLA_R
  while (!(TWCR & _BV(TWINT)));
  if (TW_STATUS != TW_MR_SLA_ACK)
    return;
  wire_count = count;
}

void Wire_beginTransmission(byte address)
{
  //Send START
  TWCR = _BV(TWEN) | _BV(TWSTA);
  TWCR |= _BV(TWINT);
  //Wait for START
  while (!(TWCR & _BV(TWINT)));
  byte status = TW_STATUS;
  if (status != TW_START && status != TW_REP_START)
    return;
  //Set slave address + write
  TWDR = (address << 1) | TW_WRITE;
  TWCR = _BV(TWINT) | _BV(TWEN);
  //Wait for SLA_W
  while (!(TWCR & _BV(TWINT)));
  if (TW_STATUS != TW_MT_SLA_ACK)
    return;
}

void Wire_write(byte data)
{
  TWDR = data;
  TWCR = _BV(TWINT) | _BV(TWEN);
  while (!(TWCR & _BV(TWINT)));
  if (TW_STATUS != TW_MT_DATA_ACK)
    return;
}

void Wire_begin()
{
  // activate internal pullups for twi.
  digitalWrite(SDA, 1);
  digitalWrite(SCL, 1);
  TWBR = GetWireClock(100000);
  TWCR = _BV(TWEN);
}