// The round 360x360 display of the ESP-VoCat (ST77916 over QSPI): pushes the
// changed tiles of the Canvas. On Windows it writes the emulator's panel memory.
#ifndef HW_VOCAT_LCD_H
#define HW_VOCAT_LCD_H

#include <stdint.h>

#include "../core/Canvas.h"

class VocatLcd {
 public:
  bool begin();
  // Sends the tiles that changed since the last push. Returns the bytes sent.
  uint32_t push(const Canvas &c);
  void setBacklight(uint8_t level);
  uint8_t backlight() const { return level_; }

 private:
  bool first_ = true;
  uint8_t level_ = 0;
};

#endif
