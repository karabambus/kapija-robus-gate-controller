// Native unit tests for the login backoff policy (pio test -e native).
// Times are plain uint32 ms fed in by hand; defaults from login_backoff.cpp:
// per-IP 3 fails -> 30 s lock doubling to 15 min, global 10 fails -> 2 min
// doubling to 60 min, 1 h idle forgiveness, 6 h escalation reset.
#include <unity.h>

#include "login_backoff.h"

namespace {
constexpr uint32_t kIpA = 0x0101A8C0;  // 192.168.1.1
constexpr uint32_t kIpB = 0x0201A8C0;
constexpr uint32_t kSec = 1000;
constexpr uint32_t kMin = 60 * kSec;
constexpr uint32_t kHour = 60 * kMin;

// n failures from ip, 1 s apart, starting at t0; returns true if any of them
// reported a new lock.
bool failN(uint32_t ip, int n, uint32_t t0) {
  bool locked = false;
  for (int i = 0; i < n; i++) {
    if (backoff::registerFailure(ip, t0 + (uint32_t)i * kSec)) locked = true;
  }
  return locked;
}
}  // namespace

void setUp() {
  backoff::reset();
}
void tearDown() {}

void test_three_fails_lock_30s() {
  TEST_ASSERT_FALSE(backoff::registerFailure(kIpA, 0));
  TEST_ASSERT_FALSE(backoff::registerFailure(kIpA, kSec));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpA, kSec));
  TEST_ASSERT_TRUE(backoff::registerFailure(kIpA, 2 * kSec));  // 3rd locks
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpA, 2 * kSec));
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpA, 2 * kSec + 30 * kSec - 1));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpA, 2 * kSec + 30 * kSec));
}

void test_lock_remain_ms() {
  failN(kIpA, 3, 0);  // locked at t=2s for 30s
  TEST_ASSERT_EQUAL_UINT32(30 * kSec, backoff::lockRemainMs(kIpA, 2 * kSec));
  TEST_ASSERT_EQUAL_UINT32(10 * kSec, backoff::lockRemainMs(kIpA, 22 * kSec));
  TEST_ASSERT_EQUAL_UINT32(0, backoff::lockRemainMs(kIpA, 40 * kSec));
}

void test_refail_after_expiry_doubles() {
  failN(kIpA, 3, 0);  // lock #1: 30 s from t=2s
  uint32_t t = 2 * kSec + 30 * kSec + kSec;  // expired
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpA, t));
  TEST_ASSERT_TRUE(backoff::registerFailure(kIpA, t));  // one strike relocks
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpA, t + 60 * kSec - 1));  // 60 s now
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpA, t + 60 * kSec));
}

void test_lock_duration_caps_at_15min() {
  // Walk the escalation: 30,60,120,240,480,900,900,... seconds.
  failN(kIpA, 3, 0);
  uint32_t lockStart = 2 * kSec;
  uint32_t expect = 30 * kSec;
  for (int round = 0; round < 8; round++) {
    TEST_ASSERT_TRUE(backoff::lockedOut(kIpA, lockStart + expect - 1));
    uint32_t t = lockStart + expect;  // just expired; refail immediately so
    TEST_ASSERT_FALSE(backoff::lockedOut(kIpA, t));  // idle reset can't kick in
    TEST_ASSERT_TRUE(backoff::registerFailure(kIpA, t));
    lockStart = t;
    expect = expect * 2 < 900 * kSec ? expect * 2 : 900 * kSec;
  }
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpA, lockStart + 900 * kSec - 1));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpA, lockStart + 900 * kSec));
}

void test_success_clears_slot() {
  failN(kIpA, 2, 0);
  backoff::registerSuccess(kIpA);
  TEST_ASSERT_FALSE(backoff::registerFailure(kIpA, 10 * kSec));
  TEST_ASSERT_FALSE(backoff::registerFailure(kIpA, 11 * kSec));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpA, 11 * kSec));  // count restarted
}

void test_idle_hour_forgives() {
  failN(kIpA, 2, 0);
  uint32_t t = kHour + 2 * kSec;  // > 1 h after the last fail
  TEST_ASSERT_FALSE(backoff::registerFailure(kIpA, t));      // counts as 1st
  TEST_ASSERT_FALSE(backoff::registerFailure(kIpA, t + kSec));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpA, t + kSec));
}

