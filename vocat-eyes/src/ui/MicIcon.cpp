#include "MicIcon.h"

#include <math.h>

#include "../core/MathUtil.h"

namespace MicIcon {

bool hit(float x, float y) {
  float dx = x - X, dy = y - Y;
  return dx * dx + dy * dy <= HIT_R * HIT_R;
}

void draw(Canvas &c, Talk::State state, uint32_t nowMs) {
  float t = nowMs * 0.001f;
  Rgb color = INK_DARK;
  float alpha = 1.0f;
  switch (state) {
    case Talk::OFF: break;
    case Talk::CONNECTING:
      color = INK_SKY;
      alpha = 0.55f + 0.45f * sinf(t * 6.0f);  // blinks while it connects
      break;
    case Talk::LISTENING: color = INK_CYAN; break;
    case Talk::SPEAKING: color = INK_ORANGE; break;
  }
  if (state != Talk::OFF) {
    // a soft halo that breathes while the conversation is on
    float r = 7.5f + 1.0f * sinf(t * 3.0f);
    c.fillCircle(X, Y, r, Paint(color, 0.18f * alpha));
  }
  Paint p(color, alpha);
  // the capsule
  c.fillCircle(X, Y - 3.6f, 1.7f, p);
  c.fillCircle(X, Y - 0.4f, 1.7f, p);
  c.fillRect(X - 1.7f, Y - 3.6f, 3.4f, 3.2f, p);
  // the holder, the stem and the foot
  c.arc(X, Y - 0.6f, 3.3f, 0.0f, 3.14159265f, 0.9f, p);
  c.thickLine(X, Y + 2.7f, X, Y + 4.6f, 0.9f, p);
  c.thickLine(X - 2.0f, Y + 4.7f, X + 2.0f, Y + 4.7f, 0.9f, p);
}

}  // namespace MicIcon
