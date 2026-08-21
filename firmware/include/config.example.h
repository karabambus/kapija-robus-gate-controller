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

// Dual mode: set true (leave WIFI_AP_MODE false) and the ESP32 joins the home
// network AND broadcasts its own AP (AP_SSID/AP_PASS) at the same time - a
// fallback control path when the router is down: phones join the AP and open
// http://192.168.4.1. NTP, mDNS and normal OTA keep working via the router
// side; if the router is unreachable the device does NOT reboot, it keeps
// serving the AP and quietly retries the router every 3 minutes.
// Single-radio caveat: AP and station share one channel, so the AP follows
// the router's channel. If the router changes channel, AP clients see a
// few-second blip and rejoin on their own. To rule even that out, set a
// fixed WiFi channel in the router instead of "auto".
#define WIFI_DUAL_MODE false

#define AP_SSID "Kapija"
#define AP_PASS "CHANGE-ME-8+CHARS"  // WPA2 requires at least 8 characters

// --- Access ---
// Set to false to skip the login page entirely and rely on WiFi security alone:
// anyone who can join the WiFi can then operate the gate and read the log
// (entries still record the caller's IP). OTA flashing keeps its own password.
#define REQUIRE_LOGIN true

// --- Tenants (per-tenant 4-digit PINs) ---
// One X(...) line per tenant: display name (shown in the access log) and a
// PIN of exactly 4 digits, both quoted. Rules:
//   - PINs must be unique (checked at boot; a duplicate entry is disabled
//     with a serial warning so the log can never name the wrong tenant)
//   - names: keep short, no ';' or line breaks (log line format)
//   - avoid guessable PINs: 0000, 1234, 2580, birth years
// Revocation: delete the tenant's line, rebuild, reflash via the browser
// /update page. A reflash/reboot also ends all existing sessions.
// Only required when REQUIRE_LOGIN is true; may be omitted otherwise.
#define TENANTS(X) \
  X("Marin", "4711") \
  X("Stan 1", "8024") \
  X("Stan 2", "3390")

// Failed-login backoff (optional; defaults shown, see config_defaults.h).
// Per-IP: 3 fails lock that IP 30 s, doubling per repeat up to 15 min.
// Global backstop (all IPs together): 10 fails lock login for everyone
// 2 min, doubling up to 60 min.
//#define LOGIN_IP_MAX_FAILS 3
//#define LOGIN_IP_LOCK_BASE_MS 30000
//#define LOGIN_IP_LOCK_MAX_MS 900000
//#define LOGIN_GLOBAL_MAX_FAILS 10
//#define LOGIN_GLOBAL_LOCK_BASE_MS 120000
//#define LOGIN_GLOBAL_LOCK_MAX_MS 3600000

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
// S.C.A. bounces while the gate travels (measured on the RB350): a level must
// hold this long before the reported state changes.
#define STATE_SETTLE_MS 2000
// Measure YOUR gate with a stopwatch and add ~20% margin (example: RB350
// measured 28 s open / 26 s close). Drive the app's "moving" countdown and
// the settle-independent backup timer.
#define TRAVEL_OPEN_MS 34000
#define TRAVEL_CLOSE_MS 31000
#define SESSION_MAX 6             // concurrent authenticated sessions kept in RAM

// Timezone: Croatia (CET/CEST with DST rules)
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// Preventive daily reboot hour (local time, 0-23; -1 disables). Clears slow
// heap/WiFi/mDNS degradation that can wedge a months-running ESP32.
#define DAILY_REBOOT_HOUR 4
