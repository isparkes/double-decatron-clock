#ifndef TIME_KEEPER_H
#define TIME_KEEPER_H

#include <Arduino.h>
#include "Ds1307.h"

// ----------------------------------------------------------------------------------------------------
// ----------------------------------------- Time keeping ---------------------------------------------
// ----------------------------------------------------------------------------------------------------
//
// Keeps a local time of day, free running off millis(), and disciplines it from
// whichever source is available:
//
//   NTP  - if Wi-Fi is configured and associates. Also writes the RTC.
//   RTC  - the battery backed DS1307 at U5.
//   none - whatever was set over the serial console, or 00:00:00.
//
// There is no date handling beyond keeping the RTC's calendar registers moving;
// the display only ever needs hours, minutes and seconds.

enum TimeSource {
  TIME_SOURCE_NONE = 0,
  TIME_SOURCE_MANUAL,
  TIME_SOURCE_RTC,
  TIME_SOURCE_NTP
};

class TimeKeeper
{
  public:
    void begin();
    void tick(unsigned long nowMillis);

    uint8_t hours() const { return _hours; }
    uint8_t minutes() const { return _minutes; }
    uint8_t seconds() const { return _seconds; }

    // True once the time has come from somewhere we believe.
    bool isSet() const { return _source != TIME_SOURCE_NONE; }

    TimeSource source() const { return _source; }
    const char *sourceName() const;

    // Did the second just roll over on this tick?
    bool secondRolledOver() const { return _secondRolledOver; }

    // Set the time by hand, and push it to the RTC if there is one.
    void setTime(uint8_t h, uint8_t m, uint8_t s);

    bool rtcPresent() const { return _rtc.isPresent(); }
    bool wifiConnected() const;

    String status() const;

  private:
    void startWifi();
    void pollNtp(unsigned long nowMillis);
    void pollRtc(unsigned long nowMillis);
    void advanceOneSecond();
    void adopt(uint8_t h, uint8_t m, uint8_t s, TimeSource source);

    Ds1307 _rtc;

    uint8_t _hours = 0;
    uint8_t _minutes = 0;
    uint8_t _seconds = 0;

    TimeSource _source = TIME_SOURCE_NONE;

    unsigned long _lastSecondMillis = 0;
    unsigned long _lastNtpCheckMillis = 0;
    unsigned long _lastRtcSyncMillis = 0;

    bool _wifiEnabled = false;
    bool _ntpConfigured = false;
    bool _secondRolledOver = false;
};

#endif
