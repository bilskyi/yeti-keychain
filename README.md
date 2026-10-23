# OLED keychain companion

An animated pixel-art face on a 1.3" 128×64 OLED, driven by an ESP32-C3 Super
Mini on a LiPo. ESP-IDF v5.3, plain C, no external libraries.

> ## ⚠️ Safety
> 1. **Never plug USB into the Super Mini while the slide switch is ON.** It
>    back-feeds 5 V into the LiPo. Switch OFF, then plug in to flash. Charge
>    only through the TP4056's own USB-C.
> 2. **Change the TP4056's Rprog from 1.2 kΩ to ~4.7–5.1 kΩ** (~200–250 mA)
>    before charging a small cell.
>
> Full wiring: [docs/WIRING.md](docs/WIRING.md).

## Build

```bash
. ./env.sh                 # ESP-IDF env (pins the working py3.12 Python env)
idf.py build               # default: display only, every feature flag OFF
```

Variants, each in its own build dir:

```bash
idf.py -B build-ssd1306 -DKC_FEATURES="DISPLAY_DRIVER=DISPLAY_SSD1306" build
idf.py -B build-allon   -DKC_FEATURES="FEATURE_BUTTONS=1;FEATURE_ACCEL=1;FEATURE_BATTERY_ADC=1;FEATURE_WIFI=1;FEATURE_BLE=1" build
idf.py -B build-analyze -DKC_ANALYZE=1 build     # GCC -fanalyzer, warnings are errors
```

Or change the default for good in `main/config.h`.

## Flash

Slide switch **OFF**, USB-C into the **Super Mini**, then:

```bash
. ./env.sh
idf.py -p /dev/cu.usbmodem* flash monitor
```

If the port does not show up: hold BOOT (GPIO9), tap RESET, release BOOT.
While USB is attached the chip will not light-sleep, so the log keeps working.

## Host tests and face preview (no hardware)

```bash
cd host
make test       # face, mood, blink timing, night policy, burn-in, battery maths
make preview    # renders every face to host/out/faces.png
make analyze    # clang static analyzer on the pure-C parts
```

## Layout

| Path | What |
|---|---|
| `main/config.h` | pins, display driver switch, feature flags, timings, power policy |
| `main/main.c` | boot + the frame loop (thin) |
| `main/display/` | `gfx` (framebuffer + shapes, pure C), `display` (SH1106/SSD1306 I²C driver, sends only changed bytes) |
| `main/face/` | `face` (expressions, blink, idle behaviours, reactions), `mood` (state machine), `hud` (battery icon) |
| `main/input/` | `input_event` (the event enum), `input` (queue + dispatcher + source table), `buttons` / `accel_lis3dh` (placeholders) |
| `main/power/` | `power` (80 MHz + auto light sleep, brownout, deep sleep), `policy` (night mode, burn-in shift), `battery` (+ pure `battery_logic`) |
| `main/net/` | WiFi/BLE placeholders |
| `host/` | host-built tests and preview |

## Adding an input later

Write an `input_source_t` (init / poll / arm_wakeup) in `main/input/`, list it
in `SOURCES[]` in `input.c` under its flag, and add the file to
`main/CMakeLists.txt`. It posts `input_event_t`s; the face and the power policy
already react to all of them.

| Event | Face | Power |
|---|---|---|
| TAP | surprised → happy | wakes from night |
| DOUBLE_TAP | love | wakes |
| SHAKE | angry | wakes |
| TILT_LEFT/RIGHT, BTN_B/C short | look left/right | wakes |
| BTN_A short | next expression | wakes |
| BTN_A long | — | toggles night |
| BTN_B/C long | happy | wakes |