void test_per_ip_isolation() {
  failN(kIpA, 3, 0);
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpA, 3 * kSec));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpB, 3 * kSec));
}

void test_slot_eviction_lru() {
  // Fill all 8 slots with 2 fails each (no locks), oldest first.
  for (uint32_t i = 0; i < 8; i++) {
    failN(100 + i, 2, i * 10 * kSec);
  }
  // A 9th IP evicts the least-recently-failed slot (ip 100).
  failN(200, 2, 100 * kSec);
  // ip 100's count is gone: two fresh fails don't lock it...
  TEST_ASSERT_FALSE(failN(100, 2, 101 * kSec));
  // ...while ip 107 kept its count: one more fail locks it.
  TEST_ASSERT_TRUE(backoff::registerFailure(100 + 7, 103 * kSec));
}

void test_global_backstop_locks_everyone() {
  // 10 single fails from 10 different IPs inside a minute.
  bool locked = false;
  for (uint32_t i = 0; i < 10; i++) {
    if (backoff::registerFailure(300 + i, i * kSec)) locked = true;
  }
  TEST_ASSERT_TRUE(locked);
  // An uninvolved IP is locked out too, for 2 min.
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpB, 10 * kSec));
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpB, 9 * kSec + 2 * kMin - 1));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpB, 9 * kSec + 2 * kMin));
}

void test_global_escalation_doubles_then_resets() {
  for (uint32_t i = 0; i < 10; i++) backoff::registerFailure(300 + i, i * kSec);
  uint32_t t = 9 * kSec + 2 * kMin + kSec;  // first global lock expired
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpB, t));
  // Second round: 10 more fails -> 4 min global lock.
  for (uint32_t i = 0; i < 10; i++) backoff::registerFailure(400 + i, t + i * kSec);
  uint32_t start2 = t + 9 * kSec;
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpB, start2 + 4 * kMin - 1));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpB, start2 + 4 * kMin));
  // 6 h of quiet resets the escalation: next round is back to 2 min.
  uint32_t t3 = start2 + 4 * kMin + 6 * kHour + kSec;
  for (uint32_t i = 0; i < 10; i++) backoff::registerFailure(500 + i, t3 + i * kSec);
  uint32_t start3 = t3 + 9 * kSec;
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpB, start3 + 2 * kMin - 1));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpB, start3 + 2 * kMin));
}

void test_global_counter_forgotten_after_idle_hour() {
  for (uint32_t i = 0; i < 9; i++) backoff::registerFailure(300 + i, i * kSec);
  // One more an hour later must NOT trip the backstop (counter forgotten).
  TEST_ASSERT_FALSE(backoff::registerFailure(kIpA, 8 * kSec + kHour + kSec));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpB, 8 * kSec + kHour + kSec));
}

void test_millis_rollover_inside_lock() {
  // Lock starts 10 s before the 32-bit wrap; must stay locked across it.
  uint32_t t0 = 0xFFFFFFFFu - 10 * kSec;
  failN(kIpA, 3, t0);
  uint32_t lockStart = t0 + 2 * kSec;         // still pre-wrap
  uint32_t afterWrap = lockStart + 15 * kSec;  // wraps past zero
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpA, afterWrap));
  TEST_ASSERT_TRUE(backoff::lockedOut(kIpA, lockStart + 30 * kSec - 1));
  TEST_ASSERT_FALSE(backoff::lockedOut(kIpA, lockStart + 30 * kSec));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_three_fails_lock_30s);
  RUN_TEST(test_lock_remain_ms);
  RUN_TEST(test_refail_after_expiry_doubles);
  RUN_TEST(test_lock_duration_caps_at_15min);
  RUN_TEST(test_success_clears_slot);
  RUN_TEST(test_idle_hour_forgives);
  RUN_TEST(test_per_ip_isolation);
  RUN_TEST(test_slot_eviction_lru);
  RUN_TEST(test_global_backstop_locks_everyone);
  RUN_TEST(test_global_escalation_doubles_then_resets);
  RUN_TEST(test_millis_rollover_inside_lock);
  RUN_TEST(test_global_counter_forgotten_after_idle_hour);
  return UNITY_END();
}
