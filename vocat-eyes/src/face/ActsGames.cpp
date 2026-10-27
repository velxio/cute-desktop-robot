// Acts where the eyes become the pieces of a game.
#include "ActKit.h"
#include "ActList.h"

// ---- Pacman: the eyes turn into pellets... and get eaten -----------------------------
namespace {
const float PAC_T0 = 0.7f, PAC_T1 = 3.7f;
const float PELLET_X[5] = {16.0f, 40.0f, 64.0f, 88.0f, 112.0f};  // 1 and 3 are the eyes
const float PAC_R = 12.0f;
const float GHOST_BEHIND = 34.0f;
const Rgb PELLET = 0xFFD8B0;

float pacX(float t) { return lerpf(-16.0f, 144.0f + GHOST_BEHIND, seg(t, PAC_T0, PAC_T1)); }
// moment when pacman reaches a pellet
float eatTime(int i) {
  return PAC_T0 + (PELLET_X[i] - 4.0f + 16.0f) / (160.0f + GHOST_BEHIND) * (PAC_T1 - PAC_T0);
}

void drawGhost(Canvas &c, float x, float y, float r, float t) {
  float xy[2 * 40];
  int n = 0;
  // dome
  for (int i = 0; i <= 12; i++) {
    float co, si;
    Canvas::unit(ACT_PI + ACT_PI * i / 12.0f, co, si);
    xy[n * 2] = x + co * r;
    xy[n * 2 + 1] = y + si * r;
    n++;
  }
  // skirt, the feet run
  float foot = fractf(t * 4.0f) < 0.5f ? 0.0f : 0.5f;
  for (int i = 0; i <= 6; i++) {
    float u = (i + foot) / 6.5f;
    if (u > 1.0f) u = 1.0f;
    xy[n * 2] = x + r - 2.0f * r * u;
    xy[n * 2 + 1] = y + r * ((i & 1) ? 0.72f : 1.0f);
    n++;
  }
  c.fillPoly(xy, n, Paint::gradient(0xFF6060, 0xE01828, y - r, y + r));
  for (int s = -1; s <= 1; s += 2) {
    c.fillEllipse(x + s * r * 0.38f + 1.0f, y - r * 0.15f, r * 0.26f, r * 0.34f, INK_WHITE);
    c.fillCircle(x + s * r * 0.38f + 2.2f, y - r * 0.12f, r * 0.14f, Rgb(0x2040D0));
  }
}
}  // namespace

void actPacmanPose(ActFrame &f) {
  float t = f.t;
  float toPellet = inOut(seg(t, 0.0f, 0.5f));
  float back = outBack(seg(t, 4.2f, 4.7f));
  for (int e = 0; e < 2; e++) {
    int pellet = e == 0 ? 1 : 3;
    if (t < 4.2f) {
      mixEye(f.pose.eye[e], dotEye(0.0f, 0.0f, 8.0f), toPellet);
      if (t >= eatTime(pellet)) f.eyeOn[e] = false;
    } else {
      f.pose.eye[e].w *= back;
      f.pose.eye[e].h *= back;
    }
  }
  if (t < 4.2f) tint(f, PELLET, toPellet);
  f.pose.mouth.w = 0.0f;
  // what just happened?
  f.gazeX = -0.8f * bump(t, 4.75f, 5.0f) + 0.8f * bump(t, 5.0f, 5.25f);
  blinkAt(f, 4.95f);
  float chase = seg(t, PAC_T0, PAC_T1);
  setEars(f, lerpf(80.0f, 20.0f, toPellet) + 80.0f * back, lerpf(80.0f, 20.0f, toPellet) + 80.0f * back,
          (chase > 0.0f && chase < 1.0f) ? EG_TREMBLE : EG_STILL, 6.0f, 9.0f);
  for (int i = 0; i < 5; i++) {
    if (f.crossed(eatTime(i))) f.chirp = CH_BLIP;
  }
  if (f.crossed(4.2f)) f.chirp = CH_QUESTION;
}

void actPacmanDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  // pellets that are not eyes
  float k = inOut(seg(t, 0.0f, 0.5f));
  for (int i = 0; i < 5; i += 2) {
    if (t < eatTime(i)) c.fillCircle(PELLET_X[i], EYE_ANCHOR_Y, 4.0f * k, PELLET);
  }
  if (t < PAC_T0 || t > PAC_T1) return;

  float x = pacX(t);
  drawGhost(c, x - GHOST_BEHIND, EYE_ANCHOR_Y, PAC_R * 0.92f, t);
  float mouth = 0.12f + 0.75f * fabsf(sinf(t * 13.0f));  // half angle, radians
  c.fillSector(x, EYE_ANCHOR_Y, PAC_R, 0.0f, mouth, M_TAU - mouth,
               Paint::gradient(0xFFF06A, 0xFFC020, EYE_ANCHOR_Y - PAC_R, EYE_ANCHOR_Y + PAC_R));
  c.fillCircle(x + 1.5f, EYE_ANCHOR_Y - 6.5f, 1.7f, INK_BLACK);
}

// ---- Pong: each eye is a paddle --------------------------------------------------------
namespace {
const float PONG_T0 = 0.6f;
const float PADDLE_X = 26.0f;                  // how far from its place each eye goes
const float WALL_L = 19.0f, WALL_R = 109.0f;   // faces of the paddles
const float PONG_VX = 112.5f, PONG_VY = 47.0f; // one crossing every 0.8 s
const float MISS_T = 4.2f;  // third time the ball reaches the right side

void pongBall(float t, float &x, float &y) {
  float u = (t - PONG_T0);
  if (u < 0.0f) u = 0.0f;
  const float span = WALL_R - WALL_L;
  float ux = u * PONG_VX + span * 0.5f;
  float xx = fmodf(ux, span * 2.0f);
  x = WALL_L + (xx < span ? xx : span * 2.0f - xx);
  if (t > MISS_T) x = WALL_R + (t - MISS_T) * PONG_VX;  // nobody was there
  float uy = u * PONG_VY + 26.0f;
  float yy = fmodf(uy, 104.0f);
  y = 6.0f + (yy < 52.0f ? yy : 104.0f - yy);
}
}  // namespace

void actPongPose(ActFrame &f) {
  float t = f.t;
  float k = inOut(seg(t, 0.0f, 0.5f)) * (1.0f - inOut(seg(t, 4.8f, 5.3f)));
  EyeParams paddle = {PADDLE_X, 0.0f, 5.0f, 22.0f, 6.0f, 6.0f, 0, 0, 0, 0, 0, 0, 0, 1.0f, 0, 0};
  float bx, by, lx, ly;
  pongBall(t - 0.06f, bx, by);
  pongBall(t - 0.5f, lx, ly);  // the right one reacts late, that is why it loses
  for (int e = 0; e < 2; e++) {
    mixEye(f.pose.eye[e], paddle, k);
    float follow = e == 0 ? by : (t > 3.4f ? ly : by);
    f.dy[e] = clampf(follow - EYE_ANCHOR_Y, -20.0f, 20.0f) * k;
  }
  f.pose.mouth.w = 0.0f;
  tint(f, INK_WHITE, k);
  // the loser lowers its head
  float lost = bump(t, 4.4f, 5.4f);
  f.dy[1] += 5.0f * lost * (1.0f - k);
  blinkAt(f, 5.5f);
  setEars(f, 110.0f - 70.0f * lost, 110.0f - 70.0f * lost);
  // hits: right 1.0 and 2.6, left 1.8 and 3.4
  if (f.crossed(1.0f) || f.crossed(1.8f) || f.crossed(2.6f) || f.crossed(3.4f)) f.chirp = CH_BLIP;
  if (f.crossed(4.35f)) f.chirp = CH_SAD;
}

void actPongDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t;
  float k = inOut(seg(t, 0.0f, 0.5f)) * (1.0f - inOut(seg(t, 4.8f, 5.3f)));
  if (k < 0.5f) return;
  for (float y = -20.0f; y < 100.0f; y += 7.0f) c.fillRect(63.2f, y, 1.6f, 3.6f, Paint(INK_GRAY, 0.8f));  // net
  if (t >= PONG_T0 && t < MISS_T + 0.2f) {
    float x, y;
    for (int i = 3; i >= 1; i--) {
      pongBall(t - i * 0.025f, x, y);
      c.fillCircle(x, y, 2.6f - i * 0.4f, Paint(INK_YELLOW, 0.5f - i * 0.12f));
    }
    pongBall(t, x, y);
    c.fillCircle(x, y, 2.8f, INK_YELLOW);
  }
  // the point blinks on the score
  bool off = t > MISS_T + 0.1f && t < 4.8f && fractf(t * 5.0f) >= 0.6f;
  c.textCentered(46.0f, -14.0f, t > MISS_T ? "1" : "0", 2, off ? INK_DARK : INK_ICE);
  c.textCentered(82.0f, -14.0f, "0", 2, INK_ICE);
}

// ---- Rainbow ----------------------------------------------------------------------------
void actRainbowPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.6f);
  float u = inOut(seg(t, 0.4f, 1.8f));
  // follows the rainbow while it is drawn, then enjoys it
  f.gazeX = lerpf(-0.8f, 0.8f, u) * k * (1.0f - seg(t, 1.8f, 2.3f));
  f.gazeY = 0.85f * k * sinf(ACT_PI * clampf(u, 0.08f, 0.92f));
  float joy = inOut(seg(t, 1.9f, 2.4f)) * k;
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.y += 9.0f * k;
    p.lidB += 0.36f * joy;
    p.lidBBend += 0.40f * joy;
  }
  f.pose.mouth.w = 16.0f * joy;
  f.pose.mouth.curve = 4.0f;
  f.pose.mouth.open = 0.0f;
  f.pose.mouth.y = 36.0f;
  f.pose.mouth.tilt = 0.0f;
  f.halo = true;
  setEars(f, 100.0f, 100.0f, joy > 0.5f ? EG_WIGGLE : EG_STILL, 12.0f, 2.0f);
  if (f.crossed(0.4f)) f.chirp = CH_WHOOSH;
  if (f.crossed(2.0f)) f.chirp = CH_HAPPY;
}

void actRainbowDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t;
  float u = inOut(seg(t, 0.4f, 1.8f));
  float fade = 1.0f - seg(t, f.duration - 0.7f, f.duration - 0.1f);
  if (u <= 0.01f || fade <= 0.01f) return;
  static const Rgb BAND[6] = {0xFF3B30, 0xFF9500, 0xFFE020, 0x34C759, 0x30A0FF, 0xA05CFF};
  for (int i = 0; i < 6; i++) {
    float r = 60.0f - i * 4.0f;
    c.fillSector(64.0f, 40.0f, r, r - 4.2f, ACT_PI, ACT_PI + ACT_PI * u, Paint(BAND[i], 0.95f * fade));
  }
  // clouds at both ends
  for (int s = 0; s < 2; s++) {
    if (s == 1 && u < 0.98f) break;
    float x = s == 0 ? 15.0f : 113.0f;
    c.pathBegin();
    c.pathAddEllipse(x - 5.0f, 42.0f, 5.0f, 4.4f);
    c.pathAddEllipse(x + 1.0f, 39.0f, 6.4f, 6.0f);
    c.pathAddEllipse(x + 7.0f, 42.5f, 4.6f, 4.0f);
    c.pathFill(Paint(INK_WHITE, fade));
  }
}

// ---- Radar: the round screen is a radar ------------------------------------------------
namespace {
const Rgb SCOPE = 0x30FF80;
const float BLIP_A[3] = {5.3f, 0.9f, 2.6f};  // where the blips are, angle and distance
const float BLIP_D[3] = {40.0f, 50.0f, 30.0f};
float sweepAt(float t) { return -ACT_PI * 0.5f + (t - 0.5f) * 3.4f; }
}  // namespace

void actRadarPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  float a = sweepAt(t);
  f.gazeX = cosf(a) * 0.8f * k;
  f.gazeY = -sinf(a) * 0.7f * k;
  for (int e = 0; e < 2; e++) {
    f.pose.eye[e].lidT += 0.14f * k;
    f.pose.eye[e].lidB += 0.10f * k;
  }
  f.pose.mouth.w = 0.0f;
  f.halo = true;
  tint(f, 0x70FFA0, k);
  setEars(f, 120.0f, 120.0f, EG_TWITCH, 8.0f, 1.5f);
  for (int i = 0; i < 3; i++) {
    // the beam passes over a blip
    float turns = (a - BLIP_A[i]) / M_TAU, before = (sweepAt(f.tPrev) - BLIP_A[i]) / M_TAU;
    if (t > 0.6f && floorf(turns) != floorf(before)) f.chirp = CH_BLIP;
  }
}

void actRadarDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  if (k < 0.05f) return;
  const float cx = DISC_CX, cy = DISC_CY;
  for (int i = 1; i <= 3; i++) c.ring(cx, cy, 20.0f * i, 0.7f, Paint(SCOPE, 0.35f * k));
  c.fillRect(cx - 62.0f, cy - 0.35f, 124.0f, 0.7f, Paint(SCOPE, 0.30f * k));
  c.fillRect(cx - 0.35f, cy - 62.0f, 0.7f, 124.0f, Paint(SCOPE, 0.30f * k));
  // beam and its afterglow
  float a = sweepAt(t);
  for (int i = 0; i < 6; i++) {
    float a1 = a - i * 0.16f;
    c.fillSector(cx, cy, 62.0f, 0.0f, a1 - 0.17f, a1, Paint(SCOPE, 0.32f * k * (1.0f - i / 6.0f)));
  }
  float co, si;
  Canvas::unit(a, co, si);
  c.thickLine(cx, cy, cx + co * 62.0f, cy + si * 62.0f, 1.2f, Paint(0xC0FFD8, k));
  // blips light up when the beam passes and fade away
  for (int i = 0; i < 3; i++) {
    float since = fmodf(a - BLIP_A[i] + M_TAU * 8.0f, M_TAU) / 3.4f;  // seconds
    if (t < 0.6f || since > 1.6f) continue;
    Canvas::unit(BLIP_A[i], co, si);
    float x = cx + co * BLIP_D[i], y = cy + si * BLIP_D[i];
    float glow = 1.0f - since / 1.6f;
    c.fillCircle(x, y, 5.0f, Paint(SCOPE, 0.25f * glow * k));
    c.fillCircle(x, y, 2.4f, Paint(0xC0FFD8, glow * k));
  }
}

// ---- Aquarium: the glass is a porthole ---------------------------------------------------
namespace {
const Rgb WATER_TOP = 0x1C6CD0, WATER_DEEP = 0x062A70;

float waterLevel(const ActFrame &f) {
  float in = inOut(seg(f.t, 0.3f, 1.6f)), out = inOut(seg(f.t, f.duration - 1.4f, f.duration - 0.3f));
  return lerpf(GLASS_BOTTOM + 6.0f, 50.0f, in * (1.0f - out));
}

void drawFish(Canvas &c, float x, float y, float s, float dir, float t, Rgb body, Rgb fin) {
  float wag = sinf(t * 9.0f) * 2.0f * s;
  c.fillTriangle(x - dir * 5.0f * s, y, x - dir * 10.0f * s, y - 4.0f * s + wag, x - dir * 10.0f * s, y + 4.0f * s + wag, fin);
  c.fillEllipse(x, y, 6.4f * s, 3.8f * s, Paint::gradient(lighten(body, 0.3f), body, y - 4.0f * s, y + 4.0f * s));
  c.fillTriangle(x - dir * 1.0f * s, y - 3.0f * s, x - dir * 4.0f * s, y - 6.4f * s, x + dir * 2.0f * s, y - 3.4f * s, fin);
  c.fillCircle(x + dir * 3.4f * s, y - 1.0f * s, 1.3f * s, INK_WHITE);
  c.fillCircle(x + dir * 3.8f * s, y - 1.0f * s, 0.7f * s, INK_BLACK);
}
}  // namespace

void actAquariumPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.6f);
  float level = waterLevel(f);
  float wet = clamp01((GLASS_BOTTOM - level) / 50.0f);
  // holds its breath: the eyes go up, away from the water
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.y -= 12.0f * wet;
    p.h *= 1.0f - 0.18f * wet;
    p.w *= 1.0f - 0.08f * wet;
  }
  f.pose.mouth.w = 0.0f;
  float fishX = lerpf(140.0f, -14.0f, seg(t, 1.8f, 5.0f));
  float watching = seg(t, 1.8f, 2.1f) * (1.0f - seg(t, 4.8f, 5.1f));
  f.gazeX = clampf((fishX - 64.0f) / 60.0f, -1.0f, 1.0f) * watching;
  f.gazeY = -0.85f * k * (0.4f + 0.6f * watching);
  f.dy[0] = f.dy[1] = sinf(t * 2.2f) * 1.5f * wet;  // floats
  f.halo = true;
  tint(f, INK_SKY, wet * 0.6f);
  setEars(f, 100.0f, 100.0f, EG_FLAP, 10.0f * wet, 0.6f);
  if (f.crossed(0.4f)) f.chirp = CH_WHOOSH;
  if (f.crossed(2.4f)) f.chirp = CH_QUESTION;
}

void actAquariumDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t;
  float level = waterLevel(f);
  if (level > GLASS_BOTTOM + 2.0f) return;
  // water, with waves on top
  const int n = 20;
  float xy[(n + 3) * 2];
  for (int i = 0; i <= n; i++) {
    float x = -2.0f + 132.0f * i / n;
    xy[i * 2] = x;
    xy[i * 2 + 1] = level + sinf(x * 0.11f + t * 3.0f) * 1.8f + sinf(x * 0.23f - t * 2.1f) * 0.9f;
  }
  xy[(n + 1) * 2] = 130.0f;
  xy[(n + 1) * 2 + 1] = GLASS_BOTTOM + 4.0f;
  xy[(n + 2) * 2] = -2.0f;
  xy[(n + 2) * 2 + 1] = GLASS_BOTTOM + 4.0f;
  c.fillPoly(xy, n + 3, Paint::gradient(WATER_TOP, WATER_DEEP, level, GLASS_BOTTOM, 0.92f));
  c.polyline(xy, n + 1, 1.2f, Paint(INK_SKY, 0.9f));

  if (level > 64.0f) return;
  // sea weed
  for (int s = 0; s < 3; s++) {
    float bx = 40.0f + s * 22.0f;
    float pts[7 * 2];
    for (int i = 0; i <= 6; i++) {
      float u = i / 6.0f;
      pts[i * 2] = bx + sinf(u * 4.0f + t * 1.6f + s) * 3.0f * u;
      pts[i * 2 + 1] = GLASS_BOTTOM - 2.0f - u * (14.0f + s * 3.0f);
    }
    c.polyline(pts, 7, 2.0f, s == 1 ? Rgb(0x30C070) : Rgb(0x209A58));
  }
  // bubbles
  for (int i = 0; i < 5; i++) {
    float ph = fractf(t * 0.35f + i * 0.2f);
    float x = 30.0f + i * 17.0f + sinf(t * 2.0f + i * 1.7f) * 2.0f;
    float y = GLASS_BOTTOM - ph * (GLASS_BOTTOM - level);
    c.ring(x, y, 1.4f + hash01(i * 3) * 1.6f, 0.7f, Paint(INK_ICE, 0.9f));
  }
  // the fish that goes by, and a small one the other way
  float fx = lerpf(140.0f, -14.0f, seg(t, 1.8f, 5.0f));
  drawFish(c, fx, 78.0f + sinf(t * 1.8f) * 3.0f, 1.25f, -1.0f, t, INK_ORANGE, INK_AMBER);
  float gx = lerpf(-14.0f, 140.0f, seg(t, 2.6f, 6.2f));
  drawFish(c, gx, 92.0f + sinf(t * 2.3f + 1.0f) * 2.0f, 0.8f, 1.0f, t + 0.3f, INK_PINK, INK_ROSE);
}

