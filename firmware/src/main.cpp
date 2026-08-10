// Gate-opener controller for a Nice Robus 350 sliding-gate drive.
//
// An ESP32 inside the Robus housing serves a small web app on the home WiFi.
// Tenants log in with a shared password and toggle the gate; the ESP32 pulses
// a relay wired to the Robus P.P. (step-by-step) dry-contact input and reads
// gate state from the S.C.A. (open-gate indicator) output via an optocoupler.
// Every action is appended to a persistent log on LittleFS.
//
// Hardware: Joy-it NodeMCU-ESP32, COM-RM01 relay, PC817 optocoupler,
// LM2596-class buck (33 V accessory tap -> 5 V). See ../hardware/WIRING.md.

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <time.h>

#include "config.h"
#include "gate_controller.h"
#include "time_util.h"
#include "web_ui.h"

namespace {
WebServer server(80);

constexpr unsigned long kWifiConnectTimeoutMs = 30000;
// Reboot if WiFi stays down this long; DHCP/AP hiccups self-heal this way.
constexpr unsigned long kWifiLostRebootMs = 60000;

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(true);  // modem sleep: lower power and self-heating
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < kWifiConnectTimeoutMs) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    // No point serving anything without network; retry from scratch.
    Serial.println("WiFi failed, rebooting in 30 s");
    delay(30000);
    ESP.restart();
  }
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}
}  // namespace

void setup() {
  gate::begin();  // first: relay pin must settle to inactive immediately
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {  // true = format on first mount
    Serial.println("LittleFS mount failed");
  }

  connectWifi();
  MDNS.begin(HOSTNAME);  // http://<HOSTNAME>.local
  timeutil::begin();

  // Over-the-air updates: `pio run -t upload` over WiFi, no USB needed.
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    // Make sure the gate can't be triggered while flash is being rewritten.
    gate::begin();
    Serial.println("OTA update starting");
  });
  ArduinoOTA.onEnd([]() { Serial.println("OTA done, rebooting"); });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("OTA error %u\n", err);
  });
  ArduinoOTA.begin();

  webui::begin(server);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  // WiFi watchdog: reboot after a sustained outage.
  static unsigned long lostSinceMs = 0;
  if (WiFi.status() == WL_CONNECTED) {
    lostSinceMs = 0;
  } else {
    if (lostSinceMs == 0) lostSinceMs = millis();
    if (millis() - lostSinceMs > kWifiLostRebootMs) ESP.restart();
  }

  // Preventive daily reboot: clears slow heap/WiFi/mDNS degradation.
  // The 2 h uptime guard keeps it from looping within the reboot hour.
  if (DAILY_REBOOT_HOUR >= 0 && timeutil::synced() &&
      millis() > 2 * 3600 * 1000UL) {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    if (tm.tm_hour == DAILY_REBOOT_HOUR) {
      Serial.println("Scheduled maintenance reboot");
      ESP.restart();
    }
  }

  delay(2);  // yield; keeps the idle task fed
}
