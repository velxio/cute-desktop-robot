#include "Codec.h"

#include <Arduino.h>

#include "../../Config.h"

#ifdef EMULATOR
bool Codec::begin() {
  ok_ = true;
  vol_ = SPEAKER_VOLUME;
  return true;
}
void Codec::setVolume(uint8_t p) { vol_ = p > 100 ? 100 : p; }
void Codec::setMute(bool) {}
void Codec::enablePa(bool on) { pa_ = on; }
bool Codec::initDac() { return true; }
bool Codec::initAdc() { return true; }
bool Codec::wr(uint8_t, uint8_t, uint8_t) { return true; }
bool Codec::rd(uint8_t, uint8_t, uint8_t &) { return true; }

#else
#include <Wire.h>

// Register sequences from the Espressif esp-bsp drivers (es8311, es7210), reduced to
// what this board uses: 16 kHz, 16 bit, MCLK = 256 x fs = 4.096 MHz.

bool Codec::wr(uint8_t addr, uint8_t reg, uint8_t v) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

bool Codec::rd(uint8_t addr, uint8_t reg, uint8_t &v) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
  v = (uint8_t)Wire.read();
  return true;
}

// ---- ES8311: the DAC ------------------------------------------------------------------
bool Codec::initDac() {
  const uint8_t A = ES8311_ADDR;
  uint8_t v;
  if (!rd(A, 0x00, v)) return false;  // is it there?
  wr(A, 0x00, 0x1F);  // reset
  delay(20);
  wr(A, 0x00, 0x00);
  wr(A, 0x00, 0x80);  // power on
  // clocks: everything on, MCLK from the MCLK pin, not inverted
  wr(A, 0x01, 0x3F);
  rd(A, 0x06, v);
  wr(A, 0x06, (uint8_t)(v & ~0x20));  // BCLK not inverted
  // dividers for MCLK 4.096 MHz and 16 kHz: pre_div 1, mult x1, adc/dac div 1,
  // single speed, lrck 0x00ff, bclk div 4, osr 0x10 (row {4096000, 16000} of the table)
  rd(A, 0x02, v);
  wr(A, 0x02, (uint8_t)((v & 0x07) | (0 << 5) | (0 << 3)));
  wr(A, 0x03, 0x10);  // fs_mode 0, adc osr
  wr(A, 0x04, 0x10);  // dac osr
  wr(A, 0x05, 0x00);  // adc div 1, dac div 1
  rd(A, 0x06, v);
  wr(A, 0x06, (uint8_t)((v & 0xE0) | (4 - 1)));  // bclk div 4
  rd(A, 0x07, v);
  wr(A, 0x07, (uint8_t)(v & 0xC0));  // lrck high = 0
  wr(A, 0x08, 0xFF);                 // lrck low
  // format: slave, I2S, 16 bit in and out
  rd(A, 0x00, v);
  wr(A, 0x00, (uint8_t)(v & 0xBF));
  wr(A, 0x09, 0x0C);  // SDP in 16 bit
  wr(A, 0x0A, 0x0C);  // SDP out 16 bit
  // analog power up
  wr(A, 0x0D, 0x01);
  wr(A, 0x0E, 0x02);
  wr(A, 0x12, 0x00);  // DAC on
  wr(A, 0x13, 0x10);  // output to the HP driver
  wr(A, 0x1C, 0x6A);
  wr(A, 0x37, 0x08);  // bypass the DAC equalizer
  wr(A, 0x32, 0x00);  // volume 0 until asked
  return true;
}

// ---- ES7210: the microphones -------------------------------------------------------------
bool Codec::initAdc() {
  const uint8_t A = ES7210_ADDR;
  uint8_t v;
  if (!rd(A, 0x00, v)) return false;
  wr(A, 0x00, 0xFF);  // reset
  wr(A, 0x00, 0x32);
  wr(A, 0x09, 0x30);  // power up timing
  wr(A, 0x0A, 0x30);
  wr(A, 0x23, 0x2A);  // high pass filters of ADC1..4
  wr(A, 0x22, 0x0A);
  wr(A, 0x21, 0x2A);
  wr(A, 0x20, 0x0A);
  wr(A, 0x11, 0x60);  // I2S format, 16 bit
  wr(A, 0x12, 0x02);  // 1xFS TDM: the four ADCs in one frame
  wr(A, 0x40, 0xC3);  // analog power, VMID
  wr(A, 0x41, 0x70);  // mic bias 2.87 V
  wr(A, 0x42, 0x70);
  // analog gain of the four microphones
  uint8_t g;
  if (MIC_GAIN_DB <= 33) g = (uint8_t)(MIC_GAIN_DB / 3);
  else if (MIC_GAIN_DB < 36) g = 12;
  else if (MIC_GAIN_DB < 37) g = 13;
  else g = 14;
  for (uint8_t r = 0x43; r <= 0x46; r++) wr(A, r, (uint8_t)(g | 0x10));
  wr(A, 0x47, 0x08);  // mic power
  wr(A, 0x48, 0x08);
  wr(A, 0x49, 0x08);
  wr(A, 0x4A, 0x08);
  // 16 kHz from MCLK 4.096 MHz: osr 0x20, adc div 1, doubler 1, dll 1, lrck 0x0100
  wr(A, 0x07, 0x20);
  wr(A, 0x02, (uint8_t)(0x01 | (1 << 6) | (1 << 7)));
  wr(A, 0x04, 0x01);
  wr(A, 0x05, 0x00);
  wr(A, 0x06, 0x04);  // DLL off
  wr(A, 0x4B, 0x0F);  // bias, ADC and PGA of mic 1-2 on
  wr(A, 0x4C, 0x0F);  // ...and of mic 3-4
  wr(A, 0x00, 0x71);  // enable
  wr(A, 0x00, 0x41);
  return true;
}

bool Codec::begin() {
  pinMode(PIN_PA_EN, OUTPUT);
  digitalWrite(PIN_PA_EN, LOW);
  bool dac = initDac();
  bool adc = initAdc();
  if (!dac) Serial.println("ES8311 (speaker) did not answer");
  if (!adc) Serial.println("ES7210 (microphones) did not answer");
  ok_ = dac && adc;
  setVolume(SPEAKER_VOLUME);
  return ok_;
}

void Codec::setVolume(uint8_t p) {
  vol_ = p > 100 ? 100 : p;
  uint8_t reg = vol_ == 0 ? 0 : (uint8_t)(vol_ * 256 / 100 - 1);
  wr(ES8311_ADDR, 0x32, reg);
}

void Codec::setMute(bool on) {
  uint8_t v;
  if (!rd(ES8311_ADDR, 0x31, v)) return;
  if (on) v |= 0x60;
  else v &= (uint8_t)~0x60;
  wr(ES8311_ADDR, 0x31, v);
}

void Codec::enablePa(bool on) {
  if (on == pa_) return;
  pa_ = on;
  digitalWrite(PIN_PA_EN, on ? HIGH : LOW);
}
#endif
