#include "time_util.h"

#include <time.h>

#include "config_defaults.h"

namespace timeutil {

namespace {
// Any epoch after ~Nov 2023 means SNTP has run; the RTC boots at 1970.
constexpr time_t kSyncThreshold = 1700000000;
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
