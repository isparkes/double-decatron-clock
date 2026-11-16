#ifndef CONFIG_H
#define CONFIG_H

//**********************************************************************************
//* User configuration. Everything here is compile-time - there is no web UI and   *
//* nothing is persisted to flash yet.                                             *
//**********************************************************************************

// ------------------------------ Debug -------------------------------

// Serial debug output at 115200 baud. Note that the serial RX pin is shared
// with the front panel BTN1 line on this board, so leaving a button connected
// while debugging will inject garbage into the command parser.
#define DEBUG_ENABLED   true

// ------------------------------ Wi-Fi / NTP -------------------------

// Leave WIFI_SSID empty ("") to skip Wi-Fi and NTP completely and run from the
// RTC (or from a time set over the serial console).
#define WIFI_SSID       ""
#define WIFI_PASSWORD   ""

#define NTP_SERVER_1    "pool.ntp.org"
#define NTP_SERVER_2    "time.nist.gov"

// POSIX timezone string, used for DST handling.
//   Europe/Berlin : "CET-1CEST,M3.5.0,M10.5.0/3"
//   Europe/London : "GMT0BST,M3.5.0/1,M10.5.0"
//   UTC           : "UTC0"
#define TIMEZONE        "CET-1CEST,M3.5.0,M10.5.0/3"

// How often to re-sync from NTP once we have a fix.
#define NTP_RESYNC_INTERVAL_MS      (60UL * 60UL * 1000UL)   // 1 hour

// How often to re-read the RTC when NTP is not available, to stop the internal
// millis() clock drifting away from it.
#define RTC_RESYNC_INTERVAL_MS      (10UL * 60UL * 1000UL)   // 10 minutes

// How long to wait for a Wi-Fi association before giving up and carrying on.
// We keep retrying in the background, this is just the startup budget.
#define WIFI_CONNECT_TIMEOUT_MS     (15UL * 1000UL)

// ------------------------------ Display -----------------------------

// Which tube shows what. Tube 1 is the pair of connections on Guide1_1 /
// Guide2_1 / Index1, tube 2 is Guide1_2 / Guide2_2 / Index2. Swap these two if
// the tubes end up the wrong way round in the case.
#define HOURS_TUBE      1
#define MINUTES_TUBE    2

// Set to true if a tube runs anti-clockwise. Which way "forwards" rotates the
// glow depends on the guide wiring, and on this board the G1/G2 pair is
// crossed between the main board and the tube carrier, so expect to need this.
#define HOURS_TUBE_REVERSED     false
#define MINUTES_TUBE_REVERSED   false

// The hours tube is a 12 hour dial. With only 30 positions it advances one
// position every 24 minutes, which gives a believable creeping hour hand.
// Set to false to make it jump straight to the hour instead.
#define HOURS_HAND_CREEPS       true

// ------------------------------ Blanking ----------------------------

// Switch the HV off between these hours to save the tubes. Set both to the same
// value to disable blanking. Hours are local time, 0-23. The display re-homes
// when it comes back on.
#define BLANK_FROM_HOUR         0
#define BLANK_TO_HOUR           0

#endif
