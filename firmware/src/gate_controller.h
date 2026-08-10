// Gate hardware interface: relay pulse on P.P. input, gate state from S.C.A.
#pragma once

namespace gate {

// Configure GPIOs. Must be called first in setup() so the relay pin is driven
// to its inactive level before anything else runs.
void begin();

// True when the S.C.A. output reports the leaf is open.
bool isOpen();

// Pulse the relay (simulated button press on the Robus P.P. input).
// Returns false if called again within the cooldown window.
bool trigger();

}  // namespace gate
