#include <Wire.h>
#include "Ds1307.h"
#include "DebugManager.h"

// ************************************************************
// See if there is an RTC on the bus at all.
// ************************************************************
bool Ds1307::begin() {
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  _present = (Wire.endTransmission() == 0);

  if (_present) {
    if (!isRunning()) {
      debugManager.debugMsg("RTC: oscillator halted, starting it");
      startClock();
    }
    debugManager.debugMsg("RTC: DS1307 found");
  } else {
    debugManager.debugMsg("RTC: no DS1307 found");
  }

  return _present;
}

// ************************************************************
// Bit 7 of register 0 is the clock halt bit.
// ************************************************************
bool Ds1307::isRunning() {
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  if (Wire.requestFrom(DS1307_I2C_ADDRESS, 1) != 1) {
    return false;
  }

  return (Wire.read() & 0x80) == 0;
}

// ************************************************************
// Clear the clock halt bit, preserving the seconds value.
// ************************************************************
void Ds1307::startClock() {
  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission() != 0) {
    return;
  }

  if (Wire.requestFrom(DS1307_I2C_ADDRESS, 1) != 1) {
    return;
  }

  uint8_t seconds = Wire.read() & 0x7f;

  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write((uint8_t)0x00);
  Wire.write(seconds);
  Wire.endTransmission();
}

// ************************************************************
// Read the time of day registers.
// ************************************************************
bool Ds1307::getTime(uint8_t &hours, uint8_t &minutes, uint8_t &seconds) {
  if (!_present) {
    return false;
  }

  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  if (Wire.requestFrom(DS1307_I2C_ADDRESS, 3) != 3) {
    return false;
  }

  uint8_t rawSeconds = Wire.read();
  uint8_t rawMinutes = Wire.read();
  uint8_t rawHours   = Wire.read();

  // Clock halt bit set means the time held is stale.
  if (rawSeconds & 0x80) {
    return false;
  }

  // Bit 6 of the hours register selects 12 hour mode. We always write 24 hour
  // mode, but a chip programmed by something else might not be.
  if (rawHours & 0x40) {
    uint8_t hour12 = bcdToDec((uint8_t)(rawHours & 0x1f));
    bool pm = (rawHours & 0x20) != 0;
    hours = (uint8_t)((hour12 % 12) + (pm ? 12 : 0));
  } else {
    hours = bcdToDec((uint8_t)(rawHours & 0x3f));
  }

  seconds = bcdToDec((uint8_t)(rawSeconds & 0x7f));
  minutes = bcdToDec((uint8_t)(rawMinutes & 0x7f));

  if (hours > 23 || minutes > 59 || seconds > 59) {
    return false;
  }

  return true;
}

// ************************************************************
// Write just the time of day registers.
// ************************************************************
bool Ds1307::setTime(uint8_t hours, uint8_t minutes, uint8_t seconds) {
  if (!_present) {
    return false;
  }

  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write((uint8_t)0x00);
  Wire.write(decToBcd(seconds));   // bit 7 clear also starts the oscillator
  Wire.write(decToBcd(minutes));
  Wire.write(decToBcd(hours));     // bit 6 clear selects 24 hour mode
  return Wire.endTransmission() == 0;
}

// ************************************************************
// Write the time and the date.
// ************************************************************
bool Ds1307::setDateTime(uint8_t hours, uint8_t minutes, uint8_t seconds,
                         uint8_t dayOfWeek, uint8_t dayOfMonth, uint8_t month, uint8_t year) {
  if (!_present) {
    return false;
  }

  Wire.beginTransmission(DS1307_I2C_ADDRESS);
  Wire.write((uint8_t)0x00);
  Wire.write(decToBcd(seconds));
  Wire.write(decToBcd(minutes));
  Wire.write(decToBcd(hours));
  Wire.write(decToBcd(dayOfWeek));
  Wire.write(decToBcd(dayOfMonth));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));
  return Wire.endTransmission() == 0;
}
