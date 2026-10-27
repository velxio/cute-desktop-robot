// A small microphone at the bottom of the glass: a tap on it starts (or ends) the
// conversation, for when the wake word does not hear you. Its color tells the state.
#ifndef UI_MICICON_H
#define UI_MICICON_H

#include <stdint.h>

#include "../core/Canvas.h"
#include "../voice/Talk.h"

namespace MicIcon {

// Position in face units (the glass is 128 units wide, its center at DISC_CY).
const float X = 64.0f;
const float Y = 93.0f;
const float HIT_R = 12.0f;  // generous: a fingertip is wide

bool hit(float x, float y);
void draw(Canvas &c, Talk::State state, uint32_t nowMs);

}  // namespace MicIcon

#endif
