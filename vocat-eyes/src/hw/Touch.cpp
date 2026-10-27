#include "Touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "../../Config.h"
#include "../core/Canvas.h"

void Touch::begin() { pinMode(PIN_TOUCH_INT, INPUT_PULLUP); }

// The chip answers 6 bytes from register 0x02: finger count, x (12 bits), y (12 bits).
// It sleeps when nobody touches it and then does not answer at all: that is "no finger".
TouchPoint Touch::read() {
  TouchPoint p;
  Wire.beginTransmission((uint8_t)TOUCH_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) {
    misses_++;
    if (misses_ > 2) last_.down = false;
    return last_.down ? last_ : p;
  }
  uint8_t b[6] = {0, 0, 0, 0, 0, 0};
  if (Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)6) == 6) {
    for (int i = 0; i < 6; i++) b[i] = (uint8_t)Wire.read();
    misses_ = 0;
    if ((b[0] & 0x0F) != 0) {
      p.down = true;
      p.x = (int16_t)(((b[1] & 0x0F) << 8) | b[2]);
      p.y = (int16_t)(((b[3] & 0x0F) << 8) | b[4]);
#if LCD_ROTATION == 2
      p.x = (int16_t)(SCREEN_W - 1 - p.x);
      p.y = (int16_t)(SCREEN_H - 1 - p.y);
#elif LCD_ROTATION == 1
      { int16_t t = p.x; p.x = (int16_t)(SCREEN_W - 1 - p.y); p.y = t; }
#elif LCD_ROTATION == 3
      { int16_t t = p.x; p.x = p.y; p.y = (int16_t)(SCREEN_H - 1 - t); }
#endif
      if (p.x < 0) p.x = 0;
      if (p.y < 0) p.y = 0;
      if (p.x >= SCREEN_W) p.x = SCREEN_W - 1;
      if (p.y >= SCREEN_H) p.y = SCREEN_H - 1;
    }
    last_ = p;
    return p;
  }
  return last_;
}
