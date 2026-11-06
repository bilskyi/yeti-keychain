# OLED keychain companion

An animated pixel-art face on a 1.3" 128×64 OLED, driven by an ESP32-C3 Super
Mini on a LiPo. ESP-IDF v5.3, plain C, no external libraries.

> ## ⚠️ Safety
> 1. **Never plug USB into the Super Mini while the slide switch is ON.** It
>    back-feeds 5 V into the LiPo. Switch OFF, then plug in to flash. Charge
>    only through the TP4056's own USB-C.
> 2. **Charge current.** The stock Rprog (1.2 kΩ) charges at ~1 A: fine for
>    the 850 mAh cell if it allows 1C. If not, or it gets hot, use 1.5 kΩ
>    (~800 mA). Watch the first charges.
>
> Full wiring: [docs/WIRING.md](docs/WIRING.md).

## Controls

Three buttons, left to right **A, B, C**, with one job each on every screen.
The bottom line of the screen shows each button's word above it.

| | A (left) | B (middle) | C (right) |
|---|---|---|---|
| Face | pet | games · **hold: music** | status card + settings |
| A list | play / change | next | back to the face |
| Game | play (each game says how) | second action | back to the list |
| Music: the list | play | next song | back to the face |
| Music: playing | stop | next song, at once | stop |

Everything is one press from the face, with no categories in between. The
games list opens on the last game played. The status card shows volume,
brightness and battery; B goes on to the volume, brightness and calibrate
settings. Music opens on the last song played, silent until A. Each song
gets one of nine animations at random, driven by its notes, never the same
one twice in a row. A crossed-out speaker in the top-right corner means the
sound is off.
Hold C anywhere to go straight home to the face.

Games: catch, maze (tilt to roll the ball), flappy, snake, dino roar (roar to
jump), reaction, dice. 11 songs. Shake it, pet the touch pad, tickle it,
talk to it. It dozes off after 10 minutes untouched, wakes when picked up, and turns the screen
off lying face down (after Settings > Calibrate).

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

## Host tests and previews (no hardware)

```bash
cd host
make test       # face, mood, menu and games, sleep policy, mic, burn-in, battery maths
make preview    # every face to host/out/faces.png, the menu and games to host/out/app.png
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

What each event does: the face in `face_on_event()` (`main/face/face.c`),
waking and sleep in `policy_on_event()` (`main/power/policy.c`), the menu and
games in `app_on_event()` (`main/app/app.c`).
