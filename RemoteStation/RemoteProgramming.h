#pragma once
#include "MessagingCommon.h"

namespace RemoteProgramming
{
  bool handleProgrammingCommand(MessageSource& msg, byte uniqueID,
    bool* ackRequired);
}