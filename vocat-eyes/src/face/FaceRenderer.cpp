#include "FaceRenderer.h"

#include "../core/MathUtil.h"

namespace {

float glowAmount = 1.0f;

// ---- outline of the superellipse -----------------------------------------------------
// The eye is cut in COLS columns, closer together near the corners where the
// outline is steep. For every column the table gives the height of the
// outline, (1 - t^n)^(1/n), for a handful of exponents; powf() runs only once,
// when the table is built.
const int COLS = 32;
const int HALF = COLS / 2;
const int EXPONENTS = 14;
const float EXPONENT[EXPONENTS] = {1.2f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 10.0f, 12.0f, 16.0f};

float colX[COLS + 1];                 // -1..1
float profile[EXPONENTS][HALF + 1];   // [exponent][column], 0..1
bool tablesReady = false;

void initTables() {
  if (tablesReady) return;
  for (int i = 0; i <= COLS; i++) colX[i] = -cosf(3.14159265f * (float)i / (float)COLS);
  colX[0] = -1.0f;
  colX[HALF] = 0.0f;
  colX[COLS] = 1.0f;
  for (int e = 0; e < EXPONENTS; e++) {
    float n = EXPONENT[e];
    for (int i = 0; i <= HALF; i++) {
      float t = fabsf(colX[i]);
      float v = 1.0f - powf(t, n);
      profile[e][i] = v <= 0.0f ? 0.0f : powf(v, 1.0f / n);
    }
  }
  tablesReady = true;
}

// Tangent of a small angle (the slope of an eyelid), without calling tanf().
inline float tanDeg(float deg) {
  float x = clampf(deg, -60.0f, 60.0f) * DEG2RADF;
  float x2 = x * x;
  return x * (1.0f + x2 * (0.333333f + x2 * (0.133333f + x2 * 0.053968f)));
}

// Heights of the outline for the exponent n, HALF + 1 numbers.
void profileFor(float n, float *out) {
  n = clampf(n, EXPONENT[0], EXPONENT[EXPONENTS - 1]);
  int e = 0;
  while (e < EXPONENTS - 2 && n > EXPONENT[e + 1]) e++;
  float t = (n - EXPONENT[e]) / (EXPONENT[e + 1] - EXPONENT[e]);
  for (int i = 0; i <= HALF; i++) out[i] = profile[e][i] + (profile[e + 1][i] - profile[e][i]) * t;
}

// Outline of the eye: superellipse cut by the two eyelids. Returns the number
// of points written (0 = nothing to draw).
int eyeOutline(float cx, float cy, const EyeParams &p, float inner, float grow, float *xy) {
  initTables();
  float w = p.w < 2.0f ? 2.0f : p.w;
  float h = p.h < 2.0f ? 2.0f : p.h;
  float hw = w * 0.5f, hh = h * 0.5f;
  float ghw = hw + grow, ghh = hh + grow;
  if (ghw < 0.25f || ghh < 0.25f) return 0;

  // Eyelids never close completely: a closed eye is drawn by squashing `h`,
  // so at least a sliver is always left between the two lids.
  float lidT = clampf(p.lidT, 0.0f, 0.95f);
  float lidB = clampf(p.lidB, 0.0f, 0.95f);
  float gap = h * (1.0f - lidT - lidB);
  if (gap < 1.5f) {
    float k = (h - 1.5f) / (h * (lidT + lidB));
    lidT *= k;
    lidB *= k;
  }
  float slopeT = tanDeg(p.lidTAng) * inner;
  float slopeB = -tanDeg(p.lidBAng) * inner;
  float topBase = -hh + lidT * h - grow;
  float botBase = hh - lidB * h + grow;
  float bendT = p.lidTBend * hh;
  float bendB = p.lidBBend * hh;
  bool lids = lidT > 0.001f || lidB > 0.001f || slopeT != 0.0f || slopeB != 0.0f || bendT != 0.0f || bendB != 0.0f;

  float pt[HALF + 1], pb[HALF + 1];
  profileFor(p.nTop, pt);
  if (p.nBot == p.nTop) {
    for (int i = 0; i <= HALF; i++) pb[i] = pt[i];
  } else {
    profileFor(p.nBot, pb);
  }

  const int n = COLS + 1;
  for (int i = 0; i < n; i++) {
    int k = i <= HALF ? i : COLS - i;
    float u = colX[i] * ghw;
    float top = -ghh * pt[k], bot = ghh * pb[k];
    if (lids) {
      float q = u / hw;
      float bow = 1.0f - q * q;
      float vt = topBase + slopeT * u + bendT * bow;
      float vb = botBase + slopeB * u - bendB * bow;
      float t = top, b = bot;
      if (vt > t) t = vt;
      if (vb < b) b = vb;
      if (t > b) {
        // the lids meet outside the eye: close the column on the outline
        float m = clampf((t + b) * 0.5f, top, bot);
        t = b = m;
      }
      top = t;
      bot = b;
    }
    // upper edge left to right, lower edge back
    xy[i * 2] = cx + u;
    xy[i * 2 + 1] = cy + top;
    int j = 2 * n - 1 - i;
    xy[j * 2] = cx + u;
    xy[j * 2 + 1] = cy + bot;
  }
  return 2 * n;
}

// ---- heart ------------------------------------------------------------------------------
const int HEART_N = 36;
float heartX[HEART_N], heartY[HEART_N];
bool heartReady = false;

void initHeart() {
  if (heartReady) return;
  float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
  for (int i = 0; i < HEART_N; i++) {
    float t = (float)i * (M_TAU / HEART_N);
    float s = sinf(t);
    heartX[i] = 16.0f * s * s * s;
    heartY[i] = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) - 2.0f * cosf(3.0f * t) - cosf(4.0f * t));
    x0 = fminf(x0, heartX[i]), x1 = fmaxf(x1, heartX[i]);
    y0 = fminf(y0, heartY[i]), y1 = fmaxf(y1, heartY[i]);
  }
  // fit to the box -1..1
  for (int i = 0; i < HEART_N; i++) {
    heartX[i] = (heartX[i] - (x0 + x1) * 0.5f) / ((x1 - x0) * 0.5f);
    heartY[i] = (heartY[i] - (y0 + y1) * 0.5f) / ((y1 - y0) * 0.5f);
  }
  heartReady = true;
}

