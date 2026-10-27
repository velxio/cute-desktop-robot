// Acts: short animations where the face does something (plays pong, sneezes,
// follows a fly...). They tell a small story, and they are computed, not
// stored: each act is a pure function of time that moves the eyes and draws
// its props.
//
// An act starts and ends on its base face, so it hangs from the emotion graph
// like any other node: the engine walks to the base face, plays the act and
// carries on.
#ifndef FACE_ACTS_H
#define FACE_ACTS_H

#include "../core/Canvas.h"
#include "FaceRenderer.h"
#include "FaceTypes.h"

// When the props of an act are drawn.
enum ActLayer : uint8_t {
  ACT_BACK = 0,  // before the eyes (the eyes cover them)
  ACT_FRONT,     // after the eyes
  ACT_POST       // effects over the finished frame
};

// What an act controls on every frame. It arrives filled with the base face.
struct ActFrame {
  float t, tPrev;  // seconds since the act started, now and on the frame before
  float duration;
  FacePose pose;
  uint8_t kind[2];
  bool eyeOn[2];      // false = that eye is not drawn
  float dx[2], dy[2]; // extra offset of each eye, px on screen
  float gazeX, gazeY; // where it looks, -1..1 (+x right, +y up)
  float blink[2];     // 0 open .. 1 shut
  bool halo;          // draw a dark rim around the eyes, so props behind them read well
  Rgb ink;            // color of the eyes and the mouth
  EarDef ears;
  uint8_t chirp;      // sound to start on this frame

  // True on the frame where the act goes past the moment `at`.
  bool crossed(float at) const { return tPrev < at && t >= at; }
};

typedef void (*ActPoseFn)(ActFrame &f);
typedef void (*ActDrawFn)(Canvas &c, const ActFrame &f, const FaceFrame &face, uint8_t layer);

struct ActDef {
  const char *name;
  float duration;  // seconds
  uint8_t base;    // face where it starts and ends; EX_NONE = over whatever face is on
  uint8_t moods;   // moods that like to play it, bit mask (1 << MOOD_x)
  ActPoseFn pose;
  ActDrawFn draw;  // may be null
};

#define MOODS_ANY 0xFF
#define IN_MOOD(m) (1u << (m))

namespace Acts {
int count();
const ActDef &get(int i);
// -1 when not found (case insensitive)
int find(const char *name);
// Text shown by the act "Say" (copied, up to 20 characters).
void setText(const char *text);
const char *text();
}  // namespace Acts

#endif
