#include <Arduino.h>

//**********************************************************************************
//* Standalone two Decatron clock.                                                 *
//*                                                                                *
//* Hours on one dekatron, minutes on the other, both read as a clock dial with    *
//* the glow as the hand. Derived from the EniacDecatron slave firmware, with the  *
//* serial link to the ENIAC master replaced by a local time source.               *
//*                                                                                *
//* Board: LOLIN(Wemos) D1 Mini (ESP8266)                                          *
//* CPU-Frequency: 160MHz                                                          *
//* Flash Size: 4MB                                                                *
//**********************************************************************************

#include "config.h"
#include "defs.h"
#include "DebugManager.h"
#include "Decatron.h"
#include "TimeKeeper.h"

static_assert(HOURS_TUBE != MINUTES_TUBE, "HOURS_TUBE and MINUTES_TUBE must be different tubes");

// Work out which physical tube gets which job, and therefore which direction
// flag and which name each one takes.
#if HOURS_TUBE == 1
  #define TUBE_1_NAME      "tube1/hours"
  #define TUBE_2_NAME      "tube2/minutes"
  #define TUBE_1_REVERSED  HOURS_TUBE_REVERSED
  #define TUBE_2_REVERSED  MINUTES_TUBE_REVERSED
#else
  #define TUBE_1_NAME      "tube1/minutes"
  #define TUBE_2_NAME      "tube2/hours"
  #define TUBE_1_REVERSED  MINUTES_TUBE_REVERSED
  #define TUBE_2_REVERSED  HOURS_TUBE_REVERSED
#endif

// ------------------------------ Components ------------------------------

Decatron tube1(Guide1_1, Guide2_1, Index1, DECATRON_1_TDC_OFFSET, TUBE_1_REVERSED, TUBE_1_NAME);
Decatron tube2(Guide1_2, Guide2_2, Index2, DECATRON_2_TDC_OFFSET, TUBE_2_REVERSED, TUBE_2_NAME);

#if HOURS_TUBE == 1
Decatron &hoursTube   = tube1;
Decatron &minutesTube = tube2;
#else
Decatron &hoursTube   = tube2;
Decatron &minutesTube = tube1;
#endif

TimeKeeper timeKeeper;

// ------------------------------ State -----------------------------------

bool hvOn = false;
bool blanked = false;
bool blankOverride = false;     // forced on/off from the serial console
bool blankOverrideValue = false;

String serialLine;

// ************************************************************
// High voltage generator
// ************************************************************
void setHv(bool on) {
  if (on == hvOn) {
    return;
  }
  hvOn = on;
  digitalWrite(HVEnable, on ? HIGH : LOW);
  debugManager.debugMsg(on ? "HV ON" : "HV OFF");
}

// ************************************************************
// A delay that keeps the watchdog happy.
// ************************************************************
void patientDelay(unsigned long ms) {
  unsigned long start = millis();
  while ((millis() - start) < ms) {
    delay(1);
  }
}

// ************************************************************
// Home both tubes at the same time. Blocking, but only ever
// called at startup and when coming out of blanking.
// ************************************************************
void homeTubes() {
  tube1.startHoming();
  tube2.startHoming();

  unsigned long start = millis();

  while (!(tube1.isHomed() && tube2.isHomed())) {
    unsigned long now = millis();
    tube1.homingTick(now);
    tube2.homingTick(now);

    if ((now - start) > HOMING_TIMEOUT_MS) {
      debugManager.debugMsg("WARNING: homing timed out, display position is a guess");
      break;
    }

    delay(1);
  }
}

// ************************************************************
// Bring the display up: HV on, let the glow strike, then home.
// ************************************************************
void startDisplay() {
  setHv(true);
  patientDelay(HV_STRIKE_DELAY_MS);
  homeTubes();
}

// ************************************************************
// Work out where the hour hand belongs, 0..29 clockwise from 12.
//
// 30 positions over a 12 hour dial is 2.5 positions per hour, so
// when the hand is allowed to creep it moves one position every
// 24 minutes.
// ************************************************************
uint8_t hoursToPosition(uint8_t hours, uint8_t minutes) {
  uint16_t hour12 = hours % 12;

#if HOURS_HAND_CREEPS
  uint16_t minutesIntoDial = (uint16_t)(hour12 * 60 + minutes);   // 0..719
  return (uint8_t)(minutesIntoDial / 24);                         // 0..29
#else
  (void)minutes;
  return (uint8_t)((hour12 * 5) / 2);                             // 0, 2, 5, 7, ...
#endif
}

// ************************************************************
// Work out where the minute hand belongs, 0..29 clockwise from
// 12. With 30 positions this advances every 2 minutes.
// ************************************************************
uint8_t minutesToPosition(uint8_t minutes) {
  return (uint8_t)(minutes / 2);
}

// ************************************************************
// Should the display be off at this hour?
// ************************************************************
bool blankedAtHour(uint8_t hour) {
#if BLANK_FROM_HOUR == BLANK_TO_HOUR
  (void)hour;
  return false;
#else
  if (BLANK_FROM_HOUR < BLANK_TO_HOUR) {
    return (hour >= BLANK_FROM_HOUR) && (hour < BLANK_TO_HOUR);
  }
  // The blanking window wraps over midnight.
  return (hour >= BLANK_FROM_HOUR) || (hour < BLANK_TO_HOUR);
#endif
}

// ************************************************************
// Point the hands at the current time.
// ************************************************************
void updateHands() {
  if (!timeKeeper.isSet()) {
    // Nothing to show yet - park both hands at 12 so it is obvious.
    hoursTube.setLogicalTarget(0);
    minutesTube.setLogicalTarget(0);
    return;
  }

  hoursTube.setLogicalTarget(hoursToPosition(timeKeeper.hours(), timeKeeper.minutes()));
  minutesTube.setLogicalTarget(minutesToPosition(timeKeeper.minutes()));
}