// ---- the other kinds of eye ------------------------------------------------------------
void pathCross(Canvas &c, float cx, float cy, float hw, float hh, float grow) {
  float th = fminf(hw, hh) * 0.45f;
  if (th < 2.0f) th = 2.0f;
  float ex = hw - th * 0.5f, ey = hh - th * 0.5f;
  if (ey < 0.5f) ey = 0.5f;
  c.pathAddLine(cx - ex, cy - ey, cx + ex, cy + ey, th + grow * 2.0f);
  c.pathAddLine(cx - ex, cy + ey, cx + ex, cy - ey, th + grow * 2.0f);
}

void pathChevron(Canvas &c, float cx, float cy, float hw, float hh, float inner, float grow) {
  float th = fminf(hw, hh) * 0.42f;
  if (th < 2.0f) th = 2.0f;
  float ex = hw - th * 0.5f, ey = hh - th * 0.5f;
  if (ey < 0.5f) ey = 0.5f;
  // Tip points to the nose.
  float tipX = cx + inner * ex * 0.7f;
  float backX = cx - inner * ex;
  c.pathAddLine(backX, cy - ey, tipX, cy, th + grow * 2.0f);
  c.pathAddLine(backX, cy + ey, tipX, cy, th + grow * 2.0f);
}

void pathSpiral(Canvas &c, float cx, float cy, float hw, float hh, float time, float dir, float grow) {
  const float turns = 2.6f;
  const int steps = 40;
  float rot = time * 6.0f * dir;
  float px = cx, py = cy;
  float th = fminf(hw, hh) * 0.13f + 0.4f + grow * 2.0f;
  for (int i = 1; i <= steps; i++) {
    float f = (float)i / steps;
    float co, si;
    Canvas::unit(f * turns * M_TAU + rot, co, si);
    float x = cx + co * hw * f;
    float y = cy + si * hh * f;
    c.pathAddLine(px, py, x, y, th);
    px = x;
    py = y;
  }
}

