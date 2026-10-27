// The clock. The ESP-VoCat has no clock chip: the time comes from the network (NTP,
// once Wi-Fi is up) or from the serial console, and the ESP32 keeps it while powered.
#ifndef HW_RTC_H
#define HW_RTC_H

#include <stdint.h>

struct ClockTime {
  bool valid = false;  // false = the time was never set since power up
  uint8_t hour = 0, minute = 0, second = 0;
  uint8_t day = 1, month = 1;
  uint16_t year = 2026;
};

class Rtc {
 public:
  void begin();
  ClockTime read();
  void set(const ClockTime &t);

 private:
  bool set_ = false;
};

#endif
