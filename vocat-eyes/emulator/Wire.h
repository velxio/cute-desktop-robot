// Wire for the emulator: an I2C bus with the two chips of the Round Display,
// the CHSC6x touch (0x2E) and the PCF8563 clock (0x51). See EmuBus.cpp.
#ifndef TwoWire_h
#define TwoWire_h

#include <stddef.h>
#include <stdint.h>

class TwoWire {
 public:
  bool begin(int sda = -1, int scl = -1, uint32_t freq = 0);
  void beginTransmission(uint8_t address);
  size_t write(uint8_t b);
  uint8_t endTransmission(bool stop = true);
  uint8_t requestFrom(uint8_t address, uint8_t count, bool stop = true);
  int available();
  int read();

 private:
  uint8_t addr_ = 0;
  uint8_t tx_[32];
  int txLen_ = 0;
  uint8_t rx_[32];
  int rxLen_ = 0, rxPos_ = 0;
  uint8_t reg_[128] = {0};  // register pointer per address
};

extern TwoWire Wire;

#endif
