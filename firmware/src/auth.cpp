#include "auth.h"

#include <esp_system.h>
#include <string.h>

#include "config_defaults.h"
#include "login_backoff.h"

namespace auth {

namespace {
struct Tenant {
  const char* name;
  const char* pin;
};

#ifndef TENANTS
// Login-off builds need no table (config_defaults.h #errors otherwise).
#define TENANTS(X)
#endif

// Compile-time PIN format check, C++11-constexpr style (single return,
// recursion): exactly 4 chars, all '0'-'9'.
constexpr bool pinDigits(const char* s, int i) {
  return s[i] == '\0' ? i == 4
                      : (i < 4 && s[i] >= '0' && s[i] <= '9' &&
                         pinDigits(s, i + 1));
}
#define T_CHECK(n, p) \
  static_assert(pinDigits(p, 0), "TENANTS: PIN must be exactly 4 digits: " n);
TENANTS(T_CHECK)

#define T_ROW(n, p) {n, p},
// Trailing sentinel keeps the array non-empty when TENANTS is empty.
const Tenant kTenants[] = {TENANTS(T_ROW){nullptr, nullptr}};
constexpr int kTenantCount =
    (int)(sizeof(kTenants) / sizeof(kTenants[0])) - 1;
static_assert(kTenantCount <= 127, "TENANTS: at most 127 tenants");

bool tenantValid[kTenantCount > 0 ? kTenantCount : 1];

String sessions[SESSION_MAX];  // ring buffer of active tokens
int8_t sessTenant[SESSION_MAX];
uint8_t nextSlot = 0;
bool lockoutEvent = false;

String randomToken() {
  // 128 bits from the hardware RNG, hex-encoded.
  char buf[33];
  for (int i = 0; i < 4; i++) {
    snprintf(buf + i * 8, 9, "%08lx", (unsigned long)esp_random());
  }
  return String(buf);
}

// "sess=" value from a Cookie header; empty string when absent.
String cookieToken(const String& cookieHeader) {
  int idx = cookieHeader.indexOf("sess=");
  if (idx < 0) return "";
  int end = cookieHeader.indexOf(';', idx);
  return cookieHeader.substring(idx + 5,
                                end < 0 ? cookieHeader.length() : end);
}

// Ring index of the session for this token, or -1.
int findSession(const String& token) {
  if (!token.length()) return -1;
  for (int i = 0; i < SESSION_MAX; i++) {
    if (sessions[i].length() && sessions[i] == token) return i;
  }
  return -1;
}
}  // namespace

void begin() {
  for (int i = 0; i < SESSION_MAX; i++) sessTenant[i] = -1;
  for (int i = 0; i < kTenantCount; i++) {
    const char* nm = kTenants[i].name;
    // Name rules protect the ';'-separated log format and keep rows short.
    tenantValid[i] =
        nm && *nm && !strchr(nm, ';') && !strchr(nm, '\n');
    if (!tenantValid[i]) {
      Serial.printf("TENANTS[%d]: invalid name, entry disabled\n", i);
      continue;
    }
    for (int j = 0; j < i; j++) {
      if (tenantValid[j] && strcmp(kTenants[i].pin, kTenants[j].pin) == 0) {
        // Disable the later duplicate so the log can never name the wrong
        // tenant; the first holder of the PIN keeps working.
        tenantValid[i] = false;
        Serial.printf("TENANTS[%d] \"%s\": duplicate PIN, entry disabled\n",
                      i, nm);
        break;
      }
    }
  }
}

bool lockedOut(uint32_t ipv4) {
  return backoff::lockedOut(ipv4, millis());
}

uint32_t lockRemainSec(uint32_t ipv4) {
  return (backoff::lockRemainMs(ipv4, millis()) + 999) / 1000;
}

int tryPin(const String& pin, uint32_t ipv4) {
  uint32_t now = millis();
  if (backoff::lockedOut(ipv4, now)) return -1;  // locked attempts don't count
  for (int i = 0; i < kTenantCount; i++) {
    if (tenantValid[i] && pin == kTenants[i].pin) {
      backoff::registerSuccess(ipv4);
      return i;
    }
  }
  if (backoff::registerFailure(ipv4, now)) lockoutEvent = true;
  return -1;
}

bool takeLockoutEvent() {
  bool e = lockoutEvent;
  lockoutEvent = false;
  return e;
}

String createSession(int tenantIdx) {
  String token = randomToken();
  sessions[nextSlot] = token;  // oldest session gets evicted
  sessTenant[nextSlot] = (int8_t)tenantIdx;
  nextSlot = (nextSlot + 1) % SESSION_MAX;
  return token;
}

int sessionTenant(const String& cookieHeader) {
  int i = findSession(cookieToken(cookieHeader));
  return i < 0 ? -1 : sessTenant[i];
}

const char* tenantName(int idx) {
  if (idx < 0 || idx >= kTenantCount || !tenantValid[idx]) return "";
  return kTenants[idx].name;
}

void destroySession(const String& cookieHeader) {
  int i = findSession(cookieToken(cookieHeader));
  if (i < 0) return;
  sessions[i] = "";
  sessTenant[i] = -1;
}

}  // namespace auth
