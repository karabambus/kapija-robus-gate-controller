#include "time_util.h"

#include <time.h>

#include "config.h"

namespace timeutil {

namespace {
// Any epoch after ~Nov 2023 means SNTP has run; the RTC boots at 1970.
constexpr time_t kSyncThreshold = 1700000000;
}

void begin() {
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
}

bool synced() {
  return time(nullptr) > kSyncThreshold;
}

String nowString() {
  if (!synced()) return "vrijeme nije sinkronizirano";
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
