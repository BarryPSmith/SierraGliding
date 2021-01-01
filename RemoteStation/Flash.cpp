#include "Flash.h"
#include "LoraMessaging.h"

namespace Flash
{  
  bool handleReadCommand(MessageSource& msg, const byte uniqueID, uint32_t add, bool* ackRequired);
  bool flashOK = false;
  SPIFlash flash(FLASH_SELECT, MANUFACTURER_ID);

  bool flashInit()
  {
    flashOK = flash.initialize();
    if (flashOK)
      flash.sleep();
    return flashOK;
  }

  bool handleFlashCommand(MessageSource& msg, const byte uniqueID,
    bool* ackRequired)
  {
    if (!flashOK)
      return false;
    FLASH_PRINTLN(F("Processing flash command..."));
    byte rwe;
    uint32_t add;
    if (msg.readByte(rwe) || 
        msg.read(add))
      return false;

#ifndef DEBUG
    if ((rwe == 'W' || rwe == 'E') && add < 32768)
      return false;
#endif

    flash.wakeup();
    auto ret = true;
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
      ret = handleReadCommand(msg, uniqueID, add, ackRequired);
    }
    flash.sleep();
    return ret;
  }
  
  bool handleReadCommand(MessageSource& msg, const byte uniqueID, uint32_t add, bool* ackRequired)
  {
    //FLASH_PRINTLN(F("Flash Read"));
    byte count;
    if (msg.read(count))
      return false;

    char internalOrExt;
    if (msg.read(internalOrExt))
      internalOrExt = 'E';
    bool external = internalOrExt == 'E';
    if (!external && internalOrExt != 'I')
      return false;
    if (!external && (add >= 32768 || add + count > 32768))
      return false;

    LoraMessageDestination<254> dest(false);
    dest.appendByte('K');
    dest.appendByte(stationID);
    dest.appendByte(uniqueID);
    dest.appendByte('F');
    dest.appendByte('R');
    //FLASH_PRINTVAR(add);
    if (external)
    {
      byte* buffer;
      if (dest.getBuffer(&buffer, count))
        return false;
      flash.readBytes(add, buffer, count);
    }
    else
      for (int i = 0; i < count; i++)
      {
        byte b = pgm_read_byte(add++);
        FLASH_PRINT(b, HEX);
        FLASH_PRINT(' ');
        dest.appendByte(b);
      }
    FLASH_PRINTLN();
    *ackRequired = false;
    return true;
  }
}