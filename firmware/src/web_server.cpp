#include "web_server.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "config.h"
#include "cues.h"
#include "wifi_portal.h"

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#else
#error "StageCue firmware requires an ESP32-class target."
#endif

namespace stagecue {

namespace {

constexpr size_t kMaxIncomingMessageSize = 512U;
AsyncWebServer gServer(80);
AsyncWebSocket gWebSocket("/ws");

String wifiModeToString(wifi_mode_t mode) {
  switch (mode) {
    case WIFI_MODE_NULL:
      return F("off");
    case WIFI_MODE_STA:
      return F("station");
    case WIFI_MODE_AP:
      return F("ap");
    case WIFI_MODE_APSTA:
      return F("ap+sta");
    default:
      return F("unknown");
  }
}

void sendJson(AsyncWebSocketClient &client, const JsonDocument &doc) {
  String payload;
  serializeJson(doc, payload);
  client.text(payload);
}

void broadcastJson(const JsonDocument &doc) {
  if (gWebSocket.count() == 0) {
    return;
  }
  String payload;
  serializeJson(doc, payload);
  gWebSocket.textAll(payload);
}

void sendAck(AsyncWebSocketClient &client, const char *action,
             bool success, const char *detail = nullptr) {
  StaticJsonDocument<160> doc;
  doc["type"] = "ack";
  doc["action"] = action;
  doc["ok"] = success;
  if (detail != nullptr) {
    doc["detail"] = detail;
  }
  sendJson(client, doc);
}

void sendError(AsyncWebSocketClient &client, const char *action,
               const char *detail) {
  sendAck(client, action, false, detail);
}

void publishCueState(uint8_t index) {
  const auto &state = getCueState(index);
  StaticJsonDocument<192> doc;
  doc["type"] = "cue";
  doc["index"] = index;
  doc["text"] = gCueTexts[index];
  doc["active"] = state.active;
  doc["updatedAt"] = state.lastChangeMs;
  broadcastJson(doc);
}

void handleTriggerRequest(uint8_t index, const char *text,
                          AsyncWebSocketClient &client) {
  if (index >= kCueCount) {
    sendError(client, "trigger", "invalid cue index");
    return;
  }

  if (text != nullptr) {
    setCueText(index, text);
  }

  triggerCue(index);
  sendAck(client, "trigger", true);
}

void handleReleaseRequest(uint8_t index, AsyncWebSocketClient &client) {
  if (index >= kCueCount) {
    sendError(client, "release", "invalid cue index");
    return;
  }

  releaseCue(index);
  sendAck(client, "release", true);
}

void handleRenameRequest(uint8_t index, const char *text,
                         AsyncWebSocketClient &client) {
  if (index >= kCueCount) {
    sendError(client, "rename", "invalid cue index");
    return;
  }

  const char *safeText = text != nullptr ? text : "";
  setCueText(index, safeText);
  sendAck(client, "rename", true);
  publishCueState(index);
}

void sendInitialState(AsyncWebSocketClient &client) {
  StaticJsonDocument<512> doc;
  doc["type"] = "init";

  JsonArray cues = doc.createNestedArray("cues");
  for (uint8_t i = 0; i < kCueCount; ++i) {
    const auto &state = getCueState(i);
    JsonObject cue = cues.createNestedObject();
    cue["index"] = i;
    cue["text"] = gCueTexts[i];
    cue["active"] = state.active;
  }

  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["mode"] = wifiModeToString(WiFi.getMode());
  wifi["ip"] = WiFi.getMode() == WIFI_MODE_AP ? WiFi.softAPIP().toString()
                                                : WiFi.localIP().toString();

  sendJson(client, doc);
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  (void)server;

  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("[WS] client #%u connected\n", client->id());
      sendInitialState(*client);
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] client #%u disconnected\n", client->id());
      break;

    case WS_EVT_ERROR:
      Serial.printf("[WS] error on client #%u\n", client->id());
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);
      if (!(info->final && info->index == 0 && info->len == len &&
            info->opcode == WS_TEXT)) {
        sendError(*client, "parse", "unsupported frame");
        return;
      }

      if (len > kMaxIncomingMessageSize) {
        sendError(*client, "parse", "payload too large");
        return;
      }

      String payload;
      payload.reserve(len + 1);
      payload.concat(reinterpret_cast<const char *>(data), len);

      DynamicJsonDocument doc(kMaxIncomingMessageSize);
      const auto error = deserializeJson(doc, payload);
      if (error) {
        sendError(*client, "parse", error.c_str());
        return;
      }

      const char *typeValue = doc["type"] | nullptr;
      if (typeValue == nullptr) {
        sendError(*client, "parse", "missing type");
        return;
      }

      const uint8_t cueIndex = static_cast<uint8_t>(doc["cue"] | 0);
      const char *textValue = doc["text"] | nullptr;

