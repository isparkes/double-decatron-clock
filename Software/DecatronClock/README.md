# DecatronClock

Standalone clock firmware for the two-decatron-clock board: **hours on one
dekatron, minutes on the other**, each read as a clock dial with the glow as the
hand.

It is derived from `EniacDecatron` in the `eniac-firmware` repository — the tube
mechanics, pin allocations and homing sequence are carried over — but the UART
link to the ENIAC master is replaced by a local time source, and the display
shows hours and minutes instead of a seconds spinner.

This is a working skeleton, not a finished product. See
[What is not here](#what-is-not-here).

## Building

```bash
pio run              # build
pio run -t upload    # build and flash
pio device monitor   # serial console, 115200 baud
```

Target is `d1_mini` (ESP8266) at 160 MHz. There are no external library
dependencies — only `Wire`, `ESP8266WiFi` and the core `time.h`.

## Configuring

Everything is compile-time, in [`include/config.h`](include/config.h). Nothing is
persisted to flash and there is no web UI.

| Setting | Purpose |
|---|---|
| `WIFI_SSID` / `WIFI_PASSWORD` | Leave `WIFI_SSID` empty to skip Wi-Fi and NTP entirely and run from the RTC. |
| `TIMEZONE` | POSIX TZ string, handles DST. Defaults to Europe/Berlin. |
| `HOURS_TUBE` / `MINUTES_TUBE` | Which physical tube does which job — swap if the tubes end up the wrong way round in the case. |
| `HOURS_TUBE_REVERSED` / `MINUTES_TUBE_REVERSED` | Set `true` for a tube whose glow runs anti-clockwise. **Expect to need this** (see below). |
| `HOURS_HAND_CREEPS` | `true` for a creeping hour hand, `false` to jump on the hour. |
| `BLANK_FROM_HOUR` / `BLANK_TO_HOUR` | Night blanking window. Set both the same to disable. |
| `DEBUG_ENABLED` | Once-per-second serial logging. |

Tube geometry and timing live in [`include/defs.h`](include/defs.h) — pins, the
TDC offsets found by calibration, and the step intervals.

### Rotation direction

Which way "forwards" turns the glow depends on the guide wiring, and on this
board the G1/G2 pair is crossed between the main board (`J1`/`J4` pins 4/5 are
`G2`/`G1`) and the A101 carrier (pins 4/5 are `G1`/`G2`). So the first thing to
check on the bench is whether the hands run clockwise. If not, set the
`*_TUBE_REVERSED` flags rather than rewiring — the direction is applied in
software, in `Decatron::logicalToPhysical()`.

The pin defines in `defs.h` are copied verbatim from `EniacDecatron`, which is
known to work on this hardware. They deliberately do **not** read across 1:1 to
the schematic net names — firmware "Decatron 1" is the board's `J4`/Decatron2
connector, and G1/G2 are swapped within each tube. Do not tidy them up without a
tube on the bench.

## How it works

### Time

`TimeKeeper` runs a free-running time of day off `millis()` and disciplines it
from whichever source is available, in priority order:

1. **NTP** — if a Wi-Fi SSID is configured. Re-syncs hourly, and writes the
   result back to the RTC so the clock is right after a power cut.
2. **RTC** — the battery-backed DS1307 at `U5`, read at boot and re-read every
   10 minutes while NTP is unavailable.
3. **Manual** — set over the serial console with `T hh:mm:ss`.

If none of those has produced a time, both hands park at 12 o'clock.

### Display

A dual-guide dekatron has 10 main cathodes with 2 guide cathodes between each
pair, so one revolution is **30 positions**. `Decatron` tracks a *physical*
position (0–29, measured from wherever the tube homed) and exposes a *logical*
one (0–29, clockwise from the 12 o'clock mark), applying the TDC offset and the
direction flag in between.

| Hand | Mapping | Resolution |
|---|---|---|
| Hours | 12-hour dial, 30 positions over 720 minutes | one position every 24 minutes |
| Minutes | 60 minutes over 30 positions | one position every 2 minutes |

Two minutes per position is the limit of the tube, not of the firmware.

### Homing

The glow can be anywhere at power-on and the only feedback is the single index
cathode (K1 on the A101 carrier, read through an EL817 optocoupler, active low).
So at startup, and whenever the HV comes back after blanking:

1. Step forwards until the index is seen.
2. Step backwards until it is seen again — approaching from a consistent
   direction removes the hysteresis in the opto, and because the index is a main
   cathode this always lands on a phase-0 position.
3. Call that position zero, then step forward `DECATRON_n_TDC_OFFSET` positions
   to reach 12 o'clock.

Homing is a cooperative state machine (`startHoming()` / `homingTick()`) rather
than a blocking loop, so both tubes home together and the watchdog keeps getting
fed. If the index is never seen it gives up after two revolutions, logs a
warning, and carries on with a position it cannot vouch for — `S` on the console
reports `HOMING FAILED` in that case.

### Serial console

115200 baud. **Note that the RX pin is also the front panel `BTN1` line on this
board**, so a button wired to `J2` will inject characters into the parser.

```
T hh:mm:ss   set the time
S            status (time source, both tube positions, HV state)
H            re-home both tubes
B            toggle forced blanking
?            help
```

## Source layout

| File | Contents |
|---|---|
| `src/main.cpp` | Setup, main loop, hand mapping, blanking, serial console |
| `include/defs.h` | Pin allocations, tube geometry, timing constants |
| `include/config.h` | User configuration |
| `src/Decatron.cpp` | One tube: guide sequencing, homing, shortest-path stepping |
| `src/TimeKeeper.cpp` | Time of day, NTP, RTC failover |
| `src/Ds1307.cpp` | Minimal DS1307 driver |
| `src/DebugManager.cpp` | Serial debug gate |

## What is not here

Deliberately out of scope for this version:

- No web UI, no REST API, no OTA.
- No configuration persistence — `config.h` is compile-time only, and there is
  no WiFiManager captive portal, so Wi-Fi credentials are baked into the build.
- No front panel support: the PIR input (`A0`) and the button (`RX`) are unread.
  The button in particular cannot be used while the serial console is active.
- No LDR dimming, and no PWM brightness control of the HV supply.
- No seconds display, no display modes, no transitions or animations.
- No date handling beyond keeping the RTC's calendar registers moving.

## Safety

The board this runs on carries **~400 V DC**, and the reservoir capacitors stay
charged after power-off. See the repository README.
