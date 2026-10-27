// Acts made of body language and screen effects.
#include <string.h>

#include "ActKit.h"
#include "ActList.h"

// ---- Yes: nods -------------------------------------------------------------------
void actYesPose(ActFrame &f) {
  float k = envelope(f, 0.2f, 0.35f);
  float s = sinf((f.t - 0.15f) * M_TAU * 1.7f);
  f.dy[0] = f.dy[1] = 7.0f * s * k;
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].lidB += 0.14f * k;
    f.pose.eye[e].lidBBend += 0.2f * k;
    if (s > 0.0f) f.pose.eye[e].h *= 1.0f - 0.10f * k * s;
  }
  tint(f, INK_MINT, 0.7f * k);
  setEars(f, f.ears.left + 12.0f * k, f.ears.right + 12.0f * k, EG_WIGGLE, 14.0f * k, 1.7f);
  if (f.crossed(0.2f)) f.chirp = CH_HAPPY;
}
void actYesDraw(Canvas &, const ActFrame &, const FaceFrame &, uint8_t) {}

// ---- No: shakes the head -------------------------------------------------------------
void actNoPose(ActFrame &f) {
  float k = envelope(f, 0.2f, 0.35f);
  float s = sinf((f.t - 0.1f) * M_TAU * 2.0f);
  f.dx[0] = f.dx[1] = 10.0f * s * k;
  // the eye that leads the turn is seen narrower
  f.pose.eye[0].w *= 1.0f + 0.12f * s * k;
  f.pose.eye[1].w *= 1.0f - 0.12f * s * k;
  for (int e = 0; e < 2; e++) f.pose.eye[e].lidT += 0.12f * k;
  tint(f, INK_ORANGE, 0.7f * k);
  setEars(f, f.ears.left, f.ears.right, EG_FLAP, 16.0f * k, 2.0f);
  if (f.crossed(0.2f)) f.chirp = CH_SAD;
}
void actNoDraw(Canvas &, const ActFrame &, const FaceFrame &, uint8_t) {}

// ---- EyeRoll --------------------------------------------------------------------------
void actEyeRollPose(ActFrame &f) {
  float k = envelope(f, 0.3f, 0.35f);
  float u = inOut(seg(f.t, 0.35f, 1.75f));
  float gx = sinf(M_TAU * u), gy = (1.0f - cosf(M_TAU * u)) * 0.5f;
  f.gazeX = gx * 0.5f;
  f.gazeY = gy * 0.6f;
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    float inner = e == 0 ? 1.0f : -1.0f;
    p.pupil = 0.5f * k;
    p.pupilW = 1.0f;
    p.px = gx * inner * 0.6f;
    p.py = -gy * 0.9f;
    p.lidT -= 0.22f * k * gy;  // opens the eyes to look up
    p.h += 6.0f * k;
  }
  setEars(f, 42.0f, 42.0f);
}
void actEyeRollDraw(Canvas &, const ActFrame &, const FaceFrame &, uint8_t) {}

// ---- Peek: hides under the screen and looks over the edge -----------------------------
static const float PEEK_HIDDEN = 92.0f, PEEK_EDGE = 66.0f;

void actPeekPose(ActFrame &f) {
  float t = f.t, y;
  if (t < 0.5f) {
    float a = t / 0.5f;
    y = a < 0.3f ? -5.0f * easeOut(a / 0.3f) : lerpf(-5.0f, PEEK_HIDDEN, easeIn((a - 0.3f) / 0.7f));
  } else if (t < 1.2f) {
    y = PEEK_HIDDEN;
  } else if (t < 1.9f) {
    y = lerpf(PEEK_HIDDEN, PEEK_EDGE, inOut(seg(t, 1.2f, 1.9f)));
  } else if (t < 3.2f) {
    y = PEEK_EDGE;
    f.gazeX = -0.85f * bump(t, 1.95f, 2.55f) + 0.85f * bump(t, 2.6f, 3.2f);
  } else {
    y = lerpf(PEEK_EDGE, 0.0f, outBack(seg(t, 3.2f, 3.7f)));
  }
  f.dy[0] = f.dy[1] = y;
  // closer together, so both fit in the bottom of the round glass
  float low = clamp01(y / PEEK_EDGE);
  for (int e = 0; e < 2; e++) f.pose.eye[e].x -= 5.0f * low;
  blinkAt(f, 4.1f);
  if (t < 1.2f) setEars(f, 10.0f, 10.0f);
  else if (t < 3.2f) setEars(f, 70.0f, 70.0f, EG_TWITCH, 12.0f, 2.0f);
  else setEars(f, 105.0f, 105.0f);
  if (f.crossed(3.2f)) f.chirp = CH_HELLO;
}
void actPeekDraw(Canvas &, const ActFrame &, const FaceFrame &, uint8_t) {}

