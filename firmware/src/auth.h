// Per-tenant PIN login (since v1.10): tenants come from the compile-time
// TENANTS table in config.h. Sessions are random tokens kept in RAM with the
// matching tenant recorded alongside; a reboot logs everyone out, which is
// acceptable for this device class. Brute-force protection lives in
// login_backoff.* (per-IP escalating lockout + global backstop).
#pragma once

#include <Arduino.h>

namespace auth {

// Boot-time validation of the TENANTS table (duplicate PINs, bad names):
// offending entries are disabled with a serial warning.
void begin();

// True while login attempts from this IP are rejected (its lockout or the
// global one).
bool lockedOut(uint32_t ipv4);

// Whole seconds until this IP may try again (for a Retry-After header).
uint32_t lockRemainSec(uint32_t ipv4);

// Validate a PIN attempt from the given client IP. Returns the tenant index
// on success, -1 on failure (also feeds the backoff bookkeeping).
int tryPin(const String& pin, uint32_t ipv4);

// True once after a failed attempt started a new lockout — the caller logs
// one "blokada" row per lock event. Reading clears the flag.
bool takeLockoutEvent();

// Create a new session bound to a tenant; returns the cookie token.
String createSession(int tenantIdx);

// Tenant index for the session in the given Cookie header, or -1.
int sessionTenant(const String& cookieHeader);

// Display name for a tenant index ("" for an invalid index).
const char* tenantName(int idx);

// Invalidate the session in the given Cookie header server-side (logout):
// the token dies now instead of lingering until ring eviction.
void destroySession(const String& cookieHeader);

}  // namespace auth
