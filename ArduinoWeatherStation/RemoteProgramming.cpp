#include "lib/SPIFlash/SPIFlash.h"
#include <util/crc16.h>
#include "ArduinoWeatherStation.h"
#include "LoraMessaging.h"
#include "MessageHandling.h"

// Remote programming over our home-grown protocols.
// See dual optiboot for actual programmer

namespace RemoteProgramming
{
#if 1 //defined(FLASH_SELECT)
#define MANUFACTURER_ID 0xEF30
  SPIFlash flash(FLASH_SELECT, MANUFACTURER_ID);
  bool flashOK = false;

  bool remoteProgrammingInit()
  {
    flashOK = flash.initialize();
    if (flashOK)
      flash.sleep();
    return flashOK;
  }
  
  //Dual Optiboot expects a header like:
  //0123456789<10+>
  //FLXIMG:SS:<Image>
  //Where SS is image size.
  constexpr unsigned int imageOffset = 10;
  constexpr byte maxPacketCount = 192;
  byte receivedPackets[maxPacketCount / 8];
  byte bytesPerPacket;
  byte totalExpectedPackets;
  uint16_t expectedCRC;

  bool handleBeginUpdate(MessageSource& msg);
  void resetDownload();
  bool handleBeginUpdateInternal(MessageSource& msg);
  bool handleBeginUpdateInternal(MessageSource& msg);
  bool handleImagePacket(MessageSource& msg);
  void handleDownloadQuery(int uniqueID, bool demandRelay);
  bool handleProgramConfirm(MessageSource& src, byte uniqueID, bool demandRelay);
  uint16_t getImageCRC();
  bool isItAllHere();
  void sendAbortMessage();
  bool handleFlashCommand(MessageSource& msg, const byte uniqueID, const bool demandRelay,
    bool* ackRequired);

  bool handleProgrammingCommand(MessageSource& msg, byte uniqueID, bool demandRelay,
    bool* ackRequired)
  {
    if (!flashOK)
      return false;

    AWS_DEBUG_PRINTLN(F("Processing program command..."));

    byte type;
    if (msg.readByte(type))
      return false;
    switch (type)
    {
    case 'B': return handleBeginUpdate(msg);
    case 'I': return handleImagePacket(msg);
    case 'Q': handleDownloadQuery(uniqueID, demandRelay);
      *ackRequired = false;
      return true;
    case 'C': return handleProgramConfirm(msg, uniqueID, demandRelay);
    case 'F': return handleFlashCommand(msg, uniqueID, demandRelay, ackRequired);
    default: return false;
    }
  }

  bool handleFlashCommand(MessageSource& msg, const byte uniqueID, const bool demandRelay,
    bool* ackRequired)
  {
    AWS_DEBUG_PRINTLN(F("Processing flash command..."));
    byte rwe;
    uint16_t add;
    byte count;
    if (msg.readByte(rwe) || 
        msg.read(add))
      return false;

#ifndef DEBUG
    if (rwe == 'W' || rwe == 'E' && add < 32768)
      return false;
#endif

    flash.wakeup();
    switch (rwe)
    {
    case 'E':
      flash.blockErase4K(add);
      break;
    case 'W':
      byte b;
      while (!msg.readByte(b))
        flash.writeByte(add++, b);
      break;
    case 'R':
      {
      byte count;
      if (msg.read(count))
      {
        flash.sleep();
        return false;
      }
      LoraMessageDestination dest(false);
      dest.appendByte('K');
      dest.appendByte(stationID);
      dest.appendByte(uniqueID);
      for (int i = 0; i < count; i++)
        dest.appendByte(flash.readByte(add++));
      }
      *ackRequired = false;
    }
    flash.sleep();
    return true;
  }

  bool handleBeginUpdate(MessageSource& msg)
  {
    resetDownload();
    if (!handleBeginUpdateInternal(msg))
    {
      resetDownload();
      return false;
    }
    return true;
  }

  void resetDownload()
  {
    bytesPerPacket = 0;
    totalExpectedPackets = 0;
    expectedCRC = 0;
    memset(receivedPackets, 0, sizeof(receivedPackets));
  }

  bool handleBeginUpdateInternal(MessageSource& msg)
  {
    if (msg.readByte(totalExpectedPackets))
      return false;
    if (totalExpectedPackets > maxPacketCount)
      return false;
    if (msg.readByte(bytesPerPacket))
      return false;
    if (msg.read(expectedCRC))
      return false;
    flash.wakeup();
    flash.blockErase32K(0x00);
    flash.sleep();
    return true;
  }

