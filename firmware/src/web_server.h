#pragma once

#include <Arduino.h>

namespace stagecue {

void startWebServer();
void notifyCueState(uint8_t index, const String &text, bool active);
void notifyAllCueStates();

}  // namespace stagecue

