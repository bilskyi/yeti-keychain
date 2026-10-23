# Ideas backlog

Not scheduled. Pull into PROGRESS.md "next" when picked up.

- Serial debug event source: type `t`/`d`/`s` on the USB console to inject
  TAP / DOUBLE_TAP / SHAKE so reactions can be tested before buttons or the
  accelerometer exist. (USB console only works while USB is attached, which
  also disables light sleep, so it is bench-only.)
- Real night mode from wall-clock time: BLE time sync from a phone, or an RTC.
- Battery % smoothing + charging detection (TP4056 CHRG/STDBY pins would
  need two more GPIOs, and only GPIO0–5 are left. Ask before wiring).
- Turn the panel fully off (0xAE) after N minutes with no input, wake on
  button/tap (deep sleep with GPIO wake). Needs FEATURE_BUTTONS or FEATURE_ACCEL.
- Tamagotchi-ish stats (hunger/energy) stored in RTC memory / NVS.
- Pet reacts to being picked up (LIS3DH free-fall / orientation change).
- Tiny games: reaction-time, coin flip, dice on shake.
- More expressions: sad, dizzy (after SHAKE), wink, crying, confused.
- Per-pixel animated mouth for "talking" when BLE notification arrives.
- OTA over WiFi (FEATURE_WIFI) for a sealed case.
