// Browser-push OTA: GET /update serves a minimal upload page, POST /update
// flashes the uploaded firmware.bin. Removes both espota failure modes (the
// device-to-computer return connection and shell/ini password mangling);
// espota/ArduinoOTA stays available as the fallback path.
#pragma once

#include <WebServer.h>

namespace webupdate {

// Register the /update routes. Call after webui::begin().
void attach(WebServer& server);

}  // namespace webupdate
