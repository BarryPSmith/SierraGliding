#include "MessagingCommon.h"

namespace Database
{
  void initDatabase();

  void storeMessage(byte messageType, byte stationID,
    MessageSource& msg);

  void doProcessing();

  bool handleDatabaseCommand(MessageSource& msg, const byte uniqueID,
    bool* ackRequired);
}