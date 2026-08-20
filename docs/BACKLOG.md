# Backlog

Planned improvements, in rough priority order. No dates promised.

## App

- Per-tenant PINs with a named access log (individual revocation)
- Home-screen app feel: web manifest + icon, vibration on press
- Faster status polling right after a trigger
- "Gate open too long" warning banner
- Per-IP rate limit on the trigger endpoint
- CSV export of the access log

## Connectivity and updates

- Browser-push OTA: upload new firmware to the device straight from a
  browser (removes the espota return-connection and password-escaping
  pitfalls described in firmware/README.md)
- Dual-mode WiFi (access point + station at the same time) as a fallback
  control path when the home router is down

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
