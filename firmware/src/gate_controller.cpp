#include "gate_controller.h"

#include <Arduino.h>

#include "config_defaults.h"

namespace gate {

namespace {
unsigned long lastTriggerMs = 0;

bool settledOpen = false;          // debounced state reported to the app
bool lastRaw = false;              // last raw S.C.A. sample
unsigned long lastRawChangeMs = 0; // when the raw level last flipped
unsigned long movingUntilMs = 0;   // backup travel deadline after a trigger

void relayWrite(bool active) {
  digitalWrite(RELAY_PIN, (active == RELAY_ACTIVE_HIGH) ? HIGH : LOW);
}

bool rawIsOpen() {
  int v = digitalRead(SCA_PIN);
  return SCA_OPEN_IS_LOW ? (v == LOW) : (v == HIGH);
}

// True while the raw level disagrees with the settled state and hasn't held
// long enough yet — i.e. S.C.A. is bouncing (gate in motion).
bool rawUnsettled() {
  return lastRaw != settledOpen &&
         millis() - lastRawChangeMs < STATE_SETTLE_MS;
}
}  // namespace

void begin() {
  pinMode(RELAY_PIN, OUTPUT);
  relayWrite(false);  // ensure inactive level before WiFi/boot noise
  pinMode(SCA_PIN, INPUT_PULLUP);
  settledOpen = lastRaw = rawIsOpen();  // trust the boot state immediately
}

void tick() {
  bool raw = rawIsOpen();
  if (raw != lastRaw) {
    lastRaw = raw;
    lastRawChangeMs = millis();
  } else if (raw != settledOpen &&
             millis() - lastRawChangeMs >= STATE_SETTLE_MS) {
    settledOpen = raw;    // level held long enough: accept the new state
    movingUntilMs = 0;    // travel finished — stop the backup timer early
  }
}

bool isOpen() {
  return settledOpen;
}

long movingRemainMs() {
  unsigned long now = millis();
  if (now < movingUntilMs) return (long)(movingUntilMs - now);
  if (rawUnsettled()) return 0;  // moving, deadline unknown (RF remote)
  return -1;
}

bool trigger() {
  unsigned long now = millis();
  if (now - lastTriggerMs < TRIGGER_COOLDOWN_MS) return false;
  lastTriggerMs = now;
  // Direction follows the settled state: closed -> opening, open -> closing.
  movingUntilMs = now + (settledOpen ? TRAVEL_CLOSE_MS : TRAVEL_OPEN_MS);
  relayWrite(true);
  delay(PULSE_MS);  // blocking is fine: the HTTP request waits on this anyway
  relayWrite(false);
  return true;
}

unsigned long msSinceTrigger() {
  return millis() - lastTriggerMs;
}

}  // namespace gate
