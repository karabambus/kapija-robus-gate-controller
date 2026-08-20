#include "web_ui.h"

#include <WiFi.h>

#include "access_log.h"
#include "auth.h"
#include "config.h"
#include "gate_controller.h"
#include "time_util.h"

// Default for config.h files created before this option existed.
#ifndef REQUIRE_LOGIN
#define REQUIRE_LOGIN true
#endif

// Catch a mistyped value (e.g. a quoted "false", which is truthy) at compile
// time instead of silently leaving login in the wrong state.
static_assert(REQUIRE_LOGIN == true || REQUIRE_LOGIN == false,
              "REQUIRE_LOGIN must be true or false (unquoted)");

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

// Client-side HR/EN dictionary. Elements opt in via data-i (textContent),
// data-ph (placeholder) or data-act (stored log-action key); the choice lives
// in localStorage per browser, so the server stays language-agnostic.
const char kI18nJs[] =
    "<script>"
    "const L={hr:{state:'Stanje',load:'učitavam…',open:'OTVORENA',"
    "closed:'ZATVORENA',btn:'OTVORI / ZATVORI',log:'Dnevnik otvaranja',"
    "out:'Odjava',logtitle:'Dnevnik',time:'Vrijeme',action:'Akcija',"
    "empty:'Još nema zapisa.',back:'← Natrag',pw:'Lozinka',login:'Prijava',"
    "wrong:'Pogrešna lozinka.',sw:'EN',a_prijava:'prijava',"
    "a_otvaranje:'otvaranje',a_zatvaranje:'zatvaranje'},"
    "en:{state:'Status',load:'loading…',open:'OPEN',closed:'CLOSED',"
    "btn:'OPEN / CLOSE',log:'Access log',out:'Log out',logtitle:'Log',"
    "time:'Time',action:'Action',empty:'No entries yet.',back:'← Back',"
    "pw:'Password',login:'Log in',wrong:'Wrong password.',sw:'HR',"
    "a_prijava:'login',a_otvaranje:'opening',a_zatvaranje:'closing'}};"
    "let lang=localStorage.getItem('lang')||'hr';"
    "function t(k){return L[lang][k]}"
    "function ta(a){return L[lang]['a_'+a]||a}"
    "function applyLang(){document.documentElement.lang=lang;"
    "document.querySelectorAll('[data-i]').forEach(e=>e.textContent=t(e.dataset.i));"
    "document.querySelectorAll('[data-ph]').forEach(e=>e.placeholder=t(e.dataset.ph));"
    "document.querySelectorAll('[data-act]').forEach(e=>e.textContent=ta(e.dataset.act));"
    "if(typeof render=='function')render()}"
    "function swLang(){lang=lang=='hr'?'en':'hr';"
    "localStorage.setItem('lang',lang);applyLang()}"
    "</script>";

bool authed() {
  if (!REQUIRE_LOGIN) return true;
  return srv->hasHeader("Cookie") &&
         auth::checkCookieHeader(srv->header("Cookie"));
}

// CSRF guard for state-changing requests. Browsers send an Origin header on
// cross-site requests, and it cannot match the host this app was served from —
// so a drive-by page on some phone's browser can't POST /toggle. The session
// cookie used to be this barrier; REQUIRE_LOGIN=false removes it, and this
// check works in both modes. Non-browser clients (curl) send no Origin.
bool originAllowed() {
  if (!srv->hasHeader("Origin")) return true;
  return srv->header("Origin") == "http://" + srv->hostHeader();
}

void sendLoginPage(bool wrongPassword) {
  String h = kPageHead;
  h += "<title>Kapija</title>";
  h += kCss;
  h += "<h1>🚗 Kapija</h1><div class=card><form method=POST action=/login>";
  if (wrongPassword) h += "<p style='color:#dc2626' data-i=wrong></p>";
  h += "<p><input type=password name=pw data-ph=pw autofocus></p>"
       "<p><button data-i=login></button></p></form>"
       "<p class=muted><a href=# data-i=sw onclick='swLang();return false'></a>"
       "</p></div>";
  h += kI18nJs;
  h += "<script>applyLang()</script>";
  srv->send(200, "text/html", h);
}

