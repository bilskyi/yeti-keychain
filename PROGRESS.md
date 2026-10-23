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

## In progress
- Nothing. Waiting for Sasha to pick the next phase.

## Next
- Phase 2 (stubs): flesh out button + LIS3DH placeholders with TODOs and the
  deep-sleep wake wiring. Only after Phase 1 is confirmed.
- Phase 3 (case): only after hardware confirmation + caliper measurements.

## Open questions
- LiPo is **300 mAh** (Sasha, 2026-10-23); physical size still unknown (needed
  for Phase 3). Stock Rprog 1.2k = ~1 A = 3.3C, too much: needs 4.7-5.1k
  (Sasha only has 100k on hand, which is fine for the divider, not Rprog).
- Battery divider will be 100k + 100k (not 1M + 1M), ratio still 2.
- Super Mini power LED: always-on, drains the cell even in deep sleep. Remove it?
