// Drawing surface of the round display: a 360x360 RGB565 framebuffer and an
// antialiased polygon rasterizer.
//
// Two coordinate systems:
//
//   pixels      0..239 on both axes, what the panel has.
//   face units  what the face, the decorations and the acts use. One face unit
//               is 1.875 px. The classic stage of 128 x 64 units sits in the
//               middle of the screen; the glass is a disc of radius 64 units
//               centered on (64, 40), so there is room above (y down to -24)
//               and below (y up to 104) the stage.
//
//                     y = -24  ......  top of the glass
//                     y =   0  +----------------+
//                              |  stage 128x64  |   eyes on the row y = 32
//                     y =  64  +----------------+
//                     y = 104  ......  bottom of the glass
//
// Every drawing call takes face units unless its name ends in "Px".
//
// The ESP32-C6 has no FPU: floats are fine to describe a shape (a few dozen
// numbers per frame) but never per pixel. Shapes become polygons and the
// rasterizer works with integers only.
//
// Pure C++ (no Arduino dependency): the same pixels come out on the robot and
// in the Windows emulator.
#ifndef CORE_CANVAS_H
#define CORE_CANVAS_H

#include <stdint.h>

#include "Color.h"

#define SCREEN_W 360
#define SCREEN_H 360

#define FU_PX 2.8125f       // pixels per face unit (360 / 128)
#define FU_ORIGIN_X 0.0f    // pixel where the face unit x = 0 falls
#define FU_ORIGIN_Y 67.5f   // pixel where the face unit y = 0 falls (180 - 40 * FU_PX)

#define STAGE_W 128
#define STAGE_H 64
#define DISC_CX 64.0f
#define DISC_CY 40.0f
#define DISC_R 64.0f

#define TILE_PX 24
#define TILES (SCREEN_W / TILE_PX)  // 15 x 15 tiles

// How a shape is filled: one color, or a vertical gradient between two rows.
struct Paint {
  Rgb a, b;       // color on the row y0 and on the row y1
  float y0, y1;   // face units; y1 <= y0 means plain color `a`
  float alpha;    // 0..1

  Paint(Rgb color = INK_WHITE, float alpha_ = 1.0f) : a(color), b(color), y0(0.0f), y1(0.0f), alpha(alpha_) {}
  static Paint gradient(Rgb top, Rgb bottom, float yTop, float yBottom, float alpha_ = 1.0f) {
    Paint p(top, alpha_);
    p.b = bottom;
    p.y0 = yTop;
    p.y1 = yBottom;
    return p;
  }
};

class Canvas {
 public:
  Canvas();
  ~Canvas();
  Canvas(const Canvas &) = delete;
  Canvas &operator=(const Canvas &) = delete;

  // Allocates the framebuffer (115 KB). False when there is no memory.
  bool begin();
  bool ready() const { return fb_ != nullptr; }

  // Black screen. Only the tiles drawn by the frame before are erased.
  void clear();

  const uint16_t *pixels() const { return fb_; }
  uint16_t *pixels() { return fb_; }

  // Tiles that may differ from the frame before: bit x of row y.
  uint16_t changedTiles(int tileRow) const { return (uint16_t)(now_[tileRow] | prev_[tileRow]); }
  // Forget the history: every tile counts as changed once.
  void touchAll();

  static inline float px(float faceUnits) { return faceUnits * FU_PX; }
  static inline float toPxX(float x) { return FU_ORIGIN_X + x * FU_PX; }
  static inline float toPxY(float y) { return FU_ORIGIN_Y + y * FU_PX; }
  static inline float fromPxX(float x) { return (x - FU_ORIGIN_X) / FU_PX; }
  static inline float fromPxY(float y) { return (y - FU_ORIGIN_Y) / FU_PX; }

