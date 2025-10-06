#pragma once

#include <Arduino.h>

namespace stagecue {

bool initDisplay();
void updateDisplay(uint8_t index, const String &text);
void clearDisplay(uint8_t index);

}  // namespace stagecue