// ---- Spin: a ride around the edge of the round glass ---------------------------------------
namespace {
float spinAngle(float t) { return M_TAU * 2.0f * inOut(seg(t, 0.5f, 3.3f)); }
}  // namespace

void actSpinPose(ActFrame &f) {
  float t = f.t;
  float out = inOut(seg(t, 0.0f, 0.5f)) * (1.0f - inOut(seg(t, 3.3f, 3.8f)));
  float a = spinAngle(t);
  float dizzy = inOut(seg(t, 3.6f, 3.9f)) * (1.0f - inOut(seg(t, 4.7f, 5.2f)));
  for (int e = 0; e < 2; e++) {
    // each eye on its side of a wheel that turns
    float side = e == 0 ? ACT_PI : 0.0f;
    float co, si;
    Canvas::unit(a + side, co, si);
    float tx = DISC_CX + co * 40.0f, ty = DISC_CY + si * 40.0f;
    float ax = e == 0 ? EYE_ANCHOR_LX : EYE_ANCHOR_RX;
    f.dx[e] = (tx - ax) * out;
    f.dy[e] = (ty - EYE_ANCHOR_Y) * out;
    EyeParams &p = f.pose.eye[e];
    p.w = lerpf(p.w, 22.0f, out);
    p.h = lerpf(p.h, 22.0f, out);
    p.nTop = lerpf(p.nTop, 2.0f, out);
    p.nBot = lerpf(p.nBot, 2.0f, out);
    if (dizzy > 0.5f) f.kind[e] = EK_SPIRAL;
  }
  blinkBoth(f, bump(t, 3.55f, 3.95f) + bump(t, 4.6f, 5.0f));  // hides the change of eye
  f.pose.mouth.w = 14.0f * dizzy;
  f.pose.mouth.curve = -1.5f;
  f.pose.mouth.open = 0.0f;
  f.pose.mouth.y = 24.0f;
  f.pose.mouth.tilt = 0.0f;
  f.dx[0] += sinf(t * 5.0f) * 3.0f * dizzy;
  f.dx[1] += sinf(t * 5.0f) * 3.0f * dizzy;
  tint(f, INK_PURPLE, dizzy);
  float speed = seg(t, 0.5f, 1.2f) * (1.0f - seg(t, 2.8f, 3.3f));
  setEars(f, 90.0f, 90.0f, EG_FLAP, 30.0f * speed + 12.0f * dizzy, 2.5f);
  if (f.crossed(0.5f)) f.chirp = CH_WHOOSH;
  if (f.crossed(3.7f)) f.chirp = CH_SAD;
}

void actSpinDraw(Canvas &c, const ActFrame &f, const FaceFrame &face, uint8_t layer) {
  if (layer != ACT_BACK) return;
  float t = f.t;
  float speed = seg(t, 0.5f, 1.2f) * (1.0f - seg(t, 2.8f, 3.3f));
  if (speed < 0.05f) return;
  // trails behind each eye
  float a = spinAngle(t);
  for (int e = 0; e < 2; e++) {
    for (int i = 1; i <= 7; i++) {
      float co, si;
      Canvas::unit(a + (e == 0 ? ACT_PI : 0.0f) - i * 0.11f * speed, co, si);
      c.fillCircle(DISC_CX + co * 40.0f, DISC_CY + si * 40.0f, 10.0f - i * 1.1f, Paint(face.ink, 0.30f * speed * (1.0f - i / 8.0f)));
    }
  }
}
