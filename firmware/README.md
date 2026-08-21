# Firmware

PlatformIO project for the ESP32 controller. No external libraries — ESP32
Arduino core only.

## Layout

```
platformio.ini            # board/framework config (esp32dev, Arduino, LittleFS)
include/
└── config.example.h      # template — copy to config.h (gitignored) and edit
src/
├── main.cpp              # boot sequence, main loop, maintenance reboots
├── net.*                 # WiFi modes (station/AP/dual), watchdog, identity
├── gate_controller.*     # relay pulse + S.C.A. state (all GPIO access)
├── auth.*                # per-tenant PIN login, RAM sessions
├── login_backoff.*       # failed-login lockout policy (per-IP + global)
├── access_log.*          # persistent log on LittleFS, rotation
├── time_util.*           # NTP + local-time formatting (Europe/Zagreb)
├── web_ui.*              # HTTP routes + HTML (Croatian UI, English toggle)
└── web_update.*          # /update page: flash new firmware from a browser
```

## Build & flash

Prerequisite: [PlatformIO](https://platformio.org/) (VS Code extension or
`pipx install platformio`). Wire the hardware first —
see [`../hardware/WIRING.md`](../hardware/WIRING.md).

```sh
cp include/config.example.h include/config.h   # then edit: WiFi, tenant PINs, pins
pio run                # compile
pio run -t upload      # flash over micro-USB (data cable)
pio device monitor     # serial console @115200, prints the IP after boot
pio test -e native     # host-side unit tests (no hardware needed)
```

Open `http://kapija.local` (or the printed IP) from a device on the same WiFi.

## OTA updates (flash over WiFi)

After the first USB flash, the device accepts firmware over the network —
no need to open the gate housing.

### Browser upload (easiest)

1. `pio run` to build, which produces `.pio/build/esp32dev/firmware.bin`.
2. Browse to `http://kapija.local/update` (user `admin`, password =
   `OTA_PASSWORD` from `config.h`).
3. Pick the `firmware.bin` and upload. The device flashes it (~15 s),
   logs an "update" row, and reboots into the new version - check the
   version on the app's diagnostics line afterwards.

A failed or interrupted upload is harmless: it lands in the inactive flash
partition and the running firmware keeps going. No firewall rules and no
password escaping involved - this path avoids every espota pitfall below.

### espota (fallback)

1. In `platformio.ini`, uncomment the three `espota` lines; `--auth` must
   match `OTA_PASSWORD` from `config.h`.
2. Be on the same network as the device, then `pio run -t upload` — it now
   uploads over WiFi to `kapija.local` instead of a serial port.
3. The device reboots into the new firmware; the access log and settings
   survive (they live in a separate flash partition).

Fallback: USB flashing always keeps working — plug a cable into the board
and comment the `espota` lines back out.

### OTA troubleshooting

| Symptom | Cause / fix |
|---|---|
| `Authenticating...FAIL` although the password is right | `$` (and other metacharacters) in `OTA_PASSWORD` get mangled twice on the way to espota: PlatformIO's ini interpolation eats single `$`, and the shell then expands `$$` to its PID. Avoid `$`-type characters in OTA passwords, or bypass both layers by calling espota.py directly (below). |
| `Authenticating...OK` then `No response from device` | Not the device — your computer's firewall. espota makes the ESP32 connect **back** to your machine on a TCP port. Pin the port with `-P 33333` and allow it in, e.g. `sudo ufw allow from 192.168.1.0/24 to any port 33333 proto tcp`. |

Direct espota call (no ini editing, password stays out of tracked files —
single quotes prevent shell mangling):

```sh
python3 ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
  -i kapija.local -p 3232 -P 33333 --auth='<OTA_PASSWORD>' \
  -f .pio/build/esp32dev/firmware.bin -r
```

**Standalone AP builds** (`WIFI_AP_MODE true`): OTA still works. Join the
ESP32's own WiFi network (`AP_SSID`) from the flashing computer and set
`upload_port = 192.168.4.1` in `platformio.ini` — mDNS (`kapija.local`) may
not resolve on the AP network, the IP always works.

## Configuration notes

- `WIFI_AP_MODE` — `false` (default) joins the home WiFi; `true` makes the
  ESP32 broadcast its own network instead (`AP_SSID`/`AP_PASS`, WPA2, phones
  browse to `http://192.168.4.1`). On the AP network there is no internet:
  connected phones drop offline for the duration, and no NTP means log
  entries show "—" instead of a date (the device reboots on a 48 h uptime
  cadence instead of the 4 a.m. schedule).
- `WIFI_DUAL_MODE` — join the home WiFi AND broadcast the own AP at the same
  time. The AP is a fallback control path for when the router is down: phones
  join it and browse to `http://192.168.4.1`. NTP, mDNS and OTA keep working
  through the router side; when the router is unreachable the device does not
  reboot - it keeps serving the AP and retries the router every 3 minutes.
  One radio means one shared channel: the AP follows the router's channel, so
  a router channel change gives AP clients a few-second blip before they
  rejoin on their own (set a fixed channel in the router to avoid even that).

- `RELAY_ACTIVE_HIGH` — if the relay clicks ON at boot and releases after,
  flip this to `false`.
- `SCA_OPEN_IS_LOW` — standard wiring pulls the GPIO low when the gate is
  open; flip only if your opto wiring inverts it.
- Session tokens live in RAM: a reboot logs all users out (by design).
