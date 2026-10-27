#include "VocatLcd.h"

#include <Arduino.h>

#include "../../Config.h"

#ifdef EMULATOR
// ---- Windows: the "panel" is memory that the emulator window shows ------------------
#include "EmuHw.h"

bool VocatLcd::begin() {
  first_ = true;
  return true;
}

void VocatLcd::setBacklight(uint8_t level) {
  level_ = level;
  emuHw.backlight = level;
}

uint32_t VocatLcd::push(const Canvas &c) {
  const uint16_t *px = c.pixels();
  if (!px) return 0;
  uint32_t bytes = 0;
  for (int ty = 0; ty < TILES; ty++) {
    uint16_t m = first_ ? 0x7FFF : c.changedTiles(ty);
    for (int tx = 0; tx < TILES; tx++) {
      if (!(m & (1u << tx))) continue;
      for (int y = ty * TILE_PX; y < (ty + 1) * TILE_PX; y++)
        for (int x = tx * TILE_PX; x < (tx + 1) * TILE_PX; x++) emuHw.lcd[y * SCREEN_W + x] = px[y * SCREEN_W + x];
      bytes += TILE_PX * TILE_PX * 2;
    }
  }
  first_ = false;
  emuHw.spiBytes += bytes;
  emuHw.lcdUpdates++;
  return bytes;
}

#else
// ---- The robot: Arduino_GFX drives the ST77916 through the QSPI bus -------------------
#include <Arduino_GFX_Library.h>

#include "vocat_st77916_init.h"

namespace {
Arduino_DataBus *bus = nullptr;
Arduino_ST77916 *gfx = nullptr;
}  // namespace

bool VocatLcd::begin() {
  // power: the LCD rail (and the SD) is on when GPIO9 is low; the codec rail of v1.2 on GPIO48
  pinMode(PIN_LCD_EN, OUTPUT);
  digitalWrite(PIN_LCD_EN, LOW);
  pinMode(PIN_CODEC_PWR, OUTPUT);
  digitalWrite(PIN_CODEC_PWR, HIGH);
  delay(50);
  // the panel reset is active HIGH on v1.2, so it is pulsed here instead of by Arduino_GFX
  pinMode(PIN_LCD_RST, OUTPUT);
#if LCD_RESET_ACTIVE_HIGH
  digitalWrite(PIN_LCD_RST, LOW);
  delay(10);
  digitalWrite(PIN_LCD_RST, HIGH);
  delay(10);
  digitalWrite(PIN_LCD_RST, LOW);
#else
  digitalWrite(PIN_LCD_RST, HIGH);
  delay(10);
  digitalWrite(PIN_LCD_RST, LOW);
  delay(10);
  digitalWrite(PIN_LCD_RST, HIGH);
#endif
  delay(120);
  bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_PCLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3);
  gfx = new Arduino_ST77916(bus, GFX_NOT_DEFINED, LCD_ROTATION, true, SCREEN_W, SCREEN_H, 0, 0, 0, 0,
                            vocat_st77916_init, sizeof(vocat_st77916_init));
  if (!gfx->begin(LCD_QSPI_HZ)) return false;
  gfx->fillScreen(0x0000);
  ledcAttach(PIN_LCD_BL, 5000, 8);
  ledcWrite(PIN_LCD_BL, 0);
  first_ = true;
  return true;
}

void VocatLcd::setBacklight(uint8_t level) {
  level_ = level;
  ledcWrite(PIN_LCD_BL, level);
}

uint32_t VocatLcd::push(const Canvas &c) {
  const uint16_t *px = c.pixels();
  if (!px || !gfx) return 0;
  uint32_t bytes = 0;
  gfx->startWrite();
  for (int ty = 0; ty < TILES; ty++) {
    uint16_t m = first_ ? 0x7FFF : c.changedTiles(ty);
    int tx = 0;
    while (tx < TILES) {
      if (!(m & (1u << tx))) {
        tx++;
        continue;
      }
      int start = tx;
      while (tx < TILES && (m & (1u << tx))) tx++;
      // one window per run of changed tiles, row by row (the rows are not contiguous)
      int x = start * TILE_PX, w = (tx - start) * TILE_PX, y0 = ty * TILE_PX;
      gfx->writeAddrWindow(x, y0, w, TILE_PX);
      for (int y = y0; y < y0 + TILE_PX; y++) gfx->writePixels((uint16_t *)&px[y * SCREEN_W + x], w);
      bytes += (uint32_t)w * TILE_PX * 2;
    }
  }
  gfx->endWrite();
  first_ = false;
  return bytes;
}
#endif
