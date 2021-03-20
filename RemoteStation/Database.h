#include "MessagingCommon.h"

namespace Database
{
  void initDatabase();

  void storeMessage(byte messageType, byte stationID,
    MessageSource& msg);

  void storeData(byte messageType, byte stationID,
    byte* buffer, byte byteCount);

  void doProcessing();

  bool handleDatabaseCommand(MessageSource& msg, const byte uniqueID,
    bool* ackRequired);

  extern unsigned long _curHeaderAddress;
  extern unsigned long _curWriteAddress;
  extern byte _curCycle;
}