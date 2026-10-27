// Acts where the face plays with an object drawn next to it.
#include <stdio.h>
#include <string.h>

#include "ActKit.h"
#include "ActList.h"

namespace {

const Rgb GOLD = 0xFFC83C;
const Rgb MOON = 0xE8E4C8;
const Rgb BUBBLE = 0x9CE4FF;
const Rgb CLOUD = 0xC8D2E0;
const Rgb LEAF = 0x58D060;
const Rgb POT = 0xD06A3C;
const Rgb TEA = 0xD89A58;
const Rgb PEARL = 0x5A3220;

// Point of a tilted ellipse around the middle of the screen. `z` tells if it
// is on the near side (> 0) or behind the face.
struct OrbitPoint {
  float x, y, z;
};

OrbitPoint orbitAt(float theta, float rx, float ry, float tilt, float cx = 64.0f, float cy = 32.0f) {
  float c, s, ct, st;
  Canvas::unit(theta, c, s);
  Canvas::unit(tilt, ct, st);
  OrbitPoint p;
  p.x = cx + rx * c * ct - ry * s * st;
  p.y = cy + rx * c * st + ry * s * ct;
  p.z = s;
  return p;
}

void drawBubble(Canvas &c, float x, float y, float r) {
  if (r < 1.2f) return;
  c.fillCircle(x, y, r, Paint(BUBBLE, 0.16f));
  c.ring(x, y, r, 1.2f, BUBBLE);
  if (r > 3.5f) c.thickLine(x - r * 0.45f, y - r * 0.15f, x - r * 0.15f, y - r * 0.45f, 1.2f, INK_WHITE);
}

void drawPop(Canvas &c, float x, float y, float u, float size) {
  // short rays flying away from the middle
  c.pathBegin();
  for (int i = 0; i < 8; i++) {
    float co, si;
    Canvas::unit(i * (M_TAU / 8.0f) + 0.3f, co, si);
    float r0 = size * (0.6f + u * 1.2f), r1 = r0 + size * 0.5f * (1.0f - u);
    c.pathAddLine(x + co * r0, y + si * r0, x + co * r1, y + si * r1, 1.3f);
  }
  c.pathFill(Paint(INK_WHITE, 1.0f - u * 0.6f));
}

void drawCloud(Canvas &c, float x, float y, float s, const Paint &p) {
  float box[8] = {x - 9.0f * s, y + 2.0f * s, x + 11.0f * s, y + 2.0f * s,
                  x + 11.0f * s, y + 8.0f * s, x - 9.0f * s, y + 8.0f * s};
  c.pathBegin();
  c.pathAddEllipse(x - 9.0f * s, y + 2.0f * s, 6.0f * s, 6.0f * s);
  c.pathAddEllipse(x, y - 2.0f * s, 8.5f * s, 8.5f * s);
  c.pathAddEllipse(x + 10.0f * s, y + 1.5f * s, 7.0f * s, 7.0f * s);
  c.pathAdd(box, 4);
  c.pathFill(p);
}

}  // namespace

// ---- Notify: a bell rings on the corner --------------------------------------------
void actNotifyPose(ActFrame &f) {
  float look = inOut(seg(f.t, 0.2f, 0.45f)) * (1.0f - inOut(seg(f.t, 2.3f, 2.7f)));
  f.gazeX = 0.85f * look;
  f.gazeY = 0.55f * look;
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].h *= 1.0f + 0.10f * look;
    f.pose.eye[e].w *= 1.0f + 0.04f * look;
    f.dx[e] = -13.0f * look;  // makes room for the bell
    f.dy[e] = 8.0f * look;
  }
  setEars(f, lerpf(f.ears.left, 118.0f, look), lerpf(f.ears.right, 118.0f, look), EG_TWITCH, 10.0f, 2.0f);
  if (f.crossed(0.2f)) f.chirp = CH_DING;
}

void actNotifyDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  float s = outBack(seg(t, 0.15f, 0.5f)) * (1.0f - inOut(seg(t, 2.2f, 2.5f)));
  if (s < 0.05f) return;
  float swing = t > 0.5f ? sinf((t - 0.5f) * 24.0f) * 0.38f * (1.0f - seg(t, 0.5f, 1.8f)) : 0.0f;
  const float ox = 98.0f, oy = -8.0f;  // the bell hangs from here
  c.fillCircle(ox, oy + 12.0f * s, 17.0f * s, INK_BLACK);
  float ca, sa;
  Canvas::unit(swing, ca, sa);
  auto X = [&](float u, float v) { return ox + s * (u * ca - v * sa); };
  auto Y = [&](float u, float v) { return oy + s * (u * sa + v * ca); };

  c.fillCircle(X(0, 19), Y(0, 19), 2.2f * s, INK_ORANGE);  // clapper
  float body[8] = {X(-5.6f, 8), Y(-5.6f, 8), X(5.6f, 8), Y(5.6f, 8), X(8, 15), Y(8, 15), X(-8, 15), Y(-8, 15)};
  c.pathBegin();
  c.pathAddEllipse(X(0, 1), Y(0, 1), 1.6f * s, 1.6f * s);  // handle
  c.pathAddEllipse(X(0, 8), Y(0, 8), 5.6f * s, 5.6f * s);  // dome
  c.pathAdd(body, 4);
  c.pathAddLine(X(-9, 15.5f), Y(-9, 15.5f), X(9, 15.5f), Y(9, 15.5f), 2.0f * s);  // rim
  c.pathFill(Paint::gradient(INK_YELLOW, INK_AMBER, oy, oy + 17.0f * s));
  // the red dot of the things that wait
  c.fillCircle(X(7, 2), Y(7, 2), 3.2f * s, INK_RED);

  // sound
  float ring = 1.0f - seg(t, 0.5f, 1.8f);
  if (t > 0.5f && ring > 0.05f && fractf(t * 5.0f) < 0.6f) {
    for (int side = -1; side <= 1; side += 2) {
      float bx = ox + side * 13.0f;
      float pts[6] = {bx, oy + 6.0f, bx + side * 3.0f, oy + 9.0f, bx, oy + 12.0f};
      c.polyline(pts, 3, 1.3f, INK_WHITE);
    }
  }
}

// ---- Orbit: a little moon goes around the face ------------------------------------
static float orbitTheta(float t) { return -ACT_PI * 0.5f + M_TAU * 2.0f * inOut(seg(t, 0.3f, 3.2f)); }
static float orbitShow(float t) { return seg(t, 0.2f, 0.5f) * (1.0f - seg(t, 3.1f, 3.4f)); }

