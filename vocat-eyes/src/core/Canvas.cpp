#include "Canvas.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Font5x7.h"

namespace {

const int SUB = 4;           // samples per pixel row
const int UNIT = 256 / SUB;  // what one fully covered sample adds
const int SAMPLES = SCREEN_H * SUB;
const int32_t X_MAX = (int32_t)SCREEN_W << 16;

// Unit circle in 64 steps, built once. Angles grow clockwise on screen.
const int CIRCLE = 64;
float circleX[CIRCLE + 1], circleY[CIRCLE + 1];
bool circleReady = false;

void initCircle() {
  if (circleReady) return;
  for (int i = 0; i <= CIRCLE; i++) {
    float a = (float)i * (6.28318530718f / CIRCLE);
    circleX[i] = cosf(a);
    circleY[i] = sinf(a);
  }
  circleReady = true;
}

// cos / sin from the table (straight lines between its 64 points).
void unitAt(float angle, float &c, float &s) {
  float k = angle * (CIRCLE / 6.28318530718f);
  float f = floorf(k);
  int i = (int)f % CIRCLE;
  if (i < 0) i += CIRCLE;
  float t = k - f;
  c = circleX[i] + (circleX[i + 1] - circleX[i]) * t;
  s = circleY[i] + (circleY[i + 1] - circleY[i]) * t;
}

inline uint16_t tileMask(int x0, int x1) {
  int t0 = x0 / TILE_PX, t1 = x1 / TILE_PX;
  return (uint16_t)(((1u << (t1 + 1)) - 1u) & ~((1u << t0) - 1u));
}

}  // namespace

Canvas::Canvas() {
  memset(now_, 0, sizeof(now_));
  memset(prev_, 0, sizeof(prev_));
  memset(delta_, 0, sizeof(delta_));
  memset(part_, 0, sizeof(part_));
}

Canvas::~Canvas() { free(fb_); }

bool Canvas::begin() {
  initCircle();
  if (!fb_) {
    fb_ = (uint16_t *)malloc((size_t)SCREEN_W * SCREEN_H * sizeof(uint16_t));
    if (!fb_) return false;
    memset(fb_, 0, (size_t)SCREEN_W * SCREEN_H * sizeof(uint16_t));
    touchAll();
  }
  return true;
}

void Canvas::touchAll() {
  for (int i = 0; i < TILES; i++) now_[i] = prev_[i] = 0x7FFF;
}

void Canvas::clear() {
  if (!fb_ && !begin()) return;
  for (int ty = 0; ty < TILES; ty++) {
    uint16_t m = now_[ty];
    prev_[ty] = m;
    now_[ty] = 0;
    if (!m) continue;
    int tx = 0;
    while (tx < TILES) {
      if (!(m & (1u << tx))) {
        tx++;
        continue;
      }
      int start = tx;
      while (tx < TILES && (m & (1u << tx))) tx++;
      size_t bytes = (size_t)(tx - start) * TILE_PX * sizeof(uint16_t);
      for (int y = ty * TILE_PX; y < (ty + 1) * TILE_PX; y++) memset(fb_ + y * SCREEN_W + start * TILE_PX, 0, bytes);
    }
  }
}

void Canvas::markRow(int y, int x0, int x1) {
  if (x0 < 0) x0 = 0;
  if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
  if (x1 < x0 || (unsigned)y >= SCREEN_H) return;
  now_[y / TILE_PX] |= tileMask(x0, x1);
}

Rgb Canvas::getPx(int x, int y) const {
  if (!fb_ || (unsigned)x >= SCREEN_W || (unsigned)y >= SCREEN_H) return 0;
  return from565(fb_[y * SCREEN_W + x]);
}

// ---- rasterizer --------------------------------------------------------------------
void Canvas::pathBegin() {
  initCircle();
  edgeCount_ = 0;
}

