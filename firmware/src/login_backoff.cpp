#include "login_backoff.h"

#ifdef ARDUINO
#include "config_defaults.h"  // config.h may override the policy knobs
#endif

// Defaults double as the native-test values; config.h can override on-device.
#ifndef LOGIN_IP_MAX_FAILS
#define LOGIN_IP_MAX_FAILS 3
#endif
#ifndef LOGIN_IP_LOCK_BASE_MS
#define LOGIN_IP_LOCK_BASE_MS 30000UL  // 30 s, doubling per re-lock
#endif
#ifndef LOGIN_IP_LOCK_MAX_MS
#define LOGIN_IP_LOCK_MAX_MS 900000UL  // 15 min cap
#endif
#ifndef LOGIN_GLOBAL_MAX_FAILS
#define LOGIN_GLOBAL_MAX_FAILS 10
#endif
#ifndef LOGIN_GLOBAL_LOCK_BASE_MS
#define LOGIN_GLOBAL_LOCK_BASE_MS 120000UL  // 2 min, doubling per round
#endif
#ifndef LOGIN_GLOBAL_LOCK_MAX_MS
#define LOGIN_GLOBAL_LOCK_MAX_MS 3600000UL  // 60 min cap
#endif

namespace backoff {

namespace {
constexpr int kSlots = 8;
// A slot (or the global fail counter) untouched this long is forgotten; also
// resets the global lock-duration escalation. Well under the ~49-day wrap,
// so plain uint32 subtraction stays correct.
constexpr uint32_t kIdleResetMs = 3600000UL;             // 1 h
constexpr uint32_t kGlobalEscalationResetMs = 21600000UL;  // 6 h

struct Slot {
  uint32_t ip;         // 0 = free
  uint8_t fails;       // consecutive fails since last lock/success
  uint8_t lockLevel;   // completed locks; sets the next lock's duration
  bool locked;
  uint32_t lockStartMs;
  uint32_t lockMs;
  uint32_t lastFailMs;
};
Slot slots[kSlots];

uint8_t globalFails;
uint8_t globalLevel;
bool globalLocked;
uint32_t globalLockStartMs;
uint32_t globalLockMs;
uint32_t globalLastFailMs;
bool globalEverFailed;

uint32_t lockDurationMs(uint8_t level, uint32_t baseMs, uint32_t maxMs) {
  uint32_t ms = baseMs;
  for (uint8_t i = 0; i < level && ms < maxMs; i++) ms *= 2;
  return ms < maxMs ? ms : maxMs;
}

void expireSlot(Slot& s, uint32_t nowMs) {
  if (s.ip == 0) return;
  if (nowMs - s.lastFailMs >= kIdleResetMs) {  // long quiet: forgive fully
    s = Slot{};
    return;
  }
  if (s.locked && nowMs - s.lockStartMs >= s.lockMs) {
    s.locked = false;  // lock served; lockLevel stays for escalation
    s.fails = 0;
  }
}

void expireGlobal(uint32_t nowMs) {
  if (!globalEverFailed) return;
  if (globalLocked && nowMs - globalLockStartMs >= globalLockMs) {
    globalLocked = false;
    globalFails = 0;
  }
  if (!globalLocked && nowMs - globalLastFailMs >= kIdleResetMs) {
    globalFails = 0;
  }
  if (!globalLocked && nowMs - globalLastFailMs >= kGlobalEscalationResetMs) {
    globalLevel = 0;
  }
}

Slot* findSlot(uint32_t ip) {
  for (int i = 0; i < kSlots; i++) {
    if (slots[i].ip == ip) return &slots[i];
  }
  return nullptr;
}

Slot& takeSlot(uint32_t ip, uint32_t nowMs) {
  Slot* s = findSlot(ip);
  if (s) return *s;
  for (int i = 0; i < kSlots; i++) {  // free slot first
    if (slots[i].ip == 0) {
      slots[i] = Slot{};
      slots[i].ip = ip;
      return slots[i];
    }
  }
  // Table full: evict the least-recently-failed entry. Resetting its
  // escalation only helps an attacker who burns 8+ addresses — which the
  // global backstop punishes anyway.
  Slot* victim = &slots[0];
  for (int i = 1; i < kSlots; i++) {
    if (nowMs - slots[i].lastFailMs > nowMs - victim->lastFailMs)
      victim = &slots[i];
  }
  *victim = Slot{};
  victim->ip = ip;
  return *victim;
}
}  // namespace

void reset() {
  for (int i = 0; i < kSlots; i++) slots[i] = Slot{};
  globalFails = 0;
  globalLevel = 0;
  globalLocked = false;
  globalLockStartMs = 0;
  globalLockMs = 0;
  globalLastFailMs = 0;
  globalEverFailed = false;
}

bool lockedOut(uint32_t ip, uint32_t nowMs) {
  expireGlobal(nowMs);
  if (globalLocked) return true;
  Slot* s = findSlot(ip);
  if (!s) return false;
  expireSlot(*s, nowMs);
  return s->ip != 0 && s->locked;
}

uint32_t lockRemainMs(uint32_t ip, uint32_t nowMs) {
  uint32_t remain = 0;
  expireGlobal(nowMs);
  if (globalLocked) remain = globalLockMs - (nowMs - globalLockStartMs);
  Slot* s = findSlot(ip);
  if (s) {
    expireSlot(*s, nowMs);
    if (s->ip != 0 && s->locked) {
      uint32_t r = s->lockMs - (nowMs - s->lockStartMs);
      if (r > remain) remain = r;
    }
  }
  return remain;
}

bool registerFailure(uint32_t ip, uint32_t nowMs) {
  bool lockStarted = false;

  Slot& s = takeSlot(ip, nowMs);
  expireSlot(s, nowMs);
  if (s.ip == 0) s.ip = ip;  // expire may have wiped an idle slot
  s.lastFailMs = nowMs;
  if (!s.locked) {
    bool relock = s.lockLevel > 0;  // already served a lock: one strike
    if (relock || ++s.fails >= LOGIN_IP_MAX_FAILS) {
      s.locked = true;
      s.lockStartMs = nowMs;
      s.lockMs =
          lockDurationMs(s.lockLevel, LOGIN_IP_LOCK_BASE_MS, LOGIN_IP_LOCK_MAX_MS);
      if (s.lockLevel < 255) s.lockLevel++;
      s.fails = 0;
      lockStarted = true;
    }
  }

  expireGlobal(nowMs);
  globalLastFailMs = nowMs;
  globalEverFailed = true;
  if (!globalLocked && ++globalFails >= LOGIN_GLOBAL_MAX_FAILS) {
    globalLocked = true;
    globalLockStartMs = nowMs;
    globalLockMs = lockDurationMs(globalLevel, LOGIN_GLOBAL_LOCK_BASE_MS,
                                  LOGIN_GLOBAL_LOCK_MAX_MS);
    if (globalLevel < 255) globalLevel++;
    globalFails = 0;
    lockStarted = true;
  }
  return lockStarted;
}

void registerSuccess(uint32_t ip) {
  Slot* s = findSlot(ip);
  if (s) *s = Slot{};
  globalFails = 0;
}

}  // namespace backoff
