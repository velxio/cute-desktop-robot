// BQ27220 fuel gauge of the ESP-VoCat (I2C 0x55): voltage, current and charge.
#ifndef HW_BATTERY_H
#define HW_BATTERY_H

#include <stdint.h>

struct BatteryState {
  bool valid = false;
  uint16_t millivolts = 0;
  int16_t milliamps = 0;  // negative while discharging
  uint8_t percent = 0;
  bool charging = false;
};

class Battery {
 public:
  void begin();
  // Reads the gauge (about 1 ms on the bus). Call every second or two.
  BatteryState read();

 private:
  bool reg16(uint8_t reg, uint16_t &out);
  uint8_t fails_ = 0;
};

#endif
