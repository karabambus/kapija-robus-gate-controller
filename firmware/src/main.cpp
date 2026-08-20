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

#include <esp_system.h>
#include <time.h>

#include "access_log.h"
#include "config.h"
#include "gate_controller.h"
#include "time_util.h"
#include "version.h"
#include "web_ui.h"

// Default for config.h files created before this option existed. web_ui.cpp
// also tests this macro with #if, where undefined likewise evaluates to 0 —
// keep false as the default so both files agree on a stale config.
#ifndef WIFI_AP_MODE
#define WIFI_AP_MODE false
#endif
static_assert(WIFI_AP_MODE == true || WIFI_AP_MODE == false,
              "WIFI_AP_MODE must be true or false (unquoted)");
#if WIFI_AP_MODE
static_assert(sizeof(AP_PASS) >= 9,
              "AP_PASS must be at least 8 characters (WPA2 minimum)");
#endif

namespace {
WebServer server(80);

constexpr unsigned long kWifiConnectTimeoutMs = 30000;
// Reboot if WiFi stays down this long; DHCP/AP hiccups self-heal this way.
constexpr unsigned long kWifiLostRebootMs = 60000;

const char* resetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "sw-reset";  // ESP.restart(): OTA, daily reboot
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "other";
  }
}

#if WIFI_AP_MODE
void startWifi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPsetHostname(HOSTNAME);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP \"" AP_SSID "\" started, IP: ");
  Serial.println(WiFi.softAPIP());
}
#else
void startWifi() {
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
#endif
}  // namespace

void setup() {
  gate::begin();  // first: relay pin must settle to inactive immediately
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {  // true = format on first mount
    Serial.println("LittleFS mount failed");
  }

  startWifi();
  MDNS.begin(HOSTNAME);  // http://<HOSTNAME>.local
#if !WIFI_AP_MODE
  timeutil::begin();  // NTP — unreachable on the standalone AP network
#endif

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

  // One boot entry per run, written once the clock has synced so it carries a
  // real timestamp (or after 60 s for AP/unsynced builds, timestamped "—").
  // The IP column of the row carries the firmware version and reset cause.
  static bool bootLogged = false;
  if (!bootLogged && (timeutil::synced() || millis() > 60000)) {
    bootLogged = true;
    accesslog::append("start", String(FW_VERSION " · ") + resetReasonStr());
  }

#if !WIFI_AP_MODE
  // WiFi watchdog: reboot after a sustained outage. Station mode only —
  // WiFi.status() never reads WL_CONNECTED while running as an AP, so this
  // would reboot-loop a standalone build.
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
#else
  // Standalone AP: no NTP, so the clock-based reboot above can never fire.
  // Reboot on uptime instead (also long before the 49-day millis() wrap).
  if (DAILY_REBOOT_HOUR >= 0 && millis() > 48 * 3600 * 1000UL) {
    Serial.println("Scheduled maintenance reboot (uptime)");
    ESP.restart();
  }
#endif

  delay(2);  // yield; keeps the idle task fed
}
