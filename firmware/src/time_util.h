// Small time helpers on top of the ESP32 SNTP client.
#pragma once

#include <Arduino.h>

namespace timeutil {

// Start NTP sync with the configured timezone. Non-blocking.
void begin();

// True once the clock has been set by NTP or a client (epoch is plausible).
bool synced();

// Set the clock from a client-supplied epoch (seconds). Accepted only while
// the clock is unsynced and the value is plausible — the AP-mode answer to
// "no NTP": the first phone that opens the app brings the time.
void setFromClient(long epochSeconds);

// Current local time as "dd.mm.yyyy. HH:MM:SS", or a placeholder before sync.
String nowString();

// Format an epoch as "dd.mm. HH:MM:SS", or "—" for epoch 0 (logged before sync).
String formatEpoch(long epoch);

}  // namespace timeutil