  // ---- shapes (face units) -----------------------------------------------------
  void fillRect(float x, float y, float w, float h, const Paint &p);
  void fillRoundRect(float x, float y, float w, float h, float r, const Paint &p);
  void rect(float x, float y, float w, float h, float thickness, const Paint &p);
  void fillCircle(float cx, float cy, float r, const Paint &p);
  void fillEllipse(float cx, float cy, float rx, float ry, const Paint &p);
  void ring(float cx, float cy, float r, float thickness, const Paint &p);
  void ellipseRing(float cx, float cy, float rx, float ry, float thickness, const Paint &p);
  // Pie slice or, with `inner` > 0, a piece of ring. Angles in radians,
  // clockwise on screen, 0 = to the right.
  void fillSector(float cx, float cy, float r, float inner, float a0, float a1, const Paint &p);
  void fillTriangle(float x0, float y0, float x1, float y1, float x2, float y2, const Paint &p);
  // Closed polygon, `xy` holds n pairs.
  void fillPoly(const float *xy, int n, const Paint &p);
  // Line with round ends.
  void thickLine(float x0, float y0, float x1, float y1, float thickness, const Paint &p);
  void line(float x0, float y0, float x1, float y1, const Paint &p) { thickLine(x0, y0, x1, y1, 1.0f, p); }
  // Chain of lines with round joints, painted as one shape.
  void polyline(const float *xy, int n, float thickness, const Paint &p, bool closed = false);
  void arc(float cx, float cy, float r, float a0, float a1, float thickness, const Paint &p);

  // ---- paths: several outlines painted as one shape --------------------------------
  // Outlines that overlap are merged; an outline added as `hole` is carved out.
  void pathBegin();
  void pathAdd(const float *xy, int n, bool hole = false);
  void pathAddEllipse(float cx, float cy, float rx, float ry, bool hole = false);
  void pathAddLine(float x0, float y0, float x1, float y1, float thickness);
  void pathFill(const Paint &p);

  // cos and sin of an angle from a table: cheap, good to 0.1 %.
  static void unit(float angle, float &c, float &s);

  // ---- text: 5x7 font ------------------------------------------------------------
  // `scale` 1 draws dots of 2 px (letters 10 x 14 px), 2 -> 4 px, 3 -> 6 px...
  // x, y = top left corner. Returns the x where the text ends.
  float text(float x, float y, const char *s, int scale, const Paint &p);
  static float textWidth(const char *s, int scale);
  static float textHeight(int scale);
  void textCentered(float cx, float y, const char *s, int scale, const Paint &p);

  // ---- straight to the pixels -----------------------------------------------------
  void fillRectPx(int x, int y, int w, int h, Rgb color, float alpha = 1.0f);
  // One glyph of a 5 x 5 bitmap (bit 24 = top left), dots of `dotPx` pixels.
  void glyph5Px(int x, int y, uint32_t bits, int dotPx, Rgb color, float alpha = 1.0f);
  // Slides the rows y0..y1 sideways (what leaves on one side is lost).
  void shiftRowsPx(int y0, int y1, int dx);
  // Multiplies the brightness of the rows y0..y1 by k (0..1).
  void dimRowsPx(int y0, int y1, float k);
  // Lays a color over the rows y0..y1.
  void tintRowsPx(int y0, int y1, Rgb color, float alpha);
  // Photo negative of the rows y0..y1.
  void invertRowsPx(int y0, int y1);
  // Squashes the picture towards the middle row, like an old TV tube that
  // opens or closes. scale 1 = untouched, 0.1 = a thin stripe.
  void squashPx(float scale);
  // Enlarges what is inside the circle (a magnifying glass). r up to 31 px.
  void magnifyPx(int cx, int cy, int r, float zoom);
  Rgb getPx(int x, int y) const;

 private:
  struct Edge {
    int32_t x;    // 16.16, at the sample being scanned
    int32_t dx;   // per sample
    int16_t y0, y1;  // first and last sample (4 per pixel row)
    int16_t dir;
  };

  void addEdge(float x0, float y0, float x1, float y1);
  void span(int32_t xa, int32_t xb);
  void markRow(int y, int x0, int x1);
  void circlePoints(float cx, float cy, float rx, float ry, float *xy, int n) const;
  static int circleSteps(float rPx);

  uint16_t *fb_ = nullptr;
  uint16_t now_[TILES], prev_[TILES];

  static const int MAX_EDGES = 720;
  static const int MAX_ACTIVE = 96;
  Edge edges_[MAX_EDGES];
  uint16_t order_[MAX_EDGES];
  uint16_t active_[MAX_ACTIVE];
  int edgeCount_ = 0;
  int16_t delta_[SCREEN_W + 2];
  uint16_t part_[SCREEN_W + 2];
  int rowMin_ = 0, rowMax_ = 0;
};

#endif
