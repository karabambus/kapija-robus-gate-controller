# Backlog

Planned improvements, in rough priority order. No dates promised.

## Near-term patches (small fixes found in code review)

- First trigger after boot is refused: lastTriggerMs starts at 0, so the
  cooldown check treats a fresh boot as "just triggered" for the first
  TRIGGER_COOLDOWN_MS of uptime. Init to the negative cooldown or
  special-case zero
- /logout has no CSRF guard: it is a plain GET with no Origin check, so
  any page a tenant visits could log every session out. Make it POST
  with the same originAllowed() check as /toggle
- Add SameSite=Lax to the session cookie: free defense-in-depth on top
  of the Origin check
- Make travel-deadline comparisons millis()-rollover safe: direct
  "millis() >= movingUntilMs" breaks at the 49-day wrap. The daily
  reboot masks it, but the unsynced/AP build only reboots on a 48 h
  cadence; use subtraction-style comparisons
- Sanity-bound /time clock donations: reject client timestamps before
  the firmware build date or absurdly far in the future, so one phone
  with a wrong clock cannot poison log timestamps in AP mode

## App

- Per-tenant PINs with a named access log (individual revocation)
- Simple numeric login: replace the password with a 4-digit PIN (0-9)
  and a numeric keypad UI on the login page. Only 10000 combinations,
  so it must land together with lockout/backoff on failed attempts
  (the per-IP rate limit item below covers part of this)
- Home-screen app feel: web manifest + icon, vibration on press
- Faster status polling right after a trigger
- "Gate open too long" warning banner
- Per-IP rate limit on the trigger endpoint
- CSV export of the access log
- Gate lock mode: admin toggle that makes the ESP ignore web triggers
  (night hours, vacation), optionally on a schedule. RF remotes keep
  working, which is the right failsafe
- Show elapsed open time ("open for N min") while the gate stands open;
  pairs with the open-too-long warning banner above
- Named devices in the access log: small config table mapping known IPs
  (DHCP reservations) to labels, so rows read "Marin's phone" instead of
  an address. Poor man's identity before per-tenant PINs land, and a
  useful feature for the clustering items below
- Event webhook: POST every log event to a configurable home-server URL.
  Doubles as the automated-collection transport for the analytics
  section and as the delivery path for future notifications (ntfy
  accepts plain POSTs)
- Android name-resolution fallback: kapija.local via mDNS is flaky in
  Android browsers. A printed QR code with the static IP, or an
  LLMNR/NetBIOS responder, removes the most likely "does not work on
  my phone" complaint
- WiFi signal strength (RSSI) in the /status diag line: real data for
  deciding whether the external-antenna hardware item is ever needed

## Analytics (unsupervised learning on the access log)

- User profiling by clustering: group trigger events into per-user usage
  profiles (time of day, day of week, frequency, device/browser) from
  access-log data. Could spot an unknown or misused PIN when an event
  falls far outside every known cluster. Depends on per-tenant PINs and
  the named access log landing first.
  Device/browser feature: log a short token parsed from the User-Agent
  header at request time (e.g. android-chrome, ios-safari, desktop).
  Raw UA strings are 100+ bytes and would rotate the log too fast;
  modern browsers freeze the UA anyway, so a coarse token is all the
  signal there is. Spoofable, so a clustering feature only, never an
  auth signal
- Opening-time pattern clustering: cluster historical opening times to
  learn the usual daily windows when the gate gets opened. A later phone
  notification feature could build on this, e.g. "gate usually opens
  around now" reminders or an alert when an opening happens well outside
  any learned window. Likely runs off-device (log export plus a small
  script), not on the ESP

Supporting infrastructure the above needs:

- Log download from the device: authenticated endpoint serving log.old +
  log.txt combined as one file (extends the CSV export item under App)
- Log clearing: admin-only clear button, plus a retention cap so the
  filesystem never fills regardless of manual clearing
- Aggregates that survive log clearing: keep a small fixed-size summary
  per user (e.g. trigger counts per hour-of-week bucket, per device
  token) updated on every trigger and stored separately from the raw
  log. Clustering history then persists when raw entries are deleted,
  and the summary never grows with traffic
- Log schema versioning: first line or filename marks the line format,
  so adding fields (user, device token) does not break the export
  parser on old files
- Data quality for time features: drop or flag epoch-0 entries (clock
  not yet synced), store UTC and convert to local time with DST handled
  at analysis time, and collapse rapid double-presses into one event
- Automated collection: something periodically pulls the log off the
  device (home-server cron or phone shortcut hitting the download
  endpoint), so analysis does not depend on remembering manual exports
- Notification channel groundwork: the ESP cannot do web push itself
  (needs HTTPS and a push service), so pattern-based phone alerts
  likely go through a relay such as a self-hosted or public ntfy topic

## Code and project maintenance

- Unit tests for the gate state machine: gate_controller.cpp holds the
  field-learned stop/reverse/partial logic and only touches millis()
  and digitalRead(), so a native PlatformIO test env with mocked time
  can lock the behavior in before v2 features start poking at it
- CI on GitHub Actions: pio run on every push, plus the ASCII-only docs
  grep as an automated check instead of a manual pre-commit habit
- Serve pages from PROGMEM with a fixed buffer instead of repeated
  String concatenation: less heap fragmentation (currently masked by
  the daily maintenance reboot)
- Server-sent events instead of 3 s status polling: would subsume the
  faster-polling item under App and smooth the countdown. Tradeoff:
  the stock WebServer cannot do it well, and ESPAsyncWebServer breaks
  the no-external-libs property, so only if polling ever annoys
- Free-heap field in /status plus a LAN uptime monitor (e.g. Uptime
  Kuma on a home server), so a dead device is noticed before a tenant
  standing at the gate notices it

## Drive features (Nice Robus board settings)

- Auto-close (board function L1) with pause time
- Condominium step-by-step mode (L2): commands during opening are ignored

## Hardware (needs a site visit)

- Hands-free trigger from the car: recognize a horn pattern (microphone)
  or headlight flashes (light sensor) at the gate. Needs a distinctive
  pattern - a single honk or flash must not open the gate for strangers
- Reed switch for certain closed-position sensing
- Battery-backed RTC (DS3231) - low priority now that browsers donate time
- Buzzer chirp on trigger
- External antenna for weak WiFi at the gate