// ---- Dash: runs out of the screen and comes back from the other side -------------------
static float dashX(float t) {
  if (t < 0.0f) return 0.0f;
  if (t < 0.35f) return -8.0f * inOut(t / 0.35f);
  if (t < 0.8f) return -8.0f + easeIn((t - 0.35f) / 0.45f) * 170.0f;
  if (t < 1.5f) return -170.0f * (1.0f - outBack((t - 0.8f) / 0.7f));
  return 0.0f;
}

void actDashPose(ActFrame &f) {
  float x = dashX(f.t);
  float v = (x - dashX(f.t - 0.02f)) / 0.02f;
  if (f.t >= 0.8f && f.t < 0.82f) v = 900.0f;  // the jump between the two edges is not speed
  float stretch = clamp01(fabsf(v) / 900.0f);
  f.dx[0] = f.dx[1] = x;
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].w *= 1.0f + 0.30f * stretch;
    f.pose.eye[e].h *= 1.0f - 0.18f * stretch;
  }
  f.gazeX = 0.7f * seg(f.t, 0.05f, 0.3f) * (1.0f - seg(f.t, 1.2f, 1.5f));
  if (f.t < 1.3f) setEars(f, 12.0f, 12.0f);
  else setEars(f, 110.0f, 110.0f, EG_WIGGLE, 10.0f, 3.0f);
  blinkAt(f, 1.85f);
  if (f.crossed(0.35f)) f.chirp = CH_WHOOSH;
}

void actDashDraw(Canvas &c, const ActFrame &f, const FaceFrame &face, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float v = (dashX(f.t) - dashX(f.t - 0.02f)) / 0.02f;
  if (f.t >= 0.8f && f.t < 0.82f) v = 900.0f;
  if (fabsf(v) < 150.0f) return;
  float len = fminf(46.0f, fabsf(v) * 0.05f);
  static const float OFF[3] = {-10.0f, 0.0f, 10.0f};
  static const float LEN[3] = {0.7f, 1.0f, 0.55f};
  for (int e = 0; e < 2; e++) {
    float edge = face.cx[e] - face.w[e] * 0.5f - 4.0f;  // it always runs to the right
    for (int i = 0; i < 3; i++) {
      float y = face.cy[e] + OFF[i];
      c.thickLine(edge, y, edge - len * LEN[i], y, 1.6f, Paint(lighten(face.ink, 0.4f), 0.75f));
    }
  }
}

// ---- Exclaim: the eyes join into a "!" -------------------------------------------------
void actExclaimPose(ActFrame &f) {
  float in = outBack(seg(f.t, 0.0f, 0.32f));
  float out = inOut(seg(f.t, f.duration - 0.4f, f.duration));
  float k = in * (1.0f - out);
  EyeParams bar = {-24.0f, -9.0f, 11.0f, 34.0f, 3.5f, 3.5f, 0, 0, 0, 0, 0, 0, 0, 1.0f, 0, 0};
  EyeParams dot = dotEye(-24.0f, 21.0f, 11.0f);
  mixEye(f.pose.eye[0], bar, k);
  mixEye(f.pose.eye[1], dot, k);
  f.pose.mouth.w *= 1.0f - clamp01(k);
  float shake = bump(f.t, 0.32f, 0.95f);
  f.dx[0] = f.dx[1] = sinf(f.t * 44.0f) * 1.6f * shake;
  tint(f, INK_AMBER, k);
  setEars(f, 92.0f, 92.0f, EG_TREMBLE, 5.0f * shake, 12.0f);
  if (f.crossed(0.2f)) f.chirp = CH_SURPRISE;
}
void actExclaimDraw(Canvas &, const ActFrame &, const FaceFrame &, uint8_t) {}

