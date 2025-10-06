#include "wifi_portal.h"

#include <Preferences.h>
#include <WiFi.h>

#include "config.h"

namespace stagecue {

namespace {

constexpr char kWifiPreferencesNamespace[] = "wifi";
constexpr char kWifiSsidKey[] = "ssid";
constexpr char kWifiPassKey[] = "pass";

Preferences gWifiPreferences;
bool gWifiPreferencesReady = false;

bool ensurePreferences() {
  if (gWifiPreferencesReady) {
    return true;
  }
  gWifiPreferencesReady = gWifiPreferences.begin(kWifiPreferencesNamespace, false);
  if (!gWifiPreferencesReady) {
    Serial.println(F("[WiFi] Unable to open preferences"));
  }
  return gWifiPreferencesReady;
}

bool loadCredentials(String &ssid, String &password) {
  if (!ensurePreferences()) {
    return false;
  }

  if (!gWifiPreferences.isKey(kWifiSsidKey)) {
    return false;
  }

  ssid = gWifiPreferences.getString(kWifiSsidKey);
  password = gWifiPreferences.getString(kWifiPassKey);
  return ssid.length() > 0;
}

}  // namespace

bool saveWifiCredentials(const String &ssid, const String &password) {
  if (ssid.isEmpty()) {
    Serial.println(F("[WiFi] SSID cannot be empty"));
    return false;
  }

  if (!ensurePreferences()) {
    return false;
  }

  gWifiPreferences.putString(kWifiSsidKey, ssid);
  gWifiPreferences.putString(kWifiPassKey, password);
  Serial.println(F("[WiFi] Credentials stored"));
  return true;
}

bool connectToSavedNetwork(uint32_t timeoutMs) {
  String ssid;
  String password;
  if (!loadCredentials(ssid, password)) {
    Serial.println(F("[WiFi] No saved credentials"));
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), password.c_str());

  Serial.print(F("[WiFi] Connecting to "));
  Serial.println(ssid);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[WiFi] Connected. IP: "));
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println(F("[WiFi] Connection timeout"));
  return false;
}

void startFallbackAccessPoint() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  if (!WiFi.softAP(kFallbackApSsid, kFallbackApPass)) {
    Serial.println(F("[WiFi] Failed to start access point"));
    return;
  }
  WiFi.softAPsetHostname("StageCue-AP");

  Serial.println(F("[WiFi] Access point active"));
  Serial.print(F("[WiFi] SSID: "));
  Serial.println(kFallbackApSsid);
  Serial.print(F("[WiFi] IP: "));
  Serial.println(WiFi.softAPIP());
}

bool startWiFiWithPortal() {
  WiFi.persistent(false);
  WiFi.setHostname("StageCue");
  if (connectToSavedNetwork()) {
    return true;
  }

  startFallbackAccessPoint();
  return false;
}

}  // namespace stagecue

