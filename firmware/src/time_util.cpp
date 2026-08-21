#include "time_util.h"

#include <sys/time.h>
#include <time.h>

#include "config_defaults.h"

namespace timeutil {

namespace {
// Any epoch after ~Nov 2023 means SNTP has run; the RTC boots at 1970.
constexpr time_t kSyncThreshold = 1700000000;

// Compile time as an epoch, parsed once from __DATE__/__TIME__
// ("Aug 21 2026" / "12:34:56"). mktime reads it in the device TZ rather than
// the build machine's — hours of skew at most, fine for a plausibility floor.
time_t buildEpoch() {
  static time_t cached = 0;
  if (cached == 0) {
    struct tm tm = {};
    tm.tm_isdst = -1;
    if (strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &tm)) {
      cached = mktime(&tm);
    }
    if (cached <= 0) cached = kSyncThreshold;  // parse failed: old floor
  }
  return cached;
}
}

void begin() {
#if WIFI_AP_MODE
  // No NTP reachable on the standalone network, but the timezone still
  // matters: historical epochs from station-mode service must keep rendering
  // in local time, not silently shift to UTC.
  setenv("TZ", TZ_INFO, 1);
  tzset();
#else
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
#endif
}

bool synced() {
  return time(nullptr) > kSyncThreshold;
}

void setFromClient(long epochSeconds) {
  if (synced()) return;  // NTP (or an earlier client) already won
  // Plausibility window: no earlier than this firmware's build (real time is
  // always after it), no later than year 2100 — one phone with a wrong clock
  // must not poison log timestamps.
  if (epochSeconds < (long)buildEpoch() || epochSeconds > 4102444800L) return;
  timeval tv = {(time_t)epochSeconds, 0};
  settimeofday(&tv, nullptr);
}

String nowString() {
  // "—" is language-neutral: shown briefly at boot in station mode, and
  // permanently in standalone AP mode where no NTP is reachable.
  if (!synced()) return "—";
  time_t t = time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  char buf[32];
  strftime(buf, sizeof(buf), "%d.%m.%Y. %H:%M:%S", &tm);
  return String(buf);
}

String formatEpoch(long epoch) {
  if (epoch <= 0) return "—";
  time_t t = (time_t)epoch;
  struct tm tm;
  localtime_r(&t, &tm);
  char buf[24];
  strftime(buf, sizeof(buf), "%d.%m. %H:%M:%S", &tm);
  return String(buf);
}

}  // namespace timeutil
