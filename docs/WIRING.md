# Wiring

> ## ⚠️ Safety: read before plugging anything in
>
> 1. **Never plug USB into the ESP32-C3 Super Mini while the slide switch is ON.**
>    The Super Mini's USB 5 V would back-feed through its 5V pin into the
>    TP4056 OUT+ and onto the LiPo. To flash: switch **OFF** first, then plug
>    USB into the Super Mini. To charge: use **only the TP4056's own USB-C**.
> 2. **Change the TP4056's Rprog from 1.2 kΩ to ~4.7–5.1 kΩ** before charging a
>    small cell. Stock 1.2 kΩ sets ~1 A, far too much for a small pouch cell;
>    4.7–5.1 kΩ gives ~200–250 mA. (Rprog is the resistor on the PROG pin,
>    usually marked `122` stock.)

## Parts

| Part | Notes |
|---|---|
| ESP32-C3 Super Mini | generic clone, native USB-C, no charger |
| 1.3" 128×64 I²C OLED, 4-pin | SH1106 assumed; SSD1306 is a one-line switch in `main/config.h` |
| TP4056 USB-C module with DW01 protection | charging + over-discharge cut-off |
| LiPo pouch cell | **850 mAh** (was 300 mAh: the stock TP4056 charges at ~1 A, ~1.2C for 850 mAh) |
| Slide switch | in OUT+ line |
| Tactile buttons ×3 | 6×6 mm, A/B/C |
| LIS3DH accelerometer breakout | I²C, shares the OLED bus |
| Passive buzzer | driven by PWM; bare magnetic ones need the NPN below |
| NPN transistor (S8050 / 2N2222 / BC547), 1 kΩ, 10 kΩ, 1N4148, 33–47 Ω | buzzer driver (bare buzzer only) |

## Connections (current, v0.1)

| From | To |
|---|---|
| OLED VCC | Super Mini **3V3** |
| OLED GND | GND |
| OLED SDA | **GPIO6** |
| OLED SCL | **GPIO7** |
| LiPo + / − | TP4056 **B+ / B−** |
| TP4056 OUT+ | slide switch → Super Mini **5V** pin |
| TP4056 OUT− | GND |

```
 LiPo ──B+/B−── TP4056 ──OUT+── [slide sw] ── 5V ┐
                  │                              │  ESP32-C3 Super Mini
                  └────OUT−─────────────────── GND│
                                                  │  3V3 ── OLED VCC
                                                  │  GND ── OLED GND
                                                  │  GPIO6 ─ OLED SDA
                                                  │  GPIO7 ─ OLED SCL
```

## Connections (Phase 2: buttons, LIS3DH, buzzer)

| From | To |
|---|---|
| Button A (left) | **GPIO20** ↔ GND (no resistor: internal pull-up; wakes from light sleep, not deep sleep) |
| Button B (middle) | **GPIO3** ↔ GND |
| LIS3DH VIN (or VCC) | **3V3** |
| LIS3DH GND | GND |
| LIS3DH SDA / SCL | **GPIO6 / GPIO7** (same wires as the OLED) |
| LIS3DH INT1 (I1) | **GPIO0** |
| LIS3DH SDO, CS | leave unconnected (breakout straps them; address 0x18 or 0x19, both probed) |
| Buzzer + | **GPIO10** |
| Buzzer − | **GND** (or GPIO21 with `PIN_BUZZER_BRIDGE 21`: bridge, louder) |
| Button C (right) | **GPIO1** ↔ GND (no resistor: internal pull-up) |
| ~~Red LED~~ | removed for now (2026-10-27): GPIO20 went to button A. To bring it back it needs another pin, then `FEATURE_LED 1` |
| TTP223 touch pad VCC / GND | **3V3** / GND |
| TTP223 I/O | **GPIO5** (no resistor; leave its A/B jumpers open: high while touched) |
| Mic module (MAX4466) VCC / GND | **3V3** / GND (not 5 V: its output must stay under 3.3 V) |
| Mic module OUT | **GPIO4** (ADC; sits at ~1.65 V when quiet) |

