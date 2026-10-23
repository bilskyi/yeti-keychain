# PROGRESS (source of truth, re-read at the start of every session)

## Decisions made
- 2026-10-23 **Toolchain: ESP-IDF v5.3.2 (C), not PlatformIO/Arduino.** Sasha
  asked not to download PlatformIO; ESP-IDF + the riscv toolchain were already
  installed (`~/esp/esp-idf`), same as the BIT project. Consequences:
  - no U8g2: own SH1106/SSD1306 driver + small graphics lib (`main/display/`);
  - no external libraries, so nothing to pin; IDF version pinned by install;
  - `pio check` (cppcheck) is replaced by GCC `-fanalyzer` on the firmware
    (`-DKC_ANALYZE=1`) + `clang --analyze` on the host-built logic;
  - sources live in `main/` (IDF convention) instead of `src/`.
- Use the py3.12 IDF Python env (`. ./env.sh`); the py3.14 env fails export.sh's
  requirements check (esp-coredump 1.17.0 excluded by constraints).
- Our component builds with `-Wall -Wextra -Werror` + extras.
- Feature flags overridable with `-DKC_FEATURES="FEATURE_X=1;..."`, one build
  dir per variant (`-B build-allon`).

## Done
- Phase 0: scaffold (git, .gitignore, CMake, sdkconfig.defaults, config.h,
  folders, docs/WIRING.md, IDEAS.md, PROGRESS.md). Empty app builds clean.

## In progress
- Phase 1: v0.1 firmware.

## Next
- Phase 1 verification, then report and STOP.

## Open questions
- LiPo capacity / size still placeholders (`{CAPACITY}`, `{SIZE}`) in the brief.
  Needed for Phase 3 (case) and for the battery % curve; not for v0.1.