// Pixels in, one edge of the outline out.
void Canvas::addEdge(float x0, float y0, float x1, float y1) {
  if (edgeCount_ >= MAX_EDGES) return;
  float ya = y0 * SUB, yb = y1 * SUB;
  int dir = 1;
  if (ya > yb) {
    float t = ya;
    ya = yb;
    yb = t;
    t = x0;
    x0 = x1;
    x1 = t;
    dir = -1;
  }
  // samples sit in the middle of their strip
  int i0 = (int)ceilf(ya - 0.5f);
  int i1 = (int)ceilf(yb - 0.5f) - 1;
  if (i1 < i0 || i1 < 0 || i0 >= SAMPLES) return;
  float slope = (x1 - x0) / (yb - ya);
  float x = x0 + ((float)i0 + 0.5f - ya) * slope;
  if (i0 < 0) {
    x += (float)(-i0) * slope;
    i0 = 0;
  }
  if (i1 >= SAMPLES) i1 = SAMPLES - 1;
  // keep the numbers inside 16.16
  if (x > 16000.0f) x = 16000.0f;
  if (x < -16000.0f) x = -16000.0f;
  if (slope > 8000.0f) slope = 8000.0f;
  if (slope < -8000.0f) slope = -8000.0f;
  Edge &e = edges_[edgeCount_++];
  e.x = (int32_t)(x * 65536.0f);
  e.dx = (int32_t)(slope * 65536.0f);
  e.y0 = (int16_t)i0;
  e.y1 = (int16_t)i1;
  e.dir = (int16_t)dir;
}

void Canvas::unit(float angle, float &c, float &s) {
  initCircle();
  unitAt(angle, c, s);
}

void Canvas::pathAdd(const float *xy, int n, bool hole) {
  if (n < 3) return;
  // Solid outlines all turn the same way, holes the other way.
  float area = 0.0f;
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    area += xy[i * 2] * xy[j * 2 + 1] - xy[j * 2] * xy[i * 2 + 1];
  }
  bool reversed = (area < 0.0f) != hole;
  float px0 = toPxX(xy[(n - 1) * 2]), py0 = toPxY(xy[(n - 1) * 2 + 1]);
  for (int i = 0; i < n; i++) {
    float px1 = toPxX(xy[i * 2]), py1 = toPxY(xy[i * 2 + 1]);
    if (reversed) addEdge(px1, py1, px0, py0);
    else addEdge(px0, py0, px1, py1);
    px0 = px1;
    py0 = py1;
  }
}

// Coverage of one sample between xa and xb (16.16 pixels).
void Canvas::span(int32_t xa, int32_t xb) {
  if (xb <= 0 || xa >= X_MAX || xb <= xa) return;
  if (xa < 0) xa = 0;
  if (xb > X_MAX) xb = X_MAX;
  int ia = xa >> 16, ib = xb >> 16;
  int32_t fa = xa & 0xFFFF, fb = xb & 0xFFFF;
  if (ia == ib) {
    part_[ia] = (uint16_t)(part_[ia] + (((fb - fa) * UNIT) >> 16));
  } else {
    part_[ia] = (uint16_t)(part_[ia] + (((65536 - fa) * UNIT) >> 16));
    part_[ib] = (uint16_t)(part_[ib] + ((fb * UNIT) >> 16));
    delta_[ia + 1] = (int16_t)(delta_[ia + 1] + UNIT);
    delta_[ib] = (int16_t)(delta_[ib] - UNIT);
  }
  if (ia < rowMin_) rowMin_ = ia;
  if (ib > rowMax_) rowMax_ = ib;
}

