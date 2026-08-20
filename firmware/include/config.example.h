// Configuration template.
// Copy to include/config.h and fill in real values before building.
// include/config.h is gitignored so credentials never end up in the repo.
#pragma once

// --- WiFi (home network that covers the gate) ---
#define WIFI_SSID "CHANGE-ME"
#define WIFI_PASS "CHANGE-ME"

// Standalone mode: set true and the ESP32 broadcasts its own WiFi network
// (AP_SSID/AP_PASS below) instead of joining the one above. Phones connect to
// that network and open http://192.168.4.1 (kapija.local may also work).
// Trade-offs: no internet on that network (phones drop offline while
// connected; no NTP, so log entries show "—" instead of a date) and OTA
// flashing means joining the AP and uploading to 192.168.4.1.
#define WIFI_AP_MODE false
#define AP_SSID "Kapija"
#define AP_PASS "CHANGE-ME-8+CHARS"  // WPA2 requires at least 8 characters

// --- Access ---
// Set to false to skip the login page entirely and rely on WiFi security alone:
// anyone who can join the WiFi can then operate the gate and read the log
// (entries still record the caller's IP). OTA flashing keeps its own password.
#define REQUIRE_LOGIN true

// Shared tenant password for the web UI (v1; per-tenant PINs planned for v2).
// Unused when REQUIRE_LOGIN is false (the login routes are not registered),
// but must stay defined — the auth module references it in every build.
#define SHARED_PASSWORD "change-me"

// mDNS hostname -> http://kapija.local
#define HOSTNAME "kapija"

// Password for over-the-air firmware updates (PlatformIO espota).
// Independent from the tenant web password — this one guards flashing.
#define OTA_PASSWORD "change-me-too"

// --- Pin mapping ---
// COM-RM01 relay module: its IN pin is driven by this GPIO.
#define RELAY_PIN 26
// true  = relay energizes on HIGH (typical for COM-RM01)
// false = relay energizes on LOW
// Bench check: if the relay clicks ON at boot and releases afterwards, flip this.
#define RELAY_ACTIVE_HIGH true

// S.C.A. sense via PC817: collector to this GPIO (INPUT_PULLUP), emitter to GND.
#define SCA_PIN 27
// true = gate OPEN pulls the GPIO LOW (optocoupler conducting) — standard wiring.
#define SCA_OPEN_IS_LOW true

// --- Behavior ---
#define PULSE_MS 500              // duration of the simulated button press on P.P.
#define TRIGGER_COOLDOWN_MS 2000  // ignore re-triggers within this window
#define SESSION_MAX 6             // concurrent authenticated sessions kept in RAM
#define LOGIN_MAX_FAILS 5         // failed logins before lockout kicks in
#define LOGIN_LOCKOUT_MS 60000    // lockout duration after too many failures

// Timezone: Croatia (CET/CEST with DST rules)
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// Preventive daily reboot hour (local time, 0-23; -1 disables). Clears slow
// heap/WiFi/mDNS degradation that can wedge a months-running ESP32.
#define DAILY_REBOOT_HOUR 4
