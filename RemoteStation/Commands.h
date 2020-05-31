#pragma once
#include "MessagingCommon.h"

namespace Commands
{
  void handleCommandMessage(MessageSource& message, bool demandRelay, byte uniqueID, bool isSpecific);
}