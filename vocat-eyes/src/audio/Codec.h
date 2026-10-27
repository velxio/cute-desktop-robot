// The two audio chips of the ESP-VoCat, set up over I2C:
//   ES8311 (0x18): DAC for the speaker, through the NS4150B amplifier (PA enable pin)
//   ES7210 (0x40): 4-channel ADC with the two microphones, TDM output
// Both run as I2S slaves from the MCLK of the ESP32 (256 x sample rate).
#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include <stdint.h>

class Codec {
 public:
  bool begin();
  void setVolume(uint8_t percent);
  uint8_t volume() const { return vol_; }
  void setMute(bool on);
  void enablePa(bool on);
  bool ok() const { return ok_; }

 private:
  bool initDac();
  bool initAdc();
  bool wr(uint8_t addr, uint8_t reg, uint8_t v);
  bool rd(uint8_t addr, uint8_t reg, uint8_t &v);
  uint8_t vol_ = 0;
  bool ok_ = false;
  bool pa_ = false;
};

#endif
