#include "Decor.h"
#include "../core/MathUtil.h"

namespace {

const Rgb TEAR = 0x6FC3FF;
const Rgb HEART = 0xFF3B6B;
const Rgb CHEEK = 0xFF5A8A;
const Rgb TONGUE = 0xFF7A9C;
const Rgb STEAM = 0xD0D8E4;

inline float fract(float v) { return v - floorf(v); }

// Pops in fast, stays, fades at the end of a 0..1 phase.
inline float lifeScale(float phase) { return smoothstepf(0.0f, 0.18f, phase) * (1.0f - smoothstepf(0.8f, 1.0f, phase)); }

void drawZ(Canvas &c, float x, float y, float s, float th, const Paint &p) {
  float pts[8] = {x, y, x + s, y, x, y + s, x + s, y + s};
  c.polyline(pts, 4, th, p);
}

void zzz(Canvas &c, const FaceFrame &f, float k) {
  float bx = f.cx[1] + 4.0f;
  float by = f.cy[1] - f.h[1] * 0.5f - 4.0f;
  for (int i = 0; i < 3; i++) {
    float ph = fract(f.time * 0.38f + i / 3.0f);
    float s = (3.0f + ph * 7.0f) * k * lifeScale(ph);
    if (s < 2.0f) continue;
    float x = bx + ph * 8.0f + sinf(ph * 9.0f + i) * 2.0f;
    float y = by - ph * 34.0f;
    drawZ(c, x, y, s, s > 6.0f ? 2.0f : 1.4f, Paint(INK_ICE, 0.55f + 0.45f * (1.0f - ph)));
  }
}

void tears(Canvas &c, const FaceFrame &f, float k) {
  for (int e = 0; e < 2; e++) {
    float side = e == 0 ? -1.0f : 1.0f;
    float top = f.cy[e] + f.h[e] * 0.5f + 1.0f;
    for (int i = 0; i < 2; i++) {
      float ph = fract(f.time * 0.95f + e * 0.37f + i * 0.5f);
      float y = top + ph * ph * (98.0f - top);
      float x = f.cx[e] + side * f.w[e] * (0.18f + 0.12f * i);
      float r = (1.8f + 1.6f * smoothstepf(0.0f, 0.3f, ph)) * k;
      FaceRenderer::drawDrop(c, x, y, r, Paint::gradient(INK_ICE, TEAR, y - r * 2.0f, y + r));
    }
  }
}

void sweat(Canvas &c, const FaceFrame &f, float k) {
  float ph = fract(f.time * 0.55f);
  float x = f.cx[1] + f.w[1] * 0.5f + 8.0f;
  float y = f.cy[1] - f.h[1] * 0.5f - 2.0f + ph * 20.0f;
  float r = 3.6f * k * lifeScale(ph);
  FaceRenderer::keepOnGlass(x, y, 6.0f);
  FaceRenderer::drawDrop(c, x, y, r, Paint::gradient(INK_ICE, TEAR, y - r * 2.0f, y + r));
}

void anger(Canvas &c, const FaceFrame &f, float k) {
  float s = (1.0f + 0.18f * sinf(f.time * 9.0f)) * k;
  float cx = 103.0f, cy = 8.0f;
  float a = 2.0f * s, b = 8.0f * s, m = 5.0f * s;
  c.pathBegin();
  for (int q = 0; q < 4; q++) {
    float sx = (q & 1) ? 1.0f : -1.0f;
    float sy = (q & 2) ? 1.0f : -1.0f;
    // Each quadrant is a rounded corner pointing to the center.
    c.pathAddLine(cx + sx * b, cy + sy * a, cx + sx * m, cy + sy * a, 2.2f);
    c.pathAddLine(cx + sx * m, cy + sy * a, cx + sx * a, cy + sy * m, 2.2f);
    c.pathAddLine(cx + sx * a, cy + sy * m, cx + sx * a, cy + sy * b, 2.2f);
  }
  c.pathFill(INK_RED);
}

void hearts(Canvas &c, const FaceFrame &f, float k) {
  static const float baseX[4] = {15.0f, 64.0f, 113.0f, 64.0f};
  for (int i = 0; i < 4; i++) {
    float ph = fract(f.time * 0.42f + i * 0.27f);
    float x = baseX[i] + sinf(f.time * 2.2f + i * 1.9f) * 3.5f;
    float y = 104.0f - ph * 128.0f;
    float r = (2.8f + 3.0f * sinf(ph * 3.14159f)) * k;
    if (i == 1 || i == 3) r *= 0.7f;
    FaceRenderer::drawHeart(c, x, y, r, r, Paint::gradient(INK_ROSE, HEART, y - r, y + r * 0.4f));
  }
}

void sparkle(Canvas &c, const FaceFrame &f, float k) {
  static const float px[6] = {18.0f, 110.0f, 64.0f, 10.0f, 118.0f, 64.0f};
  static const float py[6] = {10.0f, 12.0f, -12.0f, 56.0f, 58.0f, 90.0f};
  for (int i = 0; i < 6; i++) {
    float tw = sinf(f.time * 3.4f + i * 1.7f);
    if (tw <= 0.0f) continue;
    FaceRenderer::drawStar4(c, px[i], py[i], 6.0f * tw * k, (i & 1) ? INK_WHITE : INK_YELLOW);
  }
}

void glyph(Canvas &c, const FaceFrame &f, float k, const char *s, float wob, Rgb color) {
  if (k < 0.35f) return;
  int scale = k > 0.8f ? 3 : 2;
  float w = Canvas::textWidth(s, scale);
  float x = 64.0f - w * 0.5f + sinf(f.time * 17.0f) * wob;
  float y = -19.0f + (3 - scale) * 3.0f + sinf(f.time * 3.2f) * 1.2f;
  c.text(x, y, s, scale, color);
}

void blush(Canvas &c, const FaceFrame &f, float k) {
  for (int e = 0; e < 2; e++) {
    float side = e == 0 ? -1.0f : 1.0f;
    float bx = f.cx[e] + side * f.w[e] * 0.25f;
    float by = f.cy[e] + f.h[e] * 0.5f + 7.0f;
    c.fillEllipse(bx, by, 9.0f * k, 4.4f * k, Paint(CHEEK, 0.30f));
    c.fillEllipse(bx, by, 6.6f * k, 3.0f * k, Paint(CHEEK, 0.45f));
    for (int i = -1; i <= 1; i++) {
      float x = bx + i * 4.0f;
      float len = 1.7f * k;
      c.thickLine(x + len, by - len, x - len, by + len, 1.0f, Paint(INK_ROSE, 0.9f));
    }
  }
}

void note(Canvas &c, float x, float y, float s, Rgb color) {
  c.pathBegin();
  c.pathAddEllipse(x, y, 2.4f * s, 1.8f * s);
  c.pathAddLine(x + 1.9f * s, y, x + 1.9f * s, y - 7.0f * s, 1.3f);
  c.pathAddLine(x + 1.9f * s, y - 7.0f * s, x + 5.2f * s, y - 5.2f * s, 1.7f);
  c.pathFill(color);
}

void notes(Canvas &c, const FaceFrame &f, float k) {
  static const Rgb COLOR[3] = {INK_MINT, INK_YELLOW, INK_PINK};
  for (int i = 0; i < 3; i++) {
    float ph = fract(f.time * 0.5f + i / 3.0f);
    float side = (i & 1) ? 1.0f : -1.0f;
    float x = 64.0f + side * (50.0f + sinf(ph * 8.0f + i) * 3.0f);
    float y = 48.0f - ph * 44.0f;
    float s = 1.25f * k * lifeScale(ph);
    if (s > 0.35f) note(c, x, y, s, COLOR[i]);
  }
}

void stars(Canvas &c, const FaceFrame &f, float k) {
  for (int i = 0; i < 3; i++) {
    float a = f.time * 3.6f + i * (M_TAU / 3.0f);
    float x = 64.0f + cosf(a) * 40.0f;
    float y = 1.0f + sinf(a) * 6.0f;
    // Stars on the far side of the orbit are smaller: cheap depth cue.
    float r = (3.6f + 1.8f * sinf(a)) * k;
    FaceRenderer::drawStar5(c, x, y, r, r, a * 0.7f, INK_YELLOW);
  }
}

void shiver(Canvas &c, const FaceFrame &f, float k) {
  float j = sinf(f.time * 40.0f) * 1.2f;
  for (int s = 0; s < 2; s++) {
    float x = s == 0 ? 9.0f : STAGE_W - 9.0f;
    for (int i = 0; i < 3; i++) {
      float y = 18.0f + i * 14.0f + j * (i == 1 ? -1.0f : 1.0f);
      float d = 3.0f * k * (s == 0 ? 1.0f : -1.0f);
      float pts[6] = {x - d, y - 3.0f, x + d, y, x - d, y + 3.0f};
      c.polyline(pts, 3, 1.4f, INK_ICE);
    }
  }
}

void steam(Canvas &c, const FaceFrame &f, float k) {
  // Two wisps on the left; the right side belongs to the sweat drop.
  for (int s = 0; s < 2; s++) {
    float bx = s == 0 ? 14.0f : 24.0f;
    float ph = fract(f.time * 0.6f + s * 0.5f);
    float top = 36.0f - ph * 28.0f;
    float len = 22.0f * k * lifeScale(ph);
    if (len < 1.0f) continue;
    float pts[13 * 2];
    for (int i = 0; i <= 12; i++) {
      float t = i / 12.0f;
      pts[i * 2] = bx + sinf(t * 11.0f + f.time * 4.0f) * 2.0f;
      pts[i * 2 + 1] = top + t * len;
    }
    c.polyline(pts, 13, 1.5f, Paint(STEAM, 0.75f));
  }
}

// Bright spots on the eyes: the "watery eyes" look.
void glint(Canvas &c, const FaceFrame &f, float k) {
  for (int e = 0; e < 2; e++) {
    float r = fminf(f.w[e], f.h[e]) * 0.17f * k;
    if (r < 1.2f) continue;
    float wob = sinf(f.time * 5.0f + e) * 0.6f;
    float x = f.cx[e] - f.w[e] * 0.17f + f.gazeX * 1.5f;
    float y = f.cy[e] - f.h[e] * 0.2f + wob;
    c.fillCircle(x, y, r, Paint(INK_WHITE, 0.95f));
    c.fillCircle(x + f.w[e] * 0.33f, y + f.h[e] * 0.36f, r * 0.5f, Paint(INK_WHITE, 0.85f));
  }
}

// "..." hopping one after the other
void dots(Canvas &c, const FaceFrame &f, float k) {
  for (int i = 0; i < 3; i++) {
    float ph = fract(f.time * 0.9f - i * 0.17f);
    float hop = ph < 0.45f ? sinf(ph / 0.45f * 3.14159265f) : 0.0f;
    float x = 24.0f + i * 10.0f;
    float y = 4.0f - hop * 4.0f;
    c.fillCircle(x, y, (2.9f + hop * 0.9f) * k, lighten(f.ink, 0.5f));
  }
}

// ")))" on both sides: it is hearing something
void waves(Canvas &c, const FaceFrame &f, float k) {
  for (int s = 0; s < 2; s++) {
    float ox = s == 0 ? 20.0f : STAGE_W - 20.0f;
    // the arcs open away from the face
    float mid = s == 0 ? 3.14159265f : 0.0f;
    for (int i = 0; i < 3; i++) {
      float ph = fract(f.time * 1.1f - i * 0.22f);
      if (ph > 0.75f) continue;
      float r = (5.0f + i * 5.0f) * k;
      c.arc(ox, 32.0f, r, mid - 0.95f, mid + 0.95f, 1.6f, Paint(INK_MINT, 1.0f - i * 0.22f));
    }
  }
}

void tongue(Canvas &c, const FaceFrame &f, float k) {
  if (f.mouthW < 4.0f) return;
  float x = f.mouthX + f.mouthW * 0.18f;
  float y = f.mouthY + 1.5f;
  float rx = 3.8f * k, ry = (5.2f + sinf(f.time * 5.0f) * 0.8f) * k;
  c.pathBegin();
  c.pathAddEllipse(x, y + ry * 0.55f, rx, ry);
  float box[8] = {x - rx, y - 1.0f, x + rx, y - 1.0f, x + rx, y + ry * 0.6f, x - rx, y + ry * 0.6f};
  c.pathAdd(box, 4);
  c.pathFill(Paint::gradient(TONGUE, 0xE8486F, y, y + ry * 1.5f));
  // the groove
  c.thickLine(x, y + 2.0f, x, y + ry * 1.0f, 0.9f, 0xB82A55);
}

// Zigzag mouth with two fangs.
void fangs(Canvas &c, const FaceFrame &f, float k) {
  float y = 53.0f + sinf(f.time * 30.0f) * 0.6f;
  float x0 = 64.0f - 18.0f * k + f.gazeX * 4.0f;
  float step = 6.0f * k;
  float pts[7 * 2];
  for (int i = 0; i <= 6; i++) {
    pts[i * 2] = x0 + i * step;
    pts[i * 2 + 1] = (i & 1) ? y + 5.0f * k : y;
  }
  c.polyline(pts, 7, 1.7f, f.ink);
  c.fillTriangle(x0 + step * 0.4f, y, x0 + step * 1.6f, y, x0 + step, y + 8.0f * k, INK_WHITE);
  c.fillTriangle(x0 + step * 4.4f, y, x0 + step * 5.6f, y, x0 + step * 5.0f, y + 8.0f * k, INK_WHITE);
}

}  // namespace

