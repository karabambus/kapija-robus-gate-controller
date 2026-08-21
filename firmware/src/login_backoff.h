// Failed-login backoff policy: a per-IP lockout table with escalating lock
// durations, plus a global backstop that caps IP-rotation attacks.
// Deliberately free of Arduino types (plain uint32_t ip and nowMs) so the
// policy can be unit-tested natively; auth.cpp adapts IPAddress and millis().
// All timing uses flag + subtraction, safe across the 32-bit ms rollover.
#pragma once

#include <stdint.h>

namespace backoff {

// Forget all state (boot default; also used between unit tests).
void reset();

// True while login attempts from this IP must be rejected (its own lock or
// the global one). Expired locks are cleared as a side effect.
bool lockedOut(uint32_t ip, uint32_t nowMs);

// Milliseconds until this IP may try again; 0 when not locked.
uint32_t lockRemainMs(uint32_t ip, uint32_t nowMs);

// Record a failed attempt (call only for attempts that were actually
// evaluated — rejected-while-locked must not feed this). Returns true when
// this failure started a new lock (per-IP or global), so the caller can log
// one row per lock event instead of one per attempt.
bool registerFailure(uint32_t ip, uint32_t nowMs);

// Record a successful login: clears the IP's slot and the global counter.
void registerSuccess(uint32_t ip);

}  // namespace backoff
