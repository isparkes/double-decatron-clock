#ifndef DECATRON_H
#define DECATRON_H

#include <Arduino.h>
#include "defs.h"

// ----------------------------------------------------------------------------------------------------
// ---------------------------------------- Decatron tube ---------------------------------------------
// ----------------------------------------------------------------------------------------------------
//
// One dual-guide dekatron. The glow is moved by driving the two guide cathodes
// through a repeating 3 phase sequence:
//
//   phase 0: G1 low,  G2 low    <- glow sits on a main cathode (K0..K9)
//   phase 1: G1 high, G2 low    <- glow sits on the first guide cathode
//   phase 2: G1 low,  G2 high   <- glow sits on the second guide cathode
//
// so a full revolution is 10 cathodes x 3 phases = 30 positions.
//
// Positions come in two flavours:
//   physical - 0..29 measured from wherever the tube happened to be homed
//   logical  - 0..29 measured clockwise from the 12 o'clock mark on the dial
//
// Callers only ever deal in logical positions; the class applies the TDC offset
// found during homing and, if the tube is wired to rotate the other way, the
// direction reversal.

class Decatron
{
  public:
    Decatron(uint8_t guide1Pin, uint8_t guide2Pin, uint8_t indexPin,
             uint8_t tdcOffset, bool reversed, const char *name);

    // Configure the pins and drive the guides to a known state.
    void begin();

    // --- Homing -------------------------------------------------------------
    // Homing is a cooperative state machine rather than a blocking loop, so
    // that both tubes can be homed at the same time and the ESP8266 watchdog
    // keeps getting fed.
    void startHoming();
    void homingTick(unsigned long nowMillis);
    bool isHomed() const { return _homeState == HOME_DONE; }
    bool homingFailed() const { return _homingFailed; }

    // --- Normal running -----------------------------------------------------
    // Where on the dial the glow should be, 0..29 clockwise from 12 o'clock.
    void setLogicalTarget(uint8_t logicalPos);

    // Take at most one step towards the target. Returns true if it moved.
    bool tick(unsigned long nowMillis);

    // Is the glow where we asked for it to be?
    bool atTarget() const { return _currentPos == _targetPos; }

    // --- Accessors ----------------------------------------------------------
    uint8_t position() const { return _currentPos; }
    uint8_t target() const { return _targetPos; }
    uint8_t tdc() const { return _tdc; }
    uint8_t logicalPosition() const;
    const char *name() const { return _name; }
    bool indexAsserted() const;

    // Raw stepping, exposed for diagnostics.
    void stepForwards();
    void stepBackwards();

  private:
    enum HomeState { HOME_IDLE, HOME_SEEK_FORWARD, HOME_SEEK_BACKWARD, HOME_GOTO_TDC, HOME_DONE };

    void drivePhase();
    uint8_t logicalToPhysical(uint8_t logicalPos) const;

    const uint8_t _guide1Pin;
    const uint8_t _guide2Pin;
    const uint8_t _indexPin;
    const uint8_t _tdcOffset;
    const bool    _reversed;
    const char   *_name;

    int8_t  _digitStep = 0;    // which main cathode we are on, 0..9
    int8_t  _phaseStep = 0;    // which guide phase within that cathode, 0..2
    uint8_t _currentPos = 0;   // physical position, 0..29
    uint8_t _targetPos = 0;    // physical position we are stepping towards
    uint8_t _tdc = 0;          // physical position of 12 o'clock

    HomeState _homeState = HOME_IDLE;
    uint16_t  _homeSteps = 0;
    bool      _homingFailed = false;

    unsigned long _lastStepMillis = 0;
};

#endif
