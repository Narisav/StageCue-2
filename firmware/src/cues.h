#pragma once

#include <Arduino.h>
#include <array>

#include "config.h"

namespace stagecue {

struct CueState {
  bool active = false;
  uint32_t lastChangeMs = 0;
};

extern std::array<String, kCueCount> gCueTexts;

void initCues();
void updateCues();
void triggerCue(uint8_t index);
void releaseCue(uint8_t index);
void setCueText(uint8_t index, const String &text, bool persist = true);
const CueState &getCueState(uint8_t index);

}  // namespace stagecue

