#include "gate_controller.h"

#include <Arduino.h>

#include "config.h"

namespace gate {

namespace {
unsigned long lastTriggerMs = 0;

void relayWrite(bool active) {
  digitalWrite(RELAY_PIN, (active == RELAY_ACTIVE_HIGH) ? HIGH : LOW);
}
}  // namespace

void begin() {
  pinMode(RELAY_PIN, OUTPUT);
  relayWrite(false);  // ensure inactive level before WiFi/boot noise
  pinMode(SCA_PIN, INPUT_PULLUP);
}

bool isOpen() {
  int v = digitalRead(SCA_PIN);
  return SCA_OPEN_IS_LOW ? (v == LOW) : (v == HIGH);
}

bool trigger() {
  unsigned long now = millis();
  if (now - lastTriggerMs < TRIGGER_COOLDOWN_MS) return false;
  lastTriggerMs = now;
  relayWrite(true);
  delay(PULSE_MS);  // blocking is fine: the HTTP request waits on this anyway
  relayWrite(false);
  return true;
}

}  // namespace gate