void actOrbitPose(ActFrame &f) {
  float th = orbitTheta(f.t), k = orbitShow(f.t);
  f.gazeX = cosf(th) * 0.9f * k;
  f.gazeY = -sinf(th) * 0.55f * k;
  f.halo = true;
  setEars(f, 85.0f + 25.0f * cosf(th) * k, 85.0f - 25.0f * cosf(th) * k);
  if (f.crossed(0.3f)) f.chirp = CH_WHOOSH;
}

void actOrbitDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer == ACT_POST) return;
  float k = orbitShow(f.t);
  if (k < 0.05f) return;
  float th = orbitTheta(f.t);
  for (int i = 6; i >= 0; i--) {
    OrbitPoint p = orbitAt(th - i * 0.13f, 56.0f, 19.0f, -0.2f);
    float r = 4.0f * k * (1.0f - i / 7.5f);
    bool front = p.z > 0.0f;
    if (front != (layer == ACT_FRONT)) continue;
    Paint moon(i == 0 ? MOON : INK_YELLOW, i == 0 ? 1.0f : 0.8f - i * 0.1f);
    if (front && i == 0) FaceRenderer::drawBall(c, p.x, p.y, r, r, moon);
    else c.fillCircle(p.x, p.y, r, moon);
    if (i == 0 && r > 2.5f) c.fillCircle(p.x - r * 0.3f, p.y + r * 0.2f, r * 0.28f, Paint(0xA8A488, 0.9f));  // crater
  }
}

// ---- Swirl: ribbons spin around the face ------------------------------------------
void actSwirlPose(ActFrame &f) {
  float k = envelope(f, 0.25f, 0.4f);
  f.halo = true;
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].lidB += 0.10f * k;
    f.pose.eye[e].lidBBend += 0.15f * k;
  }
  setEars(f, f.ears.left, f.ears.right, EG_WIGGLE, 16.0f * k, 3.0f);
  if (f.crossed(0.1f)) f.chirp = CH_WHOOSH;
}

void actSwirlDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer == ACT_POST) return;
  float k = envelope(f, 0.25f, 0.4f);
  if (k < 0.05f) return;
  static const float TILT[3] = {-0.42f, 0.18f, 0.6f};
  static const float RY[3] = {14.0f, 22.0f, 30.0f};
  static const Rgb COLOR[3] = {INK_PINK, INK_YELLOW, INK_MINT};
  for (int j = 0; j < 3; j++) {
    float head = j * 2.1f + M_TAU * 1.5f * f.t;
    float sweep = 0.55f * M_TAU * k;
    const int n = 18;
    OrbitPoint prev = orbitAt(head, 56.0f, RY[j], TILT[j]);
    for (int i = 1; i <= n; i++) {
      float a = head - sweep * i / n;
      OrbitPoint p = orbitAt(a, 56.0f, RY[j], TILT[j]);
      bool front = p.z > 0.0f;
      if (front == (layer == ACT_FRONT)) {
        float th = 2.6f * (1.0f - (float)i / n) + 0.8f;
        if (front) c.thickLine(prev.x, prev.y, p.x, p.y, th + 2.4f, INK_BLACK);
        c.thickLine(prev.x, prev.y, p.x, p.y, th, COLOR[j]);
      }
      prev = p;
    }
  }
}

// ---- Burst: explodes and comes back -------------------------------------------------
void actBurstPose(ActFrame &f) {
  float t = f.t;
  if (t < 0.35f) {
    float k = inOut(t / 0.35f);
    for (int e = 0; e < 2; e++) {
      f.pose.eye[e].x = lerpf(f.pose.eye[e].x, -24.0f, k);
      f.pose.eye[e].w = lerpf(f.pose.eye[e].w, 3.0f, k);
      f.pose.eye[e].h = lerpf(f.pose.eye[e].h, 3.0f, k);
    }
    tint(f, INK_WHITE, k);
    setEars(f, 40.0f, 40.0f);
  } else if (t < 1.15f) {
    f.eyeOn[0] = f.eyeOn[1] = false;
    setEars(f, 140.0f, 140.0f, EG_TREMBLE, 8.0f, 12.0f);
  } else {
    float k = outBack(seg(t, 1.15f, 1.6f));
    for (int e = 0; e < 2; e++) {
      f.pose.eye[e].w *= k;
      f.pose.eye[e].h *= k;
    }
    setEars(f, 95.0f, 95.0f);
  }
  f.pose.mouth.w = 0.0f;
  blinkAt(f, 2.0f);
  blinkAt(f, 2.3f);
  if (f.crossed(0.35f)) f.chirp = CH_POP;
}

void actBurstDraw(Canvas &c, const ActFrame &f, const FaceFrame &face, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  static const Rgb SPARK[4] = {INK_YELLOW, INK_ORANGE, INK_WHITE, INK_PINK};
  // shock wave
  float w = seg(t, 0.35f, 0.6f);
  if (w > 0.0f && w < 1.0f)
    c.ring(64.0f, 32.0f, 4.0f + 40.0f * easeOut(w), 2.4f * (1.0f - w) + 0.6f, Paint(INK_WHITE, 1.0f - w * 0.7f));
  // sparks flying out
  for (int i = 0; i < 14; i++) {
    float u = t - (0.35f + hash01(i * 3) * 0.10f);
    const float life = 0.75f;
    if (u < 0.0f || u > life) continue;
    float co, si;
    Canvas::unit(i * (M_TAU / 14.0f) + hash01(i * 5) * 0.5f + u * 1.6f, co, si);
    float d = 60.0f * (1.0f - powf(0.78f, u * 12.0f));
    c.fillCircle(64.0f + co * d, 32.0f + si * d * 0.8f, 2.8f * (1.0f - u / life) + 0.6f, SPARK[i & 3]);
  }
  // ... and gathering again where the eyes will be
  float g = seg(t, 0.9f, 1.2f);
  if (g > 0.0f && g < 1.0f) {
    for (int e = 0; e < 2; e++) {
      float ex = e == 0 ? EYE_ANCHOR_LX : EYE_ANCHOR_RX;
      for (int i = 0; i < 7; i++) {
        float co, si;
        Canvas::unit(i * (M_TAU / 7.0f) + g * 3.0f + e, co, si);
        float d = 30.0f * (1.0f - easeIn(g));
        c.fillCircle(ex + co * d, EYE_ANCHOR_Y + si * d * 0.7f, 1.0f + g * 1.2f, mixRgb(INK_WHITE, face.ink, g));
      }
    }
  }
}

