#include <Wire.h>
#include <ESP8266WiFi.h>
#include <time.h>

#include "TimeKeeper.h"
#include "DebugManager.h"
#include "config.h"
#include "defs.h"

// Anything before this is NTP not having answered yet rather than a real time.
#define NTP_VALID_AFTER_EPOCH   1700000000UL   // Nov 2023

// How often to look at the NTP result while we are still waiting for a fix.
#define NTP_POLL_INTERVAL_MS    2000UL

// ************************************************************
// Bring up I2C, the RTC and (if configured) Wi-Fi.
// ************************************************************
void TimeKeeper::begin() {
  Wire.begin(I2C_SDA, I2C_SCL);

  if (_rtc.begin()) {
    uint8_t h, m, s;
    if (_rtc.getTime(h, m, s)) {
      adopt(h, m, s, TIME_SOURCE_RTC);
      debugManager.debugMsg("Time: taken from RTC");
    } else {
      debugManager.debugMsg("Time: RTC present but holds no valid time");
    }
  }

  _lastSecondMillis = millis();
  _lastRtcSyncMillis = _lastSecondMillis;
  _lastNtpCheckMillis = _lastSecondMillis;

  startWifi();
}

// ************************************************************
// Kick off a Wi-Fi association, if one is configured. We do not
// block waiting for it - the clock runs from the RTC meanwhile.
// ************************************************************
void TimeKeeper::startWifi() {
  const char *ssid = WIFI_SSID;

  if (ssid[0] == '\0') {
    debugManager.debugMsg("WiFi: no SSID configured, running offline");
    _wifiEnabled = false;
    return;
  }

  _wifiEnabled = true;
  WiFi.mode(WIFI_STA);
  WiFi.hostname("decatron-clock");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  debugManager.debugMsg("WiFi: connecting to " + String(WIFI_SSID));
}

bool TimeKeeper::wifiConnected() const {
  return _wifiEnabled && (WiFi.status() == WL_CONNECTED);
}

// ************************************************************
// Advance the internal clock, and discipline it from NTP or the
// RTC when they are due.
// ************************************************************
void TimeKeeper::tick(unsigned long nowMillis) {
  _secondRolledOver = false;

  while ((nowMillis - _lastSecondMillis) >= 1000UL) {
    _lastSecondMillis += 1000UL;
    advanceOneSecond();
    _secondRolledOver = true;
  }

  pollNtp(nowMillis);
  pollRtc(nowMillis);
}

// ************************************************************
// Once a second, locally.
// ************************************************************
void TimeKeeper::advanceOneSecond() {
  if (++_seconds < 60) {
    return;
  }
  _seconds = 0;

  if (++_minutes < 60) {
    return;
  }
  _minutes = 0;

  if (++_hours < 24) {
    return;
  }
  _hours = 0;
}

// ************************************************************
// Take a time from somewhere and make it ours.
// ************************************************************
void TimeKeeper::adopt(uint8_t h, uint8_t m, uint8_t s, TimeSource source) {
  _hours = h;
  _minutes = m;
  _seconds = s;
  _source = source;
  _lastSecondMillis = millis();
}

// ************************************************************
// Look for an NTP fix. configTime() starts the SNTP client in the
// background; we just keep checking whether the system clock has
// become plausible, and re-check once an hour to catch DST.
// ************************************************************
void TimeKeeper::pollNtp(unsigned long nowMillis) {
  if (!_wifiEnabled) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!_ntpConfigured) {
    configTime(TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
    _ntpConfigured = true;
    debugManager.debugMsg("NTP: Wi-Fi up, SNTP started");
  }

  unsigned long interval =
      (_source == TIME_SOURCE_NTP) ? NTP_RESYNC_INTERVAL_MS : NTP_POLL_INTERVAL_MS;

  if ((nowMillis - _lastNtpCheckMillis) < interval) {
    return;
  }
  _lastNtpCheckMillis = nowMillis;

  time_t epoch = time(nullptr);
  if ((unsigned long)epoch < NTP_VALID_AFTER_EPOCH) {
    return;   // no fix yet
  }

  struct tm local;
  localtime_r(&epoch, &local);

  bool firstFix = (_source != TIME_SOURCE_NTP);
  adopt((uint8_t)local.tm_hour, (uint8_t)local.tm_min, (uint8_t)local.tm_sec, TIME_SOURCE_NTP);

  if (firstFix) {
    debugManager.debugMsg("NTP: first fix, time is now " + String(_hours) + ":" +
                          String(_minutes) + ":" + String(_seconds));
  }

  // Keep the RTC honest so the clock is right after a power cut.
  if (_rtc.isPresent()) {
    _rtc.setDateTime((uint8_t)local.tm_hour, (uint8_t)local.tm_min, (uint8_t)local.tm_sec,
                     (uint8_t)(local.tm_wday == 0 ? 7 : local.tm_wday),
                     (uint8_t)local.tm_mday,
                     (uint8_t)(local.tm_mon + 1),
                     (uint8_t)(local.tm_year % 100));
  }
}

// ************************************************************
// When NTP is not available, re-read the RTC occasionally so the
// free running millis() clock does not drift away from it.
// ************************************************************
void TimeKeeper::pollRtc(unsigned long nowMillis) {
  if (_source == TIME_SOURCE_NTP) {
    return;
  }

  if (!_rtc.isPresent()) {
    return;
  }

  if ((nowMillis - _lastRtcSyncMillis) < RTC_RESYNC_INTERVAL_MS) {
    return;
  }
  _lastRtcSyncMillis = nowMillis;

  uint8_t h, m, s;
  if (_rtc.getTime(h, m, s)) {
    adopt(h, m, s, TIME_SOURCE_RTC);
  }
}

// ************************************************************
// Set the time by hand.
// ************************************************************
void TimeKeeper::setTime(uint8_t h, uint8_t m, uint8_t s) {
  adopt(h, m, s, TIME_SOURCE_MANUAL);

  if (_rtc.isPresent()) {
    _rtc.setTime(h, m, s);
  }

  debugManager.debugMsg("Time set by hand to " + String(h) + ":" + String(m) + ":" + String(s));
}

// ************************************************************
// Human readable source.
// ************************************************************
const char *TimeKeeper::sourceName() const {
  switch (_source) {
    case TIME_SOURCE_NTP:    return "NTP";
    case TIME_SOURCE_RTC:    return "RTC";
    case TIME_SOURCE_MANUAL: return "manual";
    default:                 return "unset";
  }
}

// ************************************************************
// One line of status for the serial console.
// ************************************************************
String TimeKeeper::status() const {
  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", _hours, _minutes, _seconds);

  String s = String(buf) + " source=" + sourceName();
  s += String(" rtc=") + (_rtc.isPresent() ? "yes" : "no");
  if (_wifiEnabled) {
    s += String(" wifi=") + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("down"));
  } else {
    s += " wifi=off";
  }
  return s;
}