namespace Decor {

void draw(Canvas &c, uint32_t flag, const FaceFrame &f, float weight) {
  if (weight <= 0.02f) return;
  float k = weight > 1.0f ? 1.0f : weight;
  switch (flag) {
    case DC_ZZZ: zzz(c, f, k); break;
    case DC_TEARS: tears(c, f, k); break;
    case DC_SWEAT: sweat(c, f, k); break;
    case DC_ANGER: anger(c, f, k); break;
    case DC_HEARTS: hearts(c, f, k); break;
    case DC_SPARKLE: sparkle(c, f, k); break;
    case DC_QUESTION: glyph(c, f, k, "?", 0.0f, INK_YELLOW); break;
    case DC_EXCLAIM: glyph(c, f, k, "!", 1.2f, INK_ORANGE); break;
    case DC_BLUSH: blush(c, f, k); break;
    case DC_NOTES: notes(c, f, k); break;
    case DC_STARS: stars(c, f, k); break;
    case DC_SHIVER: shiver(c, f, k); break;
    case DC_STEAM: steam(c, f, k); break;
    case DC_GLINT: glint(c, f, k); break;
    case DC_DOTS: dots(c, f, k); break;
    case DC_WAVES: waves(c, f, k); break;
    case DC_TONGUE: tongue(c, f, k); break;
    case DC_FANGS: fangs(c, f, k); break;
    default: break;
  }
}

}  // namespace Decor