void Canvas::pathFill(const Paint &p) {
  int n = edgeCount_;
  edgeCount_ = 0;
  if (n == 0 || p.alpha <= 0.004f) return;
  if (!fb_ && !begin()) return;

  for (int i = 0; i < n; i++) order_[i] = (uint16_t)i;
  for (int gap = n / 2; gap > 0; gap /= 2) {
    for (int i = gap; i < n; i++) {
      uint16_t t = order_[i];
      int key = edges_[t].y0;
      int j = i;
      while (j >= gap && edges_[order_[j - gap]].y0 > key) {
        order_[j] = order_[j - gap];
        j -= gap;
      }
      order_[j] = t;
    }
  }
  int first = edges_[order_[0]].y0, last = 0;
  for (int i = 0; i < n; i++)
    if (edges_[i].y1 > last) last = edges_[i].y1;

  int alpha = (int)((p.alpha > 1.0f ? 1.0f : p.alpha) * 256.0f + 0.5f);
  bool grad = p.y1 > p.y0 && p.a != p.b;
  uint16_t color = to565(p.a);
  int g0 = 0, gSpan = 1;
  if (grad) {
    g0 = (int)(toPxY(p.y0) * 16.0f);
    gSpan = (int)(toPxY(p.y1) * 16.0f) - g0;
    if (gSpan < 1) gSpan = 1;
  }

  int next = 0, nActive = 0;
  for (int row = first / SUB; row <= last / SUB; row++) {
    rowMin_ = SCREEN_W;
    rowMax_ = -1;
    for (int s = 0; s < SUB; s++) {
      int y = row * SUB + s;
      for (int i = 0; i < nActive;) {
        if (edges_[active_[i]].y1 < y) active_[i] = active_[--nActive];
        else i++;
      }
      while (next < n && edges_[order_[next]].y0 <= y) {
        if (nActive < MAX_ACTIVE && edges_[order_[next]].y1 >= y) active_[nActive++] = order_[next];
        next++;
      }
      if (nActive < 2) continue;
      for (int i = 1; i < nActive; i++) {
        uint16_t t = active_[i];
        int32_t key = edges_[t].x;
        int j = i;
        while (j > 0 && edges_[active_[j - 1]].x > key) {
          active_[j] = active_[j - 1];
          j--;
        }
        active_[j] = t;
      }
      int wind = 0;
      int32_t from = 0;
      for (int i = 0; i < nActive; i++) {
        Edge &e = edges_[active_[i]];
        if (wind == 0) from = e.x;
        wind += e.dir;
        if (wind == 0) span(from, e.x);
        e.x += e.dx;
      }
    }
    if (rowMax_ < rowMin_) continue;
    if (rowMax_ >= SCREEN_W) {
      delta_[SCREEN_W] = 0;
      part_[SCREEN_W] = 0;
      rowMax_ = SCREEN_W - 1;
    }

    if (grad) {
      int k = ((row * 16 + 8 - g0) * 256) / gSpan;
      k = k < 0 ? 0 : (k > 256 ? 256 : k);
      color = to565(rgb(redOf(p.a) + (((redOf(p.b) - redOf(p.a)) * k) >> 8),
                        greenOf(p.a) + (((greenOf(p.b) - greenOf(p.a)) * k) >> 8),
                        blueOf(p.a) + (((blueOf(p.b) - blueOf(p.a)) * k) >> 8)));
    }

    uint16_t *dst = fb_ + row * SCREEN_W;
    int run = 0;
    for (int x = rowMin_; x <= rowMax_; x++) {
      run += delta_[x];
      int cov = run + part_[x];
      delta_[x] = 0;
      part_[x] = 0;
      if (cov <= 0) continue;
      if (cov > 256) cov = 256;
      int a = (cov * alpha) >> 8;
      if (a >= 250) dst[x] = color;
      else if (a >= 4) dst[x] = blend565(dst[x], color, (uint32_t)((a + 4) >> 3));
    }
    delta_[rowMax_ + 1] = 0;
    markRow(row, rowMin_, rowMax_);
  }
}

// ---- shapes ------------------------------------------------------------------------
int Canvas::circleSteps(float rPx) {
  if (rPx < 3.0f) return 8;
  if (rPx < 12.0f) return 16;
  if (rPx < 48.0f) return 32;
  return 64;
}

// Points of an ellipse, in face units.
void Canvas::circlePoints(float cx, float cy, float rx, float ry, float *xy, int n) const {
  int step = CIRCLE / n;
  for (int i = 0; i < n; i++) {
    xy[i * 2] = cx + circleX[i * step] * rx;
    xy[i * 2 + 1] = cy + circleY[i * step] * ry;
  }
}

