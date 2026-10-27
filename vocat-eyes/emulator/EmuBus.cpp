// The I2C chips of the ESP-VoCat, simulated: the touch controller and the fuel
// gauge answer; the audio codecs only acknowledge.
#include <math.h>

#include "EmuHw.h"
#include "Wire.h"

#include "../Config.h"

TwoWire Wire;

bool TwoWire::begin(int, int, uint32_t) { return true; }

void TwoWire::beginTransmission(uint8_t address) {
  addr_ = address;
  txLen_ = 0;
}

size_t TwoWire::write(uint8_t b) {
  if (txLen_ < (int)sizeof(tx_)) tx_[txLen_++] = b;
  return 1;
}

uint8_t TwoWire::endTransmission(bool) {
  if (addr_ == TOUCH_ADDR) {
    // the chip sleeps while nobody touches it and does not answer
    if (!emuHw.touchDown) return 2;
  } else if (addr_ == BATTERY_ADDR) {
    if (!emuHw.batteryPresent) return 2;
  } else if (addr_ != ES8311_ADDR && addr_ != ES7210_ADDR && addr_ != IMU_ADDR) {
    return 2;  // nobody answered
  }
  if (txLen_ > 0) reg_[addr_ & 127] = tx_[0];
  return 0;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t count, bool) {
  rxLen_ = rxPos_ = 0;
  if (count > sizeof(rx_)) count = sizeof(rx_);
  if (address == TOUCH_ADDR) {
    if (!emuHw.touchDown) return 0;
    // the chip works in the coordinates of the glass: turned with the display
    int tx = emuHw.touchX, ty = emuHw.touchY;
#if LCD_ROTATION == 1
    { int t = tx; tx = ty; ty = EMU_LCD_W - 1 - t; }
#elif LCD_ROTATION == 2
    tx = EMU_LCD_W - 1 - tx;
    ty = EMU_LCD_H - 1 - ty;
#elif LCD_ROTATION == 3
    { int t = tx; tx = EMU_LCD_H - 1 - ty; ty = t; }
#endif
    // registers 0x02..0x07: fingers, x high, x low, y high, y low, (pressure)
    uint8_t b[6] = {1, (uint8_t)((tx >> 8) & 0x0F), (uint8_t)(tx & 0xFF), (uint8_t)((ty >> 8) & 0x0F),
                    (uint8_t)(ty & 0xFF), 0};
    for (int i = 0; i < count && i < 6; i++) rx_[rxLen_++] = b[i];
  } else if (address == BATTERY_ADDR) {
    if (!emuHw.batteryPresent) return 0;
    uint16_t v = 0;
    switch (reg_[BATTERY_ADDR]) {
      case 0x08: v = (uint16_t)emuHw.batteryMv; break;
      case 0x0A: v = emuHw.charging ? 0x0000 : 0x0001; break;  // bit 0 = discharging
      case 0x0C: v = (uint16_t)(int16_t)(emuHw.charging ? 450 : -180); break;
      case 0x2C: v = (uint16_t)emuHw.batteryPct; break;
      default: v = 0;
    }
    uint8_t b[2] = {(uint8_t)(v & 0xFF), (uint8_t)(v >> 8)};
    for (int i = 0; i < count && i < 2; i++) rx_[rxLen_++] = b[i];
  } else if (address == ES8311_ADDR || address == ES7210_ADDR || address == IMU_ADDR) {
    for (int i = 0; i < count; i++) rx_[rxLen_++] = 0;
  } else {
    return 0;
  }
  return (uint8_t)rxLen_;
}

int TwoWire::available() { return rxLen_ - rxPos_; }
int TwoWire::read() { return rxPos_ < rxLen_ ? rx_[rxPos_++] : -1; }
