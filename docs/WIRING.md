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
| LiPo pouch cell | **300 mAh**, size TBD |
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
| Button A | **GPIO1** ↔ GND (no resistor: internal pull-up) |
| Button B | **GPIO3** ↔ GND |
| Button C | **GPIO5** ↔ GND |
| LIS3DH VIN (or VCC) | **3V3** |
| LIS3DH GND | GND |
| LIS3DH SDA / SCL | **GPIO6 / GPIO7** (same wires as the OLED) |
| LIS3DH INT1 (I1) | **GPIO0** |
| LIS3DH SDO, CS | leave unconnected (breakout straps them; address 0x18 or 0x19, both probed) |
| Buzzer + | **GPIO10** |
| Buzzer − | **GND** (or GPIO20 with `PIN_BUZZER_BRIDGE 20`: bridge, louder) |

```
                 ESP32-C3 Super Mini
             ┌─────────────────────────┐
  OLED VCC ──┤ 3V3                GPIO0├── LIS3DH INT1
  LIS3DH VIN─┤ 3V3                GPIO1├── [A] ── GND
             │                    GPIO3├── [B] ── GND
  all GNDs ──┤ GND                GPIO5├── [C] ── GND
             │                    GPIO6├── SDA: OLED + LIS3DH
             │                    GPIO7├── SCL: OLED + LIS3DH
             │                   GPIO10├── buzzer driver
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
| Battery sense | GPIO4 (ADC1_CH4) | 100 kΩ / 100 kΩ divider (1 MΩ / 1 MΩ also works) from battery+ (TP4056 OUT+ side of the switch) to GND, 100 nF from GPIO4 to GND |

## Pins to avoid

- **GPIO2, GPIO8, GPIO9**: strapping pins. GPIO8 also drives the onboard LED.
- **GPIO18/19**: the native USB D−/D+.
- GPIO20 is UART0 RX: free here, since the console is the native USB.
- Only **GPIO0–5** can wake the C3 from deep sleep, so every wake source
  (buttons, LIS3DH INT1) must sit on those pins. The plan above already does.
  The buzzer is output-only, so it takes GPIO10.

## Notes / unverified

- The battery divider must sit **after** the slide switch, otherwise it drains
  ~2 µA from the cell forever even when "off". At 1 M + 1 M that is small, but
  after the switch it is zero.
- The Super Mini's 3.3 V regulator is fed from the 5V pin at battery voltage
  (3.0–4.2 V). Near empty the 3V3 rail droops below 3.3 V. The firmware treats
  a brownout reset as "battery low" (see README).
