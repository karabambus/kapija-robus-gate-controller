# Wiring & Assembly

Two stages: **bench** (desk, USB power, safe to experiment) and **install**
(inside the Robus housing, breaker off). Do not power anything until the wiring
of that stage is complete and double-checked.

Diagrams: [`bench-wiring.svg`](bench-wiring.svg),
[`install-wiring.svg`](install-wiring.svg), full shopping list: [`BOM.md`](BOM.md)

## Parts

| Part | Role |
|---|---|
| Joy-it NodeMCU-ESP32 | controller, web server |
| COM-RM01 relay module | dry contact for the Robus P.P. input |
| PC817 optocoupler | reads the Robus S.C.A. output (33 V side -> GPIO) |
| **Buck converter, >=40 V input** (LM2596-type) | 33 V accessory tap -> 5.0 V (install stage only) |
| 10 kohm resistor | S.C.A. current limit (install); 470 ohm-1 kohm for bench test |
| Glass fuse ~200 mA + inline holder | protects the Robus accessory supply |


Datasheets for all verified components live in [`datasheets/`](datasheets/).
Relay pin naming per the COM-RM01 manual: **- (GND), + (5 V supply),
S (control signal, 3-5 V logic - 3.3 V GPIO officially supported)**; the relay
energizes on HIGH.

## Stage 1 - bench (see bench-wiring.svg)

Everything runs from USB; the 33 V side does not exist yet.

| From | To | Wire color in diagram |
|---|---|---|
| ESP32 `VIN (5V)` | relay `+` | orange |
| ESP32 `VIN (5V)` | PC817 `A` (pin 1), direct | orange |
| ESP32 `GND` | relay `-` | black |
| ESP32 `GND` | PC817 `E` (pin 3) | black |
| ESP32 `GPIO 26` | relay `S` | blue |
| ESP32 `GPIO 27` | PC817 `C` (pin 4) | green |
| PC817 `K` (pin 2) | **680 ohm** -> loose test wire (touch to `GND` = "gate open") | purple/black |

The 680 ohm LED resistor sits on the cathode side here; either side of the LED
works - it must exist **exactly once** in that loop (470 ohm-1 kohm all fine,
never zero, never twice).

PC817 orientation: hold the chip with its text readable and the **dot top-left
= pin 1 (anode)**. Then: 1-A top-left, 2-K bottom-left, **3-E bottom-right,
4-C top-right** (pins number counter-clockwise - 3 and 4 sit diagonally from
1 and 2). Both diagrams draw the chip in this real physical orientation. The
chip must straddle the breadboard's center groove.

Checklist before plugging in USB:

- [ ] No wire runs between `VIN` and `GND` directly (visual short check)
- [ ] Relay `COM/NO/NC` contacts connected to **nothing**
- [ ] The PC817 LED loop (A->K) contains its series resistor - the LED must
      never sit across 5 V without one

Bench acceptance: relay stays silent at boot, web button produces one clean
~0.5 s click, holding the test button flips the app state to "OTVORENA",
entries appear in the log and survive a power cycle. Firmware flashing:
see [`../firmware/README.md`](../firmware/README.md).

## Alternative build: no perfboard ("flying" harness)

The circuit is small enough to build without a board: modules connect via
female Dupont ends, the PC817 is wired dead-bug style, junctions live in a
screw terminal strip (luster clema). Electrically identical to the perfboard
build - same net-list, same install diagram.

Harness (label every wire end with a masking-tape flag):

