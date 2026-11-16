#ifndef DEFS_H
#define DEFS_H

#include <Arduino.h>

//**********************************************************************************
//* Hardware definitions for the "two-decatron-clock" board.                       *
//*                                                                                *
//* Board: LOLIN(Wemos) D1 Mini (ESP8266), 160 MHz                                 *
//**********************************************************************************

// ************************** Pin Allocations *************************
//
// These are carried over verbatim from the EniacDecatron firmware, which is
// known to work on this board. Note that they do NOT read across 1:1 to the
// schematic net names: the tubes are swapped (firmware "Decatron 1" is the
// board's J4 / Decatron2 connector) and G1/G2 are swapped within each tube,
// because the main board and the A101 carrier label pins 4/5 the opposite way
// round. Do not "tidy" these up without a tube on the bench.
//
//   firmware signal   D1 Mini pin   board net
//   ---------------   -----------   ---------
//   Guide2_1          D6            G1_2
//   Guide1_1          D5            G2_2
//   Index1            D7            IDX2
//   Guide2_2          D4            G1_1
//   Guide1_2          D3            G2_1
//   Index2            D0            IDX1
//   HVEnable          D8            HV_EN

#define  Guide2_1   D6       // Tube 1, guide A
#define  Guide1_1   D5       // Tube 1, guide B
#define  Index1     D7       // Tube 1 index input, LOW when the glow is on the index cathode

#define  Guide2_2   D4       // Tube 2, guide A
#define  Guide1_2   D3       // Tube 2, guide B
#define  Index2     D0       // Tube 2 index input, LOW when the glow is on the index cathode

#define  HVEnable   D8       // High voltage generator enable, HIGH = on

// I2C for the on-board DS1307. D2 = GPIO4 = SDA, D1 = GPIO5 = SCL.
#define  I2C_SDA    4
#define  I2C_SCL    5

// ************************** Tube Geometry ***************************

// A dual-guide dekatron has 10 main cathodes and 2 guide cathodes between each
// pair, so one revolution is 30 discrete glow positions.
#define DECATRON_POSITIONS    30

// The index cathode is not at the "12 o'clock" position on the dial, it is
// offset. After homing we step this many positions forward from the index mark
// to reach Top Dead Centre (TDC = 12 o'clock). Calibrated on this hardware.
#define DECATRON_1_TDC_OFFSET 27
#define DECATRON_2_TDC_OFFSET 27

// ************************** Movement Timing *************************

// How long to wait between steps when the glow is chasing a new target. Lower
// is faster; below ~2 ms the glow does not reliably transfer.
#define DECATRON_STEP_INTERVAL_MS   3

// Step interval used while homing. Slower, so the index opto has time to settle
// before we sample it.
#define DECATRON_HOMING_STEP_MS     10

// Give up on a homing sweep after this many steps (2 revolutions + margin).
#define DECATRON_HOMING_MAX_STEPS   70

// Overall guard on the homing routine, in case a tube is missing or unlit.
#define HOMING_TIMEOUT_MS           10000

// How long to wait after switching the HV on before the glow has struck and the
// index opto readings can be trusted.
#define HV_STRIKE_DELAY_MS          1500

// -------------------------------------------------------------------------------
#define SOFTWARE_VERSION      "0.1.0"

#endif
