# PROGRESS (source of truth, re-read at the start of every session)

## Decisions made
- 2026-10-23 **Toolchain: ESP-IDF v5.3.2 (C), not PlatformIO/Arduino.** Sasha
  asked not to download PlatformIO; ESP-IDF + the riscv toolchain were already
  installed (`~/esp/esp-idf`), same as the BIT project. Consequences:
  - no U8g2: own SH1106/SSD1306 driver + small graphics lib (`main/display/`);
  - no external libraries, so nothing to pin; IDF version pinned by install;
  - `pio check` (cppcheck) replaced by GCC `-fanalyzer` on the firmware
    (`-DKC_ANALYZE=1`, warnings fatal) + `clang --analyze` on host-built logic;
  - sources live in `main/` (IDF convention) instead of `src/`.
- Use the py3.12 IDF Python env (`. ./env.sh`); the py3.14 env fails export.sh's
  requirements check (esp-coredump 1.17.0 excluded by constraints).
- Our component builds with `-Wall -Wextra -Werror` + extras.
- Feature flags overridable with `-DKC_FEATURES="FEATURE_X=1;..."`, one build
  dir per variant (`-B build-allon`). Sources listed explicitly in
  `main/CMakeLists.txt` (IDF can't re-glob).
- Night mode stub = uptime cycle (30 min awake / 10 min night); any input ends
  the current night; BTN_A_LONG forces night. Night face = shut eyes + zzz
  (~130 lit px vs ~1400 awake).
- Low battery / brownout: low-battery face 4 s, then deep sleep with a 10 min
  **timer retry** (a brownout can be switch bounce, not an empty cell).
- Frame pacing: 30 fps while animating, 5 fps when still; flush sends only
  changed column ranges per page, so a still face costs no I2C traffic.
- Placeholder button/LIS3DH sources exist in Phase 1 only so the all-ON build
  compiles; they produce no events.

## Done
- Phase 0: scaffold. Empty app builds clean.
- Phase 1 (v0.1), verified in software only:
  - builds: default, all-ON, SSD1306, -fanalyzer default + all-ON, all clean;
  - host: `make test` passes, `make analyze` clean, `make preview` renders;
  - default image 222,880 B (flash code 116.8 KB, rodata 43.8 KB),
    DRAM 67.7 KB / 321 KB (21 %).

- 2026-10-23 **Phase 1 confirmed on hardware** (OLED only, USB power): boot log
  `display: SH1106 at 0x3C`, Sasha sees the face. SH1106 confirmed, no shift.
  Not yet checked on hardware: battery power path, light-sleep current.
- Flash size set to 4 MB (boot log detected 4 MB on the Super Mini).

- 2026-10-23 **Phase 3 v0 (case), placeholder dimensions**; restyled as a **robot head** at Sasha's request (antenna = loop, bolt ears, visor, engraved panel + rivets, vents, KC-01 lid): `case/` OpenSCAD
  model (params.scad / keychain_case.scad), shell + lid + caps STLs, PNG
  previews, check.py (clearance >= tol + watertight), all pass; check verified
  to fail on an overlapping layout. Outer 39.6 x 45.3 x 21.6 mm. Buttons:
  3 on the front under the screen (Sasha). OLED 1.3", buttons 6x6 (Sasha).
  OpenSCAD = Homebrew `openscad@snapshot` 2026.10.11 (stable cask disabled).

- 2026-10-23 Case direction: Sasha rejected the box/robot-box and 9 concept
  sketches (case/concepts.scad); wants a goofy robot that prints without
  surprises -> **"Boltik"** (keychain_case.scad): big rounded head, lightning
  antenna = loop, dish + bolt ears, waving/hanging noodle arms, stubby legs,
  bandage + DON'T PRESS engraved, WARRANTY VOID IF ANNOYED + wind-up key on
  the back. 66 x 79 x 21.6 mm. check.py now also checks rounded corners and
  printability (overhangs / bridges from the STLs); verified to fail on bad
  geometry. All checks pass.

- 2026-10-23 Boltik v1 rejected ("too flat, few details, too sharp"): limbs
  were flat because the shell printed face down. **Boltik v2**: body prints
  back down + separate face plate prints face up, so all visible surfaces
  are round/raised; pill-profile limbs, raised bezel/rivets/bandage/blush/
  text, 2x M2 screws via OLED top holes into bosses, spacers. check.py fit
  check is now real 3D (OpenSCAD intersection probes). All checks pass with
  no relaxations; each check shown to fail on broken input.

- 2026-10-23 v2 rejected: limbs stuck to the back (not centred), legs ugly,
  "how will it stand -> make it sit". **Boltik v3**: arms/ears are separate
  teardrop-profile parts on M2 screws at mid-depth (arms posable), legs are
  separate, pinned to the face plate with filament, stretched forward: it
  sits. Keyring = thick ring on top (part of body). Switch moved to the back.
  All checks pass (10 STLs). Found and fixed by checks: switch well vs back
  text, arm reach vs desk, joint bosses printed point-up (overhang).

- 2026-10-23 Sasha found a ready model instead: the **Yeti** from
  Compagnon 309 (MakerWorld 2109424; USB-powered, ESP32-C3 + OLED + touch).
  `case/yeti/`: 3 belly buttons cut into the original body (pockets square
  to the sloped, solid belly; adds no supports); battery power via a new
  electronics carrier + back plate (2 stacked USB-C, switch, LiPo), replacing
  the original tray, whose front wall blocked the buttons. All fit probes
  pass against the real mesh; both new parts print without supports.
  Originals stay local and untracked (third-party design).

- 2026-10-25 **Phase 2 (v0.2), software only**: Sasha has buttons, LIS3DH,
  passive buzzer. Real button driver (level IRQ = light-sleep wake, debounce,
  short/long), LIS3DH (hardware click -> TAP/DOUBLE_TAP on INT1, software
  shake/tilt), buzzer (`FEATURE_SOUND`, LEDC on GPIO10, XTAL clock, chirps per
  reaction, BTN_B_LONG mutes). Main loop waits in `input_wait()` so an IRQ
  ends the idle frame early. BUTTONS/ACCEL/SOUND now default ON. Host
  `input_test` (mutation-checked); default/analyze/ssd1306/all-on/min builds clean.
  Tilt axis/sign are PLACEHOLDERs until mounted.
- 2026-10-25 **Phase 2 on hardware** (USB power, breadboard): OLED, LIS3DH
  (0x19), buttons A/B, buzzer all work. Bench fixes: taps read every frame
  (INT1 alone missed them), tap threshold 24, ringing = clicks <80 ms apart,
  double tap in software (chip's DCLICK never fired), taps dropped around
  button presses (the press jolt read as a tap). Buzzer too quiet with no
  transistor: tunes moved to 1.5-4 kHz, drive cap 3 = loud enough (Sasha).
  Optional bridge mode (− on GPIO20) is in the code, off. ~1 in 7 double taps reads as single.

## In progress
- Yeti: waiting for Sasha's review; then caliper measurements (boards,
  battery, switch, OLED position) to replace PLACEHOLDERs. Boltik design
  parked in case/.

## Next
- Phase 2 (stubs): flesh out button + LIS3DH placeholders with TODOs and the
  deep-sleep wake wiring. Only after Phase 1 is confirmed.
- Phase 3 (case): only after hardware confirmation + caliper measurements.

## Open questions
- Battery + power path + Rprog swap: **deferred by Sasha (2026-10-23)**, battery
  goes in last. Reminder before first charge: stock Rprog 1.2k is ~3x too much
  for 300 mAh; charge only supervised until swapped.
- LiPo is **300 mAh** (Sasha, 2026-10-23); physical size still unknown (needed
  for Phase 3). Stock Rprog 1.2k = ~1 A = 3.3C, too much: needs 4.7-5.1k
  (Sasha only has 100k on hand, which is fine for the divider, not Rprog).
- Battery divider will be 100k + 100k (not 1M + 1M), ratio still 2.
- Super Mini power LED: always-on, drains the cell even in deep sleep. Remove it?
