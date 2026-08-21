# Backlog

Planned improvements, in rough priority order. No dates promised.

## App

- Admin role: mark one PIN (or a flag on a PIN) as admin. Per-tenant
  PINs exist now, so this is just a flag on a TENANTS entry plus
  admin-only routes; gate lock mode and log clearing need it first
- Per-IP rate limit on the trigger endpoint (the login endpoint already
  has per-IP lockout with backoff; this item is about throttling
  authenticated /toggle spam, which is a different concern)
- Log download: authenticated endpoint serving log.old + log.txt
  combined as one CSV file (this is also the collection path for the
  analytics section below)
- Gate lock mode: admin toggle that makes the ESP ignore web triggers
  (night hours, vacation), optionally on a schedule. RF remotes keep
  working, which is the right failsafe. Needs the admin role above
- Open-time display: show elapsed open time ("open for N min") while
  the gate stands open; past a configurable threshold it escalates to
  a "gate open too long" warning banner
- Event webhook: POST every log event to a configurable URL. Doubles
  as the automated-collection transport for the analytics section and
  as the delivery path for future notifications (ntfy accepts plain
  POSTs)
- Home-screen app feel: web manifest + icon, vibration on press
- Faster status polling right after a trigger
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
  falls far outside every known cluster. The per-tenant PINs and named
  log it depends on exist now.
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

- Log download from the device: covered by the log download item under
  App
- Log clearing: clear button (needs the admin role under App), plus a
  retention cap so the filesystem never fills regardless of manual
  clearing
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
- Notification channel groundwork: browser web push proper is out of
  reach (it needs VAPID crypto and subscription management, not just
  HTTPS), but the ESP32 can POST over TLS with core libs
  (WiFiClientSecure/HTTPClient), so pattern-based phone alerts can go
  straight to a self-hosted or public ntfy topic - no home server
  required

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

- Auto-close (board function L1) with pause time. Caveat: an unattended
  auto-close looks to the ESP exactly like an RF-remote move, so every
  one would be logged as "daljinski". The log needs a distinct source
  for it (a close starting with no trigger shortly after a full open is
  almost certainly auto-close), and L1 makes the open-too-long banner
  mostly moot
- Condominium step-by-step mode (L2): commands during opening are
  ignored. Warning: this invalidates the field-learned state machine -
  press-while-opening no longer stops, so the stop/PARTLY OPEN handling
  and the reverse-time estimate are all wrong under L2. The gate
  controller and the documented app behavior need rework before ever
  enabling it

## Hardware (needs a site visit)

- Hands-free trigger from the car: recognize a horn pattern (microphone)
  or headlight flashes (light sensor) at the gate. Needs a distinctive
  pattern - a single honk or flash must not open the gate for strangers
- Parking occupancy via magnetometer at the gate: every parked car came
  through the gate, so count vehicle passages with direction and keep
  occupancy = entries - exits. A QMC5883L-class sensor (~2 EUR, I2C,
  ~1 mA off the ESP32's own 3.3 V pin) sees a passing car as a large
  slow field change; pedestrians, bikes and weather are invisible to it.
  No radio involved, so no WiFi contention, and no per-car hardware.
  Phase 1 (next site visit): one sensor inside the Robus housing, no
  detection logic - just log raw readings for a week and eyeball them
  against the named access log (free labeled data) to see if car
  signatures stand out from the transformer hum and gate-movement
  transients. Plastic enclosures are magnetically invisible; steel is
  not, so mount accordingly. Phase 2 if signal is good: detection with
  a low-pass filter for the 50 Hz transformer field, masking while the
  gate or relay is active (firmware knows both), re-baseline after each
  gate move; direction from a second sensor (second I2C controller
  avoids the fixed-address clash) via cross-correlation - sensors
  30-40 cm apart inside the housing may suffice at gate-passage speeds,
  else one sensor moves to a small plastic box down the fence (I2C over
  cable: 10 kHz clock, 2.2k pull-ups, ground-paired ethernet wire, ~5 m
  max). Known limits: differential count drifts on any missed passage,
  so it needs an occasional re-sync (admin "set count" or a 3 a.m.
  assume-all-home rule), and a car stopping over the sensor smears the
  waveform, so detection needs settle/timeout logic, not edge counting.
  Ties into the analytics section: passage waveforms are signatures,
  and clustering them (car vs van, maybe individual regulars) is a
  better-grounded unsupervised-learning target than the RF ideas
- Reed switch for certain closed-position sensing
- Battery-backed RTC (DS3231) - low priority now that browsers donate time
- Buzzer chirp on trigger
- External antenna for weak WiFi at the gate
