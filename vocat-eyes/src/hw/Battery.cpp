#include "Battery.h"

#include <Arduino.h>
#include <Wire.h>

#include "../../Config.h"

// standard commands of the BQ27220 (16 bit, little endian)
#define BQ_VOLTAGE 0x08
#define BQ_STATUS 0x0A
#define BQ_CURRENT 0x0C
#define BQ_SOC 0x2C

void Battery::begin() {}

bool Battery::reg16(uint8_t reg, uint16_t &out) {
  Wire.beginTransmission((uint8_t)BATTERY_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)BATTERY_ADDR, (uint8_t)2) != 2) return false;
  uint8_t lo = (uint8_t)Wire.read(), hi = (uint8_t)Wire.read();
  out = (uint16_t)(lo | (hi << 8));
  return true;
}

BatteryState Battery::read() {
  BatteryState b;
  uint16_t v = 0, st = 0, cur = 0, soc = 0;
  if (!reg16(BQ_VOLTAGE, v) || !reg16(BQ_STATUS, st) || !reg16(BQ_CURRENT, cur) || !reg16(BQ_SOC, soc)) {
    if (fails_ < 250) fails_++;
    return b;
  }
  fails_ = 0;
  b.valid = v > 2500 && v < 4600;
  b.millivolts = v;
  b.milliamps = (int16_t)cur;
  b.percent = (uint8_t)(soc > 100 ? 100 : soc);
  b.charging = (st & 0x0001) == 0;  // bit 0 (DSG) is set while discharging
  return b;
}
