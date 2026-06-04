# vfo-si5351-v63
Based on the VFO project by Jan Ciger (Janoc):  * https://janoc.rd-h.com/archives/649

# Si5351 VFO — HF Receiver with Selectable Frequency Conversion

An **Si5351** synthesizer controller for an HF receiver/transceiver, built on
Arduino (ATmega328). It drives a 16×2 LCD, a rotary encoder with push button,
and four function buttons. Configuration is stored in EEPROM and restored on
power-up.

The receiver supports three conversion architectures — **direct**, **single**
and **double** — selectable from the menu. The three Si5351 clock outputs act
as: `CLK0` 1st LO (tunable), `CLK1` 2nd LO (fixed, in double conversion) and
`CLK2` BFO.

> 🇵🇱 Polska wersja: [`README_PL.md`](README_PL.md)

---

## Table of Contents
- [Features](#features)
- [RF Architecture](#rf-architecture)
- [Frequency Plan](#frequency-plan)
- [Conversion Types](#conversion-types)
- [Sideband Inversion](#sideband-inversion)
- [Controls](#controls)
- [Menu](#menu)
- [IF1/IF2 Decade Editing](#if1if2-decade-editing)
- ["BFO follows IF2" Mode](#bfo-follows-if2-mode)
- [Factory Reset](#factory-reset)
- [Hardware](#hardware)
- [Project Structure](#project-structure)
- [EEPROM Map](#eeprom-map)
- [Building](#building)
- [Hardware Notes](#hardware-notes)
- [Authors and License](#authors-and-license)

---

## Features
- Receive range **0.1 – 30 MHz** (HF)
- Selectable **conversion architecture**: direct / single / double
- Three Si5351 clocks: `CLK0` 1st LO (tunable), `CLK1` 2nd LO (fixed), `CLK2` BFO
- **AM / LSB / USB / CW** modes, **RIT**, step and decade tuning
- **Editable IF1 (1–45 MHz) and IF2 (455 kHz–12 MHz)** from the menu
- **Auto-BFO** — BFO tracks IF2 (optional)
- Automatic sideband-inversion parity with a manual override
- EEPROM with **wear leveling** for the VFO frequency
- Built-in factory reset from firmware (hold button A at power-on)
- Flicker-free LCD rendering (diff renderer)

---

## RF Architecture
```
               CLK0 (1st LO)      CLK1 (2nd LO)       CLK2 (BFO)
               tunable            fixed               fixed
                   │                  │                   │
Antenna ─► [Mixer 1] ─► 1st IF ─► [Mixer 2] ─► 2nd IF ─► [Detector] ─► Audio
           dial±IF1            IF1±IF2        crystal    product
                                              filter
```
In double conversion `CLK1` (2nd LO) up-converts to a high 1st IF (≤45 MHz),
then down-converts to a crystal filter (2nd IF, 455 kHz–12 MHz). In single
conversion only one mixer is used; in direct conversion the audio is recovered
directly at a single mixer (zero IF).

---

## Frequency Plan
$f_d$ = dial, IF1 = 1st IF, IF2 = 2nd IF.

| Output | Function | Formula |
|---|---|---|
| CLK0 | 1st LO (tunable) | dial+IF1 (high) / \|dial−IF1\| (low) |
| CLK1 | 2nd LO (fixed) | IF1+IF2 (high) / \|IF1−IF2\| (low) |
| CLK2 | BFO | bfo_center ± offset |

With RX ≤ 30 MHz and IF1 ≤ 45 MHz the maximum output is 75 MHz (<100 MHz), so
all outputs safely share PLLA without re-tuning the PLL when CLK0 changes.

---

## Conversion Types
The **Conversion** menu item selects the receiver architecture; the choice is
stored in EEPROM (@18) and restored on boot:

- **Direct** — direct conversion (zero IF). `CLK0` tunes the dial directly,
  `CLK1` (2nd LO) and `CLK2` (BFO) are switched off, and audio comes straight
  out of the single mixer. IF1/IF2/BFO settings are ignored. Shown as `DC` on
  the VFO screen.
- **Single** — single conversion. One mixer: `CLK0` = dial ± IF2 (side set by
  **1st LO side**), `CLK1` off, `CLK2` = BFO near the crystal filter (IF2).
- **Double** — dual conversion (default). Two mixers: `CLK0` = dial ± IF1,
  `CLK1` = 2nd LO (IF1 ± IF2, side set by **LO2 high-side**), `CLK2` = BFO near
  IF2.

**1st LO side** selects the first-mixer injection side for single/double
conversion (`F+IF` = high-side, `F-IF`/`OFF` = low-side); it has no effect in
direct conversion. The VFO screen shows `+IF`, `-IF` or `DC` accordingly.

---

## Sideband Inversion
What matters is the **parity** of the high-side mixers:

| Mixer 1 | Mixer 2 | Net | Inverted |
|---|---|---|---|
| low | low | 0 | no |
| high | low | 1 | yes |
| low | high | 1 | yes |
| high | high | 2 | no |

The parity is computed automatically and XOR-ed with **Invert sidebands**. With
both mixers high-side there is no inversion.

---

## Controls
| Control | Function (VFO screen) |
|---|---|
| Encoder (rotate) | tune / change value |
| Encoder (OK) | change step / confirm |
| A | decade tuning |
| B | RIT (press again = reset) |
| C | enter/exit settings menu |
| TX | PTT (RIT off during TX) |

---

## Menu
| Item | Description | Range |
|---|---|---|
| VFO | main screen | 0.1–30 MHz |
| Op mode | operating mode | AM/LSB/USB/CW |
| Conversion | RX architecture | Direct / Single / Double |
| 1st LO side | 1st-mixer injection side | OFF / F-IF / F+IF |
| IF1 Freq. [Hz] | 1st IF | 1–45 MHz |
| IF2 Freq. [Hz] | 2nd IF (filter) | 455 kHz–12 MHz |
| BFO Freq. [Hz] | BFO center | 0.1–30 MHz |
| BFO follows IF2 | auto BFO tracking | yes/no |
| LO2 high-side | 2nd-LO injection side | yes/no |
| Invert sidebands | manual inversion | yes/no |
| Si5351 Cal [Hz] | calibration | −100k…+100k |

---

## IF1/IF2 Decade Editing
The IF1 and IF2 items use the `MenuNumber` **digit-by-digit** editor
(9-decade width, `Defs::IFEditWidth`):

1. **OK** — enter field editing
2. **Rotate encoder** — select the digit (decade) to edit
3. **OK** — edit the selected digit; rotation changes it by 1, 10, …, 10⁸ Hz
4. **OK** on the top digit — exit the field

This gives **1 Hz** resolution across the entire range of both IFs.

---

## "BFO follows IF2" Mode
When the **BFO follows IF2** checkbox is enabled:
- every IF2 change in the menu also sets `bfo_center` = IF2,
- on enabling, the BFO snaps to the current IF2.

This keeps the BFO at the crystal-filter center automatically — convenient when
changing the filter/IF2 without manual BFO adjustment. While active, manual
`BFO Freq.` edits are overwritten on the next IF2 change.

---

## Factory Reset
Holding **button A** while powering on (or resetting) the board wipes the whole
EEPROM (leaving the 4 bytes reserved for avrdude's flash programming counter)
and boots with the compiled-in defaults. The LCD shows
`EEPROM cleared / Release button A`. The fresh defaults are written back to
EEPROM on the next state change.

---

## Hardware
| Function | Pin |
|---|---|
| Encoder A/B | D2/D3 |
| Encoder button | A3 |
| Buttons A/B/C | A0/A1/A2 |
| TX (PTT) | D4 |
| LCD (RS,E,D4–D7) | D13,D12,D8,D9,D10,D11 |
| Si5351 I²C | A4/A5 |

Outputs: CLK0→1st mixer, CLK1→2nd mixer, CLK2→detector. Band-pass filters and
buffers between stages are recommended.

---

## Project Structure
```
├── README.md / README.en.md   documentation (PL / EN)
├── vfo-si5351-v63.ino          main program, frequency plan
├── definitions.h               config, limits, pins
├── vfo-screen.h                VFO screen + TVFOState
├── eeprom_storage.cpp/.h       EEPROM storage (VFO wear leveling)
├── ArduinoLCDApi.cpp/.h        LCD driver (diff renderer)
├── si5351.cpp/.h               Si5351 library (Etherkit)
└── lcdui/                      UI framework (CRTP, constexpr)
```

---

## EEPROM Map
`int_fast32_t` = 4 bytes on AVR. The VFO frequency lives in a wear-levelled
region after the fixed-layout header.

| Addr | Field | B |
|---|---|---|
| @0 | `bfo_center` | 4 |
| @4 | `if_freq` (IF2) | 4 |
| @8 | `si3531_correction` | 4 |
| @12 | steps: `vfo_step_idx`<<4 \| `rit_step_idx` | 1 |
| @13 | modes: opmode[1:0] ifmode[3:2] bfoFollow[4] swap[5] lo2_high[6] | 1 |
| @14 | `if1_freq` (IF1) | 4 |
| @18 | conversion type (0=Direct, 1=Single, 2=Double) | 1 |
| @19… | `vfo_freq` (wear-levelled region) | rest |

---

## Building
Libraries: `LiquidCrystal`, `Bounce2`, `Rotary`, `Wire`, `EEPROM`,
`si5351` (Etherkit). Board: ATmega328 (UNO / Nano / Pro Mini).

---

## Hardware Notes
- **Si5351 >100 MHz trap** — here all outputs stay <100 MHz, so no PLL split is
  needed. If extending to VHF, move CLK1/CLK2 to PLLB.
- **Harmonics/images** — the Si5351 outputs square waves; band-pass filters are
  required between stages.
- **Phase noise** of the tunable 1st LO in fractional mode — for the best
  dynamic range consider a lower IF1.
- **Absolute sideband** depends on the filter passband placement; set
  `Invert sidebands` once on a live signal.
- **Verify** each LO with a frequency counter.

---

## Authors and License
The VFO project is based on the work of **Jan Ciger (Janoc)**:
<https://janoc.rd-h.com/archives/649>

- **Author of the current version and modifications:** Jarosław Marek Niewiński
- **Implementation and documentation assistance:** Claude (Anthropic AI assistant)

The **Si5351** library (`si5351.h` / `si5351.cpp`) © 2015–2019 Jason Milldrum,
Dana H. Myers, used unmodified.

The whole project is distributed under the **GNU General Public License v3.0
(GPLv3)** — see the [`LICENSE`](LICENSE) file. Keep the author attributions in
accordance with the license.


