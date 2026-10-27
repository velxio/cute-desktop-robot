#include "InfoScreen.h"

#include <stdio.h>

#include "../core/MathUtil.h"

namespace {

const float CX = DISC_CX, CY = DISC_CY;
const float PI_F = 3.14159265f;

// Color of the charge: green when full, orange, red when almost empty.
Rgb chargeColor(float pct) {
  if (pct < 20.0f) return mixRgb(0xFF4030, 0xFFB020, clamp01(pct / 20.0f));
  if (pct < 50.0f) return mixRgb(0xFFB020, 0x5BE36A, clamp01((pct - 20.0f) / 30.0f));
  return 0x5BE36A;
}

// Gauge along the bottom edge of the glass: 110 degrees of arc, filled from the left.
void gauge(Canvas &c, float value01, Rgb color, float appear) {
  const float r = 60.0f, th = 4.2f;
  float mid = PI_F * 0.5f;
  float a0 = mid - 0.96f, a1 = mid + 0.96f;
  c.arc(CX, CY, r - th * 0.5f, a0, a1, th, Paint(INK_DARK, appear));
  float v = clamp01(value01) * appear;
  if (v <= 0.01f) return;
  c.arc(CX, CY, r - th * 0.5f, a1 - (a1 - a0) * v, a1, th, color);
}

// A small lightning bolt (charging).
void bolt(Canvas &c, float x, float y, Rgb color) {
  c.fillTriangle(x + 1.5f, y - 6.0f, x - 3.0f, y + 1.0f, x + 0.5f, y + 1.0f, color);
  c.fillTriangle(x - 1.5f, y + 6.0f, x + 3.0f, y - 1.0f, x - 0.5f, y - 1.0f, color);
}

}  // namespace

namespace InfoScreen {

void draw(Canvas &c, const SensorState &s, float time, const char *net, bool netOk) {
  c.clear();
  float appear = clamp01(time * 3.0f);  // `time` counts from 0 when the screen opens

  // ---- dial
  for (int i = 0; i < 12; i++) {
    float co, si;
    Canvas::unit(i * (PI_F / 6.0f) - PI_F * 0.5f, co, si);
    float r0 = (i % 3 == 0) ? 43.0f : 46.0f;
    c.thickLine(CX + co * r0, CY + si * r0, CX + co * 49.0f, CY + si * 49.0f, (i % 3 == 0) ? 2.2f : 1.2f,
                Paint(i % 3 == 0 ? INK_WHITE : INK_GRAY, appear));
  }

  // ---- hands and digital time
  if (s.clock.valid) {
    float h = (float)(s.clock.hour % 12) + s.clock.minute / 60.0f;
    float m = (float)s.clock.minute + s.clock.second / 60.0f;
    float co, si;
    Canvas::unit(h * (PI_F / 6.0f) - PI_F * 0.5f, co, si);
    c.thickLine(CX, CY, CX + co * 26.0f, CY + si * 26.0f, 4.0f, Paint(INK_WHITE, appear));
    Canvas::unit(m * (PI_F / 30.0f) - PI_F * 0.5f, co, si);
    c.thickLine(CX, CY, CX + co * 38.0f, CY + si * 38.0f, 2.6f, Paint(INK_CYAN, appear));
    Canvas::unit(s.clock.second * (PI_F / 30.0f) - PI_F * 0.5f, co, si);
    c.thickLine(CX - co * 8.0f, CY - si * 8.0f, CX + co * 42.0f, CY + si * 42.0f, 1.0f, Paint(INK_ORANGE, appear));
    c.fillCircle(CX, CY, 3.2f, INK_ORANGE);
    char txt[8];
    snprintf(txt, sizeof(txt), "%02d:%02d", s.clock.hour, s.clock.minute);
    c.textCentered(CX, CY + 14.0f, txt, 2, Paint(INK_WHITE, appear));
  } else {
    c.textCentered(CX, CY - 4.0f, "--:--", 2, Paint(INK_GRAY, appear));
  }

  // ---- battery
  if (s.battery.valid) {
    Rgb bc = chargeColor((float)s.battery.percent);
    gauge(c, s.battery.percent / 100.0f, bc, appear);
    char txt[12];
    snprintf(txt, sizeof(txt), "%d%%", (int)s.battery.percent);
    c.textCentered(CX, CY + 31.0f, txt, 1, Paint(bc, appear));
    if (s.battery.charging) bolt(c, CX + Canvas::textWidth(txt, 1) * 0.5f + 6.0f, CY + 34.0f, INK_SKY);
  } else {
    c.textCentered(CX, CY + 31.0f, "USB", 1, Paint(INK_GRAY, appear));
  }
  if (net) {
    char line[24];
    snprintf(line, sizeof(line), "%.16s", netOk ? "wifi ok" : net);  // narrow down here: 16 letters
    c.textCentered(CX, CY + 41.0f, line, 1, Paint(netOk ? (Rgb)0x5BE36A : (Rgb)0xFFB020, appear));
  }
  c.textCentered(CX, CY - 30.0f, "toca: ajustes", 1, Paint(INK_GRAY, appear));
}

}  // namespace InfoScreen