// ---- Heartbeat: a heart grows between the eyes ----------------------------------------
static float heartSize(float t) {
  float grow = outBack(seg(t, 0.3f, 0.7f));
  float beat = bump(t, 0.95f, 1.15f) + 0.7f * bump(t, 1.2f, 1.4f) + bump(t, 1.85f, 2.05f) + 0.7f * bump(t, 2.1f, 2.3f);
  float leave = 1.0f - seg(t, 2.7f, 3.3f) * 0.6f;
  return 10.0f * grow * (1.0f + 0.22f * beat) * leave;
}
static float heartY(float t) { return lerpf(36.0f, -34.0f, easeIn(seg(t, 2.6f, 3.3f))); }

void actHeartbeatPose(ActFrame &f) {
  float t = f.t;
  float there = inOut(seg(t, 0.3f, 0.7f)) * (1.0f - inOut(seg(t, 2.7f, 3.2f)));
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].x += 7.0f * there;  // make room
    f.pose.eye[e].w *= 1.0f - 0.12f * there;
  }
  f.gazeY = -0.25f * there + 0.8f * bump(t, 2.7f, 3.5f);
  f.halo = true;
  tint(f, INK_PINK, there);
  float beat = bump(t, 0.95f, 1.15f) + bump(t, 1.85f, 2.05f);
  setEars(f, 100.0f + 14.0f * beat, 100.0f + 14.0f * beat);
  if (f.crossed(0.95f)) f.chirp = CH_LOVE;
}

void actHeartbeatDraw(Canvas &c, const ActFrame &f, const FaceFrame &face, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float r = heartSize(f.t);
  if (r < 0.8f) return;
  float x = 64.0f + face.gazeX * 4.0f, y = heartY(f.t);
  FaceRenderer::drawHeart(c, x, y, r + 1.8f, r + 1.8f, INK_BLACK);
  FaceRenderer::drawHeart(c, x, y, r + 5.0f, r + 5.0f, Paint(0xFF3B6B, 0.16f));
  FaceRenderer::drawHeart(c, x, y, r, r, Paint::gradient(0xFF7A9C, 0xF0204E, y - r, y + r * 0.6f));
  c.fillEllipse(x - r * 0.42f, y - r * 0.42f, r * 0.2f, r * 0.14f, Paint(INK_WHITE, 0.8f));
}

// ---- Rain: a cloud stops over the face ------------------------------------------------
static float cloudX(float t) {
  if (t < 1.2f) return lerpf(-34.0f, 64.0f, easeOut(seg(t, 0.0f, 1.2f)));
  return lerpf(64.0f, 166.0f, easeIn(seg(t, 4.0f, 5.0f)));
}
static const float CLOUD_Y = -4.0f;

void actRainPose(ActFrame &f) {
  float t = f.t;
  float wet = inOut(seg(t, 1.3f, 1.7f)) * (1.0f - inOut(seg(t, 3.9f, 4.3f)));
  f.gazeX = clampf((cloudX(t) - 64.0f) / 64.0f, -1.0f, 1.0f) * 0.8f;
  f.gazeY = 0.75f * (1.0f - wet) * envelope(f, 0.3f, 0.5f);
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].lidT += 0.30f * wet;
    f.pose.eye[e].y += 6.0f * wet;
  }
  // shakes the water off
  float shake = bump(t, 4.3f, 4.9f);
  f.dx[0] = f.dx[1] = sinf(t * 46.0f) * 3.0f * shake;
  f.halo = true;
  setEars(f, lerpf(28.0f, 8.0f, wet), lerpf(28.0f, 8.0f, wet), shake > 0.1f ? EG_FLAP : EG_STILL, 30.0f * shake, 8.0f);
  if (f.crossed(1.3f)) f.chirp = CH_SAD;
}

void actRainDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t, x = cloudX(t);
  if (t > 1.2f && t < 4.0f) {
    float k = seg(t, 1.2f, 1.5f) * (1.0f - seg(t, 3.7f, 4.0f));
    for (int i = 0; i < 9; i++) {
      float ph = fractf(t * 1.5f + hash01(i * 3) * 1.0f);
      if (hash01(i * 11 + (int)(t * 1.5f + hash01(i * 3))) > k) continue;
      float dx = x - 18.0f + i * 4.5f;
      float y = CLOUD_Y + 8.0f + ph * 92.0f;
      c.thickLine(dx, y, dx - 1.5f, y + 5.5f, 1.5f, INK_SKY);
    }
  }
  drawCloud(c, x, CLOUD_Y, 1.15f, Paint::gradient(INK_WHITE, CLOUD, CLOUD_Y - 10.0f, CLOUD_Y + 9.0f));
}

// ---- ShootingStar ------------------------------------------------------------------
static float starU(float t) { return seg(t, 0.5f, 1.5f); }
static void starAt(float u, float &x, float &y) {
  x = lerpf(126.0f, 0.0f, u);
  y = lerpf(-14.0f, 20.0f, u * u);
}

void actShootingStarPose(ActFrame &f) {
  float t = f.t, u = starU(t);
  float watching = inOut(seg(t, 0.3f, 0.6f)) * (1.0f - inOut(seg(t, 2.6f, 3.1f)));
  f.gazeX = lerpf(0.9f, -0.9f, inOut(u)) * watching;
  f.gazeY = lerpf(0.9f, 0.35f, u) * watching;
  float awe = inOut(seg(t, 1.3f, 1.7f)) * (1.0f - inOut(seg(t, 2.6f, 3.1f)));
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].h *= 1.0f + 0.14f * awe;
    f.pose.eye[e].w *= 1.0f + 0.06f * awe;
    f.pose.eye[e].y += 5.0f * watching;
  }
  f.halo = true;
  setEars(f, 95.0f + 20.0f * watching, 95.0f + 20.0f * watching);
  if (f.crossed(0.5f)) f.chirp = CH_WHOOSH;
  if (f.crossed(1.6f)) f.chirp = CH_SURPRISE;
}

void actShootingStarDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t, u = starU(t);
  if (u > 0.0f && u < 1.0f) {
    float x, y;
    for (int i = 16; i >= 1; i--) {
      float v = u - i * 0.018f;
      if (v < 0.0f) continue;
      starAt(v, x, y);
      c.fillCircle(x, y, 2.4f - i * 0.13f, Paint(mixRgb(INK_YELLOW, INK_ORANGE, i / 16.0f), 1.0f - i / 17.0f));
    }
    starAt(u, x, y);
    FaceRenderer::drawStar4(c, x, y, 8.0f, INK_WHITE);
    c.fillCircle(x, y, 2.4f, INK_YELLOW);
  }
  // the sky keeps twinkling for a moment
  float tw = seg(t, 1.4f, 1.7f) * (1.0f - seg(t, 2.5f, 2.9f));
  if (tw > 0.05f) {
    static const float SX[6] = {20.0f, 108.0f, 62.0f, 14.0f, 114.0f, 80.0f};
    static const float SY[6] = {8.0f, 6.0f, -14.0f, 60.0f, 62.0f, 92.0f};
    for (int i = 0; i < 6; i++) {
      float s = sinf(t * 7.0f + i * 1.9f);
      if (s > 0.0f) FaceRenderer::drawStar4(c, SX[i], SY[i], 5.0f * s * tw, (i & 1) ? INK_WHITE : INK_YELLOW);
    }
  }
}