// Rings that are born in the middle and grow until they leave the eye.
void pathRings(Canvas &c, float cx, float cy, float hw, float hh, float time) {
  if (hh < 2.5f) {
    c.pathAddEllipse(cx, cy, hw, hh);
    return;
  }
  const float spacing = 0.34f;  // of the radius
  float shift = time * 0.55f;
  shift -= floorf(shift);
  for (int k = -1; k < 4; k++) {
    // lit from r0 to r1
    float r0 = ((float)k + shift) * spacing, r1 = r0 + spacing * 0.5f;
    if (r1 <= 0.02f || r0 >= 1.0f) continue;
    if (r1 > 1.0f) r1 = 1.0f;
    c.pathAddEllipse(cx, cy, hw * r1, hh * r1);
    if (r0 > 0.02f) c.pathAddEllipse(cx, cy, hw * r0, hh * r0, true);
  }
}

void pathStar5(Canvas &c, float cx, float cy, float rx, float ry, float rot) {
  const float inner = 0.46f;
  float xy[20];
  for (int i = 0; i < 10; i++) {
    float co, si;
    Canvas::unit(rot - M_TAU * 0.25f + (float)i * (M_TAU / 10.0f), co, si);
    float k = (i & 1) ? inner : 1.0f;
    xy[i * 2] = cx + co * rx * k;
    xy[i * 2 + 1] = cy + si * ry * k + ry * 0.06f;
  }
  c.pathAdd(xy, 10);
}

void pathHeart(Canvas &c, float cx, float cy, float rx, float ry) {
  initHeart();
  float xy[HEART_N * 2];
  for (int i = 0; i < HEART_N; i++) {
    xy[i * 2] = cx + heartX[i] * rx;
    xy[i * 2 + 1] = cy + heartY[i] * ry;
  }
  c.pathAdd(xy, HEART_N);
}

// Adds the outline of an eye of any kind to the path being built.
bool pathEye(Canvas &c, float cx, float cy, const EyeParams &p, uint8_t kind, float inner, float time, float grow) {
  float hw = (p.w < 2.0f ? 2.0f : p.w) * 0.5f;
  float hh = (p.h < 2.0f ? 2.0f : p.h) * 0.5f;
  switch (kind) {
    case EK_HEART:
      if (hw + grow < 0.3f || hh + grow < 0.3f) return false;
      pathHeart(c, cx, cy, hw + grow, hh + grow);
      return true;
    case EK_CROSS:
      pathCross(c, cx, cy, hw, hh, grow);
      return true;
    case EK_CHEVRON:
      pathChevron(c, cx, cy, hw, hh, inner, grow);
      return true;
    case EK_SPIRAL:
      pathSpiral(c, cx, cy, hw, hh, time, inner, grow);
      return true;
    case EK_STAR:
      if (hw + grow < 0.3f || hh + grow < 0.3f) return false;
      pathStar5(c, cx, cy, hw + grow, hh + grow, sinf(time * 2.4f) * 0.22f * inner);
      return true;
    case EK_RING:
      if (grow != 0.0f) c.pathAddEllipse(cx, cy, hw + grow, hh + grow);
      else pathRings(c, cx, cy, hw, hh, time);
      return true;
    default: {
      float xy[(COLS + 1) * 4];
      int n = eyeOutline(cx, cy, p, inner, grow, xy);
      if (n == 0) return false;
      c.pathAdd(xy, n);
      return true;
    }
  }
}

