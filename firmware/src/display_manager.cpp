#include "display_manager.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <array>

#include "config.h"

namespace stagecue {

namespace {

static_assert(kCueCount == 3, "Display array initialisers must match cue count");

std::array<Adafruit_SSD1306, kCueCount> gDisplays = {
    Adafruit_SSD1306(kScreenWidth, kScreenHeight, &Wire, -1),
    Adafruit_SSD1306(kScreenWidth, kScreenHeight, &Wire, -1),
    Adafruit_SSD1306(kScreenWidth, kScreenHeight, &Wire, -1),
};

std::array<bool, kCueCount> gDisplayReady{};

}  // namespace

bool initDisplay() {
  Wire.begin();

  bool allReady = true;
  for (uint8_t i = 0; i < kCueCount; ++i) {
    const uint8_t address = static_cast<uint8_t>(kOledBaseAddress + i);
    if (!gDisplays[i].begin(SSD1306_SWITCHCAPVCC, address)) {
      Serial.print(F("[Display] Failed to init OLED at 0x"));
      Serial.println(address, HEX);
      gDisplayReady[i] = false;
      allReady = false;
      continue;
    }

    gDisplayReady[i] = true;
    clearDisplay(i);
  }

  return allReady;
}

void updateDisplay(uint8_t index, const String &text) {
  if (index >= gDisplays.size() || !gDisplayReady[index]) {
    return;
  }

  auto &display = gDisplays[index];
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();
}

void clearDisplay(uint8_t index) {
  if (index >= gDisplays.size() || !gDisplayReady[index]) {
    return;
  }

  auto &display = gDisplays[index];
  display.clearDisplay();
  display.display();
}

}  // namespace stagecue