// ---- Fireworks -----------------------------------------------------------------------
static const float FW_X[3] = {26.0f, 102.0f, 64.0f};
static const float FW_Y[3] = {6.0f, 4.0f, -6.0f};
static const float FW_T[3] = {0.3f, 1.3f, 2.4f};
static const Rgb FW_COLOR[3] = {INK_PINK, INK_CYAN, INK_YELLOW};
static const float FW_RISE = 0.6f, FW_LIFE = 1.0f;

void actFireworksPose(ActFrame &f) {
  float t = f.t, gx = 0.0f, gy = 0.0f;
  for (int i = 0; i < 3; i++) {
    float w = bump(t, FW_T[i], FW_T[i] + FW_RISE + FW_LIFE);
    gx += (FW_X[i] - 64.0f) / 48.0f * w;
    gy += 0.9f * w;
    if (f.crossed(FW_T[i])) f.chirp = CH_WHOOSH;
    if (f.crossed(FW_T[i] + FW_RISE)) f.chirp = CH_POP;
  }
  f.gazeX = clampf(gx, -1.0f, 1.0f);
  f.gazeY = clampf(gy, 0.0f, 1.0f);
  float k = envelope(f, 0.3f, 0.5f);
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].w *= 1.0f - 0.14f * k;
    f.pose.eye[e].h *= 1.0f - 0.10f * k;
    f.pose.eye[e].y += 12.0f * k;
  }
  f.pose.mouth.y += 10.0f * k;
  f.halo = true;
  setEars(f, 110.0f, 110.0f, EG_WIGGLE, 14.0f * k, 2.5f);
}

void actFireworksDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t;
  for (int i = 0; i < 3; i++) {
    float u = t - FW_T[i];
    if (u < 0.0f) continue;
    if (u < FW_RISE) {
      float r = easeOut(u / FW_RISE);
      float y = lerpf(GLASS_BOTTOM, FW_Y[i], r);
      c.thickLine(FW_X[i], y + 2.0f, FW_X[i], y + 2.0f + 12.0f * (1.0f - r), 1.2f, Paint(INK_ORANGE, 0.8f));
      c.fillCircle(FW_X[i], y, 1.8f, INK_WHITE);
      continue;
    }
    float v = (u - FW_RISE) / FW_LIFE;
    if (v > 1.0f) continue;
    float d = 30.0f * easeOut(v);
    for (int j = 0; j < 14; j++) {
      float co, si;
      Canvas::unit(j * (M_TAU / 14.0f) + i, co, si);
      float x = FW_X[i] + co * d;
      float y = FW_Y[i] + si * d + 14.0f * v * v;  // gravity
      if (v > 0.6f && ((j + (int)(t * 20.0f)) & 1)) continue;  // they flicker as they die
      Rgb color = mixRgb(INK_WHITE, FW_COLOR[i], clamp01(v * 3.0f));
      c.fillCircle(x, y, 2.4f * (1.0f - v) + 0.6f, color);
      if (v < 0.5f) c.fillCircle(FW_X[i] + co * d * 0.55f, FW_Y[i] + si * d * 0.55f, 1.2f, Paint(FW_COLOR[i], 0.8f));
    }
  }
}

// ---- Snore: the bubble of the sleeper ----------------------------------------------------
static float snoreR(float t) {
  if (t < 1.2f) return 6.0f * inOut(seg(t, 0.2f, 1.2f));
  if (t < 2.0f) return lerpf(6.0f, 3.0f, inOut(seg(t, 1.2f, 2.0f)));
  if (t < 3.4f) return lerpf(3.0f, 12.0f, inOut(seg(t, 2.0f, 3.4f)));
  return 0.0f;
}

void actSnorePose(ActFrame &f) {
  float t = f.t;
  // wakes up with the pop, half way, and falls asleep again
  float startle = outBack(seg(t, 3.4f, 3.55f)) * (1.0f - inOut(seg(t, 4.0f, 4.8f)));
  EyeParams open = {0.0f, 0.0f, 34.0f, 24.0f, 4.0f, 4.0f, 0.30f, -4.0f, 0, 0, 0, 0, 0, 1.0f, 0, 0};
  for (int e = 0; e < 2; e++) mixEye(f.pose.eye[e], open, clamp01(startle));
  float breath = (snoreR(t) - 3.0f) / 8.0f;
  f.dy[0] = f.dy[1] = -1.5f * breath;
  f.gazeX = 0.5f * bump(t, 3.6f, 4.3f);
  setEars(f, 10.0f + 50.0f * startle, 10.0f + 50.0f * startle);
  if (f.crossed(3.4f)) f.chirp = CH_POP;
}

void actSnoreDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t, r = snoreR(t);
  const float nx = 60.0f, ny = 50.0f;
  if (r > 1.0f) drawBubble(c, nx + r * 0.7f, ny + r * 0.7f, r);
  float u = seg(t, 3.4f, 3.75f);
  if (u > 0.0f && u < 1.0f) drawPop(c, nx + 8.4f, ny + 8.4f, u, 10.0f);
}

// ---- Loading: watches a progress bar ----------------------------------------------------
static float loadProgress(float t) {
  // stutters like the real ones
  float u = seg(t, 0.4f, 2.7f);
  return clamp01(0.55f * easeOut(seg(u, 0.0f, 0.35f)) + 0.2f * inOut(seg(u, 0.45f, 0.6f)) +
                 0.25f * inOut(seg(u, 0.8f, 1.0f)));
}

void actLoadingPose(ActFrame &f) {
  float t = f.t;
  float k = envelope(f, 0.35f, 0.4f);
  float p = loadProgress(t);
  float done = inOut(seg(t, 2.7f, 2.95f));
  f.gazeX = lerpf(-0.8f, 0.8f, p) * k * (1.0f - done);
  f.gazeY = -0.75f * k * (1.0f - done);
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].y -= 9.0f * k;
    f.pose.eye[e].h *= 1.0f - 0.12f * k;
    f.pose.eye[e].lidB += 0.35f * done * k;
    f.pose.eye[e].lidBBend += 0.35f * done * k;
  }
  tint(f, INK_GREEN, done * k);
  setEars(f, lerpf(90.0f, 120.0f, p), lerpf(90.0f, 120.0f, p), done > 0.5f ? EG_WIGGLE : EG_STILL, 14.0f, 3.0f);
  if (f.crossed(2.7f)) f.chirp = CH_DING;
}

void actLoadingDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float k = envelope(f, 0.35f, 0.4f);
  if (k < 0.3f) return;
  float p = loadProgress(f.t);
  float w = 92.0f * k, x0 = 64.0f - w * 0.5f, y0 = 58.0f;
  bool done = f.t > 2.7f;
  bool flash = done && f.t < 3.1f && fractf(f.t * 8.0f) < 0.5f;
  c.fillRoundRect(x0, y0, w, 9.0f, 4.5f, flash ? INK_WHITE : Rgb(0x2A3444));
  c.fillRoundRect(x0 + 1.5f, y0 + 1.5f, w - 3.0f, 6.0f, 3.0f, INK_BLACK);
  float fill = (w - 3.0f) * p;
  if (fill > 3.0f)
    c.fillRoundRect(x0 + 1.5f, y0 + 1.5f, fill, 6.0f, 3.0f,
                    Paint::gradient(done ? INK_LIME : INK_SKY, done ? INK_GREEN : INK_BLUE, y0 + 1.0f, y0 + 8.0f));
  char txt[8];
  snprintf(txt, sizeof(txt), "%d%%", (int)(p * 100.0f + 0.5f));
  c.textCentered(64.0f, y0 + 14.0f, txt, 1, done ? INK_GREEN : INK_ICE);
}

// ---- Bubbles -------------------------------------------------------------------------
static const int BUBBLES = 8, POPPED = 3;
static const float POP_T = 2.8f;

static void bubbleAt(int i, float t, float &x, float &y, float &r) {
  float speed = 26.0f + hash01(i * 3) * 22.0f;
  float start = hash01(i * 7) * 1.6f;
  r = 3.4f + hash01(i * 5) * 5.0f;
  x = 14.0f + i * 14.0f + sinf(t * (1.4f + hash01(i) * 1.5f) + i) * 5.0f;
  y = 112.0f - (t - start) * speed;
}

void actBubblesPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  float x, y, r;
  bubbleAt(POPPED, t < POP_T ? t : POP_T, x, y, r);
  f.gazeX = clampf((x - 64.0f) / 60.0f, -1.0f, 1.0f) * k;
  f.gazeY = clampf((32.0f - y) / 30.0f, -1.0f, 1.0f) * k;
  f.halo = true;
  blinkBoth(f, bump(t, POP_T, POP_T + 0.3f));
  float jump = bump(t, POP_T, POP_T + 0.4f);
  f.dy[0] = f.dy[1] = -3.0f * jump;
  setEars(f, 95.0f + 30.0f * jump, 95.0f + 30.0f * jump, EG_WIGGLE, 8.0f * k, 1.2f);
  if (f.crossed(POP_T)) f.chirp = CH_POP;
}

void actBubblesDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t;
  for (int i = 0; i < BUBBLES; i++) {
    float x, y, r;
    if (i == POPPED && t >= POP_T) {
      bubbleAt(i, POP_T, x, y, r);
      float u = seg(t, POP_T, POP_T + 0.35f);
      if (u < 1.0f) drawPop(c, x, y, u, r);
      continue;
    }
    bubbleAt(i, t, x, y, r);
    if (y < GLASS_TOP - 10.0f || y > GLASS_BOTTOM + 10.0f) continue;
    drawBubble(c, x, y, r);
  }
}

// ---- Say: a speech balloon with a short text ------------------------------------------
namespace {
// The glass is round, a long text goes in two lines. Returns the line count.
int splitText(const char *txt, char *a, char *b, size_t size, int &scale) {
  size_t n = strlen(txt);
  scale = n <= 7 ? 2 : 1;
  a[0] = b[0] = 0;
  if (n <= 12) {
    snprintf(a, size, "%s", txt);
    return 1;
  }
  // break at the blank closest to the middle
  size_t best = n / 2;
  bool found = false;
  for (size_t d = 0; d < n / 2 && !found; d++) {
    if (txt[n / 2 + d] == ' ') best = n / 2 + d, found = true;
    else if (txt[n / 2 - d] == ' ') best = n / 2 - d, found = true;
  }
  snprintf(a, size, "%.*s", (int)best, txt);
  snprintf(b, size, "%s", txt + best + (found ? 1 : 0));
  return 2;
}
}  // namespace

void actSayPose(ActFrame &f) {
  float k = envelope(f, 0.3f, 0.35f);
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].y += 14.0f * k;
    f.pose.eye[e].h *= 1.0f - 0.20f * k;
    f.pose.eye[e].w *= 1.0f - 0.08f * k;
    f.pose.eye[e].lidB += 0.20f * k;
    f.pose.eye[e].lidBBend += 0.25f * k;
  }
  // talks
  int n = (int)strlen(Acts::text());
  float typing = seg(f.t, 0.4f, 0.4f + n * 0.09f);
  bool talking = typing > 0.0f && typing < 1.0f;
  f.pose.mouth.w = 12.0f * k;
  f.pose.mouth.y = 38.0f;
  f.pose.mouth.curve = 2.5f;
  f.pose.mouth.tilt = 0.0f;
  f.pose.mouth.open = talking ? 0.25f + 0.6f * fabsf(sinf(f.t * 17.0f)) : 0.0f;
  if (talking) f.dy[0] = f.dy[1] = -1.5f * fabsf(sinf(f.t * 17.0f));
  setEars(f, 100.0f, 100.0f, EG_WIGGLE, 10.0f * k, 2.0f);
  if (f.crossed(0.35f)) f.chirp = CH_HELLO;
}

void actSayDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float k = outBack(seg(f.t, 0.15f, 0.45f)) * (1.0f - inOut(seg(f.t, f.duration - 0.35f, f.duration)));
  if (k < 0.1f) return;
  char a[24], b[24];
  int scale;
  int lines = splitText(Acts::text(), a, b, sizeof(a), scale);
  float tw = fmaxf(Canvas::textWidth(a, scale), Canvas::textWidth(b, scale));
  float th = Canvas::textHeight(scale), gap = 2.5f;
  float bw = (tw + 12.0f) * clamp01(k), bh = (th * lines + gap * (lines - 1) + 8.0f) * fminf(k, 1.06f);
  if (bw < 8.0f || bh < 6.0f) return;
  float bottom = 22.0f, y0 = bottom - bh, x0 = 64.0f - bw * 0.5f;
  // balloon
  c.pathBegin();
  float tail[6] = {58.0f, bottom - 1.0f, 69.0f, bottom - 1.0f, 60.0f, bottom + 7.0f};
  c.pathAdd(tail, 3);
  c.pathFill(INK_WHITE);
  c.fillRoundRect(x0, y0, bw, bh, 5.0f, INK_WHITE);
  if (k < 0.9f) return;
  // letters, one by one
  int shown = (int)((f.t - 0.4f) / 0.09f);
  if (shown <= 0) return;
  int la = (int)strlen(a), lb = (int)strlen(b);
  char part[24];
  int na = shown < la ? shown : la;
  snprintf(part, sizeof(part), "%.*s", na, a);
  c.text(64.0f - Canvas::textWidth(a, scale) * 0.5f, y0 + 4.0f, part, scale, Rgb(0x14202C));
  if (lines > 1 && shown > la) {
    int nb = shown - la < lb ? shown - la : lb;
    snprintf(part, sizeof(part), "%.*s", nb, b);
    c.text(64.0f - Canvas::textWidth(b, scale) * 0.5f, y0 + 4.0f + th + gap, part, scale, Rgb(0x14202C));
  }
}

