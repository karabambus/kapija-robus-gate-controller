# Nice Robus — Hardware Research Notes

Source: official Nice "Robus 350" installer manual (EN), plus Nice/reseller docs for
RB400/600/1000 (same platform: BlueBUS control unit, SM radio connector).
Confirm exact model from the label during inspection — everything below applies to
the whole Robus family unless noted.

## What it is

- Nice (Italy) **Robus** = 24V electromechanical gearmotor line for **sliding gates**,
  models Robus 350 (350 kg), RB400, RB600/600P, RB1000/1000P, RB250HS/500HS.
- 230Vac mains in, internal control unit, IP44, optional PS124 24V backup battery.
- Robus 350: max 250VA, gate leaf up to 15 m; speed & force programmable (6 levels).

## ⚠ Radio = ROLLING CODE (confirmed — RF replay is impossible)

From the manual's spec table:

| Receiver | Coding | Remotes |
|---|---|---|
| **SMXI** | 53-bit **FLOR rolling code**, 433.92 MHz | FLOR, VERY VR, ERGO, PLANO, FLO2R-S… |
| **SMXIS** | 64-bit **SMILO rolling code**, 433.92 MHz | SM2, Smilo family |
| (OXI on newer units) | O-Code rolling | Era One, ON2E… |

The receiver plugs into the board's "SM" connector. There is no fixed-code option on
this platform → **the 433MHz TX replay plan is dead. The wired trigger below is the way.**
(FS1000A-style RF TX modules are not needed for this project.)

New remotes are paired via a memorization procedure (Mode I: T1=Step-by-step,
T2=Pedestrian gate, T3=Open, T4=Close; Mode II: any key → any command). Also
"remote memorization": a new transmitter can be cloned-in near the gate using an
already-memorized one — useful when handing remotes to tenants.

## ✅ The good news: control board terminals (our integration points)

| Terminal | Function | Notes for us |
|---|---|---|
| **P.P.** | Step-by-step input, **normally-open dry contact** — closing the contact = same as remote button (open→stop→close cycle) | **← our trigger point.** ESP32 + relay/optocoupler across this. |
| **STOP** | Safety stop input (NO, NC, or 8.2kΩ; self-recognized) | Don't touch — leave existing wiring. |
| **BLUEBUS** | 2-wire bus for photocells (MOFB) etc. | Don't touch. |
| **S.C.A.** | "Open Gate Indicator" output, **24V max 4W** lamp | **← free gate-state feedback!** Level-2 function L4 can set it to "on if leaf is open". ESP32 reads it via optocoupler → app shows true open/closed. |
| **FLASH** | Flashing lamp output (12V 21W, LUCYB) | Diagnostics: flash counts signal error codes. |
| **AERIAL** | Antenna for radio receiver | n/a |
| **24Vcc tap** | Accessory power at STOP/P.P. terminals: **24Vdc (−30/+50%), max 100mA** | **← can power the ESP32** via a 24V→5V buck converter (ESP32 avg draw fits within 100mA@24V ≈ 2.4W budget). No separate mains adapter needed. |

The step-by-step sequence itself is configurable (Level-2 function L2):
Open–stop–close–stop / Open–stop–close–open / Open–close–open–close /
**Condominium mode** / Close / Man-present. *Condominium mode is interesting for a
multi-tenant setup: a command during opening is ignored, so tenants can't accidentally
stop/reverse the gate mid-open.*

## ✅ Auto-close is built into the board — no ESP32 close-pulse needed

- Level-1 function **L1 "Automatic Closing"**: gate closes by itself after the pause time.
  Factory OFF — just switch it ON.
- **Pause time** (Level-2, L1): 5 / 15 / 30 / 45 / 60 / 80 s (Robus 350; RB600/1000
  goes up to 180 s). Factory 30 s.
- Related: **L2 "Close after photo"** (close 5 s after car clears the photocells —
  ideal for a car gate), **L3 "Always close"** (closes an open gate after power failure).
- Programming is done with the 3 board keys [Open▲][Stop/Set][Close▼] and LEDs
  L1–L6: hold **Set** ~3 s → L1 flashes → ▲/▼ to pick function → **Set** toggles →
  wait 10 s to exit. (Level-2: hold Set through the value selection.)

