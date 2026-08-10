# Bill of Materials

Everything needed to build the controller as installed. Approximate hobby-store
prices; total well under 30 € (assuming you own the tools). Datasheets for the
verified parts are in [`datasheets/`](datasheets/).

## Electronics

| Qty | Part | Spec / verified example | ~€ | Notes |
|---|---|---|---|---|
| 1 | ESP32 dev board | ESP32-WROOM devkit (Joy-it NodeMCU-ESP32, 30-pin, micro-USB) | 10–15 | S3/C3 boards also work; avoid ESP8266. Check VIN, GND, and two free GPIOs are on the same pin column — makes the harness cleaner. |
| 1 | 5 V relay module, 1-channel | Joy-it COM-RM01 | 2 | Control pin must accept 3.3 V logic (COM-RM01 officially does: S = 3–5 V, energizes on HIGH). |
| 1 | PC817 optocoupler | Sharp PC817 (buy 2–3, they cost cents) | <1 | Reads the S.C.A. output. |
| 1 | Buck converter, **input ≥40 V** | LM2596-type module | 2–5 | **Critical spec.** The Robus "24 V" accessory tap measures 33 V (spec allows up to ~36 V). Mini360 (23 V max) burns instantly; the Joy-it SBC-XL6019 is a step-UP and cannot do 33 V→5 V — we bought one by mistake, don't repeat it. |
| 1 | Fuse ~200 mA + inline holder | 5×20 mm glass, or a 0.2 A polyfuse | 1 | Protects the Robus accessory supply from faults in our circuit. Not 6.3 A — 0.2 A. |
| 1 | Resistor 10 kΩ | 1/4 W | <1 | S.C.A. current limit at 33 V. |
| 1 | Resistor 680 Ω | 470 Ω–1 kΩ all fine | <1 | Bench test only (opto LED from 5 V). |

## Wiring & assembly

| Qty | Item | Notes |
|---|---|---|
| ~3 m | Stranded signal wire 0.5 mm² | Two colors minimum. Oversized for the <100 mA currents — chosen for mechanical strength in screw terminals. |
| 1 set | Dupont jumper wires, female-female | Module interconnects; a crimp kit gives cleaner results. |
| 1 | Screw terminal strip ("luster" type) | Junction points of the flying harness — no perfboard needed. |
| — | Heat-shrink tubing, zip ties, masking tape | Tape flags to label every wire end. |
| 2 | Wire ferrules | The Robus P.P. terminal takes two wires in one screw. |
| opt. | Conformal coating spray (Plastik 70) | The housing is IP44 — condensation, not rain, is the long-term enemy. |

## Tools

Soldering iron + solder, multimeter (mandatory — tap voltage and polarity are
verified before anything is connected), wire strippers, small flat screwdriver
for terminals, PH2 screwdriver for the housing, USB **data** cable matching the
ESP32 board (first flash is over USB; updates are OTA afterwards).

Bench extras: breadboard and male-female jumpers, plus any 5 V USB phone
charger — the 33 V side doesn't exist on the bench.

## Situational

- **Weak WiFi at the gate** → ESP32 variant with u.FL/IPEX connector + external antenna.
- **No room inside the Robus housing** → IP54+ box, cable gland, 4-core 0.5 mm² cable.
- **Extra remote for a tenant** → depends on the receiver plugged into the board's
  SM connector: SMXI → FLOR family (FLO2R-S / Era Flor); SMXIS → Smilo SM2.
  Photograph the receiver module before ordering; gate-automation dealers stock
  these, general electronics shops usually don't. Remotes take 12 V 23A batteries.
