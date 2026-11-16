# Two Decatron Clock

KiCad hardware for a two-tube Decatron (dekatron) clock driven by a Wemos D1 Mini
(ESP8266), plus the tube carrier board and an early, unfinished firmware sketch.

The board generates its own ~400 V anode supply and ~110 V guide rail, drives two
dual-guide A101 dekatrons, and reads each tube's index mark back through an
optocoupler.

> ⚠️ **This board carries ~400 V DC.** The HV multiplier and the tube connectors are
> live whenever `HV_EN` is asserted, and the reservoir capacitors stay charged after
> power-off. Treat the whole top side as hot.

## Status

The hardware is finished and was fabricated (`adfed9c Fab`, Oct 2021), with small
fixes through to Dec 2023.

**The standalone firmware was never finished.** The sketch under `Software/` stops
at version 0.1: it brings up Wi-Fi, NTP, the DS1307 and the full web config UI, and
it can step each tube, but there is no timer ISR and nothing ever renders the
prepared `displayHours` / `displayMins` / `displaySecs` values onto the tubes. The
only stepping that happens is the manual, button-driven index hunt in `setup()`.
Its pin map in `Software/ClockDefs.h` also predates commit `835bd50` ("Rearrange
outputs so that IDX work correctly") and **does not match the board as fabricated** —
see the pin table below for the as-built mapping.

Instead of being finished as a standalone clock, the board was repurposed as the
**dual-Decatron seconds display add-on for the ENIAC nixie clock**. The firmware it
actually runs lives in the separate `eniac-firmware` repository, as
`EniacDecatron` — an ESP8266 slave that receives a 5-byte framed packet
(`0xAA`, hours, minutes, seconds, control) once per second over UART from the ENIAC
ESP32 master, homes both tubes against their index marks, and drives tube 1 as a
rotating seconds hand. `DecatronTest` in the same repository emulates the master for
bench testing.

Because the add-on takes its time over the serial link, the on-board DS1307, the
Wi-Fi stack and the web UI are all unused in that role. Note that the front-panel
connector's `BTN1` pin lands on the Wemos `RX` pin — in ENIAC use that pin carries
serial data from the master instead of a button.

`Software/DecatronClock/` is a newer attempt at the standalone case: a PlatformIO
project built on the EniacDecatron tube handling, with the UART link to the ENIAC
master replaced by a local time source (NTP → DS1307 → serial), showing hours on one
tube and minutes on the other. It is a working skeleton rather than a finished
clock — see [`Software/DecatronClock/README.md`](Software/DecatronClock/README.md).

## Repository layout

| Path | Description |
|---|---|
| `Hardware/` | Main board: Wemos D1 Mini carrier, HV supply, guide drivers, index sensing, RTC. KiCad project `WemosDecatron`. |
| `Hardware/A101base/` | Dual A101 tube carrier board. KiCad project `A101base`. |
| `Hardware/WemosDecatronExternal.sch` | Off-board front-panel sheet (button + PIR). Legacy KiCad 4 schematic, documentation only — no PCB, and it still shows an 8-way front connector where the main board now has a 5-way one. Rendered as `WemosDecatronExternal.svg`. |
| `Hardware/Recovered.pretty/` | Recovered footprints. |
| `Software/DecatronClock/` | Standalone clock firmware: hours on one tube, minutes on the other. PlatformIO, ESP8266. |
| `Software/*.ino`, `Software/*.cpp` | Abandoned v0.1 standalone firmware (Arduino IDE, ESP8266). Reference only. |

Gerbers and drill files for `A101base` are generated into `A101base/CAM/`, which is
git-ignored along with the KiCad `*-backups` directories.

## Hardware

### Main board — `Hardware/WemosDecatron`

65 × 75 mm, two layers, through-hole throughout, four M3 mounting holes.
Title block: *"Wemos D1 Two Decatron clock"*, Rev 002, Glowtime Ltd.

**Power in.** DC enters on `J3`, through reverse-polarity diode `D1` (UF4007) to
`VIN`, then an L7805 (`U3`) produces the 5 V `VCC` that feeds the Wemos.

**High voltage.** An MC34063A (`U4`) switches an IRF640 (`Q2`) through a 100 µH
inductor (`L1`), with `R1` (0R18 2 W) as the current sense. `D3` rectifies the
switch node into the ~150 V rail (`C13` reservoir). A three-stage
Cockcroft–Walton multiplier (`C1`–`C4`, `D4`–`D7`) hanging off the same switch node
produces the ~400 V anode rail. Feedback is set by `R4`/`R5`/`RV1`; `RV1` (1 k) trims
the output.

**HV enable.** `Q5` (2N7000), driven from `HV_EN`, switches the feedback divider so
the firmware can turn the generator off for blanking. Solder jumper `JP1` ties
`HV_EN` to `VCC` for bench bring-up without a microcontroller.

**Tube drive.** Each tube's anode is fed from the 400 V rail through a 470 k series
resistor (`R16` for tube 1, `R2` for tube 2). `R12` (33 k) drops the 150 V rail to
the ~110 V guide rail. Each guide sits at 110 V through a 470 k pull-up
(`R9`/`R11` for tube 1, `R18`/`R20` for tube 2) and is pulled to ground by an MPSA42
(`Q3`/`Q4` and `Q6`/`Q7`). The common cathode return `RTN` is decoupled by `C5` and
`C15` (added in `021e908`, "Add RTN smoothing caps").

**Index sensing.** The index cathode of each tube drives the LED of an EL817
optocoupler (`U1`, `U6`) against `RTN`; the transistor side pulls `IDX1`/`IDX2` down
against a 3V3 pull-up (`R15`, `R21`). The index signal is therefore **active low** and
fully isolated from the HV side.

**Timekeeping.** A DS1307 (`U5`) with crystal `Y1` and CR1220 backup cell `BT1`,
on the Wemos I²C pins, also brought out on the 4-way header `J5` (added in
`6aa0615`).

**Test pads.** `W1` 5V, `W2`/`W3` GND, `W4` the 150 V rail (silk reads 170 V),
`W5` 110 V, `W6` `RTN` (silk reads 55 V), `W7` 400 V, `W8` `VIN`.

### Tube carrier — `Hardware/A101base`

71 × 39 mm, two layers. Two A101 dekatron footprints (`D1`, `D2`), each with its own
5-way header, plus two M3 mounting holes. Title block: *"A101 tube mount"*, Rev 001.

Per tube it commons K0 and K2–K9 to the return pin and brings out the anode, the
**K1 index cathode**, and the two guides. Note that the index is taken from K1, not
K0.

## Connectors

### Main board

| Ref | Pins | Function | Pinout |
|---|---|---|---|
| `J1` | 5 | Decatron 1 | 1 `400V_1` · 2 `IDX_IN1` · 3 `RTN` · 4 `G2_1` · 5 `G1_1` |
| `J4` | 5 | Decatron 2 | 1 `400V_2` · 2 `IDX_IN2` · 3 `RTN` · 4 `G2_2` · 5 `G1_2` |
| `J2` | 5 | Front panel | 1 `GND` · 2 `BTN1` · 3 `PIR` · 4 `VCC` (5 V) · 5 `3V3` |
| `J3` | 2 | DC input | 1 `GND` · 2 `V+` |
| `J5` | 4 | I²C | 1 `SDA` · 2 `SCL` · 3 `3V3` · 4 `GND` |

### A101 carrier (`J1`, `J2`)

| Pin | Signal |
|---|---|
| 1 | Anode |
| 2 | K1 (index) |
| 3 | Common return (K0, K2–K9) |
| 4 | G1 |
| 5 | G2 |

**Guide ordering:** with a straight 1:1 cable, main-board pin 4 (`G2_n`) lands on the
carrier's `G1` and pin 5 (`G1_n`) on `G2`, so the guide pair is swapped and the glow
rotates the opposite way round. The firmware compensates in its phase sequence —
`EniacDecatron` steps the glow *forward* by *decrementing* the phase. Keep this in
mind before "fixing" either end.

## ESP8266 pin map (as fabricated)

Taken from the PCB netlist, not from `Software/ClockDefs.h`.

`EniacDecatron` and `Software/DecatronClock` use the same set of pins, but their
`defs.h` deliberately maps them differently: firmware "Decatron 1" is the board's
`J4`/Decatron2 connector, and G1/G2 are swapped within each tube, to compensate for
the pin 4/5 ordering described above. That mapping is empirical and known to work —
do not reconcile it with this table without a tube on the bench. (The
`EniacDecatron/SPECIFICATION.md` pin table is itself out of date with respect to its
own `defs.h`.)

| Signal | D1 Mini pin | Direction | Notes |
|---|---|---|---|
| `G1_1` | D4 | out | Tube 1 guide 1 |
| `G2_1` | D3 | out | Tube 1 guide 2 |
| `IDX1` | D0 | in | Tube 1 index, active low (opto) |
| `G1_2` | D6 | out | Tube 2 guide 1 |
| `G2_2` | D5 | out | Tube 2 guide 2 |
| `IDX2` | D7 | in | Tube 2 index, active low (opto) |
| `HV_EN` | D8 | out | HV generator enable, active high |
| `SDA` | D2 | — | DS1307 / `J5` |
| `SCL` | D1 | — | DS1307 / `J5` |
| `PIR` | A0 | in | Front panel `J2` |
| `BTN1` | RX | in | Front panel `J2`; also the UART RX used by `EniacDecatron` |

## Working with the files

The schematics are KiCad 6 format (`version 20211123`); `WemosDecatron.kicad_pcb` has
since been re-saved by a newer KiCad and will want to migrate the schematic on open.
The legacy KiCad 5 files (`.sch`, `.net`, `.lib`, `.cmp`, `.mod`) are kept alongside
for reference and are not maintained.

Footprints come from a global `NixieClockMaster` library that is **not** part of this
repository; the local `fp-lib-table` is empty, so that library must be configured
globally in KiCad for the boards to open with footprints resolved.
`Hardware/WemosDecatron.csv` is a BOM export, generated before the latest revisions —
check it against the schematic before ordering.

### Firmware

For a standalone clock, build `Software/DecatronClock` with PlatformIO
(`pio run -t upload`, env `d1_mini`). No external library dependencies.

For the add-on role, build `EniacDecatron` from the `eniac-firmware` repository, also
with PlatformIO (env `d1_mini`, 160 MHz, 115200 baud).

The loose sketch at the top of `Software/` is an Arduino IDE project (board:
*LOLIN(WeMos) D1 R2 & mini*, 4 MB flash / 2 MB FS). It depends on Time (Margolis
1.5.0) and WiFiManager (tzapu 0.14.0). It is kept for reference only — it is
incomplete and its pin map is out of date.