// ---- Thinking: the eyes become "..." ---------------------------------------------------
static float thinkPulse(float t, int i) {
  float p = fractf((t - 0.35f) * 0.95f - i * 0.17f);
  return p < 0.5f ? sinf(p * 2.0f * ACT_PI) : 0.0f;
}

void actThinkingPose(ActFrame &f) {
  float k = envelope(f, 0.35f, 0.4f);
  for (int e = 0; e < 2; e++) {
    float pulse = thinkPulse(f.t, e == 0 ? 0 : 2) * k;
    mixEye(f.pose.eye[e], dotEye(0.0f, 0.0f, 10.0f * (1.0f + 0.3f * pulse)), k);
    f.dy[e] = -5.0f * pulse;
  }
  setEars(f, 106.0f, 60.0f, EG_TWITCH, 10.0f, 0.9f);
  if (f.crossed(0.1f)) f.chirp = CH_QUESTION;
}

void actThinkingDraw(Canvas &c, const ActFrame &f, const FaceFrame &face, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float k = envelope(f, 0.35f, 0.4f);
  float pulse = thinkPulse(f.t, 1) * k;
  float r = 5.0f * k * (1.0f + 0.3f * pulse), y = 32.0f - 5.0f * pulse;
  c.fillCircle(64.0f, y, r + 2.0f, Paint(face.ink, 0.18f));
  c.fillCircle(64.0f, y, r, Paint::gradient(lighten(face.ink, 0.34f), face.ink, y - r, y + r));
}

// ---- Read: follows lines of text that appear as it reads -----------------------------
static const float READ_T0 = 0.45f, READ_LINE = 1.2f, READ_SWEEP = 0.95f;

void actReadPose(ActFrame &f) {
  float k = envelope(f, 0.35f, 0.4f);
  int line = (int)floorf((f.t - READ_T0) / READ_LINE);
  if (line < 0) line = 0;
  if (line > 2) line = 2;
  float u = seg(f.t, READ_T0 + line * READ_LINE, READ_T0 + line * READ_LINE + READ_SWEEP);
  f.gazeX = lerpf(-0.8f, 0.8f, u) * k;
  f.gazeY = (-0.45f - 0.17f * line) * k;
  for (int e = 0; e < 2; e++) f.pose.eye[e].y -= 9.0f * k;
  setEars(f, 118.0f, 118.0f);
}

void actReadDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float k = envelope(f, 0.35f, 0.4f);
  if (k < 0.5f) return;
  for (int row = 0; row < 3; row++) {
    float u = seg(f.t, READ_T0 + row * READ_LINE, READ_T0 + row * READ_LINE + READ_SWEEP);
    float y = 60.0f + row * 7.0f;
    // the glass is round: lower lines are shorter
    float half = FaceRenderer::glassHalfWidth(y) - 12.0f;
    float left = 64.0f - half, right = 64.0f + half;
    float limit = left + (right - left) * u;
    float x = left;
    for (int n = 0; x < right - 2.0f; n++) {
      float len = 5.0f + hash01(row * 31 + n) * 15.0f;
      float end = fminf(x + len, right);
      if (x < limit) c.thickLine(x, y, fminf(end, limit), y, 1.7f, Paint(INK_ICE, 0.9f));
      x = end + 4.5f;
    }
  }
}