      if (strcmp(typeValue, "trigger") == 0) {
        handleTriggerRequest(cueIndex, textValue, *client);
      } else if (strcmp(typeValue, "release") == 0) {
        handleReleaseRequest(cueIndex, *client);
      } else if (strcmp(typeValue, "rename") == 0) {
        handleRenameRequest(cueIndex, textValue, *client);
      } else if (strcmp(typeValue, "ping") == 0) {
        sendAck(*client, "ping", true);
      } else {
        sendError(*client, "parse", "unknown type");
      }
      break;
    }

    case WS_EVT_PONG:
      break;
  }
}

void registerHttpRoutes() {
  gServer.serveStatic("/", LittleFS, "/")
      .setDefaultFile("index.html")
      .setCacheControl("max-age=3600, public");

  gServer.on("/api/cues", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<384> doc;
    JsonArray cues = doc.createNestedArray("cues");
    for (uint8_t i = 0; i < kCueCount; ++i) {
      const auto &state = getCueState(i);
      JsonObject cue = cues.createNestedObject();
      cue["index"] = i;
      cue["text"] = gCueTexts[i];
      cue["active"] = state.active;
    }
    doc["count"] = kCueCount;

    String payload;
    serializeJson(doc, payload);
    auto *response = request->beginResponse(200, "application/json", payload);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  gServer.on("/api/cues/trigger", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("cue", true)) {
      request->send(400, "text/plain", "Missing cue parameter");
      return;
    }

    const uint8_t index = static_cast<uint8_t>(request->getParam("cue", true)->value().toInt());
    const String text = request->hasParam("text", true)
                            ? request->getParam("text", true)->value()
                            : String();

    if (index >= kCueCount) {
      request->send(400, "text/plain", "Invalid cue index");
      return;
    }

    if (text.length() > 0) {
      setCueText(index, text);
    }
    triggerCue(index);
    auto *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  gServer.on("/api/cues/release", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("cue", true)) {
      request->send(400, "text/plain", "Missing cue parameter");
      return;
    }

    const uint8_t index = static_cast<uint8_t>(request->getParam("cue", true)->value().toInt());
    if (index >= kCueCount) {
      request->send(400, "text/plain", "Invalid cue index");
      return;
    }

    releaseCue(index);
    auto *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  gServer.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    const int16_t networkCount = WiFi.scanNetworks();
    DynamicJsonDocument doc(1024);
    JsonArray networks = doc.to<JsonArray>();
    for (int16_t i = 0; i < networkCount; ++i) {
      JsonObject entry = networks.createNestedObject();
      entry["ssid"] = WiFi.SSID(i);
      entry["rssi"] = WiFi.RSSI(i);
      entry["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();

    String payload;
    serializeJson(networks, payload);
    auto *response = request->beginResponse(200, "application/json", payload);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  gServer.on("/save_wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid", true) || !request->hasParam("password", true)) {
      request->send(400, "text/plain", "Missing credentials");
      return;
    }

    const String ssid = request->getParam("ssid", true)->value();
    const String password = request->getParam("password", true)->value();

    if (!saveWifiCredentials(ssid, password)) {
      request->send(500, "text/plain", "Unable to persist credentials");
      return;
    }

    auto *response =
        request->beginResponse(200, "text/plain", "Credentials saved. Rebooting...");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
    delay(100);
    ESP.restart();
  });

  gServer.on("/api/health", HTTP_GET, [](AsyncWebServerRequest *request) {
    auto *response = request->beginResponse(200, "application/json", "{\"status\":\"ok\"}");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  gServer.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });
}

}  // namespace

void startWebServer() {
  if (!LittleFS.begin()) {
    Serial.println(F("[Web] LittleFS mount failed, attempting format"));
    if (!LittleFS.begin(true)) {
      Serial.println(F("[Web] Unable to mount LittleFS"));
      return;
    }
  }

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  registerHttpRoutes();

  gWebSocket.onEvent(onWebSocketEvent);
  gServer.addHandler(&gWebSocket);

  gServer.begin();
  Serial.println(F("[Web] HTTP server started on port 80"));
}

void notifyCueState(uint8_t index, const String &text, bool active) {
  StaticJsonDocument<192> doc;
  doc["type"] = "cue";
  doc["index"] = index;
  doc["text"] = text;
  doc["active"] = active;
  doc["updatedAt"] = millis();
  broadcastJson(doc);
}

void notifyAllCueStates() {
  StaticJsonDocument<512> doc;
  doc["type"] = "snapshot";
  JsonArray cues = doc.createNestedArray("cues");
  for (uint8_t i = 0; i < kCueCount; ++i) {
    const auto &state = getCueState(i);
    JsonObject cue = cues.createNestedObject();
    cue["index"] = i;
    cue["text"] = gCueTexts[i];
    cue["active"] = state.active;
    cue["updatedAt"] = state.lastChangeMs;
  }
  broadcastJson(doc);
}

}  // namespace stagecue

