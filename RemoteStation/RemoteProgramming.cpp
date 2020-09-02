//#include <avr/wdt.h>
#include <util/crc16.h>
#include "ArduinoWeatherStation.h"
#include "LoraMessaging.h"
#include "MessageHandling.h"
#include "Flash.h"

//#define DEBUG_PROGRAMMING
#ifdef DEBUG_PROGRAMMING
#define PROGRAM_PRINT AWS_DEBUG_PRINT
#define PROGRAM_PRINTLN AWS_DEBUG_PRINTLN
#define PROGRAM_PRINTVAR PRINT_VARIABLE
#else
#define PROGRAM_PRINT(...)  do { } while (0)
#define PROGRAM_PRINTLN(...)  do { } while (0)
#define PROGRAM_PRINTVAR(...)  do { } while (0)
#endif

// Remote programming over our home-grown protocols.
// See dual optiboot for actual programmer

namespace RemoteProgramming
{
#if 1 // FLASH_AVAILABLE
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
  void handleDownloadQuery(int uniqueID);
  bool handleProgramConfirm(MessageSource& src, byte uniqueID);
  uint16_t getImageCRC();
  bool isItAllHere();
  void sendAbortMessage();

  bool handleProgrammingCommand(MessageSource& msg, byte uniqueID,
    bool* ackRequired)
  {
    if (!Flash::flashOK)
      return false;

    PROGRAM_PRINTLN(F("Processing program command..."));

    byte type;
    if (msg.readByte(type))
      return false;
    switch (type)
    {
    case 'B': return handleBeginUpdate(msg);
    case 'I': return handleImagePacket(msg);
    case 'Q': handleDownloadQuery(uniqueID);
      *ackRequired = false;
      return true;
    case 'C': return handleProgramConfirm(msg, uniqueID);
    default: return false;
    }
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
    Flash::flash.wakeup();
    Flash::flash.blockErase32K(0x00);
    Flash::flash.sleep();
    return true;
  }

  bool handleImagePacket(MessageSource& msg)
  {
    byte packetIndex;
    if (msg.readByte(packetIndex))
      return false;
    //If out of bounds:
    if (packetIndex >= totalExpectedPackets)
    {
      PROGRAM_PRINTLN(F("RP: Out Of Range"));
      return false;
    }
    //If already received:
    byte* packetIdentifier = &receivedPackets[packetIndex / 8];
    byte testBit = 1 << (packetIndex % 8);
    if (((*packetIdentifier) & testBit) != 0)
    {
      PROGRAM_PRINTLN(F("RP: Duplicate"));
      return false;
    }
    //If packet doesn't belong to the image we're receiving:
    uint16_t imageCRC;
    if (msg.read(imageCRC) || imageCRC != expectedCRC)
    {
      PROGRAM_PRINTLN(F("RP: CRC mismatch"));
      return false;
    }
    //If incorrect length:
    auto msgLen = msg.getMessageLength();
    auto msgLoc = msg.getCurrentLocation();
    if (msgLen - msgLoc != bytesPerPacket)
    {
      PROGRAM_PRINTLN(F("RP: Length mismatch"));
      PROGRAM_PRINTVAR(msgLen);
      PROGRAM_PRINTVAR(msgLoc);
      PROGRAM_PRINTVAR(bytesPerPacket);
      return false;
    }
    unsigned int packetStart = packetIndex * bytesPerPacket + imageOffset;
    unsigned int packetEnd = packetStart + bytesPerPacket;
    Flash::flash.wakeup();
    for (unsigned int i = packetStart; i < packetEnd; i++)
    {
      byte b;
      if (msg.readByte(b))
      {
        resetDownload();
        sendAbortMessage();
        Flash::flash.sleep();
        PROGRAM_PRINTLN(F("RP: read failed"));
        return false;
      }
      Flash::flash.writeByte(i, b);
    }
    Flash::flash.sleep();
    *packetIdentifier |= testBit;

    return true;
  }

  void handleDownloadQuery(int uniqueID)
  {
    LoraMessageDestination<254> msg(false);
      msg.appendByte('K');
    msg.appendByte(stationID);
    msg.appendByte(uniqueID);
    msg.appendByte('P');
    msg.appendByte('Q');
    msg.appendT(expectedCRC);
    msg.appendT(totalExpectedPackets);
    bool allHere = isItAllHere();
    msg.appendT(allHere);
    if (allHere)
    {
      Flash::flash.wakeup();
      msg.appendT(getImageCRC() == expectedCRC);
      Flash::flash.sleep();
    }
    else
      msg.appendT(false);
    msg.append(receivedPackets, sizeof(receivedPackets));
    msg.finishAndSend();
  }

  bool handleProgramConfirm(MessageSource& src, byte uniqueID)
  {
    uint16_t commandCRC;
    if (src.read(commandCRC) || commandCRC != expectedCRC)
      return false;
    if (bytesPerPacket == 0 || totalExpectedPackets == 0)
      return false;
    if (!isItAllHere())
      return false;
    Flash::flash.wakeup();
    if (getImageCRC() != expectedCRC)
      return false;

    LoraMessageDestination<20> msg(false);

      msg.appendByte('K');
    msg.appendByte(stationID);
    msg.appendByte(uniqueID);
    msg.appendByte('P');
    msg.appendByte('C');
    msg.appendT(commandCRC);
    msg.append(F("Programming!"), 12);
    msg.finishAndSend();

    //Write the last little bit for Dual Optiboot to program the image:
    unsigned int imageSize = ((unsigned int)totalExpectedPackets) * bytesPerPacket;
    Flash::flash.writeByte(0x07, (imageSize >> 8) & 0xFF);
    Flash::flash.writeByte(0x08, imageSize & 0xFF);
    Flash::flash.writeBytes(0x00, "FLXIMG:", 7);
    Flash::flash.writeByte(0x09, ':');
    //Wait for the WDT to kick in...
    while (1);
  }

  uint16_t getImageCRC()
  {
    unsigned short crc = 0xFFFF;

    size_t max = totalExpectedPackets * bytesPerPacket + imageOffset;

    for (size_t i = imageOffset; i < max; /*incremented inner loop*/)
    {
      size_t toRead = max - i;
      if (toRead > 64)
        toRead = 64;
      byte buffer[64];
      Flash::flash.readBytes(i, buffer, toRead);
      for (byte j = 0; j < (byte)toRead; j++)
        crc = _crc_ccitt_update(crc, buffer[j]);
      i += toRead;
      //This _used to_ be able to take longer than 8 seconds, but we fixed that.
      //wdt_reset();
    }

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
    LoraMessageDestination<20> msg(false);
    msg.appendByte('K');
    msg.appendByte(stationID);
    msg.appendByte(MessageHandling::getUniqueID());
    msg.appendByte('P');
    msg.append(F("Abort"), 5);
    msg.appendT(expectedCRC);
  }
#else
  bool remoteProgrammingInit() { return false; }
  bool handleProgrammingCommand(MessageSource& msg, byte uniqueID,
    bool* ackRequired)
  {
    return false; 
  }
#endif
}