// ---- Sneeze ---------------------------------------------------------------------------
void actSneezePose(ActFrame &f) {
  float t = f.t;
  float level = (inOut(seg(t, 0.2f, 0.45f)) + inOut(seg(t, 0.7f, 0.95f)) + inOut(seg(t, 1.2f, 1.45f))) / 3.0f;
  MouthParams m = {0.0f, 0.0f, 0.0f, 22.0f, 0.0f};
  if (t < 1.6f) {
    for (int e = 0; e < 2; e++) {
      f.pose.eye[e].lidT += 0.45f * level;
      f.pose.eye[e].lidB += 0.10f * level;
    }
    f.dy[0] = f.dy[1] = -7.0f * level;
    f.dx[0] = f.dx[1] = sinf(t * 55.0f) * 1.2f * level;
    m.w = 12.0f * level;
    m.open = 0.7f;
    setEars(f, lerpf(80.0f, 12.0f, level), lerpf(80.0f, 12.0f, level));
  } else if (t < 2.1f) {
    for (int e = 0; e < 2; e++) {
      f.kind[e] = EK_CHEVRON;
      f.pose.eye[e].w = 30.0f;
      f.pose.eye[e].h = 22.0f;
    }
    float slam = lerpf(-7.0f, 9.0f, easeOut(seg(t, 1.6f, 1.68f)));
    f.dy[0] = f.dy[1] = slam * (1.0f - inOut(seg(t, 1.75f, 2.1f)));
    f.dx[0] = f.dx[1] = sinf(t * 70.0f) * 2.0f * (1.0f - seg(t, 1.6f, 2.0f));
    m.w = 18.0f;
    m.open = 1.0f;
    setEars(f, 140.0f, 140.0f);
  } else {
    float dazed = 1.0f - seg(t, 2.3f, 3.0f);
    for (int e = 0; e < 2; e++) f.pose.eye[e].lidT += 0.3f * dazed;
    m.w = 18.0f * (1.0f - seg(t, 2.1f, 2.4f));
    m.open = 1.0f - seg(t, 2.1f, 2.4f);
    blinkAt(f, 2.65f);
    blinkAt(f, 3.0f);
    setEars(f, 80.0f, 80.0f);
  }
  blinkBoth(f, bump(t, 2.0f, 2.25f));  // hides the change of eye
  f.pose.mouth = m;
  tint(f, INK_ROSE, level * (1.0f - seg(t, 1.6f, 2.4f)));
  if (f.crossed(1.6f)) f.chirp = CH_SNEEZE;
}

void actSneezeDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float u = f.t - 1.62f;
  const float life = 0.5f;
  if (u < 0.0f || u > life) return;
  for (int i = 0; i < 14; i++) {
    bool left = (i & 1) != 0;
    float spread = (hash01(i * 7) - 0.5f) * 1.3f;
    float a = (left ? ACT_PI : 0.0f) + spread;
    float speed = 70.0f + hash01(i * 13) * 70.0f;
    float d = speed * u * (1.0f - 0.6f * u / life);
    float co, si;
    Canvas::unit(a, co, si);
    c.fillCircle(64.0f + co * (10.0f + d), 56.0f + si * d * 0.6f, 1.6f * (1.0f - u / life) + 0.6f,
                 Paint(INK_SKY, 1.0f - u / life));
  }
}

// ---- screen effects --------------------------------------------------------------------
// Old TV tube. phase 0 = off, up to 0.35 a line grows from a dot, up to 1 the
// picture opens from the line.
static void crt(Canvas &c, float phase) {
  if (phase >= 1.0f) return;
  const int mid = SCREEN_H / 2;
  if (phase < 0.35f) {
    c.fillRectPx(0, 0, SCREEN_W, SCREEN_H, INK_BLACK);
    if (phase <= 0.0f) return;
    float w = lerpf(3.0f, 120.0f, easeOut(phase / 0.35f));
    c.fillRectPx((int)(120.0f - w), mid - 3, (int)(w * 2.0f), 6, INK_CYAN, 0.35f);
    c.fillRectPx((int)(120.0f - w), mid - 1, (int)(w * 2.0f), 3, INK_WHITE);
    return;
  }
  float s = lerpf(0.04f, 1.0f, outBack((phase - 0.35f) / 0.65f));
  c.squashPx(s);
  // the beam is brighter than the picture while it opens
  if (s < 0.5f) {
    c.fillRectPx(0, mid - 3, SCREEN_W, 6, INK_CYAN, 0.35f * (1.0f - s * 2.0f));
    c.fillRectPx(0, mid - 1, SCREEN_W, 3, INK_WHITE, 1.0f - s * 2.0f);
  }
}