// ************************************************************
// Serial console. Note that the RX pin is also the front panel
// BTN1 line on this board.
//
//   T hh:mm:ss  set the time
//   S           status
//   H           re-home both tubes
//   B           toggle forced blanking
//   ?           help
// ************************************************************
void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  T hh:mm:ss   set the time"));
  Serial.println(F("  S            show status"));
  Serial.println(F("  H            re-home both tubes"));
  Serial.println(F("  B            toggle forced blanking"));
  Serial.println(F("  ?            this help"));
}

void printStatus() {
  Serial.println("Decatron clock " + String(SOFTWARE_VERSION));
  Serial.println("  time    : " + timeKeeper.status());
  Serial.println("  hours   : " + String(hoursTube.name()) +
                 " pos=" + String(hoursTube.logicalPosition()) +
                 " phys=" + String(hoursTube.position()) +
                 " tdc=" + String(hoursTube.tdc()) +
                 (hoursTube.homingFailed() ? " HOMING FAILED" : ""));
  Serial.println("  minutes : " + String(minutesTube.name()) +
                 " pos=" + String(minutesTube.logicalPosition()) +
                 " phys=" + String(minutesTube.position()) +
                 " tdc=" + String(minutesTube.tdc()) +
                 (minutesTube.homingFailed() ? " HOMING FAILED" : ""));
  Serial.println(String("  hv      : ") + (hvOn ? "on" : "off") +
                 (blanked ? " (blanked)" : "") +
                 (blankOverride ? " (forced)" : ""));
}

void handleCommand(const String &line) {
  String cmd = line;
  cmd.trim();
  if (cmd.length() == 0) {
    return;
  }

  char verb = toupper(cmd.charAt(0));

  switch (verb) {
    case 'T': {
      int h = 0, m = 0, s = 0;
      if (sscanf(cmd.c_str() + 1, " %d:%d:%d", &h, &m, &s) == 3 &&
          h >= 0 && h <= 23 && m >= 0 && m <= 59 && s >= 0 && s <= 59) {
        timeKeeper.setTime((uint8_t)h, (uint8_t)m, (uint8_t)s);
        Serial.println("OK");
      } else {
        Serial.println(F("Bad time, expected T hh:mm:ss"));
      }
      break;
    }

    case 'S':
      printStatus();
      break;

    case 'H':
      Serial.println(F("Re-homing"));
      homeTubes();
      updateHands();
      break;

    case 'B':
      blankOverride = !blankOverride;
      blankOverrideValue = blankOverride;
      Serial.println(blankOverride ? F("Blanking forced on") : F("Blanking released"));
      break;

    case '?':
      printHelp();
      break;

    default:
      Serial.println(F("Unknown command, try ?"));
      break;
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      handleCommand(serialLine);
      serialLine = "";
      continue;
    }

    if (serialLine.length() < 32) {
      serialLine += c;
    }
  }
}

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------------  Set up  --------------------------------------------
// ----------------------------------------------------------------------------------------------------
void setup() {
  // HV off before anything else.
  pinMode(HVEnable, OUTPUT);
  digitalWrite(HVEnable, LOW);
  hvOn = false;

  Serial.begin(115200);
  debugManager.setUp(DEBUG_ENABLED);

  Serial.println();
  Serial.println("--------------------------------------------------");
  Serial.println("Two Decatron Clock " + String(SOFTWARE_VERSION));
  Serial.println("--------------------------------------------------");

  tube1.begin();
  tube2.begin();

  timeKeeper.begin();

  startDisplay();
  updateHands();

  printStatus();
  debugManager.debugMsg("Startup done");
}

// ----------------------------------------------------------------------------------------------------
// -----------------------------------------------  Loop  ---------------------------------------------
// ----------------------------------------------------------------------------------------------------
void loop() {
  unsigned long nowMillis = millis();

  handleSerial();
  timeKeeper.tick(nowMillis);

  // ------------------- Blanking ----------------------------------------

  bool wantBlanked = blankOverride
                       ? blankOverrideValue
                       : (timeKeeper.isSet() && blankedAtHour(timeKeeper.hours()));

  if (wantBlanked != blanked) {
    blanked = wantBlanked;
    debugManager.debugMsg(blanked ? "Blanked" : "Unblanked");

    if (blanked) {
      setHv(false);
    } else {
      // The glow position is lost while the HV is off, so start again.
      startDisplay();
      updateHands();
      nowMillis = millis();
    }
  }

  // ------------------- Move the hands ----------------------------------

  if (!blanked) {
    if (timeKeeper.secondRolledOver()) {
      updateHands();
    }

    hoursTube.tick(nowMillis);
    minutesTube.tick(nowMillis);
  }

  // ------------------- Once per second logging -------------------------

  if (timeKeeper.secondRolledOver() && debugManager.getDebug()) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
             timeKeeper.hours(), timeKeeper.minutes(), timeKeeper.seconds());

    debugManager.debugMsg(String(buf) +
                          " src=" + timeKeeper.sourceName() +
                          " hours=" + String(hoursTube.logicalPosition()) +
                          "->" + String(hoursToPosition(timeKeeper.hours(), timeKeeper.minutes())) +
                          " mins=" + String(minutesTube.logicalPosition()) +
                          "->" + String(minutesToPosition(timeKeeper.minutes())) +
                          (blanked ? " BLANKED" : ""));
  }

  delay(1);
}
