#include "gate_controller.h"

#include <Arduino.h>

#include "config_defaults.h"

namespace gate {

namespace {
unsigned long lastTriggerMs = 0;
bool everTriggered = false;  // without this, boot at lastTriggerMs=0 reads
                             // as "just triggered" and refuses the first press

bool settledOpen = false;          // debounced state reported to the app
bool lastRaw = false;              // last raw S.C.A. sample
unsigned long lastRawChangeMs = 0; // when the raw level last flipped
unsigned long movingUntilMs = 0;   // backup travel deadline after a trigger

// Travel bookkeeping. The Robus step-by-step sequence is asymmetric
// (field-verified): a press during OPENING stops the gate, but a press
// during CLOSING reverses it to fully open (obstacle-safety behavior).
enum Move { kIdle, kOpening, kClosing, kUnknown };
Move move = kIdle;
unsigned long moveStartMs = 0;

// A web stop-press leaves the gate somewhere mid-travel. S.C.A. can only say
// "not closed", but we know the stop happened, so the OPEN state gets a
// "partial" qualifier until the next clean full travel. (An RF-remote stop is
// invisible to us — that case still reads as plain OPEN.)
bool pendingPartial = false;  // stop-press issued, waiting for settle
bool partialOpen = false;     // settled open, but known to be mid-travel

// External-move detection (RF remote / wall button): S.C.A. blinks during
// travel, so a burst of raw transitions while we are idle means the gate is
// moving without us. Three transitions within the window rules out a single
// electrical blip (which makes at most two).
constexpr uint8_t kFlipConfirm = 3;
constexpr unsigned long kFlipWindowMs = 4000;
uint8_t flipCount = 0;
unsigned long flipWindowStartMs = 0;

// Detected external move, for the main loop to log: 0 none, 1 opening,
// 2 closing. Without this, a remote-opened gate leaves no trace unless
// someone happens to be watching the app.
uint8_t extEvent = 0;

// S.C.A. blink cadence diagnostics (min/max ms between raw transitions,
// gaps over 10 s ignored) — to verify the settle and flip-confirm
// thresholds against the real signal instead of guesses.
unsigned long minFlipMs = 0, maxFlipMs = 0;
uint32_t flipTotal = 0;

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
    unsigned long gap = millis() - lastRawChangeMs;
    if (lastRawChangeMs != 0 && gap < 10000) {
      if (minFlipMs == 0 || gap < minFlipMs) minFlipMs = gap;
      if (gap > maxFlipMs) maxFlipMs = gap;
      flipTotal++;
    }
    lastRaw = raw;
    lastRawChangeMs = millis();
    if (millis() - flipWindowStartMs > kFlipWindowMs) {
      flipWindowStartMs = millis();
      flipCount = 1;
    } else if (++flipCount >= kFlipConfirm && move == kIdle) {
      // Sustained blinking with no web trigger: an RF remote (or wall
      // button) started a move. Direction follows the settled state; the
      // burst's first flip approximates when the travel began.
      move = settledOpen ? kClosing : kOpening;
      moveStartMs = flipWindowStartMs;
      movingUntilMs =
          flipWindowStartMs + (settledOpen ? TRAVEL_CLOSE_MS : TRAVEL_OPEN_MS);
      extEvent = settledOpen ? 2 : 1;
    }
  } else if (raw != settledOpen &&
             millis() - lastRawChangeMs >= STATE_SETTLE_MS) {
    settledOpen = raw;    // level held long enough: accept the new state
    partialOpen = raw && pendingPartial;
    pendingPartial = false;
    movingUntilMs = 0;    // travel finished — stop the backup timer early
    move = kIdle;
  }
  // Subtraction-style compare: safe across the 49-day millis() rollover,
  // which a direct "millis() >= movingUntilMs" is not.
  if (move != kIdle && (long)(millis() - movingUntilMs) >= 0 &&
      !rawUnsettled()) {
    move = kIdle;  // backup timer expired and S.C.A. is quiet: travel over
    pendingPartial = false;
  }
}

bool isOpen() {
  return settledOpen;
}

long movingRemainMs() {
  if (move != kIdle) {  // idle leaves movingUntilMs stale — never compare it
    long rem = (long)(movingUntilMs - millis());  // rollover-safe subtraction
    if (rem > 0) return rem;
  }
  if (rawUnsettled()) return 0;  // moving, deadline unknown (RF remote)
  return -1;
}

bool trigger() {
  unsigned long now = millis();
  if (everTriggered && now - lastTriggerMs < TRIGGER_COOLDOWN_MS) return false;
  everTriggered = true;
  lastTriggerMs = now;
  if (movingRemainMs() < 0) {
    // Idle: direction follows the settled state.
    move = settledOpen ? kClosing : kOpening;
    moveStartMs = now;
    movingUntilMs = now + (settledOpen ? TRAVEL_CLOSE_MS : TRAVEL_OPEN_MS);
    pendingPartial = false;
  } else if (move == kClosing) {
    // Press during closing reverses to FULL OPEN (Robus safety sequence).
    // Reopen time ~ distance already closed, scaled to opening speed.
    unsigned long elapsed = now - moveStartMs;
    unsigned long est =
        (unsigned long)((uint64_t)elapsed * TRAVEL_OPEN_MS / TRAVEL_CLOSE_MS) +
        3000;
    if (est > TRAVEL_OPEN_MS) est = TRAVEL_OPEN_MS;
    move = kOpening;
    moveStartMs = now;
    movingUntilMs = now + est;
    pendingPartial = false;
  } else {
    // Press during opening (or unknown motion) = stop. Outcome position is
    // unpredictable: drop the countdown, show plain MOVING, let the settled
    // S.C.A. state decide (half-open reads as OPEN, qualified "partial").
    move = kUnknown;
    movingUntilMs = now;  // already-passed deadline (a 0 sentinel would read
                          // as far-future once millis() wraps past 2^31)
    pendingPartial = true;
  }
  relayWrite(true);
  delay(PULSE_MS);  // blocking is fine: the HTTP request waits on this anyway
  relayWrite(false);
  return true;
}

unsigned long msSinceTrigger() {
  return millis() - lastTriggerMs;
}

int moveState() {
  if (movingRemainMs() < 0) return 0;
  return move == kOpening ? 1 : move == kClosing ? 2 : 3;
}

bool isPartial() {
  return settledOpen && partialOpen;
}

uint8_t takeExternalEvent() {
  uint8_t e = extEvent;
  extEvent = 0;
  return e;
}

String blinkStats() {
  if (flipTotal == 0) return "";
  return String(minFlipMs) + "-" + String(maxFlipMs) + "ms x" +
         String(flipTotal);
}

}  // namespace gate
