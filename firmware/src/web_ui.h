// HTTP routes and HTML pages. UI text is Croatian (it is tenant-facing);
// code and comments are English.
#pragma once

#include <WebServer.h>

namespace webui {

// Register all routes on the given server instance.
void begin(WebServer& server);

}  // namespace webui
