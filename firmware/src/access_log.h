// Persistent access log on LittleFS.
// Line format: "<epoch>;<action>;<client-ip>\n" — epoch 0 = clock not yet synced.
// Rotation: when log.txt outgrows the limit it becomes log.old (one generation).
#pragma once

#include <Arduino.h>

namespace accesslog {

// Mount is done by the caller (LittleFS.begin); nothing to init here yet.
void append(const char* action, const String& clientIp);

// All entries as HTML table rows, newest first (log.old + log.txt combined).
String renderRowsHtml();

}  // namespace accesslog
