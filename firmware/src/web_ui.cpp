#include "web_ui.h"

#include <WiFi.h>

#include "access_log.h"
#include "auth.h"
#include "config.h"
#include "gate_controller.h"
#include "time_util.h"

namespace webui {

namespace {
WebServer* srv = nullptr;

const char kCss[] =
    "<style>body{font-family:sans-serif;max-width:420px;margin:24px auto;"
    "padding:0 12px;background:#f4f4f5;color:#18181b}h1{font-size:1.3em}"
    ".card{background:#fff;border-radius:12px;padding:20px;"
    "box-shadow:0 1px 4px rgba(0,0,0,.12)}button,input[type=password]{"
    "font-size:1.1em;padding:12px;border-radius:10px;border:1px solid #d4d4d8;"
    "width:100%;box-sizing:border-box}button{background:#2563eb;color:#fff;"
    "border:0;cursor:pointer;font-weight:600}button:disabled{background:#a1a1aa}"
    "#state{font-size:1.05em;margin:10px 0}.open{color:#dc2626;font-weight:700}"
    ".closed{color:#16a34a;font-weight:700}a{color:#2563eb}"
    "table{width:100%;border-collapse:collapse;font-size:.9em}"
    "td{padding:6px 4px;border-bottom:1px solid #e4e4e7}"
    ".muted{color:#71717a;font-size:.85em}</style>";

const char kPageHead[] =
    "<!doctype html><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>";

bool authed() {
  return srv->hasHeader("Cookie") &&
         auth::checkCookieHeader(srv->header("Cookie"));
}

void sendLoginPage(bool wrongPassword) {
  String h = kPageHead;
  h += "<title>Kapija</title>";
  h += kCss;
  h += "<h1>🚗 Kapija</h1><div class=card><form method=POST action=/login>";
  if (wrongPassword) h += "<p style='color:#dc2626'>Pogrešna lozinka.</p>";
  h += "<p><input type=password name=pw placeholder='Lozinka' autofocus></p>"
       "<p><button>Prijava</button></p></form></div>";
  srv->send(200, "text/html", h);
}

void sendMainPage() {
  String h = kPageHead;
  h += "<title>Kapija</title>";
  h += kCss;
  h +=
      "<h1>🚗 Kapija</h1><div class=card>"
      "<div id=state>Stanje: <span class=muted>učitavam…</span></div>"
      "<button id=btn onclick=trig()>OTVORI / ZATVORI</button>"
      "<p class=muted><a href=/log>Dnevnik otvaranja</a> · "
      "<a href=/logout>Odjava</a></p>"
      "<p class=muted id=diag></p></div>"
      "<script>"
      "async function st(){try{let r=await fetch('/status');"
      "if(r.status==401){location='/';return}"
      "let j=await r.json();document.getElementById('state').innerHTML="
      "'Stanje: '+(j.open?'<span class=open>OTVORENA</span>'"
      ":'<span class=closed>ZATVORENA</span>')"
      "+' <span class=muted>('+j.time+')</span>';"
      "document.getElementById('diag').textContent="
      "j.ip+' · signal '+j.rssi+' dBm · uptime '+j.up;}catch(e){}}"
      "async function trig(){let b=document.getElementById('btn');"
      "b.disabled=true;await fetch('/toggle',{method:'POST'});"
      "setTimeout(()=>{b.disabled=false;st()},2000);}"
      "st();setInterval(st,3000);"
      "</script>";
  srv->send(200, "text/html", h);
}

void handleRoot() {
  if (!authed()) {
    sendLoginPage(false);
    return;
  }
  sendMainPage();
}

void handleLogin() {
  if (auth::lockedOut()) {
    srv->send(429, "text/plain", "Previše pokušaja. Pričekaj minutu.");
    return;
  }
  if (auth::tryPassword(srv->arg("pw"))) {
    String token = auth::createSession();
    srv->sendHeader("Set-Cookie",
                    "sess=" + token + "; HttpOnly; Max-Age=31536000; Path=/");
    srv->sendHeader("Location", "/");
    srv->send(303);
    accesslog::append("prijava", srv->client().remoteIP().toString());
  } else {
    sendLoginPage(true);
  }
}

void handleLogout() {
  srv->sendHeader("Set-Cookie", "sess=x; Max-Age=0; Path=/");
  srv->sendHeader("Location", "/");
  srv->send(303);
}

void handleToggle() {
  if (!authed()) {
    srv->send(401, "text/plain", "unauthorized");
    return;
  }
  // Log intent based on current state; the Robus P.P. input is a toggle.
  const char* action = gate::isOpen() ? "zatvaranje" : "otvaranje";
  if (gate::trigger()) {
    accesslog::append(action, srv->client().remoteIP().toString());
    srv->send(200, "text/plain", "ok");
  } else {
    srv->send(200, "text/plain", "cooldown");
  }
}

void handleStatus() {
  if (!authed()) {
    srv->send(401, "application/json", "{}");
    return;
  }
  unsigned long upMin = millis() / 60000UL;
  String j = "{\"open\":";
  j += gate::isOpen() ? "true" : "false";
  j += ",\"time\":\"" + timeutil::nowString() + "\"";
  j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"rssi\":" + String(WiFi.RSSI());
  j += ",\"up\":\"" + String(upMin / 1440) + "d " +
       String((upMin / 60) % 24) + "h " + String(upMin % 60) + "m\"}";
  srv->send(200, "application/json", j);
}

void handleLog() {
  if (!authed()) {
    srv->sendHeader("Location", "/");
    srv->send(303);
    return;
  }
  String rows = accesslog::renderRowsHtml();
  String h = kPageHead;
  h += "<title>Kapija — dnevnik</title>";
  h += kCss;
  h += "<h1>📋 Dnevnik</h1><div class=card><table>"
       "<tr><td><b>Vrijeme</b></td><td><b>Akcija</b></td><td><b>IP</b></td></tr>";
  h += rows.length() ? rows
                     : "<tr><td colspan=3 class=muted>Još nema zapisa.</td></tr>";
  h += "</table><p><a href=/>← Natrag</a></p></div>";
  srv->send(200, "text/html", h);
}
}  // namespace

void begin(WebServer& server) {
  srv = &server;
  // The Cookie header is not collected by default.
  static const char* headerKeys[] = {"Cookie"};
  server.collectHeaders(headerKeys, 1);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/log", HTTP_GET, handleLog);
  server.onNotFound([]() {
    srv->sendHeader("Location", "/");
    srv->send(303);
  });
}

}  // namespace webui
