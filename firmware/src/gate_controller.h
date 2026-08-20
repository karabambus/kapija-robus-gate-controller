// Gate hardware interface: relay pulse on P.P. input, gate state from S.C.A.
#pragma once

namespace gate {

// Configure GPIOs. Must be called first in setup() so the relay pin is driven
// to its inactive level before anything else runs.
void begin();

// Sample S.C.A. and advance the settle/travel state machine. Call every loop.
void tick();

// Settled gate state: S.C.A. bounces during travel, so a level must hold for
// STATE_SETTLE_MS before this changes.
bool isOpen();

// Milliseconds until the expected end of travel: >0 while a web-triggered
// move runs (backup timer from TRAVEL_*_MS, independent of S.C.A.); 0 when
// S.C.A. is bouncing without a known deadline (e.g. RF-remote move);
// -1 when the gate is not moving.
long movingRemainMs();

// Pulse the relay (simulated button press on the Robus P.P. input).
// Returns false if called again within the cooldown window.
bool trigger();

// Milliseconds since the last successful trigger (large value if never).
unsigned long msSinceTrigger();

}  // namespace gate
