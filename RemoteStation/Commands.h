#pragma once
#include "MessagingCommon.h"

namespace Commands
{
  void handleCommandMessage(MessageSource& message, byte uniqueID, bool isSpecific);
}