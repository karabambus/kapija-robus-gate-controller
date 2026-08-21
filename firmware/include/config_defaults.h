// Include this instead of config.h in any file that tests a feature macro.
// Back-compat defaults for config.h files created before an option existed,
// plus compile-time validation — kept in ONE place so every translation unit
// agrees on feature state (a bare `#if FLAG` on an undefined macro silently
// evaluates to 0, which can diverge from another file's #ifndef default).
#pragma once

#include "config.h"

#ifndef REQUIRE_LOGIN
#define REQUIRE_LOGIN true
#endif
static_assert(REQUIRE_LOGIN == true || REQUIRE_LOGIN == false,
              "REQUIRE_LOGIN must be true or false (unquoted)");

#if REQUIRE_LOGIN && !defined(TENANTS)
#error \
    "v1.10+: login uses per-tenant PINs. Define a TENANTS table in include/config.h (see include/config.example.h). SHARED_PASSWORD is no longer used."
#endif

// Failed-login backoff (v1.10) — defaults for older config.h files.
// Per-IP: N fails lock that IP for the base time, doubling per re-lock up to
// the cap. Global backstop counts fails from ALL IPs together.
#ifndef LOGIN_IP_MAX_FAILS
#define LOGIN_IP_MAX_FAILS 3
#endif
#ifndef LOGIN_IP_LOCK_BASE_MS
#define LOGIN_IP_LOCK_BASE_MS 30000UL
#endif
#ifndef LOGIN_IP_LOCK_MAX_MS
#define LOGIN_IP_LOCK_MAX_MS 900000UL
#endif
#ifndef LOGIN_GLOBAL_MAX_FAILS
#define LOGIN_GLOBAL_MAX_FAILS 10
#endif
#ifndef LOGIN_GLOBAL_LOCK_BASE_MS
#define LOGIN_GLOBAL_LOCK_BASE_MS 120000UL
#endif
#ifndef LOGIN_GLOBAL_LOCK_MAX_MS
#define LOGIN_GLOBAL_LOCK_MAX_MS 3600000UL
#endif

#ifndef WIFI_AP_MODE
#define WIFI_AP_MODE false
#endif
static_assert(WIFI_AP_MODE == true || WIFI_AP_MODE == false,
              "WIFI_AP_MODE must be true or false (unquoted)");

#ifndef WIFI_DUAL_MODE
#define WIFI_DUAL_MODE false
#endif
static_assert(WIFI_DUAL_MODE == true || WIFI_DUAL_MODE == false,
              "WIFI_DUAL_MODE must be true or false (unquoted)");
static_assert(!(WIFI_AP_MODE && WIFI_DUAL_MODE),
              "WIFI_AP_MODE and WIFI_DUAL_MODE are mutually exclusive: "
              "dual mode already includes the access point");

#if WIFI_AP_MODE || WIFI_DUAL_MODE
static_assert(sizeof(AP_PASS) >= 9,
              "AP_PASS must be at least 8 characters (WPA2 minimum)");
#endif

// Gate state settling / travel timing (v1.2) — defaults for older config.h.
#ifndef STATE_SETTLE_MS
#define STATE_SETTLE_MS 2000
#endif
#ifndef TRAVEL_OPEN_MS
#define TRAVEL_OPEN_MS 30000
#endif
#ifndef TRAVEL_CLOSE_MS
#define TRAVEL_CLOSE_MS 30000
#endif
