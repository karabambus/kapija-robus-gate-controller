#include "net.h"

#include <WiFi.h>

#include "config_defaults.h"

namespace net {

namespace {
constexpr unsigned long kWifiConnectTimeoutMs = 30000;
// Station mode: reboot if WiFi stays down this long (self-heals hiccups).
constexpr unsigned long kWifiLostRebootMs = 60000;
// Dual mode: retry the router this often while disconnected. Each retry scans
// channels for ~1-2 s, briefly delaying packets for AP clients, so keep it
// infrequent — the AP is doing the real work during an outage.
constexpr unsigned long kStaRetryMs = 180000;

#if !WIFI_AP_MODE
// Wait for the station to associate, without giving up the CPU for long.
bool waitForSta() {
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < kWifiConnectTimeoutMs) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}
#endif
}  // namespace

#if WIFI_AP_MODE

void begin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPsetHostname(HOSTNAME);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP \"" AP_SSID "\" started, IP: ");
  Serial.println(WiFi.softAPIP());
}

void maintain() {}  // nothing to watch: no station link exists

#elif WIFI_DUAL_MODE

void begin() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.softAPsetHostname(HOSTNAME);
  // Station FIRST: the single radio shares one channel, and starting the AP
  // after the station has associated brings it up already on the router's
  // channel — no channel hop (= no AP-client blip) on a normal boot.
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (waitForSta()) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    // Router unreachable — exactly what the AP is for. Keep going; the
    // station keeps retrying in maintain(). When the router reappears the AP
    // hops to its channel once (connected phones rejoin automatically).
    Serial.println("WiFi not reachable, continuing on AP only");
  }
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP \"" AP_SSID "\" started, IP: ");
  Serial.println(WiFi.softAPIP());
}

void maintain() {
  // Never reboot for a WiFi outage here: that would kill the fallback AP at
  // the exact moment it is needed. Just nudge the station periodically.
  static unsigned long lastRetryMs = 0;
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastRetryMs < kStaRetryMs) return;
  lastRetryMs = millis();
  WiFi.reconnect();
}

#else  // plain station mode

void begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(true);  // modem sleep: lower power and self-heating
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (!waitForSta()) {
    // No point serving anything without network; retry from scratch.
    Serial.println("WiFi failed, rebooting in 30 s");
    delay(30000);
    ESP.restart();
  }
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void maintain() {
  static unsigned long lostSinceMs = 0;
  if (WiFi.status() == WL_CONNECTED) {
    lostSinceMs = 0;
  } else {
    if (lostSinceMs == 0) lostSinceMs = millis();
    if (millis() - lostSinceMs > kWifiLostRebootMs) ESP.restart();
  }
}

#endif

bool staConnected() {
#if WIFI_AP_MODE
  return false;
#else
  return WiFi.status() == WL_CONNECTED;
#endif
}

bool isOwnOrigin(const String& origin) {
  if (origin == "http://" HOSTNAME ".local") return true;
#if WIFI_AP_MODE || WIFI_DUAL_MODE
  if (origin == "http://" + WiFi.softAPIP().toString()) return true;
#endif
#if !WIFI_AP_MODE
  if (staConnected() && origin == "http://" + WiFi.localIP().toString())
    return true;
#endif
  return false;
}

String ipString() {
#if WIFI_AP_MODE
  return WiFi.softAPIP().toString();
#elif WIFI_DUAL_MODE
  return staConnected() ? WiFi.localIP().toString()
                        : WiFi.softAPIP().toString();
#else
  return WiFi.localIP().toString();
#endif
}

String netString() {
#if WIFI_AP_MODE
  return "AP · " + String(WiFi.softAPgetStationNum()) + " conn.";
#elif WIFI_DUAL_MODE
  String ap = "AP " + String(WiFi.softAPgetStationNum());
  if (staConnected()) return "signal " + String(WiFi.RSSI()) + " dBm · " + ap;
  return "router down · " + ap;
#else
  return "signal " + String(WiFi.RSSI()) + " dBm";
#endif
}

}  // namespace net