## Diagnostics worth knowing

- BlueBUS LED: 1 flash/second = OK; flash-count patterns = error table (manual §7.7).
- FLASH lamp blink counts: 2 = photocell triggered, 3 = force limiter (friction),
  4 = STOP input, 6 = max cycles/hour exceeded, etc.
- Board fuses: F1/F2 accessible next to the terminals.
- Maintenance schedule: every 6 months or 10,000 cycles.

## Revised architecture (supersedes the RF plan)

```
Tenant phone ──WiFi──▶ ESP32 (in/near Robus housing)
                         │ web page + access log (unchanged)
                         ├─ GPIO ─▶ optocoupler/relay ──▶ P.P. terminal   (trigger)
                         ├─ GPIO ◀─ optocoupler ◀── S.C.A. 24V output      (gate state)
                         └─ 5V ◀── buck 24V→5V ◀── 24Vcc accessory tap    (power)
Auto-close: board function L1 = ON, pause time to taste. ESP32 sends open pulses only.
Remotes: untouched, keep working (they hit the same step-by-step logic).
```

Notes:
- P.P. wants a clean dry contact: a small relay module is simplest; an optocoupler
  (e.g. PC817, LED side from ESP32 GPIO via ~220Ω) is silent and compact — both fine.
- S.C.A. read: 24V through an optocoupler input (with series resistor ~2.2–4.7kΩ) to
  an ESP32 GPIO. Set board function L4 (level 2) to "on if leaf is open".
- Mains work caution: the terminals area is 24V logic, but 230V enters the same
  housing. Kill power at the breaker before wiring; the manual requires disconnecting
  supply before touching terminals.

## Risk of damaging the Robus — and the rules that prevent it

Destructive mistakes (avoid by design):
- Working powered: a slipped tool/probe bridging pins kills boards → **breaker OFF**.
- Voltage into P.P.: it wants a **dry contact only** → relay/opto, never a GPIO wire.
- Shorting the 24V tap / buck failing short → check polarity with multimeter first,
  and put a **~200 mA inline fuse in our 24V feed** so our fault blows our fuse.

Recoverable mistakes:
- Wrong terminal (e.g. STOP): gate misbehaves until fixed; worst case a blown board
  fuse — F1/F2 are standard replaceable ("1.6A T" visible in photos).
- Accidental programming via [Set]: all functions can be set back; device
  recognition can be re-run (§4.3).
- ⚠ Never hold the **button on the radio receiver module** — long-press deletes ALL
  memorized remotes (Table 11). Re-memorizing is possible but tedious.
- Stripped screw / cracked trim: cosmetic; penetrating oil + correct bit.

Safety net: 2007 unit, no warranty; replacement control board (RBA3) is a ~€100–150
stock spare — worst case is a board swap, never a dead gearmotor. Motor/gearbox are
untouched by this project. Also: disconnect the PS124 buffer battery if present
(manual requirement), and photograph all wiring before changing anything.

## Environment / survivability (heat, cold, moisture)

- ESP32 module rating −40…+85 °C vs Robus's own ambient spec −20…+50 °C — the ESP32
  tolerates more than the environment Nice's own board already survives in.
- Firmware mitigations (free): CPU at 80 MHz, WiFi modem sleep — cuts self-heating.
- Mounting: low in the housing, away from the transformer/motor, boards vertical or
  components-down so condensation can't pool. Housing is IP44 → condensation, not
  rain, is the long-term enemy; optional conformal coating spray (Plastik 70) on the
  finished perfboard.
- First component to age is the buck module's electrolytic caps — buy a decent
  LM2596-type module, not the cheapest, and keep it in the cooler zone.
- No batteries in the design (24V tap power) → nothing that hates heat/cold cycles.

## Manual sources

- Official Nice manual PDF (Robus family, EN): niceforyou.com /upload/manuals/idv0420a00en.pdf
- Robus 350 installer manual (the one these notes are extracted from):
  remote-control-esma.com …/NICE_ROBUS350_RBA2_EN_.pdf — not included in the
  repo (copyright); download from the sources above.
- Quick guide + RB600/RB1000 manuals: manuals.easygates.co.uk, manualslib.com.
