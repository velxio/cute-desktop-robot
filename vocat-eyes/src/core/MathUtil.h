// Small math helpers shared by the engine: easing curves, smooth noise,
// a deterministic PRNG and a critically damped spring.
#ifndef CORE_MATHUTIL_H
#define CORE_MATHUTIL_H

#include <math.h>
#include <stdint.h>

#ifndef M_TAU
#define M_TAU 6.28318530717958647692f
#endif
#define DEG2RADF 0.01745329252f

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float clamp01(float v) { return clampf(v, 0.0f, 1.0f); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smoothstepf(float e0, float e1, float x) {
  float t = clamp01((x - e0) / (e1 - e0));
  return t * t * (3.0f - 2.0f * t);
}

// ---- Easing (t in 0..1) -----------------------------------------------------
enum Ease : uint8_t {
  EASE_LINEAR = 0,
  EASE_IN_OUT,    // smooth start and stop (default for calm changes)
  EASE_OUT,       // fast start, soft landing (reactions)
  EASE_OUT_BACK,  // overshoots a little and settles (lively, cartoon-like)
  EASE_OUT_ELASTIC,
  EASE_IN
};

static inline float ease(Ease e, float t) {
  t = clamp01(t);
  switch (e) {
    case EASE_IN_OUT:
      return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) * 0.5f;
    case EASE_OUT: {
      float u = 1.0f - t;
      return 1.0f - u * u * u;
    }
    case EASE_OUT_BACK: {
      const float c1 = 1.70158f, c3 = c1 + 1.0f;
      float u = t - 1.0f;
      return 1.0f + c3 * u * u * u + c1 * u * u;
    }
    case EASE_OUT_ELASTIC: {
      if (t <= 0.0f) return 0.0f;
      if (t >= 1.0f) return 1.0f;
      return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * (M_TAU / 3.0f)) + 1.0f;
    }
    case EASE_IN:
      return t * t * t;
    default:
      return t;
  }
}

// ---- Seamless 1D noise built from three sines (range about -1..1) -----------
static inline float loopNoise(float t, float period, float seed) {
  float p = (t / period) * M_TAU;
  return 0.55f * sinf(p + seed) + 0.30f * sinf(2.0f * p + seed * 1.7f + 1.1f) +
         0.15f * sinf(3.0f * p + seed * 2.3f + 2.4f);
}

// ---- xorshift32 PRNG --------------------------------------------------------
class Rng {
 public:
  explicit Rng(uint32_t seed = 0x1234ABCDu) : s_(seed ? seed : 0x1234ABCDu) {}
  void seed(uint32_t seed) { s_ = seed ? seed : 0x1234ABCDu; }
  uint32_t next() {
    uint32_t x = s_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return s_ = x;
  }
  // 0..1
  float unit() { return (float)(next() >> 8) * (1.0f / 16777216.0f); }
  float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
  // integer in [lo, hi)
  int rangeInt(int lo, int hi) { return hi > lo ? lo + (int)(next() % (uint32_t)(hi - lo)) : lo; }
  bool chance(float p) { return unit() < p; }

 private:
  uint32_t s_;
};

// ---- Critically damped spring (used for gaze and ears) ----------------------
struct Spring {
  float pos = 0.0f;
  float vel = 0.0f;
  // omega: angular frequency, higher = snappier
  void update(float target, float omega, float dt) {
    float x = omega * dt;
    float e = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    float d = pos - target;
    float tmp = (vel + omega * d) * dt;
    vel = (vel - omega * tmp) * e;
    pos = target + (d + tmp) * e;
  }
};

#endif