void drawPupil(Canvas &c, float cx, float cy, const EyeParams &p, Rgb ink) {
  if (p.pupil < 0.02f) return;
  float w = p.w < 2.0f ? 2.0f : p.w, h = p.h < 2.0f ? 2.0f : p.h;
  float r = fminf(w, h) * 0.5f * p.pupil;
  float rx = r * (p.pupilW > 0.001f ? p.pupilW : 1.0f), ry = r;
  // A slit is as tall as the eye allows.
  if (p.pupilW > 0.001f && p.pupilW < 0.6f) ry = fminf(h * 0.42f, r * 1.7f);
  if (rx < 0.7f || ry < 0.7f) return;
  float roomX = w * 0.5f - rx - 1.5f, roomY = h * 0.5f - ry - 1.5f;
  if (roomX < 0.0f) roomX = 0.0f;
  if (roomY < 0.0f) roomY = 0.0f;
  float x = cx + clampf(p.px, -1.0f, 1.0f) * roomX;
  float y = cy + clampf(p.py, -1.0f, 1.0f) * roomY;
  c.fillEllipse(x, y, rx, ry, darken(ink, 0.92f));
  // a glint gives it life
  if (rx > 3.0f && ry > 3.0f) c.fillCircle(x - rx * 0.35f, y - ry * 0.35f, fmaxf(1.0f, rx * 0.28f), INK_WHITE);
}

}  // namespace

