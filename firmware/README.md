# Firmware

PlatformIO project for the ESP32 controller. No external libraries — ESP32
Arduino core only.

## Layout

```
platformio.ini            # board/framework config (esp32dev, Arduino, LittleFS)
include/
└── config.example.h      # template — copy to config.h (gitignored) and edit
src/
├── main.cpp              # boot, WiFi, mDNS, NTP, server wiring, watchdog
├── gate_controller.*     # relay pulse + S.C.A. state (all GPIO access)
├── auth.*                # shared-password login, RAM sessions, lockout
├── access_log.*          # persistent log on LittleFS, rotation
├── time_util.*           # NTP + local-time formatting (Europe/Zagreb)
└── web_ui.*              # HTTP routes + HTML (Croatian UI, English toggle)
```

## Build & flash

Prerequisite: [PlatformIO](https://platformio.org/) (VS Code extension or
`pipx install platformio`). Wire the hardware first —
see [`../hardware/WIRING.md`](../hardware/WIRING.md).

```sh
cp include/config.example.h include/config.h   # then edit: WiFi, password, pins
pio run                # compile
pio run -t upload      # flash over micro-USB (data cable)
pio device monitor     # serial console @115200, prints the IP after boot
```

Open `http://kapija.local` (or the printed IP) from a device on the same WiFi.

## OTA updates (flash over WiFi)

After the first USB flash, the device accepts firmware over the network —
no need to open the gate housing:

1. In `platformio.ini`, uncomment the three `espota` lines; `--auth` must
   match `OTA_PASSWORD` from `config.h`.
2. Be on the same network as the device, then `pio run -t upload` — it now
   uploads over WiFi to `kapija.local` instead of a serial port.
3. The device reboots into the new firmware; the access log and settings
   survive (they live in a separate flash partition).

Fallback: USB flashing always keeps working — plug a cable into the board
and comment the `espota` lines back out.

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

- `RELAY_ACTIVE_HIGH` — if the relay clicks ON at boot and releases after,
  flip this to `false`.
- `SCA_OPEN_IS_LOW` — standard wiring pulls the GPIO low when the gate is
  open; flip only if your opto wiring inverts it.
- Session tokens live in RAM: a reboot logs all users out (by design).
