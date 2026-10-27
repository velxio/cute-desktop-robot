// Small toolbox for writing acts.
#ifndef FACE_ACTKIT_H
#define FACE_ACTKIT_H

#include "../core/MathUtil.h"
#include "Acts.h"
#include "FaceMath.h"

#define ACT_PI 3.14159265f

// The round glass has room below the stage: things that stand on the floor
// (a pot, a mug, a rocket...) stand on this row.
#define GROUND 84.0f
// Rows where the glass ends at the middle of the screen.
#define GLASS_TOP (DISC_CY - DISC_R)
#define GLASS_BOTTOM (DISC_CY + DISC_R)

// 0 before `a`, 1 after `b`, a straight ramp in between.
static inline float seg(float t, float a, float b) { return clamp01((t - a) / (b - a)); }
static inline float inOut(float x) { return ease(EASE_IN_OUT, x); }
static inline float outBack(float x) { return ease(EASE_OUT_BACK, x); }
static inline float easeOut(float x) { return ease(EASE_OUT, x); }
static inline float easeIn(float x) { return ease(EASE_IN, x); }
static inline float fractf(float v) { return v - floorf(v); }

// Rises during `in` seconds, stays at 1 and falls during the last `out` seconds.
static inline float envelope(const ActFrame &f, float in, float out) {
  return inOut(seg(f.t, 0.0f, in)) * (1.0f - inOut(seg(f.t, f.duration - out, f.duration)));
}

// 0 -> 1 -> 0 between a and b.
static inline float bump(float t, float a, float b) {
  float x = seg(t, a, b);
  return sinf(x * ACT_PI);
}

// Repeatable pseudo random number in 0..1 for an integer.
static inline float hash01(int n) {
  uint32_t x = (uint32_t)n * 374761393u + 668265263u;
  x = (x ^ (x >> 13)) * 1274126177u;
  x ^= x >> 16;
  return (float)(x & 0xFFFFFF) * (1.0f / 16777216.0f);
}

static inline void mixEye(EyeParams &e, const EyeParams &target, float k) { e = lerpEye(e, target, k); }

// Round dot of `size` pixels, the usual target when the eyes turn into things.
static inline EyeParams dotEye(float x, float y, float size) {
  EyeParams e = {x, y, size, size, 2.0f, 2.0f, 0, 0, 0, 0, 0, 0, 0, 1.0f, 0, 0};
  return e;
}

static inline void blinkBoth(ActFrame &f, float amount) {
  if (amount > f.blink[0]) f.blink[0] = amount;
  if (amount > f.blink[1]) f.blink[1] = amount;
}

// A quick blink centered on the moment `at`.
static inline void blinkAt(ActFrame &f, float at) { blinkBoth(f, bump(f.t, at - 0.09f, at + 0.11f)); }

// Four corners, given in order around the shape.
static inline void quad(Canvas &c, float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy,
                        const Paint &p) {
  float xy[8] = {ax, ay, bx, by, cx, cy, dx, dy};
  c.fillPoly(xy, 4, p);
}

// Blends the color of the face towards `to`.
static inline void tint(ActFrame &f, Rgb to, float k) { f.ink = mixRgb(f.ink, to, clamp01(k)); }

static inline void setEars(ActFrame &f, float l, float r, uint8_t gesture = EG_STILL, float amp = 0.0f,
                           float hz = 0.0f) {
  f.ears.left = l;
  f.ears.right = r;
  f.ears.gesture = gesture;
  f.ears.amp = amp;
  f.ears.hz = hz;
}

#endif
