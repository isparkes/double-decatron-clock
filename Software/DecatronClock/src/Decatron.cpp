#include "Decatron.h"
#include "DebugManager.h"

Decatron::Decatron(uint8_t guide1Pin, uint8_t guide2Pin, uint8_t indexPin,
                   uint8_t tdcOffset, bool reversed, const char *name)
  : _guide1Pin(guide1Pin),
    _guide2Pin(guide2Pin),
    _indexPin(indexPin),
    _tdcOffset(tdcOffset),
    _reversed(reversed),
    _name(name)
{
}

// ************************************************************
// Set up the pins. The guides are left low, which parks the glow
// on whichever main cathode it happens to be nearest.
// ************************************************************
void Decatron::begin() {
  pinMode(_guide1Pin, OUTPUT);
  pinMode(_guide2Pin, OUTPUT);
  pinMode(_indexPin, INPUT);

  _digitStep = 0;
  _phaseStep = 0;
  _currentPos = 0;
  _targetPos = 0;
  drivePhase();
}

// ************************************************************
// The index opto pulls the input low when the glow is sitting on
// the index cathode.
// ************************************************************
bool Decatron::indexAsserted() const {
  return digitalRead(_indexPin) == LOW;
}

// ************************************************************
// Drive the guide cathodes for the current phase.
// ************************************************************
void Decatron::drivePhase() {
  switch (_phaseStep) {
    case 0:
      digitalWrite(_guide1Pin, LOW);
      digitalWrite(_guide2Pin, LOW);
      break;
    case 1:
      digitalWrite(_guide1Pin, HIGH);
      digitalWrite(_guide2Pin, LOW);
      break;
    case 2:
      digitalWrite(_guide1Pin, LOW);
      digitalWrite(_guide2Pin, HIGH);
      break;
  }
}

// ************************************************************
// Move the glow on one position.
// ************************************************************
void Decatron::stepForwards() {
  _phaseStep++;
  if (_phaseStep > 2) {
    _phaseStep = 0;
    _digitStep++;
    if (_digitStep > 9) {
      _digitStep = 0;
    }
  }

  _currentPos = (uint8_t)(_phaseStep + _digitStep * 3);
  drivePhase();
}

// ************************************************************
// Move the glow back one position.
// ************************************************************
void Decatron::stepBackwards() {
  _phaseStep--;
  if (_phaseStep < 0) {
    _phaseStep = 2;
    _digitStep--;
    if (_digitStep < 0) {
      _digitStep = 9;
    }
  }

  _currentPos = (uint8_t)(_phaseStep + _digitStep * 3);
  drivePhase();
}

// ************************************************************
// Begin the homing sequence.
//
// The glow can be anywhere when we start, and we have no way of
// reading its position other than the single index cathode. So we
// sweep forwards until the index is seen, then back until it is
// seen again. Approaching from a consistent direction removes the
// hysteresis in the opto, and leaves us sitting exactly on the
// index cathode - which, because the index is a main cathode, is
// always a phase 0 position.
// ************************************************************
void Decatron::startHoming() {
  debugManager.debugMsg(String(_name) + ": homing");
  _homeState = HOME_SEEK_FORWARD;
  _homeSteps = 0;
  _homingFailed = false;
  _lastStepMillis = millis();
}

// ************************************************************
// One step of the homing state machine. Call repeatedly until
// isHomed() returns true.
// ************************************************************
void Decatron::homingTick(unsigned long nowMillis) {
  if (_homeState == HOME_DONE || _homeState == HOME_IDLE) {
    return;
  }

  if ((nowMillis - _lastStepMillis) < DECATRON_HOMING_STEP_MS) {
    return;
  }
  _lastStepMillis = nowMillis;

  // Note that we sample the index *before* stepping, so what we read is the
  // settled result of the previous step.
  switch (_homeState) {
    case HOME_SEEK_FORWARD:
      if (indexAsserted()) {
        _homeState = HOME_SEEK_BACKWARD;
        _homeSteps = 0;
        break;
      }
      stepForwards();
      if (++_homeSteps > DECATRON_HOMING_MAX_STEPS) {
        debugManager.debugMsg(String(_name) + ": WARNING index not found going forwards");
        _homingFailed = true;
        _homeState = HOME_SEEK_BACKWARD;
        _homeSteps = 0;
      }
      break;

    case HOME_SEEK_BACKWARD:
      if (indexAsserted()) {
        // We are on the index cathode. Call this physical position zero.
        _digitStep = 0;
        _phaseStep = 0;
        _currentPos = 0;
        _homeState = HOME_GOTO_TDC;
        _homeSteps = 0;
        break;
      }
      stepBackwards();
      if (++_homeSteps > DECATRON_HOMING_MAX_STEPS) {
        debugManager.debugMsg(String(_name) + ": WARNING index not found going backwards");
        _homingFailed = true;
        _digitStep = 0;
        _phaseStep = 0;
        _currentPos = 0;
        _homeState = HOME_GOTO_TDC;
        _homeSteps = 0;
      }
      break;

    case HOME_GOTO_TDC:
      if (_homeSteps >= _tdcOffset) {
        _tdc = _currentPos;
        _targetPos = _currentPos;
        _homeState = HOME_DONE;
        debugManager.debugMsg(String(_name) + ": homed, tdc=" + String(_tdc) +
                              (_homingFailed ? " (position NOT trustworthy)" : ""));
        break;
      }
      stepForwards();
      _homeSteps++;
      break;

    default:
      break;
  }
}

// ************************************************************
// Convert a position on the dial (0 = 12 o'clock, increasing
// clockwise) into a physical tube position.
// ************************************************************
uint8_t Decatron::logicalToPhysical(uint8_t logicalPos) const {
  uint8_t offset = (uint8_t)(logicalPos % DECATRON_POSITIONS);
  if (_reversed) {
    offset = (uint8_t)((DECATRON_POSITIONS - offset) % DECATRON_POSITIONS);
  }
  return (uint8_t)((_tdc + offset) % DECATRON_POSITIONS);
}

// ************************************************************
// Where the glow is, as a dial position.
// ************************************************************
uint8_t Decatron::logicalPosition() const {
  uint8_t offset = (uint8_t)((_currentPos + DECATRON_POSITIONS - _tdc) % DECATRON_POSITIONS);
  if (_reversed) {
    offset = (uint8_t)((DECATRON_POSITIONS - offset) % DECATRON_POSITIONS);
  }
  return offset;
}

// ************************************************************
// Ask for the glow to be moved to a dial position.
// ************************************************************
void Decatron::setLogicalTarget(uint8_t logicalPos) {
  _targetPos = logicalToPhysical(logicalPos);
}

// ************************************************************
// Take one step towards the target, by the shortest way round.
// Returns true if we moved.
// ************************************************************
bool Decatron::tick(unsigned long nowMillis) {
  if (_homeState != HOME_DONE) {
    return false;
  }

  if (_currentPos == _targetPos) {
    return false;
  }

  if ((nowMillis - _lastStepMillis) < DECATRON_STEP_INTERVAL_MS) {
    return false;
  }
  _lastStepMillis = nowMillis;

  uint8_t forwardDistance =
      (uint8_t)((_targetPos + DECATRON_POSITIONS - _currentPos) % DECATRON_POSITIONS);

  if (forwardDistance <= (DECATRON_POSITIONS / 2)) {
    stepForwards();
  } else {
    stepBackwards();
  }

  return true;
}
