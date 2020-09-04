#include "lib/SPIFlash/SPIFlash.h"
#include "MessagingCommon.h"
#include "ArduinoWeatherStation.h"

#pragma once
#define FLASH_AVAILABLE defined(FLASH_SELECT)
#define MANUFACTURER_ID 0xEF30

#ifdef DEBUG_FLASH
#define FLASH_PRINT AWS_DEBUG_PRINT
#define FLASH_PRINTLN AWS_DEBUG_PRINTLN
#define FLASH_PRINTVAR PRINT_VARIABLE
#else
#define FLASH_PRINT(...)  do { } while (0)
#define FLASH_PRINTLN(...)  do { } while (0)
#define FLASH_PRINTVAR(...)  do { } while (0)
#endif

#if !FLASH_AVAILABLE
#error It is assumed that flash will always be available
#endif

namespace Flash
{
  extern SPIFlash flash;
  extern bool flashOK;
  
  bool flashInit();
  bool handleFlashCommand(MessageSource& msg, const byte uniqueID,
    bool* ackRequired);  
}