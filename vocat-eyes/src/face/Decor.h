// Animated decorations drawn around the eyes (Zzz, tears, hearts, ...).
// Each one is a small closed-form animation of time, no stored frames.
#ifndef FACE_DECOR_H
#define FACE_DECOR_H

#include "FaceRenderer.h"

namespace Decor {

// `weight` (0..1) is how present the decoration is; it grows and shrinks while
// the face moves between expressions.
void draw(Canvas &c, uint32_t flag, const FaceFrame &f, float weight);

}  // namespace Decor

#endif
