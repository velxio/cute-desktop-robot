// Colors. The engine works with 24-bit RGB and the panel with RGB565.
#ifndef CORE_COLOR_H
#define CORE_COLOR_H

#include <stdint.h>

typedef uint32_t Rgb;  // 0xRRGGBB

static inline Rgb rgb(int r, int g, int b) {
  r = r < 0 ? 0 : (r > 255 ? 255 : r);
  g = g < 0 ? 0 : (g > 255 ? 255 : g);
  b = b < 0 ? 0 : (b > 255 ? 255 : b);
  return ((Rgb)r << 16) | ((Rgb)g << 8) | (Rgb)b;
}

static inline int redOf(Rgb c) { return (int)((c >> 16) & 255); }
static inline int greenOf(Rgb c) { return (int)((c >> 8) & 255); }
static inline int blueOf(Rgb c) { return (int)(c & 255); }

// t in 0..1
static inline Rgb mixRgb(Rgb a, Rgb b, float t) {
  if (t <= 0.0f) return a;
  if (t >= 1.0f) return b;
  int k = (int)(t * 256.0f);
  return rgb(redOf(a) + (((redOf(b) - redOf(a)) * k) >> 8), greenOf(a) + (((greenOf(b) - greenOf(a)) * k) >> 8),
             blueOf(a) + (((blueOf(b) - blueOf(a)) * k) >> 8));
}

static inline Rgb lighten(Rgb c, float t) { return mixRgb(c, 0xFFFFFF, t); }
static inline Rgb darken(Rgb c, float t) { return mixRgb(c, 0x000000, t); }

static inline uint16_t to565(Rgb c) {
  return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
}

static inline Rgb from565(uint16_t c) {
  int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  return rgb((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
}

// Blend of two RGB565 colors, alpha 0..32 (0 = all `bg`).
static inline uint16_t blend565(uint16_t bg, uint16_t fg, uint32_t alpha) {
  uint32_t b = ((uint32_t)bg | ((uint32_t)bg << 16)) & 0x07E0F81Fu;
  uint32_t f = ((uint32_t)fg | ((uint32_t)fg << 16)) & 0x07E0F81Fu;
  uint32_t r = ((((f - b) * alpha) >> 5) + b) & 0x07E0F81Fu;
  return (uint16_t)(r | (r >> 16));
}

// ---- palette -----------------------------------------------------------------
// (INK_ prefix: raylib owns the plain color names in the emulator)
const Rgb INK_BLACK = 0x000000;
const Rgb INK_WHITE = 0xFFFFFF;
const Rgb INK_CYAN = 0x35E0FF;    // the calm robot
const Rgb INK_MINT = 0x50FFBE;
const Rgb INK_GREEN = 0x5BE36A;
const Rgb INK_LIME = 0xB6F04A;
const Rgb INK_YELLOW = 0xFFD84A;
const Rgb INK_AMBER = 0xFFB020;
const Rgb INK_ORANGE = 0xFF8A3A;
const Rgb INK_RED = 0xFF4030;
const Rgb INK_CRIMSON = 0xE8203A;
const Rgb INK_PINK = 0xFF5A96;
const Rgb INK_ROSE = 0xFF9EBB;
const Rgb INK_PURPLE = 0xB07CFF;
const Rgb INK_LILAC = 0xC8B0FF;
const Rgb INK_INDIGO = 0x8478E6;
const Rgb INK_BLUE = 0x4F8CFF;
const Rgb INK_SKY = 0x8FD0FF;
const Rgb INK_ICE = 0xC4ECFF;
const Rgb INK_GRAY = 0x8A93A6;
const Rgb INK_DARK = 0x2A3040;
const Rgb INK_BROWN = 0xA86A3C;

#endif
