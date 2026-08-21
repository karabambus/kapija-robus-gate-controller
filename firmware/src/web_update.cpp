#include "web_update.h"

#include <Update.h>

#include "access_log.h"
#include "config_defaults.h"
#include "gate_controller.h"
#include "net.h"
#include "version.h"

namespace webupdate {

namespace {
WebServer* srv = nullptr;

// Set at UPLOAD_FILE_START, read by the final handler. The upload callback
// cannot refuse the stream mid-flight, so an unauthorized upload is simply
// never written to flash and the final handler reports the rejection.
bool uploadOk = false;
bool uploadDenied = false;

// Admin tool, deliberately plain — not the Machine Panel theme.
const char kPage[] =
    "<!doctype html><meta name=viewport content='width=device-width,"
    "initial-scale=1'><title>kapija update</title>"
    "<body style='font-family:monospace;max-width:420px;margin:2em auto'>"
    "<h3>Firmware update</h3>"
    "<p>Running: " FW_VERSION "</p>"
    "<form method=POST action=/update enctype=multipart/form-data>"
    "<p><input type=file name=fw accept=.bin required></p>"
    "<p><button>Upload &amp; flash</button></p></form>"
    "<p>Takes ~15 s, then the device reboots. Do not close the tab.</p>";

bool basicAuthOk() {
  return srv->authenticate("admin", OTA_PASSWORD);
}

bool originOk() {
  return !srv->hasHeader("Origin") || net::isOwnOrigin(srv->header("Origin"));
}

void handlePage() {
  if (!basicAuthOk()) {
    srv->requestAuthentication();
    return;
  }
  srv->send(200, "text/html", kPage);
}

void handleUpload() {
  HTTPUpload& up = srv->upload();
  switch (up.status) {
    case UPLOAD_FILE_START:
      uploadOk = false;
      uploadDenied = !basicAuthOk() || !originOk();
      if (uploadDenied) return;
      // Same relay-safety move as ArduinoOTA onStart: make sure the gate
      // cannot be triggered while flash is being rewritten.
      gate::begin();
      Serial.printf("Web update start: %s\n", up.filename.c_str());
      uploadOk = Update.begin(UPDATE_SIZE_UNKNOWN);
      break;
    case UPLOAD_FILE_WRITE:
      if (uploadOk && Update.write(up.buf, up.currentSize) != up.currentSize)
        uploadOk = false;
      break;
    case UPLOAD_FILE_END:
      if (uploadOk) uploadOk = Update.end(true);
      break;
    case UPLOAD_FILE_ABORTED:
    default:
      Update.abort();
      uploadOk = false;
      break;
  }
}

void handleDone() {
  if (uploadDenied) {
    if (!basicAuthOk()) {
      srv->requestAuthentication();
    } else {
      srv->send(403, "text/plain", "forbidden");
    }
    return;
  }
  if (!uploadOk || Update.hasError()) {
    // A failed upload lands in the inactive OTA partition — the running
    // firmware is untouched, so report and carry on without rebooting.
    srv->send(500, "text/plain",
              String("update failed: ") + Update.errorString());
    return;
  }
  accesslog::append("update", srv->client().remoteIP().toString());
  srv->send(200, "text/plain", "ok, rebooting");
  srv->client().flush();
  delay(500);  // let the response leave before the link drops
  ESP.restart();
}
}  // namespace

void attach(WebServer& server) {
  srv = &server;
  server.on("/update", HTTP_GET, handlePage);
  server.on("/update", HTTP_POST, handleDone, handleUpload);
}

}  // namespace webupdate