// ---- Glitch ---------------------------------------------------------------------------
void actGlitchPose(ActFrame &f) {
  float g = envelope(f, 0.15f, 0.3f);
  int fi = (int)(f.t * 20.0f);
  f.dx[0] = f.dx[1] = (hash01(fi * 3 + 1) - 0.5f) * 7.0f * g;
  if (hash01(fi * 5 + 2) < 0.15f * g) blinkBoth(f, 1.0f);
  // the colors break too
  float pick = hash01(fi * 41 + 3);
  if (pick < 0.22f * g) f.ink = 0xFF30C0;
  else if (pick < 0.40f * g) f.ink = 0x40FF60;
  setEars(f, f.ears.left, f.ears.right, EG_TREMBLE, 8.0f * g, 14.0f);
  if (f.crossed(0.15f) || f.crossed(0.8f)) f.chirp = CH_BLIP;
}

void actGlitchDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_POST) return;
  float g = envelope(f, 0.15f, 0.3f);
  if (g < 0.05f) return;
  int fi = (int)(f.t * 20.0f);
  int bands = 2 + (int)(hash01(fi * 7 + 1) * 4.0f * g);
  for (int b = 0; b < bands; b++) {
    int y0 = 40 + (int)(hash01(fi * 13 + b * 5) * 150.0f);
    int h = 4 + (int)(hash01(fi * 17 + b * 3) * 20.0f);
    int shift = (int)((hash01(fi * 19 + b * 11) - 0.5f) * 84.0f * g);
    c.shiftRowsPx(y0, y0 + h - 1, shift);
  }
  static const Rgb LINE[3] = {0x00F0FF, 0xFF30C0, 0xFFFFFF};
  for (int i = 0; i < 3; i++) {
    if (hash01(fi * 29 + i) < 0.5f * g) {
      int y = 20 + (int)(hash01(fi * 31 + i) * 200.0f);
      c.tintRowsPx(y, y + 1, LINE[i], 0.8f);
    }
  }
  if (hash01(fi * 23) < 0.10f * g) c.invertRowsPx(0, SCREEN_H - 1);
}

// ---- PowerOn ---------------------------------------------------------------------------
void actPowerOnPose(ActFrame &f) {
  float up = seg(f.t, 0.45f, 1.3f);
  setEars(f, lerpf(0.0f, f.ears.left, up), lerpf(0.0f, f.ears.right, up));
  blinkAt(f, 1.35f);
  if (f.crossed(0.12f)) f.chirp = CH_POWER_ON;
}

void actPowerOnDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_POST) return;
  crt(c, seg(f.t, 0.12f, 1.0f));
}

// ---- Reboot ----------------------------------------------------------------------------
void actRebootPose(ActFrame &f) {
  float down = seg(f.t, 0.0f, 0.5f), up = seg(f.t, 1.6f, 2.3f);
  float k = 1.0f - down + up;
  setEars(f, f.ears.left * clamp01(k), f.ears.right * clamp01(k));
  blinkAt(f, 2.45f);
  if (f.crossed(0.05f)) f.chirp = CH_POWER_OFF;
  if (f.crossed(1.5f)) f.chirp = CH_POWER_ON;
}

void actRebootDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_POST) return;
  float t = f.t;
  if (t < 0.7f) {
    crt(c, 1.0f - seg(t, 0.0f, 0.7f));
  } else if (t < 1.5f) {
    c.fillRectPx(0, 0, SCREEN_W, SCREEN_H, INK_BLACK);
    c.text(34.0f, 28.0f, ">", 2, 0x40FF70);
    if (fractf(t * 2.5f) < 0.5f) c.fillRect(48.0f, 28.0f, 9.0f, 14.0f, 0x40FF70);  // cursor
  } else {
    crt(c, seg(t, 1.5f, 2.3f));
  }
}