Touch pad: keep your finger off it while powering on (it calibrates then).
Behind the case wall it works through ~2 mm of plastic. Mic: the gain pot
on the back, start at the middle; too high and talking reads as blowing
(the log's `heard ... overload` %). Point the mic at a hole in the case.
Both are optional: with nothing on GPIO5 it never feels a touch, and with
nothing on GPIO4 the firmware logs `no mic` and keeps its ears off.

```
                 ESP32-C3 Super Mini
             ┌─────────────────────────┐
  OLED VCC ──┤ 3V3                GPIO0├── LIS3DH INT1
  LIS3DH VIN─┤ 3V3                GPIO1├── [C] ── GND
             │                    GPIO3├── [B] ── GND
  all GNDs ──┤ GND                GPIO4├── mic OUT
             │                    GPIO5├── TTP223 I/O
             │                    GPIO6├── SDA: OLED + LIS3DH
             │                    GPIO7├── SCL: OLED + LIS3DH
             │                   GPIO10├── buzzer driver
             │                   GPIO20├── [A] ── GND
             │ 5V (USB / later LiPo)    │
             └─────────────────────────┘
```

Buzzer driver, **bare 2-pin buzzer** (magnetic, ~16 Ω: too much current for a
GPIO, so it goes through a transistor):

```
 3V3 (or 5V for louder) ──┬──────────┐
                          │          │
                        buzzer +   1N4148 (stripe to +)
                        buzzer −     │
                          ├──────────┘
                          │
                      33–47 Ω   (limits peak current; skip on USB if too quiet)
                          │
                          C
 GPIO10 ── 1 kΩ ──┬────── B   NPN (S8050 / 2N2222 / BC547)
                  │       E
                10 kΩ     │
                  │       │
 GND ─────────────┴───────┴──────
```

- **No transistor yet (current bench setup):** buzzer + → GPIO10, − → GND,
  directly, at max pin drive (`SOUND_DRIVE_CAP 3`); tunes sit at 1.5-4 kHz,
  near the buzzer's resonance. Loud enough on the bench. For louder with no
  parts: − → GPIO20 and `PIN_BUZZER_BRIDGE 20` (pins in antiphase, double the
  voltage; both sit low at rest).
- A small **piezo** disc instead: GPIO10 → 100 Ω → piezo → GND, no transistor.
- A **3-pin module** (VCC / I/O / GND) already carries its transistor: VCC →
  3V3, GND → GND, I/O → GPIO10. If it beeps constantly at rest, it is a
  "low level trigger" module: set `SOUND_ACTIVE_LOW 1` in `main/config.h`.

## Reserved for later (already in `main/config.h`, not wired)

| Function | Pin | Notes |
|---|---|---|
| Battery sense | **GPIO2** (ADC1; strapping pin, so check it still boots) | 100 kΩ / 100 kΩ divider from the 5V pin (after the switch) to GND, middle to GPIO2, optional 100 nF from GPIO2 to GND. Then `PIN_BATTERY_ADC 2`, `FEATURE_BATTERY_ADC 1` |
| Charger sense | **GPIO21** | TP4056 **IN+** (USB 5 V, only while the charging cable is in) → 5.1 kΩ → GPIO21 → 2 × 5.1 kΩ in series → GND (fitted 2026-10-28). ~3.3 V with the cable, 0 V without. Only the end of the last resistor touches GND: the joints between them must not. Drives the charging animation; "full" comes from the battery voltage (≥ ~4.15 V, switch on). With the switch off and the cable in, ~0.8 mA leaks into the unpowered chip through the pin's clamp diode: charge with the switch on |

## Pins to avoid

- **GPIO2, GPIO8, GPIO9**: strapping pins. GPIO8 also drives the onboard LED.
- **GPIO18/19**: the native USB D−/D+.
- GPIO20 is UART0 RX: free here, since the console is the native USB. GPIO21
  is UART0 TX: the ROM drives it at boot, so nothing that drives it hard
  belongs there (the charger-sense divider, 100k/200k, is weak enough).
- Only **GPIO0–5** can wake the C3 from deep sleep, so every wake source
  (buttons, LIS3DH INT1) must sit on those pins. Button A on GPIO20 is the
  exception: it wakes from light sleep only, and B or C wakes from deep sleep.
  The buzzer is output-only, so it takes GPIO10.

## Notes / unverified

- The battery divider must sit **after** the slide switch, otherwise it drains
  ~2 µA from the cell forever even when "off". At 1 M + 1 M that is small, but
  after the switch it is zero.
- The Super Mini's 3.3 V regulator is fed from the 5V pin at battery voltage
  (3.0–4.2 V). Near empty the 3V3 rail droops below 3.3 V. The firmware treats
  a brownout reset as "battery low" (see README).
