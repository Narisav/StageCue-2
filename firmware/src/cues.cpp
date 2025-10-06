#include "cues.h"

#include <Preferences.h>
#include <array>

#include "display_manager.h"
#include "web_server.h"

namespace stagecue {

namespace {

constexpr char kCuePreferencesNamespace[] = "cue_texts";

std::array<CueState, kCueCount> gCueStates{};
std::array<bool, kCueCount> gLastButtonState{};
std::array<uint32_t, kCueCount> gLastButtonChangeMs{};
Preferences gCuePreferences;
bool gPreferencesReady = false;

inline const char *cuePreferenceKey(uint8_t index) {
  static char key[8];
  snprintf(key, sizeof(key), "cue%u", static_cast<unsigned>(index));
  return key;
}

void persistCueText(uint8_t index, const String &text) {
  if (!gPreferencesReady) {
    return;
  }
  gCuePreferences.putString(cuePreferenceKey(index), text);
}

void applyCueState(uint8_t index, bool active) {
  if (index >= kCueCount) {
    return;
  }

  if (gCueStates[index].active == active && gCueStates[index].lastChangeMs != 0U) {
    return;
  }

  gCueStates[index].active = active;
  gCueStates[index].lastChangeMs = millis();
  digitalWrite(kCueLEDs[index], active ? HIGH : LOW);
  notifyCueState(index, gCueTexts[index], active);
}

void ensureButtonDefaults(uint8_t index) {
  gLastButtonState[index] = digitalRead(kCueButtons[index]) == HIGH;
  gLastButtonChangeMs[index] = millis();
}

}  // namespace

std::array<String, kCueCount> gCueTexts{};

void initCues() {
  for (size_t i = 0; i < kCueCount; ++i) {
    pinMode(kCueLEDs[i], OUTPUT);
    digitalWrite(kCueLEDs[i], LOW);
    pinMode(kCueButtons[i], INPUT_PULLUP);
  }

  if (gCuePreferences.begin(kCuePreferencesNamespace, false)) {
    gPreferencesReady = true;
  } else {
    Serial.println(F("[Cues] Unable to open preferences storage"));
  }

  for (size_t i = 0; i < kCueCount; ++i) {
    if (gPreferencesReady && gCuePreferences.isKey(cuePreferenceKey(i))) {
      gCueTexts[i] = gCuePreferences.getString(cuePreferenceKey(i));
    } else {
      gCueTexts[i] = kDefaultCueTexts[i];
    }

    ensureButtonDefaults(i);
    updateDisplay(static_cast<uint8_t>(i), gCueTexts[i]);
    applyCueState(static_cast<uint8_t>(i), false);
  }
}

void updateCues() {
  const uint32_t now = millis();

  for (uint8_t i = 0; i < kCueCount; ++i) {
    const bool currentLevel = digitalRead(kCueButtons[i]) == HIGH;

    if (currentLevel != gLastButtonState[i]) {
      if (now - gLastButtonChangeMs[i] >= kButtonDebounceMillis) {
        gLastButtonState[i] = currentLevel;
        gLastButtonChangeMs[i] = now;

        if (!currentLevel) {  // button pressed (active low)
          triggerCue(i);
        } else {  // button released
          if (gCueStates[i].active) {
            releaseCue(i);
          }
        }
      }
    }

    if (gCueStates[i].active &&
        kCueAutoReleaseMillis > 0U &&
        now - gCueStates[i].lastChangeMs >= kCueAutoReleaseMillis) {
      releaseCue(i);
    }
  }
}

void triggerCue(uint8_t index) {
  if (index >= kCueCount) {
    return;
  }

  updateDisplay(index, gCueTexts[index]);
  applyCueState(index, true);
}

void releaseCue(uint8_t index) {
  if (index >= kCueCount) {
    return;
  }

  applyCueState(index, false);
}

void setCueText(uint8_t index, const String &text, bool persist) {
  if (index >= kCueCount) {
    return;
  }

  if (text.length() > 0) {
    gCueTexts[index] = text;
  } else {
    gCueTexts[index] = kDefaultCueTexts[index];
  }

  updateDisplay(index, gCueTexts[index]);

  if (persist) {
    persistCueText(index, gCueTexts[index]);
  }
}

const CueState &getCueState(uint8_t index) {
  static CueState invalidState{};
  if (index >= kCueCount) {
    return invalidState;
  }
  return gCueStates[index];
}

}  // namespace stagecue