void Canvas::pathAddEllipse(float cx, float cy, float rx, float ry, bool hole) {
  if (rx <= 0.0f || ry <= 0.0f) return;
  initCircle();
  int n = circleSteps((rx > ry ? rx : ry) * FU_PX);
  float xy[CIRCLE * 2];
  circlePoints(cx, cy, rx, ry, xy, n);
  pathAdd(xy, n, hole);
}

void Canvas::fillPoly(const float *xy, int n, const Paint &p) {
  pathBegin();
  pathAdd(xy, n);
  pathFill(p);
}

void Canvas::fillRect(float x, float y, float w, float h, const Paint &p) {
  if (w <= 0.0f || h <= 0.0f) return;
  float xy[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
  fillPoly(xy, 4, p);
}

void Canvas::rect(float x, float y, float w, float h, float t, const Paint &p) {
  if (w <= 0.0f || h <= 0.0f) return;
  if (t * 2.0f >= w || t * 2.0f >= h) {
    fillRect(x, y, w, h, p);
    return;
  }
  float out[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
  float in[8] = {x + t, y + t, x + w - t, y + t, x + w - t, y + h - t, x + t, y + h - t};
  pathBegin();
  pathAdd(out, 4);
  pathAdd(in, 4, true);
  pathFill(p);
}

void Canvas::fillRoundRect(float x, float y, float w, float h, float r, const Paint &p) {
  if (w <= 0.0f || h <= 0.0f) return;
  float m = (w < h ? w : h) * 0.5f;
  if (r > m) r = m;
  if (r < 0.3f) {
    fillRect(x, y, w, h, p);
    return;
  }
  initCircle();
  // a quarter of circle on every corner
  int q = circleSteps(r * FU_PX) / 4;
  int step = CIRCLE / (q * 4);
  float xy[2 * 4 * 17];
  int n = 0;
  const float cxs[4] = {x + w - r, x + r, x + r, x + w - r};
  const float cys[4] = {y + h - r, y + h - r, y + r, y + r};
  for (int c = 0; c < 4; c++) {
    for (int i = 0; i <= q; i++) {
      int k = (c * q + i) * step;
      xy[n * 2] = cxs[c] + circleX[k] * r;
      xy[n * 2 + 1] = cys[c] + circleY[k] * r;
      n++;
    }
  }
  fillPoly(xy, n, p);
}

void Canvas::fillEllipse(float cx, float cy, float rx, float ry, const Paint &p) {
  if (rx <= 0.0f || ry <= 0.0f) return;
  initCircle();
  int n = circleSteps((rx > ry ? rx : ry) * FU_PX);
  float xy[CIRCLE * 2];
  circlePoints(cx, cy, rx, ry, xy, n);
  fillPoly(xy, n, p);
}

void Canvas::fillCircle(float cx, float cy, float r, const Paint &p) { fillEllipse(cx, cy, r, r, p); }

void Canvas::ellipseRing(float cx, float cy, float rx, float ry, float t, const Paint &p) {
  if (rx <= 0.0f || ry <= 0.0f) return;
  float ix = rx - t, iy = ry - t;
  if (ix <= 0.2f || iy <= 0.2f) {
    fillEllipse(cx, cy, rx, ry, p);
    return;
  }
  pathBegin();
  pathAddEllipse(cx, cy, rx, ry);
  pathAddEllipse(cx, cy, ix, iy, true);
  pathFill(p);
}

// `r` is the outer radius.
void Canvas::ring(float cx, float cy, float r, float t, const Paint &p) { ellipseRing(cx, cy, r, r, t, p); }

void Canvas::fillSector(float cx, float cy, float r, float inner, float a0, float a1, const Paint &p) {
  if (r <= 0.0f || a1 <= a0) return;
  initCircle();
  float sweep = a1 - a0;
  if (sweep > 6.2831853f) sweep = 6.2831853f;
  int n = (int)ceilf(sweep / 6.2831853f * (float)circleSteps(r * FU_PX));
  if (n < 2) n = 2;
  if (n > CIRCLE) n = CIRCLE;
  float xy[(CIRCLE + 1) * 4 + 2];
  int k = 0;
  for (int i = 0; i <= n; i++) {
    float c, s;
    unitAt(a0 + sweep * (float)i / (float)n, c, s);
    xy[k * 2] = cx + c * r;
    xy[k * 2 + 1] = cy + s * r;
    k++;
  }
  if (inner > 0.05f && inner < r) {
    for (int i = n; i >= 0; i--) {
      float c, s;
      unitAt(a0 + sweep * (float)i / (float)n, c, s);
      xy[k * 2] = cx + c * inner;
      xy[k * 2 + 1] = cy + s * inner;
      k++;
    }
  } else {
    xy[k * 2] = cx;
    xy[k * 2 + 1] = cy;
    k++;
  }
  fillPoly(xy, k, p);
}

void Canvas::fillTriangle(float x0, float y0, float x1, float y1, float x2, float y2, const Paint &p) {
  float xy[6] = {x0, y0, x1, y1, x2, y2};
  fillPoly(xy, 3, p);
}

// Outline of a line with round ends, in face units. Returns the point count.
static int capsule(float x0, float y0, float x1, float y1, float r, int steps, float *xy) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  float ux = 1.0f, uy = 0.0f;
  if (len > 0.001f) {
    ux = dx / len;
    uy = dy / len;
  }
  float vx = -uy, vy = ux;
  int half = steps / 2, stride = CIRCLE / steps;
  int n = 0;
  // around the far end: from -90 to +90 degrees of the direction
  for (int i = -half / 2; i <= half / 2; i++) {
    int k = (i * stride + CIRCLE) % CIRCLE;
    float c = circleX[k], s = circleY[k];
    xy[n * 2] = x1 + (ux * c + vx * s) * r;
    xy[n * 2 + 1] = y1 + (uy * c + vy * s) * r;
    n++;
  }
  // around the near end: from 90 to 270
  for (int i = half / 2; i <= half + half / 2; i++) {
    int k = (i * stride) % CIRCLE;
    float c = circleX[k], s = circleY[k];
    xy[n * 2] = x0 + (ux * c + vx * s) * r;
    xy[n * 2 + 1] = y0 + (uy * c + vy * s) * r;
    n++;
  }
  return n;
}

void Canvas::pathAddLine(float x0, float y0, float x1, float y1, float t) {
  if (t <= 0.0f) return;
  initCircle();
  int steps = circleSteps(t * 0.5f * FU_PX);
  // long chains: coarser ends, so the outline fits in the edge table
  if (edgeCount_ > MAX_EDGES / 2 && steps > 8) steps = 8;
  float xy[(CIRCLE + 4) * 2];
  int n = capsule(x0, y0, x1, y1, t * 0.5f, steps, xy);
  pathAdd(xy, n);
}

void Canvas::thickLine(float x0, float y0, float x1, float y1, float t, const Paint &p) {
  pathBegin();
  pathAddLine(x0, y0, x1, y1, t);
  pathFill(p);
}

void Canvas::polyline(const float *pts, int n, float t, const Paint &p, bool closed) {
  if (n < 2 || t <= 0.0f) return;
  pathBegin();
  int last = closed ? n : n - 1;
  for (int i = 0; i < last; i++) {
    int j = (i + 1) % n;
    pathAddLine(pts[i * 2], pts[i * 2 + 1], pts[j * 2], pts[j * 2 + 1], t);
  }
  pathFill(p);
}

void Canvas::arc(float cx, float cy, float r, float a0, float a1, float t, const Paint &p) {
  if (a1 <= a0 || r <= 0.0f) return;
  initCircle();
  float sweep = a1 - a0;
  int n = (int)ceilf(sweep / 6.2831853f * (float)circleSteps(r * FU_PX));
  if (n < 2) n = 2;
  if (n > 32) n = 32;
  float pts[33 * 2];
  for (int i = 0; i <= n; i++) {
    float c, s;
    unitAt(a0 + sweep * (float)i / (float)n, c, s);
    pts[i * 2] = cx + c * r;
    pts[i * 2 + 1] = cy + s * r;
  }
  polyline(pts, n + 1, t, p);
}

// ---- pixels -------------------------------------------------------------------------
void Canvas::fillRectPx(int x, int y, int w, int h, Rgb color, float alpha) {
  if (!fb_ && !begin()) return;
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > SCREEN_W) w = SCREEN_W - x;
  if (y + h > SCREEN_H) h = SCREEN_H - y;
  if (w <= 0 || h <= 0 || alpha <= 0.004f) return;
  uint16_t c = to565(color);
  int a = (int)((alpha > 1.0f ? 1.0f : alpha) * 32.0f + 0.5f);
  for (int j = y; j < y + h; j++) {
    uint16_t *dst = fb_ + j * SCREEN_W + x;
    if (a >= 32) {
      for (int i = 0; i < w; i++) dst[i] = c;
    } else {
      for (int i = 0; i < w; i++) dst[i] = blend565(dst[i], c, (uint32_t)a);
    }
    markRow(j, x, x + w - 1);
  }
}

