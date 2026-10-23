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
| LiPo pouch cell | **capacity / size: TBD (still a placeholder)** |
| Slide switch | in OUT+ line |
| Tactile buttons ×3 | later, not wired yet |
| LIS3DH accelerometer | later, not bought yet |

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

## Reserved for later (already in `main/config.h`, not wired)

| Function | Pin | Notes |
|---|---|---|
| Button A | GPIO1 | to GND, internal pull-up |
| Button B | GPIO3 | to GND, internal pull-up |
| Button C | GPIO5 | to GND, internal pull-up |
| LIS3DH SDA/SCL | GPIO6 / GPIO7 | shares the OLED's I²C bus (address 0x18 or 0x19, never clashes with 0x3C) |
| LIS3DH INT1 | GPIO0 | wake source |
| Battery sense | GPIO4 (ADC1_CH4) | 1 MΩ / 1 MΩ divider from battery+ (TP4056 OUT+ side of the switch) to GND, 100 nF from GPIO4 to GND |

## Pins to avoid

- **GPIO2, GPIO8, GPIO9**: strapping pins. GPIO8 also drives the onboard LED.
- **GPIO18/19**: the native USB D−/D+.
- Only **GPIO0–5** can wake the C3 from deep sleep, so every wake source
  (buttons, LIS3DH INT1) must sit on those pins. The plan above already does.

## Notes / unverified

- The battery divider must sit **after** the slide switch, otherwise it drains
  ~2 µA from the cell forever even when "off". At 1 M + 1 M that is small, but
  after the switch it is zero.
- The Super Mini's 3.3 V regulator is fed from the 5V pin at battery voltage
  (3.0–4.2 V). Near empty the 3V3 rail droops below 3.3 V. The firmware treats
  a brownout reset as "battery low" (see README).
