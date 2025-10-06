#include "config.h"
#include "cues.h"
#include "display_manager.h"
#include "web_server.h"
#include "wifi_portal.h"

using namespace stagecue;

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!initDisplay()) {
    Serial.println(F("[Setup] Display initialisation failed"));
  }

  initCues();

  if (!startWiFiWithPortal()) {
    Serial.println(F("[Setup] Operating in access point mode"));
  }

  startWebServer();
}

void loop() {
  updateCues();
}
