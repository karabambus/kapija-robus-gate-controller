# Nice Robus platform notes

The facts about the Nice Robus drive family that this project relies on.
Verified on a ROBUS350; RB400/600/1000 share the same control platform.

## Terminals used

The control board's low-voltage terminal row is FLASH, S.C.A., BLUEBUS,
STOP, P.P. This project touches only:

| Terminal | Function | Used for |
|---|---|---|
| P.P. | step-by-step dry-contact input | relay pulse = "button press" |
| S.C.A. | open-gate indicator output | gate state via optocoupler |
| P.P./STOP inner pins | accessory power tap | powers the ESP32 |

Everything else (photocells on BLUEBUS, the flashing lamp, the radio
receiver) stays untouched.

## Accessory power reality

The tap is specified as 24 Vdc (-30/+50%), max 100 mA - but it measures
33 V unloaded. Any regulator fed from it must tolerate at least 40 V
input (LM2596 class). Details and part list in [BOM.md](BOM.md).

## Board settings and behavior

- Level-2 function L4 ("open gate indicator") must be set so S.C.A. is
  on while the leaf is open. This is the only board setting the project
  needs.
- S.C.A. blinks during travel and is steady at the end positions. The
  firmware settle-filters it and uses the blinking to detect movement,
  including moves started with an RF remote.
- The step-by-step sequence is asymmetric (field-verified): a command
  during opening STOPS the gate; a command during closing REVERSES it
  to full open (obstacle-safety behavior).
- Optional later: L1 auto-close, L2 condominium mode - see
  [../docs/BACKLOG.md](../docs/BACKLOG.md).

## Radio

The existing rolling-code remotes (433.92 MHz) keep working unchanged.
RF replay is impossible by design (rolling code), which is exactly why
this project triggers the gate over a wire instead.
