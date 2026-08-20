# Requirements — v1 (rev 3: Plan B confirmed, no auto-close)

> **v1 ACCEPTED 2026-08-20.** Installed at the gate 2026-08-04; ran ~2 weeks
> unattended with no intervention, exceeding acceptance criterion 5.
> All five acceptance criteria met.
>
> Post-acceptance option (2026-08-20): a `REQUIRE_LOGIN` build flag
> (default `true`, see `firmware/include/config.example.h`) can disable the
> web login, relaxing **F2** for installs that rely on WiFi security alone.
> Acceptance was run with the login enabled.

Rev 2, 2026-07-30: opener identified as **Nice Robus** (sliding gate, BlueBUS platform).
Manual research (see `NICE-ROBUS-NOTES.md`) resolved risks R1–R3 and R5 of rev 1 and
**changed the trigger method from RF replay to a wired step-by-step contact.**
Rev 3, 2026-07-30: owner reviewed both plans and confirmed **Plan B** (ESP32 wired at
the gate; the "sacrificial remote at home" Plan A was considered and rejected).
**Auto-close is out of v1** — the gate is closed by a second press (app or remote),
same as today. Board function L1 stays OFF; enabling it later is a v2 option.

## Context

- Opener: **Nice Robus** family (exact model from label at inspection — RB350/400/600/1000
  all share the same control platform).
- Radio is **rolling code** (SMXI/FLOR or SMXIS/Smilo, 433.92 MHz) → RF replay impossible.
- Board offers: **P.P.** step-by-step dry-contact input, **S.C.A.** open-gate-indicator
  24V output, **24Vdc/100mA accessory tap**, built-in auto-close (function L1).
- New tenant moving in; multiple tenants over time. WiFi confirmed near gate.

## Functional requirements (v1)

| # | Requirement |
|---|-------------|
| F1 | Tenant can trigger the gate from a phone browser on the home WiFi (local web page served by ESP32). |
| F2 | Web page protected by a **shared password** (v1). |
| F3 | Existing RF remotes keep working, completely unaffected. |
| F4 | Every web trigger **logged** (timestamp), viewable on a log page, survives reboot. |
| F5 | **No auto-close in v1.** The gate is closed by a second press (app or remote), exactly as today. The app button acts as a toggle, labeled from gate state (F7): "Open" when closed, "Close" when open. |
| F6 | ESP32 triggers the gate by **pulsing a relay/optocoupler across the P.P. terminal** (~0.5 s contact closure). |
| F7 | **Gate state in the app**: ESP32 reads the S.C.A. output (board L4 = "on if leaf is open") and shows open/closed. |
| F8 | ESP32 **powered from the board's 24Vdc accessory tap** via a 24V→5V buck converter (stay well under the 100 mA @ 24V budget). |

## Nice board configuration (one-time, at install)

