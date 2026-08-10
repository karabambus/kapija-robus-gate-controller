// Session management for the shared-password login (v1).
// Sessions are random tokens kept in RAM; a reboot logs everyone out,
// which is acceptable for this device class.
#pragma once

#include <Arduino.h>

namespace auth {

// True while login attempts are rejected due to too many failures.
bool lockedOut();

// Validate a password attempt. Registers failures and manages lockout.
bool tryPassword(const String& password);

// Create a new session and return its token (to be set as a cookie).
String createSession();

// True if the given Cookie header carries a valid session token.
bool checkCookieHeader(const String& cookieHeader);

}  // namespace auth
