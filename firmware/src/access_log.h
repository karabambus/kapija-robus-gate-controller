// Persistent access log on LittleFS.
// Line format: "<epoch>;<action>;<client-ip>[;<tenant>]\n" — epoch 0 = clock
// not yet synced; the tenant field only appears on rows from a PIN session.
// Rotation: when log.txt outgrows the limit it becomes log.old (one generation).
#pragma once

#include <Arduino.h>

namespace accesslog {

// Mount is done by the caller (LittleFS.begin); nothing to init here yet.
// tenant: display name for the row, or nullptr/"" for unattributed rows
// (boot, RF remote, login-off builds).
void append(const char* action, const String& clientIp,
            const char* tenant = nullptr);

// All entries as HTML table rows, newest first (log.old + log.txt combined).
String renderRowsHtml();

}  // namespace accesslog
