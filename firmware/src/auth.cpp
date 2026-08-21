#include "auth.h"

#include <esp_system.h>

#include "config.h"

namespace auth {

namespace {
String sessions[SESSION_MAX];  // ring buffer of active tokens
uint8_t nextSlot = 0;
uint8_t failCount = 0;
bool lockoutActive = false;
unsigned long lockoutStartMs = 0;

String randomToken() {
  // 128 bits from the hardware RNG, hex-encoded.
  char buf[33];
  for (int i = 0; i < 4; i++) {
    snprintf(buf + i * 8, 9, "%08lx", (unsigned long)esp_random());
  }
  return String(buf);
}
}  // namespace

bool lockedOut() {
  // Flag + subtraction instead of an absolute deadline: safe across the
  // 49-day millis() rollover.
  if (lockoutActive && millis() - lockoutStartMs >= LOGIN_LOCKOUT_MS) {
    lockoutActive = false;
  }
  return lockoutActive;
}

bool tryPassword(const String& password) {
  if (lockedOut()) return false;
  if (password == SHARED_PASSWORD) {
    failCount = 0;
    return true;
  }
  if (++failCount >= LOGIN_MAX_FAILS) {
    lockoutActive = true;
    lockoutStartMs = millis();
    failCount = 0;
  }
  return false;
}

String createSession() {
  String token = randomToken();
  sessions[nextSlot] = token;  // oldest session gets evicted
  nextSlot = (nextSlot + 1) % SESSION_MAX;
  return token;
}

bool checkCookieHeader(const String& cookieHeader) {
  int idx = cookieHeader.indexOf("sess=");
  if (idx < 0) return false;
  int end = cookieHeader.indexOf(';', idx);
  String token =
      cookieHeader.substring(idx + 5, end < 0 ? cookieHeader.length() : end);
  for (int i = 0; i < SESSION_MAX; i++) {
    if (sessions[i].length() && sessions[i] == token) return true;
  }
  return false;
}

}  // namespace auth