namespace FaceRenderer {

void setGlow(float amount) { glowAmount = clampf(amount, 0.0f, 1.5f); }

float glassHalfWidth(float y) {
  float d = y - DISC_CY;
  float k = DISC_R * DISC_R - d * d;
  return k <= 0.0f ? 0.0f : sqrtf(k);
}

void keepOnGlass(float &x, float &y, float margin) {
  float r = DISC_R - margin;
  if (r < 1.0f) r = 1.0f;
  float dx = x - DISC_CX, dy = y - DISC_CY;
  float d2 = dx * dx + dy * dy;
  if (d2 <= r * r) return;
  float k = r / sqrtf(d2);
  x = DISC_CX + dx * k;
  y = DISC_CY + dy * k;
}

void drawHeart(Canvas &c, float cx, float cy, float rx, float ry, const Paint &p) {
  if (rx < 0.3f || ry < 0.3f) return;
  c.pathBegin();
  pathHeart(c, cx, cy, rx, ry);
  c.pathFill(p);
}

void drawDrop(Canvas &c, float cx, float cy, float r, const Paint &p) {
  if (r < 0.4f) return;
  float tip[6] = {cx - r * 0.92f, cy - r * 0.35f, cx + r * 0.92f, cy - r * 0.35f, cx, cy - r * 2.4f};
  c.pathBegin();
  c.pathAddEllipse(cx, cy, r, r);
  c.pathAdd(tip, 3);
  c.pathFill(p);
}

// Four thin spikes: |x|^0.5 + |y|^0.5 = r^0.5
void drawStar4(Canvas &c, float cx, float cy, float r, const Paint &p) {
  if (r < 0.5f) return;
  const float k = 0.25f;  // where the outline crosses the diagonals
  float xy[16] = {cx, cy - r, cx + r * k, cy - r * k, cx + r, cy, cx + r * k, cy + r * k,
                  cx, cy + r, cx - r * k, cy + r * k, cx - r, cy, cx - r * k, cy - r * k};
  c.fillPoly(xy, 8, p);
}

void drawStar5(Canvas &c, float cx, float cy, float rx, float ry, float rot, const Paint &p) {
  if (rx < 0.6f || ry < 0.6f) return;
  c.pathBegin();
  pathStar5(c, cx, cy, rx, ry, rot);
  c.pathFill(p);
}

void drawBall(Canvas &c, float cx, float cy, float rx, float ry, const Paint &p) {
  c.fillEllipse(cx, cy, rx + 1.6f, ry + 1.6f, Paint(INK_BLACK, p.alpha));
  c.fillEllipse(cx, cy, rx, ry, p);
}

void drawEye(Canvas &c, float cx, float cy, const EyeParams &p, uint8_t kind, bool left, float time, Rgb ink,
             float grow, bool lit) {
  float inner = left ? 1.0f : -1.0f;
  float hh = (p.h < 2.0f ? 2.0f : p.h) * 0.5f;

  if (!lit) {
    c.pathBegin();
    if (pathEye(c, cx, cy, p, kind, inner, time, grow)) c.pathFill(ink);
    return;
  }

  static const float REACH[3] = {6.4f, 3.6f, 1.5f};
  static const float LIGHT[3] = {0.07f, 0.12f, 0.20f};
  // a squashed (blinking) eye shines less
  float shine = glowAmount * clampf(hh / 6.0f, 0.25f, 1.0f);
  float reach = clampf(hh / 8.0f, 0.4f, 1.0f);
  Paint body = Paint::gradient(lighten(ink, 0.34f), darken(ink, 0.06f), cy - hh, cy + hh);

  if (kind == EK_SHAPE) {
    float xy[(COLS + 1) * 4], rim[(COLS + 1) * 4];
    int n = eyeOutline(cx, cy, p, inner, grow, xy);
    if (n == 0) return;
    // halo: light that spills around the eye. Only the rim is painted, the
    // eye itself covers the rest.
    for (int i = 0; i < 3 && shine > 0.01f; i++) {
      int m = eyeOutline(cx, cy, p, inner, grow + REACH[i] * reach, rim);
      if (m == 0) continue;
      c.pathBegin();
      c.pathAdd(rim, m);
      c.pathAdd(xy, n, true);
      c.pathFill(Paint(ink, LIGHT[i] * shine));
    }
    c.pathBegin();
    c.pathAdd(xy, n);
    c.pathFill(body);
    if (grow == 0.0f) drawPupil(c, cx, cy, p, ink);
    return;
  }

  for (int i = 1; i < 3 && shine > 0.01f; i++) {
    c.pathBegin();
    if (pathEye(c, cx, cy, p, kind, inner, time, grow + REACH[i] * reach)) c.pathFill(Paint(ink, LIGHT[i] * shine));
  }
  c.pathBegin();
  if (pathEye(c, cx, cy, p, kind, inner, time, grow)) c.pathFill(body);
}

void drawMouth(Canvas &c, float cx, float cy, const MouthParams &m, Rgb ink) {
  if (m.w < 2.0f) return;
  float hw = m.w * 0.5f;
  float open = clamp01(m.open);
  float gapMax = m.w * 0.42f;
  const int N = 14;

  float mid[(N + 1) * 2];
  float half[N + 1];
  for (int i = 0; i <= N; i++) {
    float q = -1.0f + 2.0f * (float)i / (float)N;
    float k = 1.0f - q * q;
    mid[i * 2] = cx + q * hw;
    mid[i * 2 + 1] = cy + m.curve * k - m.curve * 0.5f - m.tilt * q * 0.5f;
    half[i] = open * gapMax * sqrtf(k);
  }

  if (open * gapMax < 1.2f) {
    // closed: a stroke
    c.polyline(mid, N + 1, 2.2f, ink);
    return;
  }

  // open: dark inside, bright lips
  float lens[(N + 1) * 4];
  for (int i = 0; i <= N; i++) {
    lens[i * 2] = mid[i * 2];
    lens[i * 2 + 1] = mid[i * 2 + 1] - half[i];
    int j = 2 * (N + 1) - 1 - i;
    lens[j * 2] = mid[i * 2];
    lens[j * 2 + 1] = mid[i * 2 + 1] + half[i];
  }
  c.fillPoly(lens, 2 * (N + 1), darken(ink, 0.80f));
  c.polyline(lens, 2 * (N + 1), 1.9f, ink, true);
}

}  // namespace FaceRenderer