  bool handleImagePacket(MessageSource& msg)
  {
    byte packetIndex;
    if (msg.readByte(packetIndex))
      return false;
    //If out of bounds:
    if (packetIndex >= totalExpectedPackets)
      return false;
    //If already received:
    if (receivedPackets[packetIndex / 8] & 1 << packetIndex % 8 != 0)
      return false;
    //If packet doesn't belong to the image we're receiving:
    uint16_t imageCRC;
    if (msg.read(imageCRC) || imageCRC != expectedCRC)
      return false;
    //If incorrect length:
    if (msg.getMessageLength() - msg.getCurrentLocation() != bytesPerPacket)
      return false;
    unsigned int packetStart = packetIndex * bytesPerPacket + imageOffset;
    unsigned int packetEnd = packetStart + bytesPerPacket;
    flash.wakeup();
    for (unsigned int i = packetStart; i < packetEnd; i++)
    {
      byte b;
      if (msg.readByte(b))
      {
        resetDownload();
        sendAbortMessage();
        flash.sleep();
        return false;
      }
      flash.writeByte(i, b);
    }
    flash.sleep();
    return true;
  }

  void handleDownloadQuery(int uniqueID, bool demandRelay)
  {
    LoraMessageDestination msg(false);
    if (demandRelay)
      msg.appendByte('K' & 0x80);
    else
      msg.appendByte('K');
    msg.appendByte(stationID);
    msg.appendByte(uniqueID);
    msg.appendT(expectedCRC);
    msg.appendT(totalExpectedPackets);
    bool allHere = isItAllHere();
    msg.appendT(allHere);
    if (allHere)
    {
      flash.wakeup();
      msg.appendT(getImageCRC() == expectedCRC);
      flash.sleep();
    }
    else
      msg.appendT(false);
    msg.append(receivedPackets, sizeof(receivedPackets));
    msg.finishAndSend();
  }

  bool handleProgramConfirm(MessageSource& src, byte uniqueID, bool demandRelay)
  {
    uint16_t commandCRC;
    if (src.read(commandCRC) || commandCRC != expectedCRC)
      return false;
    if (bytesPerPacket == 0 || totalExpectedPackets == 0)
      return false;
    if (!isItAllHere())
      return false;
    flash.wakeup();
    if (getImageCRC() != expectedCRC)
      return false;

    LoraMessageDestination msg(false);
    if (demandRelay)
      msg.appendByte('K' & 0x80);
    else
      msg.appendByte('K');
    msg.appendByte(stationID);
    msg.appendByte(uniqueID);
    msg.appendT(commandCRC);
    msg.append(F("Programming!"), 12);
    msg.finishAndSend();

    //Write the last little bit for Dual Optiboot to program the image:
    unsigned int imageSize = totalExpectedPackets * bytesPerPacket;
    flash.writeByte(0x07, imageSize >> 8);
    flash.writeByte(0x08, imageSize & 0xFF);
    flash.writeBytes(0x00, "FLXIMG:", sizeof("FLXIMG:"));
    flash.writeByte(0x09, ':');
    //Wait for the WDT to kick in...
    while (1);
  }

  uint16_t getImageCRC()
  {
    unsigned short crc = 0xFFFF;

    size_t max = totalExpectedPackets * bytesPerPacket + imageOffset;

    for (size_t i = imageOffset; i < max; i++)
      crc = _crc_ccitt_update(crc, flash.readByte(i));

    return crc;
  }

  bool isItAllHere()
  {
    for (byte i = 0; i < totalExpectedPackets / 8; i++)
    {
      if (receivedPackets[i] != 0xFF)
        return false;
    }
    for (byte i = 0; i < totalExpectedPackets % 8; i++)
    {
      if (receivedPackets[totalExpectedPackets / 8] & 1 << i == 0)
        return false;
    }
    return true;
  }

  void sendAbortMessage()
  {
    LoraMessageDestination msg(false);
    msg.appendByte('K');
    msg.appendByte(stationID);
    msg.appendByte(MessageHandling::getUniqueID());
    msg.append(F("Abort"), 5);
    msg.appendT(expectedCRC);
  }
#else
  bool remoteProgrammingInit() { return false; }
  bool handleProgrammingCommand(MessageSource& msg, byte uniqueID, bool demandRelay,
    bool* ackRequired)
  {
    return false; 
  }
#endif
}