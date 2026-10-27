// Blending of poses, shared by the engine and the acts.
#ifndef FACE_FACEMATH_H
#define FACE_FACEMATH_H

#include "../core/MathUtil.h"
#include "FaceTypes.h"

static inline EyeParams lerpEye(const EyeParams &a, const EyeParams &b, float t) {
  EyeParams r;
  r.x = lerpf(a.x, b.x, t);
  r.y = lerpf(a.y, b.y, t);
  r.w = lerpf(a.w, b.w, t);
  r.h = lerpf(a.h, b.h, t);
  r.nTop = lerpf(a.nTop, b.nTop, t);
  r.nBot = lerpf(a.nBot, b.nBot, t);
  r.lidT = lerpf(a.lidT, b.lidT, t);
  r.lidTAng = lerpf(a.lidTAng, b.lidTAng, t);
  r.lidTBend = lerpf(a.lidTBend, b.lidTBend, t);
  r.lidB = lerpf(a.lidB, b.lidB, t);
  r.lidBAng = lerpf(a.lidBAng, b.lidBAng, t);
  r.lidBBend = lerpf(a.lidBBend, b.lidBBend, t);
  r.pupil = lerpf(a.pupil, b.pupil, t);
  // An eye without pupil has no opinion about its shape or place.
  bool pa = a.pupil > 0.001f, pb = b.pupil > 0.001f;
  float wa = a.pupilW > 0.001f ? a.pupilW : 1.0f;
  float wb = b.pupilW > 0.001f ? b.pupilW : 1.0f;
  if (pa && !pb) {
    r.pupilW = wa, r.px = a.px, r.py = a.py;
  } else if (pb && !pa) {
    r.pupilW = wb, r.px = b.px, r.py = b.py;
  } else {
    r.pupilW = lerpf(wa, wb, t);
    r.px = lerpf(a.px, b.px, t);
    r.py = lerpf(a.py, b.py, t);
  }
  return r;
}

// A mouth that appears or disappears grows in place instead of flying in.
static inline MouthParams lerpMouth(const MouthParams &a, const MouthParams &b, float t) {
  bool ha = a.w >= 2.0f, hb = b.w >= 2.0f;
  MouthParams m = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  if (!ha && !hb) return m;
  if (!ha) {
    m = b;
    m.w = b.w * t;
    m.open = b.open * t;
    return m;
  }
  if (!hb) {
    m = a;
    m.w = a.w * (1.0f - t);
    m.open = a.open * (1.0f - t);
    return m;
  }
  m.w = lerpf(a.w, b.w, t);
  m.curve = lerpf(a.curve, b.curve, t);
  m.open = lerpf(a.open, b.open, t);
  m.y = lerpf(a.y, b.y, t);
  m.tilt = lerpf(a.tilt, b.tilt, t);
  return m;
}

#endif