void Canvas::shiftRowsPx(int y0, int y1, int dx) {
  if (!fb_ || dx == 0) return;
  if (y0 < 0) y0 = 0;
  if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
  if (dx >= SCREEN_W || dx <= -SCREEN_W) dx = dx > 0 ? SCREEN_W - 1 : -(SCREEN_W - 1);
  for (int y = y0; y <= y1; y++) {
    uint16_t *row = fb_ + y * SCREEN_W;
    if (dx > 0) {
      memmove(row + dx, row, (size_t)(SCREEN_W - dx) * sizeof(uint16_t));
      memset(row, 0, (size_t)dx * sizeof(uint16_t));
    } else {
      memmove(row, row - dx, (size_t)(SCREEN_W + dx) * sizeof(uint16_t));
      memset(row + SCREEN_W + dx, 0, (size_t)(-dx) * sizeof(uint16_t));
    }
    markRow(y, 0, SCREEN_W - 1);
  }
}

void Canvas::dimRowsPx(int y0, int y1, float k) {
  if (!fb_) return;
  if (y0 < 0) y0 = 0;
  if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
  int a = (int)((k < 0.0f ? 0.0f : (k > 1.0f ? 1.0f : k)) * 32.0f + 0.5f);
  if (a >= 32) return;
  for (int y = y0; y <= y1; y++) {
    uint16_t *row = fb_ + y * SCREEN_W;
    for (int x = 0; x < SCREEN_W; x++)
      if (row[x]) row[x] = blend565(0, row[x], (uint32_t)a);
    // nothing new was lit: the tiles are already marked
  }
}

