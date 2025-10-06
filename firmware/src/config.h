#pragma once

#include <Arduino.h>
#include <stddef.h>

namespace stagecue {

// ──────────────────────────────────────────────────────────────────────────────
// Wi-Fi configuration
// ──────────────────────────────────────────────────────────────────────────────
inline constexpr char kFallbackApSsid[] = "CueLight_AP";
inline constexpr char kFallbackApPass[] = "12345678";

// ──────────────────────────────────────────────────────────────────────────────
// Display configuration
// ──────────────────────────────────────────────────────────────────────────────
inline constexpr uint8_t kScreenWidth = 128;
inline constexpr uint8_t kScreenHeight = 64;
inline constexpr uint8_t kOledBaseAddress = 0x3C;

// ──────────────────────────────────────────────────────────────────────────────
// Cue configuration
// ──────────────────────────────────────────────────────────────────────────────
inline constexpr size_t kCueCount = 3U;
inline constexpr uint8_t kCueLEDs[kCueCount] = {25, 26, 27};
inline constexpr uint8_t kCueButtons[kCueCount] = {32, 33, 34};
inline constexpr uint32_t kCueAutoReleaseMillis = 1500U;
inline constexpr uint32_t kButtonDebounceMillis = 50U;

extern const char *const kDefaultCueTexts[kCueCount];

}  // namespace stagecue

