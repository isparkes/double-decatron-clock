#ifndef DS1307_H
#define DS1307_H

#include <Arduino.h>

// ----------------------------------------------------------------------------------------------------
// ------------------------------------------ DS1307 RTC ----------------------------------------------
// ----------------------------------------------------------------------------------------------------
//
// Minimal driver for the battery backed DS1307 fitted at U5 on the board. Only
// the time-of-day registers are used; the date registers are read and written
// so the chip keeps a sane calendar, but the clock itself does not care.

#define DS1307_I2C_ADDRESS 0x68

class Ds1307
{
  public:
    // Probe the bus. Returns true if a DS1307 answered. Wire.begin() must have
    // been called already.
    bool begin();

    bool isPresent() const { return _present; }

    // True if the oscillator is running (clock halt bit clear).
    bool isRunning();

    // Clear the clock halt bit.
    void startClock();

    // Read the time of day. Returns false if the RTC is absent, halted, or
    // returns something that cannot be a valid time.
    bool getTime(uint8_t &hours, uint8_t &minutes, uint8_t &seconds);

    // Write the time of day, leaving the date registers alone.
    bool setTime(uint8_t hours, uint8_t minutes, uint8_t seconds);

    // Write the full date and time.
    bool setDateTime(uint8_t hours, uint8_t minutes, uint8_t seconds,
                     uint8_t dayOfWeek, uint8_t dayOfMonth, uint8_t month, uint8_t year);

  private:
    static uint8_t decToBcd(uint8_t val) { return (uint8_t)((val / 10 * 16) + (val % 10)); }
    static uint8_t bcdToDec(uint8_t val) { return (uint8_t)((val / 16 * 10) + (val % 16)); }

    bool _present = false;
};

#endif