- Level-2 L4 (S.C.A. function) = "on if the leaf is open" — the only board setting v1 needs.
- Leave L1 "Automatic Closing" OFF (owner's decision; v2 option).
- Optional consideration for v2: Level-2 L2 step-by-step mode = "Condominium"
  (commands during opening are ignored — tenants can't accidentally reverse the gate
  mid-open; a command during closing still reopens).

## Non-goals for v1 (v2 backlog)

- Per-tenant PINs and named log (individual revocation) — planned v2.
- Auto-close (board function L1 + pause time) — five-minute board-key procedure, v2.
- "Sacrificial remote" architecture (Plan A) — evaluated, rejected in favor of Plan B.
- Access from outside the home network.
- Pedestrian-gate command (radio Mode I T2 does partial open — could be a second app button in v2, wired to nothing extra: partial open is only available via radio, so v2 would need an OXI/second channel trick — parked).
- ~~English localization alongside the Croatian UI~~ — **done 2026-08-20**:
  client-side HR/EN toggle on every page, Croatian default, choice saved per
  browser (localStorage); stored log entries stay in Croatian and are translated
  at display time.
- Better UI (visual polish of the web app).
- ~~Public README polish~~ — **done 2026-08-20** (screenshots, features, build
  guide, ASCII-only). Follow-ups from owner's post-publish review:
  - **README screenshots should show the English UI** (current ones are the
    Croatian default; regenerate with `localStorage lang='en'` in the stub).
  - **Rework docs/ for a public audience**: REQUIREMENTS.md and
    NICE-ROBUS-NOTES.md are internal development notes (requirements revs,
    risk logs, session history) with little value to a stranger building one.
    Replace with a simple public BACKLOG.md (planned features only) and move
    or heavily trim the development notes; keep the Robus platform facts that
    a builder actually needs (terminal map, L4 setting, 33 V warning) close to
    the hardware docs.
- Suggestions batch (added 2026-08-20):
  - ~~Boot log entry + firmware version~~ — **done 2026-08-20**: every boot
    appends a "start" log row carrying `FW_VERSION` + reset cause (power-on /
    sw-reset / watchdog / brownout / panic); version also shown on the diag line.
  - ~~Cap log page at newest 100 entries~~ — **done 2026-08-20**: the full
    two-generation log (~120 kB) no longer gets built into one heap String.
  - **PWA home-screen app**: inline web manifest + icon (data URI) so tenants
    can "Add to Home Screen"; `navigator.vibrate` on press for physical feel.
  - **Fast poll after press**: poll /status at 1 s for ~30 s after a trigger
    (pairs with travel-time progress below).
  - **"Gate open too long" banner**: caution banner (and blinking tab title)
    when state stays OPEN past ~15 min.
  - **Per-IP rate limit on /toggle**: ~6 triggers/min per IP — limits relay
    wear from a runaway LAN script on the passwordless build.
  - **CSV log export** (`/log.csv`) for off-device archiving.
  - **Browser-push OTA** (prevents the espota pitfalls hit on 2026-08-20): an
    HTTP `/update` endpoint on the device (ESP32 `Update.h`, guarded by
    OTA_PASSWORD + the origin check) so a new firmware.bin is uploaded TO the
    device like any file upload. Removes both espota failure modes: no
    device→computer return connection (host firewalls irrelevant) and no
    ini/shell password mangling. espota stays as fallback.
  - **Dual-mode WiFi (AP+STA)** (added 2026-08-20, after the AP field test):
    ESP32 supports WIFI_AP_STA — join the home network AND broadcast the own
    AP simultaneously. Would give a fallback control path when the router is
    down, and would have made the AP-mode test trivial (no network hopping to
    flash back). Needs thought: channel is shared with the router's, NTP and
    OTA available via STA side, watchdog semantics change.
  - ~~Client-supplied time for AP mode~~ — **done 2026-08-20 (v1.7)**: any
    browser opening the app donates its clock via POST /time when /status
    reports sync:false; origin-guarded, accepted only while unsynced, value
    must be plausible (2023–2100). Field-verified in AP mode: phone joined the
    AP and timestamps went from "—" to correct within seconds. (Hardware
    alternative DS3231 RTC stays on the v3 list, now low priority.)
  - **v3 hardware (needs site visit)**: reed switch for certain closed-position
    sensing; buzzer chirp on trigger.
- ~~Travel-time progress~~ — **done 2026-08-20 (v1.2)**: field test showed
  S.C.A. actually *bounces* during travel (the earlier "instant transition"
  note only held for the settled ends), so v1.2 got both: a settle filter
  (state must hold 4 s) and a per-direction backup travel timer from stopwatch
  measurements + ~20% margin (28→34 s open, 26→31 s close). The app shows a
  MOVING chip with a live countdown; panel colors follow settled states only.
- ~~Standalone AP mode~~ — **done 2026-08-20, field-tested same day** (phone
  joined the AP, UI + trigger + donated clock all worked): `WIFI_AP_MODE` build flag
  (default `false`); `true` makes the ESP32 broadcast its own WPA2 network
  (`AP_SSID`/`AP_PASS`) instead of joining the home WiFi. OTA keeps working
  (join the AP, upload to 192.168.4.1). Known AP-mode trade-offs: phones drop
  offline while connected; no NTP, so log entries show "—" instead of a date
  and the maintenance reboot runs on a 48 h uptime cadence instead of at 4 a.m.

## Remaining risks / open questions

| # | Risk / question | Resolution path |
|---|-----------------|-----------------|
| R1 | ~~Exact model unknown~~ **RESOLVED 2026-07-30:** label photo = **ROBUS350, serial 07/07** — exactly the manual in `docs/manual/`. Board photos confirm terminal row FLASH·S.C.A.·BLUEBUS·STOP·P.P. as documented; **P.P. and S.C.A. are unwired/free**; FLASH has a lamp, BLUEBUS has photocells; L1–L6 all off (factory defaults, no auto-close active). | — |
| R2 | Free space + antenna performance for ESP32 inside the Robus housing (plastic, IP44). | Photos show usable space below the control unit; measure at next visit. |
| R3 | ~~24V tap~~ **RESOLVED 2026-07-30: measured 33V DC** across the inner STOP(−)/P.P.(+) pins, unloaded, tap free. Confirms the ≥40V-input buck requirement (Mini360 would have died). | — |
| R4 | WiFi signal at the exact opener location. **Measured 2026-08-20** (v1.1 diag line, ESP32 inside the housing): **−89 dBm — marginal.** Works, but could explain any flakiness; consider antenna orientation, moving the router, or a repeater if problems recur. | Watch it on the diag line; improve if drops occur. |
| R6 | S.C.A. measured 0V — but gate state during measurement unclear (factory function = open-gate indicator: 0V when closed is *correct*), and unloaded open-collector outputs can read 0V on a 10MΩ meter anyway. | Optional retest: 200V DC range, gate fully open. Definitive test at install: set L4 = "on if leaf open", opto as load. Fallback: reed switch. |
| R5 | Radio receiver model (SMXI vs SMXIS) hidden behind wires in photos — only matters for buying tenant remotes. | Photo of the module on the SM connector ("Rx" area, lower board) at next visit. |

## Acceptance criteria (v1 done when…)

1. Tenant's phone on home WiFi: open page → password → tap **Open** → gate moves;
   second tap closes it.
2. Original remotes still work; photocells and STOP behavior unchanged.
3. App shows correct open/closed state (matches reality after remote-triggered moves too).
4. Log page lists every web trigger with date/time, surviving reboot.
5. Runs installed for a week without intervention.