void sendMainPage() {
  String h = kPageHead;
  h += "<title>Kapija</title>";
  h += kCss;
  h +=
      "<h1>🚗 Kapija</h1><div class=card>"
      "<div id=state></div>"
      "<button id=btn onclick=trig() data-i=btn></button>"
      "<p class=muted><a href=/log data-i=log></a>"
#if REQUIRE_LOGIN
      " · <a href=/logout data-i=out></a>"
#endif
      " · <a href=# data-i=sw onclick='swLang();return false'></a></p>"
      "<p class=muted id=diag></p></div>";
  h += kI18nJs;
  h +=
      "<script>"
      "let j=null;"
      "function render(){let s=document.getElementById('state');"
      "if(!j){s.innerHTML=t('state')+': <span class=muted>'+t('load')+'</span>';"
      "return}"
      "s.innerHTML=t('state')+': '+(j.open?'<span class=open>'+t('open')+'</span>'"
      ":'<span class=closed>'+t('closed')+'</span>')"
      "+' <span class=muted>('+j.time+')</span>';"
      "document.getElementById('diag').textContent="
      "j.ip+' · '+j.net+' · uptime '+j.up}"
      "async function st(){try{let r=await fetch('/status');"
      "if(r.status==401){location='/';return}"
      "j=await r.json();render()}catch(e){}}"
      "async function trig(){let b=document.getElementById('btn');"
      "b.disabled=true;await fetch('/toggle',{method:'POST'});"
      "setTimeout(()=>{b.disabled=false;st()},2000);}"
      "applyLang();st();setInterval(st,3000);"
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
    srv->send(429, "text/plain",
              "Previše pokušaja, pričekaj minutu. / "
              "Too many attempts, wait a minute.");
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
  if (!originAllowed()) {
    srv->send(403, "text/plain", "forbidden");
    return;
  }
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
#if WIFI_AP_MODE
  j += ",\"ip\":\"" + WiFi.softAPIP().toString() + "\"";
  j += ",\"net\":\"AP · " + String(WiFi.softAPgetStationNum()) + " conn.\"";
#else
  j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"net\":\"signal " + String(WiFi.RSSI()) + " dBm\"";
#endif
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
  h += "<h1>📋 <span data-i=logtitle></span></h1><div class=card><table>"
       "<tr><td><b data-i=time></b></td><td><b data-i=action></b></td>"
       "<td><b>IP</b></td></tr>";
  h += rows.length() ? rows
                     : "<tr><td colspan=3 class=muted data-i=empty></td></tr>";
  h += "</table><p><a href=/ data-i=back></a> · "
       "<a href=# data-i=sw onclick='swLang();return false'></a></p></div>";
  h += kI18nJs;
  h += "<script>applyLang()</script>";
  srv->send(200, "text/html", h);
}
}  // namespace

void begin(WebServer& server) {
  srv = &server;
  // Origin feeds the CSRF check; Cookie is only needed when login is on.
  // (Neither is collected by the server unless asked for.)
  static const char* headerKeys[] = {"Origin", "Cookie"};
  server.collectHeaders(headerKeys, REQUIRE_LOGIN ? 2 : 1);

  server.on("/", HTTP_GET, handleRoot);
  if (REQUIRE_LOGIN) {
    // Without the login gate these routes are dead surface: /login would
    // still accept password guesses (and SHARED_PASSWORD may not be treated
    // as secret then), so don't register them at all.
    server.on("/login", HTTP_POST, handleLogin);
    server.on("/logout", HTTP_GET, handleLogout);
  }
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/log", HTTP_GET, handleLog);
  server.onNotFound([]() {
    srv->sendHeader("Location", "/");
    srv->send(303);
  });
}

}  // namespace webui
