#pragma once

#include <Arduino.h>

namespace stagecue {

bool startWiFiWithPortal();
bool saveWifiCredentials(const String &ssid, const String &password);
bool connectToSavedNetwork(uint32_t timeoutMs = 10000);
void startFallbackAccessPoint();

}  // namespace stagecue