| # | Wire | Length | Ends |
|---|---|---|---|
| 1 | P.P. inner (+33 V) -> **fuse holder** -> buck `IN+` | 30 cm | bare / solder |
| 2 | STOP inner (-) -> buck `IN-` | 30 cm | bare / solder |
| 3 | buck `OUT+` -> clema **+5V node** | 10 cm | solder / bare |
| 4 | buck `OUT-` -> clema **GND node** | 10 cm | solder / bare |
| 5 | +5V node -> ESP32 `VIN` | 10 cm | bare / Dupont F |
| 6 | +5V node -> relay `+` | 10 cm | bare / Dupont F |
| 7 | GND node -> ESP32 `GND` | 10 cm | bare / Dupont F |
| 8 | GND node -> relay `-` | 10 cm | bare / Dupont F |
| 9 | GND node -> PC817 leg 3 `E` | 15 cm | bare / solder to leg |
| 10 | ESP32 `D26` -> relay `S` | - | uncut Dupont F-F |
| 11 | ESP32 `D27` -> PC817 leg 4 `C` | 15 cm | Dupont F / solder to leg |
| 12 | S.C.A. **right screw (+)** -> **10 kohm inline** -> PC817 leg 1 `A` | 30 cm | bare / solder to leg |
| 13 | S.C.A. **left screw (-)** -> PC817 leg 2 `K` | 30 cm | bare / solder to leg |
| 14 | relay `COM` -> P.P. outer | 30 cm | screw / screw |
| 15 | relay `NO` -> P.P. inner | 30 cm | screw / screw |

Techniques: resistor and fuse are soldered **inline** (leg-to-wire, heatshrink
over each joint); PC817 gets one wire per leg, each leg individually
heat-shrunk, chip hangs in the bundle; a dab of hot glue over every seated
Dupont connector. Home smoke test before the gate: power ESP32 over USB -
app loads, relay clicks, briefly grounding the D27 wire flips the state.
The opto path is first tested live at the gate (S.C.A. gives it 33 V/10 kohm).

## Stage 2 - install (see install-wiring.svg)

**Breaker OFF. Verify dead: no LEDs on the Robus board.** The 230 V mains
terminal at the bottom of the housing is never touched by this project.

Order of work:

1. **Prepare the buck converter first, on the bench** (LM2596-type, >=40 V
   input - NOT a boost-type module like the XL6019, which can only step
   voltage up): feed it 9-12 V, turn the trim pot until the output reads
   **5.0 V**. Only a pre-adjusted converter goes to the gate.
2. Robus **P.P. inner pin (+)** -> fuse 200 mA -> buck `IN+` (red).
3. Robus **STOP inner pin (-)** -> buck `IN-` (black).
4. Buck `OUT+` -> ESP32 `VIN` and relay `+` (orange).
   Buck `OUT-` -> ESP32 `GND`, relay `-`, PC817 `E` (grey).
5. Relay `COM` -> P.P. **outer** pin; relay `NO` -> P.P. **inner** pin (purple).
   Dry contact - polarity irrelevant. The P.P. inner pin now holds two wires
   (tap feed + relay NO); use a ferrule.
6. S.C.A. **right screw (+)** -> **10 kohm** -> PC817 `A`; S.C.A. **left screw (-)**
   -> PC817 `K` (cyan). Polarity can differ between units - if the app never
   shows "open", swap these two wires.
7. GPIO wiring identical to bench: `GPIO 26` -> relay `S`, `GPIO 27` -> PC817 `C`.
8. Existing wiring (FLASH, BLUEBUS, STOP outer, mains) - **untouched**.

Checklist before breaker ON:

- [ ] Multimeter continuity: no short between buck `IN+` and `IN-`
- [ ] Fuse present in the `+` feed
- [ ] Relay contact wires go to P.P. pins only (not STOP)
- [ ] All strands captured in terminals, no loose whiskers

First power-up:

1. Breaker ON -> Robus boots normally (BlueBUS LED blinks 1x/s), ESP32 LED on.
2. Remote still opens the gate -> nothing regressed.
3. Phone on WiFi -> `http://kapija.local` -> PIN login (if `REQUIRE_LOGIN`
   is on) -> button moves the gate.
4. On the Robus board set Level-2 function **L4 = "on if leaf is open"**
   (manual section 7.2.3, Table 14) -> app state matches reality; if inverted or
   always closed, swap the S.C.A. wires (step 6).
