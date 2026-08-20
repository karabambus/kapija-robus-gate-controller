#include "access_log.h"

#include <LittleFS.h>
#include <time.h>

#include "config.h"
#include "time_util.h"

namespace accesslog {

namespace {
constexpr const char* kLogFile = "/log.txt";
constexpr const char* kLogOld = "/log.old";
// ~60 kB per generation keeps two generations well under the LittleFS
// partition size while holding years of entries at ~30 bytes each.
constexpr size_t kRotateBytes = 60000;

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

// Prepend rows from one file so that later entries end up on top.
void collectRows(const char* path, String& rows) {
  File f = LittleFS.open(path, "r");
  if (!f) return;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length()) rows = lineToRow(line) + rows;
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
  String rows;
  collectRows(kLogOld, rows);
  collectRows(kLogFile, rows);
  return rows;
}

}  // namespace accesslog