// ---- Scanner ---------------------------------------------------------------------------
static float scanX(float t) {
  if (t < 1.2f) return lerpf(-8.0f, 136.0f, inOut(seg(t, 0.2f, 1.2f)));
  return lerpf(136.0f, -8.0f, inOut(seg(t, 1.2f, 2.2f)));
}

void actScannerPose(ActFrame &f) {
  float k = envelope(f, 0.2f, 0.25f);
  f.gazeX = clampf((scanX(f.t) - 64.0f) / 64.0f, -1.0f, 1.0f) * k;
  for (int e = 0; e < 2; e++) f.pose.eye[e].lidT += 0.16f * k;
  setEars(f, 120.0f, 120.0f);
  if (f.crossed(0.2f) || f.crossed(1.2f)) f.chirp = CH_BLIP;
}

void actScannerDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_POST) return;
  int x = (int)Canvas::toPxX(scanX(f.t));
  int dir = f.t < 1.2f ? 1 : -1;
  const Rgb beam = 0x40FF90;
  // afterglow, weaker the further from the beam
  for (int i = 12; i >= 1; i--) {
    int gx = x - dir * (i * 4 + 2);
    c.fillRectPx(dir > 0 ? gx - 3 : gx, 0, 4, SCREEN_H, beam, 0.34f * (1.0f - i / 13.0f));
  }
  c.fillRectPx(x - 3, 0, 6, SCREEN_H, beam, 0.55f);
  c.fillRectPx(x - 1, 0, 2, SCREEN_H, INK_WHITE, 0.9f);
}

// ---- Matrix: "Wake up..." and the code rains over the face --------------------------------
namespace {
// 5x5 glyphs, top row first
#define GLYPH(a, b, c, d, e) \
  (((uint32_t)(a) << 20) | ((uint32_t)(b) << 15) | ((uint32_t)(c) << 10) | ((uint32_t)(d) << 5) | (uint32_t)(e))
const uint32_t GLYPHS[16] = {
  GLYPH(0b11111, 0b00001, 0b00110, 0b00100, 0b01000),
  GLYPH(0b00010, 0b00100, 0b01100, 0b10100, 0b00100),
  GLYPH(0b00100, 0b11111, 0b10001, 0b00010, 0b01100),
  GLYPH(0b00000, 0b11111, 0b00100, 0b00100, 0b11111),
  GLYPH(0b00010, 0b11111, 0b00110, 0b01010, 0b10010),
  GLYPH(0b01000, 0b11111, 0b01001, 0b01001, 0b10011),
  GLYPH(0b00100, 0b11111, 0b00100, 0b11111, 0b00100),
  GLYPH(0b01111, 0b01001, 0b10001, 0b00010, 0b01100),
  GLYPH(0b11111, 0b00001, 0b00001, 0b00001, 0b11111),
  GLYPH(0b00000, 0b01010, 0b01010, 0b10001, 0b10001),
  GLYPH(0b00000, 0b01110, 0b00000, 0b00000, 0b11111),
  GLYPH(0b01110, 0b10011, 0b10101, 0b11001, 0b01110),
  GLYPH(0b00100, 0b01100, 0b00100, 0b00100, 0b01110),
  GLYPH(0b10010, 0b10010, 0b10010, 0b00010, 0b01100),
  GLYPH(0b11111, 0b10001, 0b10001, 0b10001, 0b11111),
  GLYPH(0b11111, 0b10001, 0b00001, 0b00010, 0b01100),
};

const float RAIN_T0 = 1.3f;
const char *const WAKE = "WAKE UP...";
const Rgb CODE = 0x30E060;
const Rgb CODE_HEAD = 0xD8FFE0;
// rain, in pixels
const int CELL_W = 15, CELL_H = 14, DOT = 2, COLUMNS = SCREEN_W / CELL_W;
}  // namespace

void actMatrixPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.3f, 0.5f);
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].lidT += 0.22f * k;
    f.pose.eye[e].lidB += 0.12f * k;
  }
  // reads the message, then looks around inside the code
  float reading = seg(t, 0.15f, 1.2f) * (1.0f - seg(t, 1.2f, 1.5f));
  float scan = seg(t, RAIN_T0 + 0.2f, 2.2f) * (1.0f - seg(t, f.duration - 1.0f, f.duration - 0.5f));
  f.gazeX = lerpf(0.75f * sinf(t * 1.3f) * scan, lerpf(-0.6f, 0.6f, seg(t, 0.15f, 1.2f)), reading);
  f.gazeY = 0.9f * reading;
  int fi = (int)(t * 12.0f);
  if (scan > 0.5f && hash01(fi * 7) < 0.08f) f.dx[0] = f.dx[1] = (hash01(fi * 3) - 0.5f) * 7.0f;
  blinkAt(f, RAIN_T0 + 0.05f);
  tint(f, 0x50FF80, seg(t, RAIN_T0, RAIN_T0 + 0.5f) * (1.0f - seg(t, f.duration - 1.0f, f.duration - 0.4f)));
  setEars(f, 120.0f, 120.0f, EG_TREMBLE, 3.0f * scan, 10.0f);
  for (int i = 0; i < 10; i += 2) {
    if (f.crossed(0.15f + i * 0.11f)) f.chirp = CH_BLIP;
  }
  if (f.crossed(RAIN_T0)) f.chirp = CH_POWER_ON;
}

void actMatrixDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  float t = f.t;
  if (layer == ACT_FRONT) {
    if (t < 0.1f || t > RAIN_T0 + 0.25f) return;
    int n = (int)((t - 0.15f) / 0.11f);
    if (n > 10) n = 10;
    if (n < 0) n = 0;
    char part[12];
    memcpy(part, WAKE, (size_t)n);
    part[n] = 0;
    float x0 = 64.0f - Canvas::textWidth(WAKE, 1) * 0.5f, y0 = -8.0f;
    float x = c.text(x0, y0, part, 1, CODE);
    if (fractf(t * 3.0f) < 0.6f) c.fillRect(x, y0, 5.0f, Canvas::textHeight(1), CODE);  // cursor
    return;
  }
  if (layer != ACT_POST) return;

  float amount = seg(t, RAIN_T0, 2.3f) * (1.0f - seg(t, f.duration - 1.3f, f.duration - 0.6f));
  if (amount <= 0.0f) return;
  for (int col = 0; col < COLUMNS; col++) {
    if (hash01(col * 11 + 1) > amount * 1.15f) continue;  // the columns come and go one by one
    float speed = 110.0f + hash01(col * 3) * 130.0f;      // pixels per second
    int len = 6 + (int)(hash01(col * 5) * 8.0f);
    float period = SCREEN_H + len * CELL_H + 60.0f + hash01(col * 7) * 200.0f;
    float head = fmodf((t - RAIN_T0) * speed + hash01(col * 13) * period, period) - CELL_H;
    int headRow = (int)floorf(head / CELL_H);
    for (int j = 0; j <= len; j++) {
      int row = headRow - j;
      int y = row * CELL_H;
      if (y < -CELL_H || y >= SCREEN_H) continue;
      // the glyphs stay in place and change now and then; the light is what falls
      int shuffle = (int)(t * 5.0f + hash01(col * 17 + row * 29) * 5.0f);
      int g = (int)(hash01(col * 31 + row * 17 + shuffle * 7) * 16.0f);
      float fade = 1.0f - (float)j / (float)(len + 1);
      // a dark plate behind, so the code reads over the eyes too
      c.fillRectPx(col * CELL_W, y, CELL_W, CELL_H, INK_BLACK, 0.55f * fade);
      c.glyph5Px(col * CELL_W + 3, y + 2, GLYPHS[g & 15], DOT, j == 0 ? CODE_HEAD : CODE, j == 0 ? 1.0f : 0.25f + 0.75f * fade);
    }
  }
}