// ---- Fly: follows a fly until it lands on its nose -------------------------------------
static void flyAt(float t, float &x, float &y) {
  const float land = 3.2f, sit = 3.6f, leave = 4.6f;
  float wx = 64.0f + 48.0f * sinf(2.1f * t + 1.0f) + 6.0f * sinf(9.0f * t);
  float wy = 30.0f + 26.0f * sinf(3.3f * t) + 4.0f * sinf(11.0f * t + 2.0f);
  if (t < 0.4f) {
    // comes in from the top right corner
    float a = easeOut(t / 0.4f);
    x = lerpf(120.0f, wx, a);
    y = lerpf(-20.0f, wy, a);
  } else if (t < land) {
    x = wx, y = wy;
  } else if (t < sit) {
    float a = inOut((t - land) / (sit - land));
    x = lerpf(wx, 64.0f, a);
    y = lerpf(wy, 33.0f, a);
  } else if (t < leave) {
    x = 64.0f, y = 33.0f;
  } else {
    float a = easeIn(seg(t, leave, leave + 0.5f));
    x = lerpf(64.0f, 124.0f, a);
    y = lerpf(33.0f, -24.0f, a);
  }
}

void actFlyPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.35f, 0.5f);
  float x, y;
  flyAt(t, x, y);
  float sitting = inOut(seg(t, 3.3f, 3.7f)) * (1.0f - inOut(seg(t, 4.55f, 4.75f)));
  float gx = clampf((x - 64.0f) / 56.0f, -1.0f, 1.0f), gy = clampf((32.0f - y) / 26.0f, -1.0f, 1.0f);
  // the body of the eye follows a little, the pupil does the rest
  f.gazeX = gx * 0.35f * k * (1.0f - sitting);
  f.gazeY = gy * 0.35f * k * (1.0f - sitting);
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    float inner = e == 0 ? 1.0f : -1.0f;
    p.pupil = 0.44f * k;
    p.pupilW = 1.0f;
    // px is measured towards the nose, the fly is measured on the screen
    p.px = lerpf(gx * inner * 0.9f, 0.95f, sitting);
    p.py = lerpf(-gy * 0.9f, 0.15f, sitting);
    p.x -= 3.0f * sitting;
  }
  // shakes it off
  float shake = bump(t, 4.5f, 5.0f);
  f.dx[0] = f.dx[1] = sinf(t * 50.0f) * 4.0f * shake;
  blinkBoth(f, bump(t, 4.75f, 5.05f));
  setEars(f, 90.0f + 30.0f * gx * k, 90.0f - 30.0f * gx * k, shake > 0.1f ? EG_FLAP : EG_TWITCH, 20.0f, 6.0f);
  if (f.crossed(3.6f)) f.chirp = CH_QUESTION;
}

void actFlyDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  if (f.t > 5.1f) return;
  float x, y;
  flyAt(f.t, x, y);
  bool sitting = f.t > 3.6f && f.t < 4.6f;
  c.fillEllipse(x, y, 4.6f, 4.0f, INK_BLACK);
  // wings: a blur while it flies, folded when it sits
  bool up = sitting || ((int)(f.t * 40.0f) & 1);
  float wy = up ? -3.6f : -1.2f;
  c.fillEllipse(x - 3.0f, y + wy * 0.6f - 1.0f, 3.0f, 1.6f, Paint(INK_ICE, 0.8f));
  c.fillEllipse(x + 3.0f, y + wy * 0.6f - 1.0f, 3.0f, 1.6f, Paint(INK_ICE, 0.8f));
  c.fillEllipse(x, y, 2.8f, 2.2f, Rgb(0x8A93A6));
  c.fillCircle(x - 1.2f, y - 0.4f, 0.9f, INK_RED);
  c.fillCircle(x + 1.2f, y - 0.4f, 0.9f, INK_RED);
}

// ---- Ball: watches a ball bounce across ------------------------------------------------
static const float BALL_T0 = 0.3f, BALL_T1 = 4.0f;
static const float BALL_FLOOR = 82.0f;

static void ballAt(float t, float &x, float &y, float &squash) {
  float u = seg(t, BALL_T0, BALL_T1);
  x = lerpf(-8.0f, 136.0f, u);
  float b = u * 4.0f;
  int n = (int)floorf(b);
  if (n > 3) n = 3;
  float fr = b - n;
  float h = 64.0f * powf(0.72f, (float)n);
  float up = 4.0f * fr * (1.0f - fr);
  y = BALL_FLOOR - 6.0f - h * up;
  squash = clamp01(1.0f - up * 6.0f);  // only near the floor
}

void actBallPose(ActFrame &f) {
  float k = envelope(f, 0.3f, 0.45f);
  float x, y, sq;
  ballAt(f.t, x, y, sq);
  f.gazeX = clampf((x - 64.0f) / 60.0f, -1.0f, 1.0f) * k;
  f.gazeY = clampf((32.0f - y) / 28.0f, -1.0f, 1.0f) * k;
  for (int e = 0; e < 2; e++) f.pose.eye[e].y -= 5.0f * k;
  f.halo = true;
  setEars(f, 95.0f + f.gazeX * 25.0f, 95.0f - f.gazeX * 25.0f);
  float u = seg(f.t, BALL_T0, BALL_T1) * 4.0f, up = seg(f.tPrev, BALL_T0, BALL_T1) * 4.0f;
  if (floorf(u) != floorf(up) && u < 4.0f) f.chirp = CH_BLIP;
}

void actBallDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  if (f.t < BALL_T0 || f.t > BALL_T1) return;
  float x, y, sq;
  ballAt(f.t, x, y, sq);
  float rx = 5.4f * (1.0f + 0.35f * sq), ry = 5.4f * (1.0f - 0.35f * sq);
  float cy = y + 5.4f - ry;
  // floor shadow
  float high = clamp01((BALL_FLOOR - 6.0f - y) / 50.0f);
  c.fillEllipse(x, BALL_FLOOR + 1.0f, (5.0f + 2.0f * sq) * (1.0f - 0.4f * high), 1.3f, Paint(INK_GRAY, 0.7f - 0.4f * high));
  FaceRenderer::drawBall(c, x, cy, rx, ry, Paint::gradient(0xFF8070, 0xE02838, cy - ry, cy + ry));
  c.fillCircle(x - rx * 0.35f, cy - ry * 0.4f, rx * 0.22f, Paint(INK_WHITE, 0.85f));
}

// ---- Hula: spins a hoop ------------------------------------------------------------------
static const float HULA_HZ = 1.5f;

void actHulaPose(ActFrame &f) {
  float k = envelope(f, 0.5f, 0.6f);
  float s = sinf(f.t * M_TAU * HULA_HZ), c2 = cosf(f.t * M_TAU * HULA_HZ);
  f.dx[0] = f.dx[1] = 4.5f * s * k;
  f.dy[0] = f.dy[1] = -1.5f * fabsf(c2) * k;
  for (int e = 0; e < 2; e++) f.pose.eye[e].y -= 3.0f * k;
  f.halo = true;
  setEars(f, 100.0f, 100.0f, EG_FLAP, 22.0f * k, HULA_HZ);
  if (f.crossed(0.5f)) f.chirp = CH_HAPPY;
}

void actHulaDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer == ACT_POST) return;
  float t = f.t;
  float in = outBack(seg(t, 0.1f, 0.6f)), out = easeIn(seg(t, f.duration - 0.6f, f.duration));
  float cy = lerpf(-40.0f, 42.0f, in) - out * 90.0f;
  float ph = t * M_TAU * HULA_HZ;
  float cx = 64.0f - 5.0f * sinf(ph);
  float tilt = 0.10f * cosf(ph);
  const int n = 32;
  for (int i = 0; i < n; i++) {
    float a = i * (M_TAU / n) + ph * 0.5f;
    OrbitPoint p = orbitAt(a, 52.0f, 10.0f, tilt, cx, cy);
    bool front = p.z > 0.0f;
    if (front != (layer == ACT_FRONT)) continue;
    OrbitPoint q = orbitAt(a + M_TAU / n, 52.0f, 10.0f, tilt, cx, cy);
    if (front) c.thickLine(p.x, p.y, q.x, q.y, 4.6f, INK_BLACK);
    // stripes, so the spin is seen
    c.thickLine(p.x, p.y, q.x, q.y, 2.2f, ((i / 2) & 1) ? INK_PINK : INK_YELLOW);
  }
}

// ---- Dance: eyes to the beat over an equalizer -------------------------------------------
static const float BEAT_HZ = 2.0f;

void actDancePose(ActFrame &f) {
  float k = envelope(f, 0.4f, 0.5f);
  float s = sinf(f.t * M_TAU * BEAT_HZ * 0.5f);
  f.dy[0] = -6.0f * fmaxf(0.0f, s) * k;
  f.dy[1] = -6.0f * fmaxf(0.0f, -s) * k;
  f.dx[0] = f.dx[1] = 3.0f * sinf(f.t * M_TAU * BEAT_HZ * 0.25f) * k;
  for (int e = 0; e < 2; e++) f.pose.eye[e].y -= 6.0f * k;
  f.pose.mouth.y -= 4.0f * k;
  // the lights of the dance floor
  static const Rgb LIGHTS[4] = {INK_PINK, INK_YELLOW, INK_MINT, INK_PURPLE};
  float beat = f.t * BEAT_HZ * 0.5f;
  int step = (int)floorf(beat);
  tint(f, mixRgb(LIGHTS[step & 3], LIGHTS[(step + 1) & 3], inOut(beat - step)), k);
  setEars(f, 95.0f, 95.0f, EG_FLAP, 30.0f * k, BEAT_HZ * 0.5f);
  if (f.crossed(0.4f)) f.chirp = CH_HAPPY;
  if (f.crossed(2.4f)) f.chirp = CH_LAUGH;
}

void actDanceDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float k = envelope(f, 0.4f, 0.5f);
  if (k < 0.05f) return;
  static const Rgb BAR[4] = {INK_MINT, INK_LIME, INK_YELLOW, INK_PINK};
  // the bars stand on the round edge of the glass
  const int bars = 13;
  for (int i = 0; i < bars; i++) {
    float v = fabsf(sinf(f.t * (3.0f + i * 0.7f) + i * 1.3f));
    // the beat kicks every bar
    float beat = 1.0f - fractf(f.t * BEAT_HZ);
    float h = (3.0f + 13.0f * v + 4.0f * beat * beat) * k;
    float x = 64.0f + (i - (bars - 1) * 0.5f) * 8.0f;
    float dx = x - DISC_CX;
    float base = DISC_CY + sqrtf(60.0f * 60.0f - dx * dx);
    for (int s = 0; s * 3.2f < h; s++) {
      int level = s > 4 ? 3 : (s > 2 ? 2 : (s > 0 ? 1 : 0));
      c.fillRoundRect(x - 3.0f, base - 2.4f - s * 3.2f, 6.0f, 2.4f, 0.8f, BAR[level]);
    }
  }
}

// ---- Plant: a flower grows between the eyes ---------------------------------------------
static float plantRise(float t, float dur) { return outBack(seg(t, 0.2f, 0.6f)) * (1.0f - easeIn(seg(t, dur - 0.7f, dur - 0.2f))); }

void actPlantPose(ActFrame &f) {
  float t = f.t;
  float k = envelope(f, 0.4f, 0.5f);
  float bloom = inOut(seg(t, 2.5f, 3.2f)) * (1.0f - seg(t, f.duration - 0.8f, f.duration - 0.3f));
  f.gazeY = lerpf(-0.8f, -0.25f, seg(t, 0.9f, 2.6f)) * k;
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.x += 9.0f * k;  // opens a gap for the plant
    p.y -= 9.0f * k;
    p.w *= 1.0f - 0.14f * k;
    p.h *= 1.0f - 0.22f * k;
    p.lidB += 0.30f * bloom;
    p.lidBBend += 0.35f * bloom;
  }
  f.pose.mouth.w = 0.0f;
  f.halo = true;
  tint(f, INK_MINT, bloom);
  setEars(f, 90.0f + 25.0f * bloom, 90.0f + 25.0f * bloom, bloom > 0.5f ? EG_WIGGLE : EG_STILL, 12.0f, 2.0f);
  if (f.crossed(0.9f)) f.chirp = CH_QUESTION;
  if (f.crossed(2.6f)) f.chirp = CH_HAPPY;
}

void actPlantDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  float rise = plantRise(t, f.duration);
  if (rise < 0.02f) return;
  float base = GROUND + 8.0f + 30.0f * (1.0f - rise);  // bottom of the pot
  float by = base - 11.0f;
  // pot
  quad(c, 54.0f, by, 74.0f, by, 70.5f, base, 57.5f, base, Paint::gradient(POT, 0x9A4424, by, base));
  c.fillRoundRect(52.0f, by - 3.0f, 24.0f, 4.0f, 1.2f, 0xE8835A);

  float grow = inOut(seg(t, 0.9f, 2.4f));
  if (grow <= 0.0f) return;
  float sway = sinf(t * 2.2f) * 2.0f;
  float h = 36.0f * grow;
  // stem, bending with the sway
  float pts[11 * 2];
  for (int i = 0; i <= 10; i++) {
    float u = i / 10.0f;
    pts[i * 2] = 64.0f + sway * u * u;
    pts[i * 2 + 1] = by - 3.0f - h * u;
  }
  c.polyline(pts, 11, 2.0f, LEAF);
  float px = pts[20], py = pts[21];
  // leaves
  float l1 = outBack(seg(t, 1.4f, 1.8f)), l2 = outBack(seg(t, 1.8f, 2.2f));
  if (l1 > 0.05f) c.fillEllipse(64.0f - 6.0f * l1 + sway * 0.1f, by - 3.0f - h * 0.35f, 6.0f * l1, 2.5f * l1, LEAF);
  if (l2 > 0.05f) c.fillEllipse(64.0f + 6.0f * l2 + sway * 0.3f, by - 3.0f - h * 0.6f, 6.0f * l2, 2.5f * l2, 0x3CB050);
  // flower
  float bloom = outBack(seg(t, 2.5f, 3.1f));
  float bud = seg(t, 2.1f, 2.5f);
  if (bloom > 0.05f) {
    c.fillCircle(px, py, 9.6f * bloom, INK_BLACK);
    c.pathBegin();
    for (int i = 0; i < 5; i++) {
      float co, si;
      Canvas::unit(i * (M_TAU / 5.0f) + t * 0.6f, co, si);
      c.pathAddEllipse(px + co * 5.0f * bloom, py + si * 5.0f * bloom, 3.4f * bloom, 3.4f * bloom);
    }
    c.pathFill(Paint::gradient(INK_ROSE, INK_PINK, py - 8.0f, py + 8.0f));
    c.fillCircle(px, py, 2.8f * bloom, INK_YELLOW);
  } else if (bud > 0.05f) {
    c.fillCircle(px, py, 2.6f * bud, INK_PINK);
  }
}

// ---- Drink: sips a bubble tea ------------------------------------------------------------
void actDrinkPose(ActFrame &f) {
  float t = f.t;
  float k = envelope(f, 0.4f, 0.5f);
  float sip = inOut(seg(t, 1.0f, 1.4f)) * (1.0f - inOut(seg(t, 3.6f, 4.0f)));
  // closed and smiling while it drinks
  EyeParams arc = {9.0f, -10.0f, 32.0f, 18.0f, 9.0f, 9.0f, 0.22f, 0, -0.75f, 0.58f, 0, 0.75f, 0, 1.0f, 0, 0};
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.x += 9.0f * k;
    p.y -= 9.0f * k;
    p.w *= 1.0f - 0.12f * k;
    p.h *= 1.0f - 0.15f * k;
    mixEye(p, arc, sip);
  }
  float gulp = fabsf(sinf(t * M_TAU * 1.6f)) * sip;
  f.dy[0] = f.dy[1] = -1.6f * gulp;
  f.gazeY = -0.6f * k * (1.0f - sip);
  f.halo = true;
  f.pose.mouth.w = 0.0f;
  // "ahh"
  float ahh = bump(t, 4.0f, 4.7f);
  if (ahh > 0.05f) {
    f.pose.mouth.w = 12.0f * ahh;
    f.pose.mouth.open = 0.8f;
    f.pose.mouth.curve = 2.0f;
    f.pose.mouth.y = 16.0f;
  }
  setEars(f, 95.0f, 95.0f, sip > 0.5f ? EG_WIGGLE : EG_STILL, 10.0f, 1.6f);
  if (f.crossed(1.2f)) f.chirp = CH_HAPPY;
  if (f.crossed(4.0f)) f.chirp = CH_LAUGH;
}

void actDrinkDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  float rise = outBack(seg(t, 0.2f, 0.7f)) * (1.0f - easeIn(seg(t, f.duration - 1.1f, f.duration - 0.6f)));
  if (rise < 0.02f) return;
  const float H = 30.0f;
  float bottom = GROUND + 8.0f + 50.0f * (1.0f - rise);
  float top = bottom - H;  // rim of the cup
  const float cx = 64.0f;
  auto halfAt = [&](float y) { return lerpf(12.0f, 9.0f, (y - top) / H); };
  // the glass
  quad(c, cx - 13.0f, top, cx + 13.0f, top, cx + 10.0f, bottom + 1.0f, cx - 10.0f, bottom + 1.0f, INK_BLACK);
  // tea going down
  float level = lerpf(top + 5.0f, bottom - 9.0f, seg(t, 1.3f, 3.6f));
  quad(c, cx - halfAt(level) + 1.0f, level, cx + halfAt(level) - 1.0f, level, cx + 8.0f, bottom - 1.0f, cx - 8.0f,
       bottom - 1.0f, Paint::gradient(0xF0C088, TEA, level, bottom));
  // pearls
  for (int i = 0; i < 6; i++) {
    float x = cx - 6.0f + i * 2.4f, y = bottom - 3.4f - (i & 1) * 2.8f;
    // one goes up the straw with every sip
    float up = fractf(t * 1.6f + i * 0.2f);
    bool sucked = t > 1.3f && t < 3.6f && i == ((int)(t * 1.6f) % 6);
    if (sucked) {
      x = lerpf(x, cx + 5.0f, up);
      y = lerpf(y, top - 10.0f, up);
    }
    c.fillCircle(x, y, 1.7f, PEARL);
  }
  float glass[8] = {cx - 12.0f, top, cx - 9.0f, bottom, cx + 9.0f, bottom, cx + 12.0f, top};
  c.polyline(glass, 4, 1.5f, Paint(INK_ICE, 0.9f));
  c.thickLine(cx - 13.5f, top, cx + 13.5f, top, 2.4f, INK_WHITE);  // lid
  // straw
  c.thickLine(cx + 2.5f, bottom - 4.0f, cx + 6.5f, top - 14.0f, 4.6f, INK_BLACK);
  c.thickLine(cx + 2.5f, bottom - 4.0f, cx + 6.5f, top - 14.0f, 2.6f, INK_PINK);
}
