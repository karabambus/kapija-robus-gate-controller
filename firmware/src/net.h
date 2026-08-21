// WiFi ownership: mode selection (station / standalone AP / dual AP+STA),
// connection watchdog, and the device's network identity. Everything that
// depends on the WIFI_*_MODE flags lives here so main.cpp and web_ui.cpp
// stay mode-agnostic.
#pragma once

#include <Arduino.h>

namespace net {

// Bring the radio up per the configured mode. Station mode reboots if the
// router stays unreachable; dual mode starts the fallback AP instead.
void begin();

// Per-mode connection watchdog. Call every loop pass.
// Station: reboot after a sustained outage (DHCP/AP hiccups self-heal).
// Dual: never reboot — retry the router quietly while the AP carries on.
void maintain();

// True when the station side is associated with the router (always false in
// standalone AP mode).
bool staConnected();

// CSRF identity check: does this Origin header value name one of the device's
// own addresses? In dual mode both the station IP and the AP IP must pass,
// or the UI breaks on one of the two networks.
bool isOwnOrigin(const String& origin);

// Primary IP for the diag line: station IP when associated, AP IP otherwise.
String ipString();

// Human-readable link summary for the diag line ("signal -87 dBm",
// "AP · 1 conn.", or both in dual mode).
String netString();

}  // namespace net
