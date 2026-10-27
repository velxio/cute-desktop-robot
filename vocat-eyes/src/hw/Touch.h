// CST816S capacitive touch of the ESP-VoCat glass (I2C 0x15, INT on GPIO10).
#ifndef HW_TOUCH_H
#define HW_TOUCH_H

#include <stdint.h>

struct TouchPoint {
  bool down = false;
  int16_t x = 0, y = 0;  // pixels, 0..359
};

class Touch {
 public:
  void begin();
  TouchPoint read();

 private:
  TouchPoint last_;
  uint8_t misses_ = 0;
};

#endif