void Canvas::invertRowsPx(int y0, int y1) {
  if (!fb_) return;
  if (y0 < 0) y0 = 0;
  if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
  for (int y = y0; y <= y1; y++) {
    uint16_t *row = fb_ + y * SCREEN_W;
    for (int x = 0; x < SCREEN_W; x++) row[x] = (uint16_t)~row[x];
    markRow(y, 0, SCREEN_W - 1);
  }
}

void Canvas::squashPx(float scale) {
  if (!fb_ || scale >= 0.999f) return;
  const int mid = SCREEN_H / 2;
  const size_t bytes = (size_t)SCREEN_W * sizeof(uint16_t);
  if (scale < 0.004f) scale = 0.004f;
  int inv = (int)(256.0f / scale);  // 24.8
  // From the middle outwards: the row that is read is always further out
  // than the one that is written, so no copy of the picture is needed.
  for (int k = 0; k < mid; k++) {
    int src = (int)(((long)k * inv) >> 8);
    for (int side = 0; side < 2; side++) {
      int d = side ? mid + k : mid - 1 - k;
      int s = side ? mid + src : mid - 1 - src;
      if (s < 0 || s >= SCREEN_H) memset(fb_ + d * SCREEN_W, 0, bytes);
      else if (s != d) memcpy(fb_ + d * SCREEN_W, fb_ + s * SCREEN_W, bytes);
    }
  }
  touchAll();
}

