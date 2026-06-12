# Agent Indicator — Power Design

> Rev 0.2 · 中文版: [../02-power-design.md](../02-power-design.md) · Companion: [01-system-design.md](01-system-design.md)

## 1. Power Topology

```
USB-C PD ──► CH224K ──► VPD(15V≥3A) ──► ideal diode ─┐
  (request 15V)                        (LM5050-1+PFET)│
                                                      ├──► VIN_SEL ──► MP2760 ──► VSYS(9~12.6V)
XT30 ──► rev-prot PFET+TVS ──► VIN(12-24V) ─► ideal ──┘            (NVDC buck-boost   │
                                          diode (priority)         charger+power path)│
                                                                        ▲▼            │
                                            3S 18650 ◄── S-8254A prot ──┘             │
                                            (11.1-12.6V)  HY2213×3 balance            │
                                                                                      │
        ┌─────────────────────────────────────────────────────────────────────────────┤
        │                                     │                                       │
   TPS56637                               MP2315S                                     │
   VLED 5.0V/5A ──► PMOS sw (LED_PWR_EN)  5V0 2A ──┬─► TPS2121 ◄── VUSB (data-port VBUS)
   ──► WS2812B Matrix/Ring/Bars                    │   (seamless mux + anti-backfeed)
                                                   ├─► SGM2212-3.3 ──► VDD 3.3V
                                                   │    (SD card / IMU / codec logic / I2C)
                                                   └─► ferrite+tantalum ──► VAUDIO 5V (NS4150B)
```

Key points:
- **MP2760 NVDC power path**: when an input is present the system runs from it while charging; on input removal it switches to battery seamlessly — no extra switching circuit.
- **Input priority**: XT30 and VPD are ORed through ideal diodes. The XT30 branch gets a 30mV bias so it wins at equal voltage; at 24V XT30 wins naturally.
- **VLED / 5V0 isolation**: two independent bucks, LED ripple never reaches logic; the data-port VBUS only enters TPS2121, which muxes against 5V0 with no backfeed (USB debug works with main power removed; LEDs stay off).

## 2. Component Selection

| Node | Part | Key specs | Notes |
|---|---|---|---|
| PD sink | **CH224K** | requests 15V (CFG), 100W class | 15V > 12.6V + charge margin; LED pin indicates PD success |
| Input ORing | **LM5050-1** ×2 + PFET (AO4407A) | ideal-diode controller | LM74700 as integrated option |
| XT30 protection | reverse PFET + **SMBJ26A** TVS + NTC | 24V input | CAN 4P header (CANH/L/12V/GND) sits next to XT30 |
| Charger | **MP2760** | 1-4S NVDC buck-boost, I2C, VIN 2.7-22V, ≤6A | Charges 3S from either 12V XT30 or 15V PD; I2C status; alt. BQ25756 (controller + ext. FETs, more power) |
| Battery protection | **S-8254A** + dual NMOS | 3S OV/UV/OC/SC | classic solution; alt. BQ76905 (with gauge) |
| Balancing | **HY2213-BB3A** ×3 | 4.2V shunt 66mA per cell | simple, reliable end-of-charge balancing |
| Battery monitor | 3-cell dividers → MCU ADC + **INA226** (VSYS high side) | — | SoC estimate + power telemetry |
| VLED buck | **TPS56637** | 4.5-28V in, 6A | 5.0V output near the LED connectors; upstream PMOS gated by LED_PWR_EN |
| 5V0 buck | **MP2315S** | 4.5-24V, 3A | logic / audio |
| USB anti-backfeed | **TPS2121** | dual-input power mux, seamless | 5V0 (priority) vs VUSB → 5V0_SYS |
| VDD LDO | **SGM2212-3.3** | 1A, low dropout | SD peak ~200mA + headroom |
| VAUDIO | ferrite + 220µF | filtered from 5V0_SYS | decoupling for 3W class-D bursts |

## 3. Power Budget

WS2812B at a typical measured 16mA/channel (240mW/LED full white at 5V); firmware limits VLED to 4.5A.

| Load | Qty | Typical (animation @25%) | Peak (full white) |
|---|---|---|---|
| Matrix 8×8 | 64 | 0.77A | 3.07A |
| Matrix 16×16 (expansion) | 256 | 3.07A | 12.3A → firmware-limited |
| Circle | 24 | 0.29A | 1.15A |
| Usage 20 + mic bar 64 | 84 | 1.0A | 4.0A |
| **VLED total (8×8 build)** | 172 | **~2.1A / 10.4W** | limited 4.5A / 22.5W |
| LCD (panel+backlight) | — | 1.2W | 1.5W |
| ESP32-S3 (Wi-Fi active) | — | 0.8W | 1.7W |
| Audio (3W PA bursts) | — | 0.2W | 3W |
| **System total** | — | **~14W** | ~29W |

- PD 15V×3A = 45W: covers system peak + ~1A charge; at 24V on XT30 charging can go to 2A.
- Runtime: 3S×3000mAh ≈ 33Wh, typical 14W → **~2.3h**; standby (LEDs off, LCD dim) <1.5W → >20h.
- 16×16 expansion: recommend XT30 power; the limiter caps full-white at 4.5A (~35% average brightness); a second TPS56637 footprint is reserved.

## 4. Protection & Details

1. **OV/UV**: S-8254A at 4.25V/2.7V per cell; MP2760 terminates at 12.6V, C/10 cutoff.
2. **Balancing**: HY2213 shunts 66mA when any cell reaches 4.2V; pairs with MP2760 end-of-charge taper.
3. **Anti-backfeed**: USB VBUS only enters TPS2121; VPD/VIN are one-way through ideal diodes; the VLED PMOS blocks reverse leakage from LED boards.
4. **Surge/ESD**: TVS at XT30; USBLC6-2SC6 on both USB-C ports; PESD1CAN on CAN.
5. **Thermals**: TPS56637 with copper pour + via array; MP2760 ~1W loss at 2A charge, same treatment.
6. **Power-up sequencing**: VDD before LED_PWR_EN/BL_EN (firmware-delayed via TCA9554) to avoid WS2812 flash at power-on.
7. **Sensing**: per-cell dividers 100k/10k + RC into ADC (taps before the protection board), static drain <15µA.

## 5. Connectors

| Interface | Connector | Pins |
|---|---|---|
| PD input | USB-C 16P (PD only) | CC to CH224K |
| Data USB | USB-C 16P | D+/D- → GPIO19/20, VBUS → TPS2121 |
| External power | XT30-M | 12-24V |
| CAN | XH2.54-4P | 12V / CANH / CANL / GND (120Ω jumper at the end) |
| Battery | XH2.54-4P (B-,B1,B2,B+) | full balance leads |
| LED outputs | SH1.0-3P ×3 | VLED / DIN / GND (matrix tiles cascade via 2.54 headers) |
| Expansion I2C | SH1.0-4P (Qwiic-compatible) | 3V3 / GND / SDA / SCL |
