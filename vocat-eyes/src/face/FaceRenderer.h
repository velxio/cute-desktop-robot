// Rasterizer of the procedural face: turns eye / mouth parameters into shapes
// for the canvas. Everything is in face units.
#ifndef FACE_FACERENDERER_H
#define FACE_FACERENDERER_H

#include "../core/Canvas.h"
#include "FaceTypes.h"

// Everything the decorations need to know about the frame being drawn.
struct FaceFrame {
  float cx[2], cy[2];  // eye centers
  float w[2], h[2];    // eye sizes after all the layers were applied
  float gazeX, gazeY;  // -1..1
  float mouthX, mouthY, mouthW;
  float time;          // seconds, free running
  Rgb ink;             // color of the face right now
};

namespace FaceRenderer {

// `left` tells on which side of the screen the eye is (needed to mirror the
// slopes: the nose side of the left eye is on its right).
// Here `p.px / p.py` are the final position of the pupil in screen terms
// (-1 = left / up edge of the eye, +1 = right / down edge).
//
// lit = true   the eye as it shines: glow, shaded body and pupil.
// lit = false  a flat copy in the color `ink`, `grow` units bigger (or smaller).
//              A black copy drawn first leaves a rim that separates the eye
//              from whatever is behind it.
void drawEye(Canvas &c, float cx, float cy, const EyeParams &p, uint8_t kind, bool left, float time, Rgb ink,
             float grow = 0.0f, bool lit = true);

void drawMouth(Canvas &c, float cx, float cy, const MouthParams &m, Rgb ink);

// Primitive shapes reused by the decorations and the acts.
void drawHeart(Canvas &c, float cx, float cy, float rx, float ry, const Paint &p);
void drawDrop(Canvas &c, float cx, float cy, float r, const Paint &p);
void drawStar4(Canvas &c, float cx, float cy, float r, const Paint &p);
void drawStar5(Canvas &c, float cx, float cy, float rx, float ry, float rot, const Paint &p);
// Disc with a dark rim, visible over both the dark screen and a lit eye.
void drawBall(Canvas &c, float cx, float cy, float rx, float ry, const Paint &p);

// The glass is round: half of its width on the row `y` (0 outside the glass).
float glassHalfWidth(float y);
// Moves a point inside the glass, `margin` units away from the edge.
void keepOnGlass(float &x, float &y, float margin);

// How strong the halo around the eyes is, 0 = none (saves a few ms per frame).
void setGlow(float amount);

}  // namespace FaceRenderer

#endif