void Canvas::magnifyPx(int cx, int cy, int r, float zoom) {
  static uint16_t copy[64 * 64];
  if (!fb_ || r < 2 || zoom <= 1.0f) return;
  if (r > 31) r = 31;
  const int side = 2 * r + 1;
  for (int j = 0; j < side; j++) {
    int y = cy - r + j;
    for (int i = 0; i < side; i++) {
      int x = cx - r + i;
      copy[j * 64 + i] = ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H) ? fb_[y * SCREEN_W + x] : 0;
    }
  }
  int inv = (int)(256.0f / zoom);
  for (int j = -r; j <= r; j++) {
    int y = cy + j;
    if ((unsigned)y >= SCREEN_H) continue;
    int x0 = SCREEN_W, x1 = -1;
    for (int i = -r; i <= r; i++) {
      int x = cx + i;
      if ((unsigned)x >= SCREEN_W || i * i + j * j > r * r) continue;
      int si = (i * inv) / 256, sj = (j * inv) / 256;
      fb_[y * SCREEN_W + x] = copy[(sj + r) * 64 + (si + r)];
      if (x < x0) x0 = x;
      if (x > x1) x1 = x;
    }
    if (x1 >= x0) markRow(y, x0, x1);
  }
}

void Canvas::tintRowsPx(int y0, int y1, Rgb color, float alpha) {
  if (y0 < 0) y0 = 0;
  if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
  if (y1 < y0) return;
  fillRectPx(0, y0, SCREEN_W, y1 - y0 + 1, color, alpha);
}

// ---- text ---------------------------------------------------------------------------
static inline int dotOf(int scale) {
  if (scale < 1) scale = 1;
  return (int)((float)scale * FU_PX + 0.5f);
}

float Canvas::textWidth(const char *s, int scale) {
  int n = (int)strlen(s);
  if (n == 0) return 0.0f;
  return (float)((n * 6 - 1) * dotOf(scale)) / FU_PX;
}

float Canvas::textHeight(int scale) { return (float)(7 * dotOf(scale)) / FU_PX; }

float Canvas::text(float x, float y, const char *s, int scale, const Paint &p) {
  int dot = dotOf(scale);
  int px0 = (int)floorf(toPxX(x) + 0.5f), py0 = (int)floorf(toPxY(y) + 0.5f);
  for (; *s; s++, px0 += 6 * dot) {
    unsigned char ch = (unsigned char)*s;
    if (ch < 0x20 || ch > 0x7E) ch = '?';
    const uint8_t *g = FONT5X7[ch - 0x20];
    for (int col = 0; col < 5; col++) {
      uint8_t bits = g[col];
      int row = 0;
      while (row < 7) {
        if (!(bits & (1 << row))) {
          row++;
          continue;
        }
        int start = row;
        while (row < 7 && (bits & (1 << row))) row++;
        fillRectPx(px0 + col * dot, py0 + start * dot, dot, (row - start) * dot, p.a, p.alpha);
      }
    }
  }
  return fromPxX((float)px0);
}

void Canvas::textCentered(float cx, float y, const char *s, int scale, const Paint &p) {
  text(cx - textWidth(s, scale) * 0.5f, y, s, scale, p);
}

void Canvas::glyph5Px(int x, int y, uint32_t bits, int dotPx, Rgb color, float alpha) {
  if (dotPx < 1) dotPx = 1;
  if (x <= -5 * dotPx || y <= -5 * dotPx || x >= SCREEN_W || y >= SCREEN_H) return;
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      if (bits & (1u << (24 - (r * 5 + c)))) fillRectPx(x + c * dotPx, y + r * dotPx, dotPx, dotPx, color, alpha);
    }
  }
}
