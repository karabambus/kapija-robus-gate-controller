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

#include <esp_system.h>
#include <time.h>

#include "access_log.h"
#include "config_defaults.h"
#include "gate_controller.h"
#include "net.h"
#include "time_util.h"
#include "version.h"
#include "web_ui.h"
#include "web_update.h"

namespace {
WebServer server(80);

// Don't restart under someone's feet: skip while the gate is open or was
// triggered recently (the check re-fires on later loop passes until safe).
bool rebootIsSafe() {
  return !gate::isOpen() && gate::msSinceTrigger() > 10 * 60 * 1000UL;
}

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

}  // namespace

void setup() {
  gate::begin();  // first: relay pin must settle to inactive immediately
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {  // true = format on first mount
    Serial.println("LittleFS mount failed");
  }

  net::begin();
  MDNS.begin(HOSTNAME);  // http://<HOSTNAME>.local
  timeutil::begin();  // always: TZ setup matters even when NTP is unreachable

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
  webupdate::attach(server);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  gate::tick();

  // Detected RF-remote / wall-button moves get a log row too ("daljinski"
  // in the source column, where web entries carry the client IP).
  uint8_t ext = gate::takeExternalEvent();
  if (ext) {
    accesslog::append(ext == 1 ? "otvaranje" : "zatvaranje", "daljinski");
  }

  // One boot entry per run, written once the clock has synced so it carries a
  // real timestamp (or after 60 s for AP/unsynced builds, timestamped "—").
  // The IP column of the row carries the firmware version and reset cause.
  static bool bootLogged = false;
  if (!bootLogged && (timeutil::synced() || millis() > 60000)) {
    bootLogged = true;
    accesslog::append("start", String(FW_VERSION " · ") + resetReasonStr());
  }

  net::maintain();  // per-mode WiFi watchdog (reboot / quiet retry / nothing)

  // Preventive maintenance reboot: clears slow heap/WiFi/mDNS degradation.
  // With a synced clock (NTP, or a browser-donated time) it fires daily at
  // DAILY_REBOOT_HOUR; the 2 h uptime guard keeps it from looping within the
  // reboot hour. Unsynced (standalone AP nobody has visited yet), it falls
  // back to a 48 h uptime cadence — long before the 49-day millis() wrap.
  if (DAILY_REBOOT_HOUR >= 0) {
    if (timeutil::synced() && millis() > 2 * 3600 * 1000UL) {
      time_t t = time(nullptr);
      struct tm tm;
      localtime_r(&t, &tm);
      if (tm.tm_hour == DAILY_REBOOT_HOUR && rebootIsSafe()) {
        Serial.println("Scheduled maintenance reboot");
        ESP.restart();
      }
    } else if (!timeutil::synced() && millis() > 48 * 3600 * 1000UL &&
               rebootIsSafe()) {
      Serial.println("Scheduled maintenance reboot (uptime)");
      ESP.restart();
    }
  }

  delay(2);  // yield; keeps the idle task fed
}
