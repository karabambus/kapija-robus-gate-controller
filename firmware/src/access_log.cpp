#include "access_log.h"

#include <LittleFS.h>
#include <time.h>

#include <vector>

#include "config.h"
#include "time_util.h"

namespace accesslog {

namespace {
constexpr const char* kLogFile = "/log.txt";
constexpr const char* kLogOld = "/log.old";
// ~60 kB per generation keeps two generations well under the LittleFS
// partition size while holding years of entries at ~30 bytes each.
constexpr size_t kRotateBytes = 60000;
// The page renders only the newest entries: both generations together can
// reach ~120 kB of rows — far more than the heap can hold in one String.
constexpr size_t kMaxRows = 100;

// Convert one raw log line into an HTML table row; empty string if malformed.
String lineToRow(const String& line) {
  int a = line.indexOf(';');
  if (a < 0) return "";
  int b = line.indexOf(';', a + 1);
  if (b < 0) return "";
  long epoch = line.substring(0, a).toInt();
  String action = line.substring(a + 1, b);
  String ip = line.substring(b + 1);
  // data-act carries the stored action key so the page can translate it
  // client-side without rewriting old log entries.
  return "<tr><td>" + timeutil::formatEpoch(epoch) + "</td><td><span data-act=" +
         action + ">" + action + "</span></td><td class=muted>" + ip +
         "</td></tr>";
}

// Feed one file's lines, oldest-first, into a ring of the newest kMaxRows.
void collectLines(const char* path, std::vector<String>& ring, size_t& total) {
  File f = LittleFS.open(path, "r");
  if (!f) return;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length()) ring[total++ % kMaxRows] = line;
  }
  f.close();
}
}  // namespace

void append(const char* action, const String& clientIp) {
  File f = LittleFS.open(kLogFile, "a");
  if (!f) return;
  long epoch = timeutil::synced() ? (long)time(nullptr) : 0;
  f.printf("%ld;%s;%s\n", epoch, action, clientIp.c_str());
  size_t size = f.size();
  f.close();
  if (size > kRotateBytes) {
    LittleFS.remove(kLogOld);
    LittleFS.rename(kLogFile, kLogOld);
  }
}

String renderRowsHtml() {
  std::vector<String> ring(kMaxRows);
  size_t total = 0;
  collectLines(kLogOld, ring, total);  // older generation first: chronological
  collectLines(kLogFile, ring, total);
  size_t n = total < kMaxRows ? total : kMaxRows;
  String rows;
  rows.reserve(n * 100);
  for (size_t i = 0; i < n; i++) {  // newest entry on top
    rows += lineToRow(ring[(total - 1 - i) % kMaxRows]);
  }
  if (total > kMaxRows) {
    rows += "<tr><td colspan=3 class=muted data-i=older></td></tr>";
  }
  return rows;
}

}  // namespace accesslog
