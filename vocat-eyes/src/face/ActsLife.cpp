// Acts about everyday things: a book, music, a butterfly, a coffee...
#include <string.h>

#include "ActKit.h"
#include "ActList.h"

namespace {

const Rgb PAGE = 0xF2EFE4;
const Rgb PAGE_SHADE = 0xCFC9B8;
const Rgb PRINT = 0x5A5F6E;
const Rgb COVER = 0xB8403A;
const Rgb STEAM = 0xD0D8E4;
const Rgb COFFEE = 0x6A3A1E;
const Rgb METAL = 0xC4CCD8;
const Rgb LENS_GLASS = 0x0C1016;

// Moves the eyes out of the way of something drawn in the middle or below.
void makeRoom(ActFrame &f, float k, float apart, float up, float narrower, float shorter) {
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.x += apart * k;
    p.y -= up * k;
    p.w *= 1.0f - narrower * k;
    p.h *= 1.0f - shorter * k;
  }
}

// Slides in from below at the start and back down at the end. 0 = hidden.
float riseFromBelow(const ActFrame &f, float in0, float in1, float outBefore) {
  return outBack(seg(f.t, in0, in1)) * (1.0f - easeIn(seg(f.t, f.duration - outBefore, f.duration - outBefore + 0.5f)));
}

void drawNote(Canvas &c, float x, float y, float s, Rgb color) {
  c.pathBegin();
  c.pathAddEllipse(x, y, 2.4f * s, 1.8f * s);
  c.pathAddLine(x + 1.9f * s, y, x + 1.9f * s, y - 7.0f * s, 1.3f);
  c.pathAddLine(x + 1.9f * s, y - 7.0f * s, x + 5.2f * s, y - 5.2f * s, 1.7f);
  c.pathFill(color);
}

}  // namespace

// ---- Book: reads, turns the page and gets to the good part ----------------------------
static const float BOOK_TURN = 3.0f;
static const float BOOK_HALF = 40.0f;  // width of one page

void actBookPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  makeRoom(f, k, 0.0f, 11.0f, 0.08f, 0.26f);
  float gx;
  if (t < BOOK_TURN) gx = lerpf(-0.75f, -0.1f, fractf(seg(t, 0.8f, 2.9f) * 2.999f));
  else gx = lerpf(0.1f, 0.75f, fractf(seg(t, 3.5f, 4.5f) * 1.999f));
  float wow = bump(t, 4.5f, 5.3f);
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.lidT += 0.10f * k - 0.10f * wow;
    p.h *= 1.0f + 0.28f * wow;
  }
  f.pose.mouth.w = 0.0f;
  f.gazeX = gx * k * (1.0f - wow);
  f.gazeY = -0.8f * k;
  blinkAt(f, 2.0f);
  blinkAt(f, 3.3f);
  setEars(f, 112.0f + 30.0f * wow, 112.0f + 30.0f * wow);
  if (f.crossed(BOOK_TURN)) f.chirp = CH_BLIP;
  if (f.crossed(4.5f)) f.chirp = CH_SURPRISE;
}

static void bookLines(Canvas &c, float x0, float x1, float yOuter, float yInner, bool left, int seed) {
  // the page is a little lower at the spine
  for (int line = 0; line < 5; line++) {
    float x = x0;
    for (int n = 0; x < x1 - 2.0f; n++) {
      float len = 4.0f + hash01(seed + line * 13 + n * 5) * 7.0f;
      float end = fminf(x + len, x1);
      float ua = left ? (x - (64.0f - BOOK_HALF)) / BOOK_HALF : ((64.0f + BOOK_HALF) - x) / BOOK_HALF;
      float ub = left ? (end - (64.0f - BOOK_HALF)) / BOOK_HALF : ((64.0f + BOOK_HALF) - end) / BOOK_HALF;
      float ya = lerpf(yOuter, yInner, ua) + 3.5f + line * 3.4f;
      float yb = lerpf(yOuter, yInner, ub) + 3.5f + line * 3.4f;
      c.thickLine(x, ya, end, yb, 1.0f, PRINT);
      x = end + 3.0f;
    }
  }
}

void actBookDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float rise = riseFromBelow(f, 0.2f, 0.7f, 0.7f);
  if (rise < 0.02f) return;
  float oy = (1.0f - rise) * 50.0f;
  float yo = 56.0f + oy, yi = 60.0f + oy;  // top of the page at the edge and at the spine
  const float H = 21.0f;
  const float xl = 64.0f - BOOK_HALF, xr = 64.0f + BOOK_HALF;
  // cover
  quad(c, 64, yi + 1.5f, xl - 2.0f, yo + 1.0f, xl - 2.0f, yo + H + 2.5f, 64, yi + H + 3.0f, COVER);
  quad(c, 64, yi + 1.5f, xr + 2.0f, yo + 1.0f, xr + 2.0f, yo + H + 2.5f, 64, yi + H + 3.0f, COVER);
  // pages, darker towards the spine
  quad(c, 64, yi, xl, yo, xl, yo + H, 64, yi + H, PAGE);
  quad(c, 64, yi, xr, yo, xr, yo + H, 64, yi + H, PAGE);
  quad(c, 64, yi, 58, yi - 0.6f, 58, yi + H - 0.6f, 64, yi + H, PAGE_SHADE);
  quad(c, 64, yi, 70, yi - 0.6f, 70, yi + H - 0.6f, 64, yi + H, PAGE_SHADE);
  int page = f.t < BOOK_TURN + 0.25f ? 0 : 40;
  bookLines(c, xl + 4.0f, 60.0f, yo, yi, true, 3 + page);
  bookLines(c, 68.0f, xr - 4.0f, yo, yi, false, 23 + page);

  // the page that turns
  float u = seg(f.t, BOOK_TURN, BOOK_TURN + 0.5f);
  if (u > 0.0f && u < 1.0f) {
    float ca, sa;
    Canvas::unit(u * ACT_PI, ca, sa);
    float x = 64.0f + BOOK_HALF * ca, lift = 12.0f * sa;
    float ye = lerpf(yi, yo, fabsf(ca)) - lift;
    quad(c, 64, yi, x, ye, x, ye + H, 64, yi + H, INK_WHITE);
    float rim[8] = {64, yi, x, ye, x, ye + H, 64, yi + H};
    c.polyline(rim, 4, 0.9f, PAGE_SHADE);
  }
}

// ---- Headphones: lost in the music ------------------------------------------------------
static float headBob(float t) { return fabsf(sinf(t * M_TAU * 1.0f)); }

void actHeadphonesPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.5f, 0.6f);
  float best = inOut(seg(t, 3.2f, 3.5f)) * (1.0f - inOut(seg(t, 4.6f, 4.9f)));  // the part it likes
  EyeParams shut = {-5.0f, -2.0f, 34.0f, 18.0f, 9.0f, 9.0f, 0.22f, 0, -0.75f, 0.58f, 0, 0.75f, 0, 1.0f, 0, 0};
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.x -= 5.0f * k;
    mixEye(p, shut, k * (1.0f - best));
    p.lidB += 0.28f * best * k;
    p.lidBBend += 0.32f * best * k;
  }
  float bob = headBob(t) * k * (1.0f + best);
  f.dy[0] = f.dy[1] = -2.5f * bob;
  f.dx[0] = f.dx[1] = 2.5f * sinf(t * M_TAU * 0.5f) * k;
  f.pose.mouth.w = 12.0f * k;
  f.pose.mouth.curve = 3.0f;
  f.pose.mouth.open = 0.5f * best;
  f.pose.mouth.y = 23.0f;
  f.pose.mouth.tilt = 0.0f;
  setEars(f, 95.0f, 95.0f, EG_FLAP, 26.0f * k, 1.0f);
  if (f.crossed(0.6f) || f.crossed(3.2f)) f.chirp = CH_TUNE;
}

void actHeadphonesDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t, k = envelope(f, 0.5f, 0.6f);
  float in = outBack(seg(t, 0.1f, 0.6f)), out = easeIn(seg(t, f.duration - 0.6f, f.duration - 0.1f));
  float oy = -80.0f * (1.0f - in) - 100.0f * out - 2.5f * headBob(t) * k;
  float sway = 2.5f * sinf(t * M_TAU * 0.5f) * k;
  const float cx = 64.0f + sway, cy = 36.0f + oy;
  // band, over the top of the round glass
  c.arc(cx, cy, 53.0f, ACT_PI, M_TAU, 3.4f, Paint::gradient(INK_WHITE, METAL, cy - 54.0f, cy - 20.0f));
  // cups
  for (int s = -1; s <= 1; s += 2) {
    float x = cx + s * 53.0f;
    c.fillEllipse(x, cy, 9.5f, 16.0f, INK_BLACK);
    c.fillEllipse(x, cy, 8.0f, 14.5f, Paint::gradient(0xFF6A5A, 0xC82030, cy - 14.0f, cy + 14.0f));
    c.fillEllipse(x, cy, 3.6f, 9.0f, Rgb(0x30181C));
  }
  // notes leaving the cups
  static const Rgb COLOR[4] = {INK_MINT, INK_YELLOW, INK_PINK, INK_SKY};
  for (int i = 0; i < 4; i++) {
    float ph = fractf(t * 0.55f + i * 0.25f);
    float side = (i & 1) ? 1.0f : -1.0f;
    float x = 64.0f + side * (36.0f + sinf(ph * 7.0f + i) * 3.0f);
    float y = 84.0f - ph * 34.0f;
    float s = 1.2f * k * inOut(seg(ph, 0.0f, 0.2f)) * (1.0f - seg(ph, 0.75f, 1.0f));
    if (s < 0.4f) continue;
    drawNote(c, x, y, s, COLOR[i]);
  }
}

// ---- Butterfly: watches it, runs after it and ends up with it on the head ----------------
namespace {
const float PERCH_Y = 2.0f;  // above the eyes

bool butterflyAt(float t, float &x, float &y) {
  float wx = 92.0f + 18.0f * sinf(2.3f * t), wy = 10.0f + 14.0f * sinf(3.7f * t + 1.0f);
  if (t < 0.3f) return false;
  if (t < 0.9f) {
    float a = easeOut(seg(t, 0.3f, 0.9f));
    x = lerpf(132.0f, wx, a);
    y = lerpf(-10.0f, wy, a);
  } else if (t < 2.2f) {
    x = wx, y = wy;
  } else if (t < 3.1f) {
    x = lerpf(wx, -16.0f, easeIn(seg(t, 2.2f, 3.1f)));
    y = wy;
  } else if (t < 4.3f) {
    return false;
  } else if (t < 5.3f) {
    float a = easeOut(seg(t, 4.3f, 5.3f));
    x = lerpf(146.0f, 64.0f, a);
    y = lerpf(22.0f, PERCH_Y, a) + 5.0f * sinf(t * 9.0f) * (1.0f - a);
  } else if (t < 6.4f) {
    x = 64.0f, y = PERCH_Y;
  } else {
    float a = easeIn(seg(t, 6.4f, 7.0f));
    x = lerpf(64.0f, 86.0f, a);
    y = lerpf(PERCH_Y, -36.0f, a);
  }
  return true;
}

float chaseX(float t) {
  if (t < 2.4f) return 0.0f;
  if (t < 3.3f) return -150.0f * easeIn(seg(t, 2.4f, 3.3f));
  if (t < 4.6f) return 150.0f;
  if (t < 5.5f) return 150.0f * (1.0f - outBack(seg(t, 4.6f, 5.5f)));
  return 0.0f;
}
}  // namespace

void actButterflyPose(ActFrame &f) {
  float t = f.t, x = 0.0f, y = 0.0f;
  float dx = chaseX(t);
  bool running = (t > 2.4f && t < 3.3f) || (t > 4.6f && t < 5.5f);
  float crouch = bump(t, 2.1f, 2.45f);
  float sits = inOut(seg(t, 5.4f, 5.8f)) * (1.0f - inOut(seg(t, 6.4f, 6.8f)));
  float pant = bump(t, 5.5f, 6.3f);
  if (butterflyAt(t, x, y)) {
    float k = envelope(f, 0.3f, 0.4f);
    f.gazeX = clampf((x - (64.0f + dx)) / 60.0f, -1.0f, 1.0f) * k;
    f.gazeY = clampf((32.0f - y) / 26.0f, -1.0f, 1.0f) * k;
  }
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    f.dx[e] = dx;
    f.dy[e] = 3.0f * crouch + (running ? -4.0f * fabsf(sinf(t * 13.0f)) : 0.0f);
    if (running) {
      p.w *= 1.15f;
      p.h *= 0.9f;
    }
    p.h *= (1.0f - 0.1f * crouch) * (1.0f + 0.06f * pant * sinf(t * 22.0f));
    p.x -= 3.0f * sits;
    p.y += 5.0f * sits;
    p.lidB += 0.26f * sits;
    p.lidBBend += 0.3f * sits;
  }
  if (running) setEars(f, 20.0f, 20.0f);
  else if (sits > 0.3f) setEars(f, 100.0f, 100.0f, EG_WIGGLE, 8.0f, 1.2f);
  else setEars(f, 95.0f + 30.0f * f.gazeX, 95.0f - 30.0f * f.gazeX, EG_TWITCH, 10.0f, 1.5f);
  if (f.crossed(0.5f)) f.chirp = CH_QUESTION;
  if (f.crossed(2.4f)) f.chirp = CH_WHOOSH;
  if (f.crossed(5.4f)) f.chirp = CH_HAPPY;
}

void actButterflyDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float x, y;
  if (!butterflyAt(f.t, x, y)) return;
  bool sitting = f.t > 5.3f && f.t < 6.4f;
  float flap = fabsf(cosf(f.t * (sitting ? 5.0f : 17.0f)));
  float w = 1.6f + 6.0f * flap;
  // first a dark rim, then the wings
  for (int s = -1; s <= 1; s += 2) {
    c.fillEllipse(x + s * (w + 0.5f), y - 2.2f, w + 1.5f, 6.4f, INK_BLACK);
    c.fillEllipse(x + s * (w * 0.8f + 0.5f), y + 3.6f, w * 0.8f + 1.5f, 4.8f, INK_BLACK);
  }
  for (int s = -1; s <= 1; s += 2) {
    c.fillEllipse(x + s * (w + 0.5f), y - 2.2f, w, 4.9f, Paint::gradient(INK_YELLOW, INK_ORANGE, y - 7.0f, y + 3.0f));
    c.fillEllipse(x + s * (w * 0.8f + 0.5f), y + 3.6f, w * 0.8f, 3.3f, Paint::gradient(INK_ORANGE, INK_PINK, y, y + 7.0f));
    if (w > 3.2f) c.fillCircle(x + s * (w + 0.5f), y - 2.2f, 1.4f, Rgb(0x3A1E58));
  }
  c.thickLine(x, y - 4.4f, x, y + 5.4f, 1.9f, Rgb(0x4A3A30));
  c.thickLine(x, y - 4.4f, x - 3.0f, y - 8.6f, 0.8f, INK_ICE);
  c.thickLine(x, y - 4.4f, x + 3.0f, y - 8.6f, 0.8f, INK_ICE);
}

// ---- Coffee: half asleep until the first sip ---------------------------------------------
void actCoffeePose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  float sleepy = inOut(seg(t, 0.2f, 0.9f)) * (1.0f - seg(t, 3.3f, 3.45f));
  float sip = inOut(seg(t, 2.2f, 2.5f)) * (1.0f - inOut(seg(t, 3.0f, 3.3f)));
  float jolt = outBack(seg(t, 3.4f, 3.6f)) * (1.0f - inOut(seg(t, 4.6f, 5.2f)));
  makeRoom(f, k, 9.0f, 9.0f, 0.12f, 0.15f);
  EyeParams shut = {9.0f, -9.0f, 30.0f, 16.0f, 9.0f, 9.0f, 0.55f, 0, 0.7f, 0.22f, 0, -0.7f, 0, 1.0f, 0, 0};
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.lidT += 0.45f * sleepy * (1.0f - sip);
    mixEye(p, shut, sip);
    p.h *= 1.0f + 0.38f * jolt;
    p.w *= 1.0f + 0.08f * jolt;
    p.pupil = 0.26f * clamp01(jolt);
    p.pupilW = 1.0f;
  }
  float buzz = bump(t, 3.4f, 4.5f);
  f.dx[0] = f.dx[1] = sinf(t * 62.0f) * 1.6f * buzz;
  f.gazeY = -0.6f * k * (1.0f - sip) * (1.0f - clamp01(jolt));
  f.pose.mouth.w = 0.0f;
  tint(f, INK_INDIGO, sleepy * (1.0f - sip));
  tint(f, INK_AMBER, clamp01(jolt));
  if (jolt > 0.2f) setEars(f, 150.0f, 150.0f, EG_TREMBLE, 7.0f * buzz, 13.0f);
  else setEars(f, lerpf(80.0f, 28.0f, sleepy), lerpf(80.0f, 28.0f, sleepy));
  if (f.crossed(0.3f)) f.chirp = CH_YAWN;
  if (f.crossed(3.4f)) f.chirp = CH_SURPRISE;
}

void actCoffeeDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  float rise = outBack(seg(t, 1.0f, 1.5f)) * (1.0f - easeIn(seg(t, f.duration - 1.0f, f.duration - 0.5f)));
  if (rise < 0.02f) return;
  float sip = inOut(seg(t, 2.2f, 2.5f)) * (1.0f - inOut(seg(t, 3.0f, 3.3f)));
  float top = GROUND + 4.0f - 22.0f + 50.0f * (1.0f - rise) - 12.0f * sip;
  // handle, mug, coffee
  c.ring(77.0f, top + 10.0f, 6.4f, 2.6f, INK_WHITE);
  c.fillRoundRect(51.0f, top - 1.5f, 26.0f, 25.0f, 4.0f, INK_BLACK);
  c.fillRoundRect(52.5f, top, 23.0f, 22.0f, 3.0f, Paint::gradient(INK_WHITE, METAL, top, top + 22.0f));
  c.fillEllipse(64.0f, top + 1.6f, 10.0f, 2.2f, COFFEE);
  FaceRenderer::drawHeart(c, 64.0f, top + 12.0f, 3.6f, 3.4f, INK_RED);
  // steam
  float hot = rise * (1.0f - sip) * (1.0f - seg(t, 3.3f, 3.6f));
  if (hot > 0.5f) {
    for (int s = 0; s < 2; s++) {
      float bx = 60.0f + s * 8.0f;
      float pts[9 * 2];
      for (int i = 0; i <= 8; i++) {
        float u = i / 8.0f;
        pts[i * 2] = bx + sinf(u * 6.0f + t * 4.0f + s * 2.0f) * 2.0f;
        pts[i * 2 + 1] = top - 4.0f - u * 13.0f;
      }
      c.polyline(pts, 9, 1.4f, Paint(STEAM, 0.75f));
    }
  }
}

// ---- Sunglasses: they come down slowly, as they must ------------------------------------
static float glassesY(float t, float duration) {
  float y = lerpf(-70.0f, 0.0f, seg(t, 0.3f, 1.7f));
  y -= 2.5f * bump(t, 1.7f, 1.95f);
  y += lerpf(0.0f, -80.0f, easeIn(seg(t, duration - 0.6f, duration - 0.1f)));
  return y;
}

void actSunglassesPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.3f, 0.4f);
  float cool = inOut(seg(t, 1.9f, 2.2f)) * (1.0f - seg(t, f.duration - 0.7f, f.duration - 0.4f));
  // behind the glasses the eyes are half shut, and hidden
  float covered = seg(t, 1.0f, 1.7f) * (1.0f - seg(t, f.duration - 0.6f, f.duration - 0.3f));
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.lidT += 0.25f * k * (1.0f - covered);
    p.h = lerpf(p.h, 15.0f, covered);
    p.w = lerpf(p.w, 30.0f, covered);
  }
  f.dy[0] = f.dy[1] = -2.0f * cool;
  f.pose.mouth.w = 18.0f * cool;
  f.pose.mouth.curve = 3.0f;
  f.pose.mouth.open = 0.0f;
  f.pose.mouth.y = 26.0f;
  f.pose.mouth.tilt = 5.0f;
  setEars(f, lerpf(f.ears.left, 125.0f, cool), lerpf(f.ears.right, 70.0f, cool));
  if (f.crossed(1.7f)) f.chirp = CH_BLIP;
  if (f.crossed(2.2f)) f.chirp = CH_HAPPY;
}

void actSunglassesDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  float gy = 32.0f + glassesY(t, f.duration) + f.dy[0];
  if (gy < GLASS_TOP - 20.0f) return;
  EyeParams lens = {0, 0, 44.0f, 27.0f, 9.0f, 3.2f, 0, 0, 0, 0, 0, 0, 0, 1.0f, 0, 0};
  const float lx = 40.0f, rx = 88.0f;
  // frame, then the dark glass
  c.thickLine(lx + 20.0f, gy - 8.0f, rx - 20.0f, gy - 8.0f, 3.0f, METAL);
  c.thickLine(lx - 21.0f, gy - 9.0f, 2.0f, gy - 13.0f, 2.4f, METAL);
  c.thickLine(rx + 21.0f, gy - 9.0f, 126.0f, gy - 13.0f, 2.4f, METAL);
  for (int e = 0; e < 2; e++) {
    float x = e == 0 ? lx : rx;
    FaceRenderer::drawEye(c, x, gy, lens, EK_SHAPE, e == 0, 0.0f, INK_BLACK, 1.5f, false);
    FaceRenderer::drawEye(c, x, gy, lens, EK_SHAPE, e == 0, 0.0f, METAL, 0.0f, false);
    FaceRenderer::drawEye(c, x, gy, lens, EK_SHAPE, e == 0, 0.0f, LENS_GLASS, -2.0f, false);
    // the glint that crosses the glass, or the one that stays
    float s = seg(t, 2.2f, 2.9f);
    if (s > 0.0f && s < 1.0f) {
      float xo = lerpf(-17.0f, 17.0f, s);
      c.thickLine(x + xo - 3.0f, gy + 7.0f, x + xo + 3.0f, gy - 7.0f, 2.6f, Paint(INK_WHITE, 0.9f));
      c.thickLine(x + xo - 8.0f, gy + 7.0f, x + xo - 2.0f, gy - 7.0f, 1.2f, Paint(INK_WHITE, 0.7f));
    } else {
      c.thickLine(x - 14.0f, gy - 1.0f, x - 9.0f, gy - 7.0f, 1.8f, Paint(INK_SKY, 0.8f));
    }
  }
}

// ---- Rocket: 3, 2, 1 ------------------------------------------------------------------
static const float LIFT_OFF = 3.0f;
static const float PAD = GROUND + 8.0f;

static float rocketBase(float t) {
  float y = PAD + 60.0f * (1.0f - outBack(seg(t, 0.2f, 0.7f)));
  if (t > LIFT_OFF) {
    float u = t - LIFT_OFF;
    y -= 110.0f * u * u;
  }
  return y;
}

void actRocketPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  makeRoom(f, k, 10.0f, 4.0f, 0.12f, 0.10f);
  float tip = rocketBase(t) - 30.0f;
  float counting = seg(t, 1.1f, 1.3f) * (1.0f - seg(t, LIFT_OFF - 0.1f, LIFT_OFF));
  float gy = clampf((32.0f - tip) / 30.0f, -0.8f, 1.0f);
  f.gazeY = lerpf(gy, 0.75f, counting) * k;
  float rumble = seg(t, LIFT_OFF - 0.3f, LIFT_OFF) * (1.0f - seg(t, LIFT_OFF + 0.6f, LIFT_OFF + 1.0f));
  f.dx[0] = f.dx[1] = sinf(t * 57.0f) * 1.7f * rumble;
  float awe = bump(t, 3.9f, 5.1f);
  for (int e = 0; e < 2; e++) f.pose.eye[e].h *= 1.0f + 0.14f * awe;
  f.pose.mouth.w = 0.0f;
  float up = seg(t, LIFT_OFF, LIFT_OFF + 0.4f);
  setEars(f, lerpf(70.0f, 140.0f, up), lerpf(70.0f, 140.0f, up), rumble > 0.3f ? EG_TREMBLE : EG_STILL, 6.0f, 12.0f);
  if (f.crossed(1.2f) || f.crossed(1.8f) || f.crossed(2.4f)) f.chirp = CH_BLIP;
  if (f.crossed(LIFT_OFF)) f.chirp = CH_WHOOSH;
  if (f.crossed(4.3f)) f.chirp = CH_DING;
}

void actRocketDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_FRONT) return;
  float t = f.t;
  if (t > f.duration - 0.4f) return;

  if (t >= 1.2f && t < LIFT_OFF) {
    const char *n = t < 1.8f ? "3" : (t < 2.4f ? "2" : "1");
    float beat = fractf((t - 1.2f) / 0.6f);
    c.textCentered(64.0f, -18.0f, n, 3, Paint(t < 2.4f ? INK_YELLOW : INK_RED, 1.0f - 0.6f * beat));
  }

  float b = rocketBase(t);
  if (b > GLASS_TOP - 6.0f) {
    const float x = 64.0f;
    // smoke stays on the ground
    for (int i = 0; i < 6; i++) {
      float u = t - (LIFT_OFF - 0.2f + i * 0.12f);
      if (u < 0.0f || u > 1.5f) continue;
      float side = (i & 1) ? 1.0f : -1.0f;
      float sx = x + side * (5.0f + 30.0f * easeOut(u / 1.5f));
      float r = 3.0f + 6.0f * u / 1.5f;
      c.fillCircle(sx, PAD - 3.0f - 2.0f * u, r, Paint(STEAM, 0.85f * (1.0f - u / 1.5f)));
    }
    // flame
    if (t > LIFT_OFF - 0.25f) {
      float len = 10.0f + 9.0f * hash01((int)(t * 30.0f));
      c.fillTriangle(x - 4.5f, b - 3.0f, x + 4.5f, b - 3.0f, x, b - 3.0f + len, INK_ORANGE);
      c.fillTriangle(x - 2.4f, b - 3.0f, x + 2.4f, b - 3.0f, x, b - 3.0f + len * 0.55f, INK_YELLOW);
    }
    // body
    c.fillTriangle(x - 6.0f, b - 15.0f, x - 12.0f, b - 3.0f, x - 6.0f, b - 5.0f, INK_RED);
    c.fillTriangle(x + 6.0f, b - 15.0f, x + 12.0f, b - 3.0f, x + 6.0f, b - 5.0f, INK_RED);
    c.fillRect(x - 3.0f, b - 5.0f, 6.0f, 3.0f, INK_GRAY);
    c.fillRect(x - 6.0f, b - 26.0f, 12.0f, 21.0f, Paint::gradient(INK_WHITE, METAL, b - 26.0f, b - 5.0f));
    c.fillTriangle(x - 6.0f, b - 26.0f, x + 6.0f, b - 26.0f, x, b - 37.0f, INK_RED);
    c.fillCircle(x, b - 19.0f, 3.4f, INK_DARK);
    c.fillCircle(x, b - 19.0f, 2.4f, INK_SKY);
  }
  // it is a star now
  FaceRenderer::drawStar4(c, 64.0f, -8.0f, 6.5f * bump(t, 4.2f, 5.0f), INK_WHITE);
}

// ---- Snow: a flake lands on its nose -------------------------------------------------------
namespace {
void flake(Canvas &c, float x, float y, float r, float alpha) {
  if (r < 1.2f) {
    c.fillCircle(x, y, 0.9f, Paint(INK_WHITE, alpha));
    return;
  }
  c.pathBegin();
  for (int i = 0; i < 3; i++) {
    float co, si;
    Canvas::unit(i * (ACT_PI / 3.0f) + 0.3f, co, si);
    c.pathAddLine(x - co * r, y - si * r, x + co * r, y + si * r, 1.1f);
  }
  c.pathFill(Paint(INK_WHITE, alpha));
}

bool heroFlake(float t, float &x, float &y, float &r) {
  if (t < 1.4f || t > 4.5f) return false;
  float fall = seg(t, 1.4f, 3.4f);
  x = 64.0f + 7.0f * sinf(t * 1.6f) * (1.0f - seg(t, 2.6f, 3.4f));
  y = lerpf(-30.0f, 35.0f, fall);
  r = 4.2f * (1.0f - seg(t, 3.7f, 4.5f));  // melts
  return r > 0.4f;
}
}  // namespace

void actSnowPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.4f, 0.5f);
  float cross = inOut(seg(t, 3.1f, 3.5f)) * (1.0f - inOut(seg(t, 4.4f, 4.8f)));
  float shiver = bump(t, 4.4f, 5.4f);
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.pupil = 0.42f * k;
    p.pupilW = 1.0f;
    p.px = lerpf(0.0f, 0.9f, cross);
    p.py = lerpf(-0.7f, 0.3f, cross);
    p.lidT += 0.2f * shiver;
    p.lidB += 0.1f * shiver;
  }
  f.gazeY = 0.35f * k * (1.0f - cross);
  f.dx[0] = f.dx[1] = sinf(t * 48.0f) * 2.0f * shiver;
  f.halo = true;
  tint(f, INK_ICE, k * 0.8f);
  setEars(f, lerpf(85.0f, 40.0f, shiver), lerpf(85.0f, 40.0f, shiver), shiver > 0.2f ? EG_TREMBLE : EG_STILL, 6.0f, 13.0f);
  if (f.crossed(3.4f)) f.chirp = CH_BLIP;
  if (f.crossed(4.5f)) f.chirp = CH_SCARED;
}

void actSnowDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  float t = f.t;
  if (layer == ACT_FRONT) {
    float x, y, r;
    if (heroFlake(t, x, y, r)) {
      c.fillCircle(x, y, r + 1.5f, INK_BLACK);
      flake(c, x, y, r, 1.0f);
    }
    return;
  }
  if (layer != ACT_BACK) return;
  for (int i = 0; i < 24; i++) {
    float speed = 16.0f + hash01(i * 5) * 18.0f;
    float y = t * speed - 30.0f - hash01(i * 11) * 110.0f;
    if (y < GLASS_TOP - 4.0f || y > GLASS_BOTTOM + 4.0f) continue;
    float x = hash01(i * 3) * 128.0f + 4.0f * sinf(t * (0.8f + hash01(i)) + i);
    bool big = hash01(i * 7) > 0.6f;
    flake(c, x, y, big ? 2.8f : 1.0f, big ? 1.0f : 0.75f);
  }
  // it settles on the bottom of the round glass
  float g = 9.0f * seg(t, 1.0f, 4.5f) * (1.0f - seg(t, f.duration - 0.8f, f.duration - 0.2f));
  if (g > 0.5f) {
    const int n = 24;
    float xy[(n + 1) * 2 + 4];
    for (int i = 0; i <= n; i++) {
      float x = 20.0f + 88.0f * i / n;
      float edge = DISC_CY + FaceRenderer::glassHalfWidth(DISC_CY + (x - DISC_CX));  // the circle, turned a quarter
      xy[i * 2] = x;
      xy[i * 2 + 1] = edge - g * (0.7f + 0.3f * sinf(x * 0.15f));
    }
    xy[(n + 1) * 2] = 108.0f;
    xy[(n + 1) * 2 + 1] = GLASS_BOTTOM + 4.0f;
    xy[(n + 2) * 2] = 20.0f;
    xy[(n + 2) * 2 + 1] = GLASS_BOTTOM + 4.0f;
    c.fillPoly(xy, n + 3, Paint::gradient(INK_WHITE, INK_ICE, GLASS_BOTTOM - 14.0f, GLASS_BOTTOM));
  }
}

// ---- Search: a magnifying glass looks at it --------------------------------------------
namespace {
const float LENS_R = 15.0f;

void lensAt(float t, float &x, float &y) {
  struct Key {
    float t, x, y;
  };
  static const Key K[] = {{0.2f, 128, 96}, {1.0f, 88, 32}, {1.9f, 88, 32}, {2.6f, 40, 32},
                          {3.5f, 40, 32},  {4.0f, 64, 50}, {4.4f, 64, 50}, {5.0f, -10, -34}};
  const int n = (int)(sizeof(K) / sizeof(K[0]));
  if (t <= K[0].t) {
    x = K[0].x, y = K[0].y;
    return;
  }
  for (int i = 1; i < n; i++) {
    if (t <= K[i].t) {
      float a = inOut(seg(t, K[i - 1].t, K[i].t));
      x = lerpf(K[i - 1].x, K[i].x, a);
      y = lerpf(K[i - 1].y, K[i].y, a);
      return;
    }
  }
  x = K[n - 1].x, y = K[n - 1].y;
}
}  // namespace

void actSearchPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.3f, 0.4f);
  float x, y;
  lensAt(t, x, y);
  f.gazeX = clampf((x - 64.0f) / 64.0f, -1.0f, 1.0f) * 0.5f * k;
  f.gazeY = clampf((32.0f - y) / 32.0f, -1.0f, 1.0f) * 0.5f * k;
  for (int e = 0; e < 2; e++) {
    float ex = e == 0 ? EYE_ANCHOR_LX : EYE_ANCHOR_RX;
    float d = sqrtf((x - ex) * (x - ex) + (y - 32.0f) * (y - 32.0f));
    float near = clamp01(1.0f - d / 30.0f);
    f.pose.eye[e].h *= 1.0f + 0.10f * near;
  }
  setEars(f, 95.0f + 35.0f * f.gazeX * 2.0f, 95.0f - 35.0f * f.gazeX * 2.0f, EG_TWITCH, 8.0f, 1.0f);
  if (f.crossed(1.0f) || f.crossed(2.6f)) f.chirp = CH_QUESTION;
}

void actSearchDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  if (layer != ACT_POST) return;
  float x, y;
  lensAt(f.t, x, y);
  if (x < -LENS_R - 16.0f || x > STAGE_W + LENS_R + 16.0f) return;

  // what is under the glass, bigger
  c.magnifyPx((int)Canvas::toPxX(x), (int)Canvas::toPxY(y), (int)Canvas::px(LENS_R), 1.7f);
  c.fillCircle(x, y, LENS_R, Paint(INK_SKY, 0.10f));
  // handle and rim
  const float d = 0.7071f;
  float hx0 = x + (LENS_R + 2.0f) * d, hy0 = y + (LENS_R + 2.0f) * d;
  float hx1 = x + (LENS_R + 15.0f) * d, hy1 = y + (LENS_R + 15.0f) * d;
  c.thickLine(hx0, hy0, hx1, hy1, 7.0f, INK_BLACK);
  c.thickLine(hx0, hy0, hx1, hy1, 4.4f, INK_BROWN);
  c.ring(x, y, LENS_R + 3.2f, 1.6f, INK_BLACK);
  c.ring(x, y, LENS_R + 1.6f, 2.6f, Paint::gradient(INK_WHITE, METAL, y - LENS_R, y + LENS_R));
  c.thickLine(x - 9.0f, y - 4.0f, x - 5.0f, y - 9.0f, 1.4f, Paint(INK_WHITE, 0.75f));  // reflection
}

// ---- Photo: say cheese ---------------------------------------------------------------------
static const float FLASH = 2.4f;

void actPhotoPose(ActFrame &f) {
  float t = f.t, k = envelope(f, 0.3f, 0.5f);
  float smile = inOut(seg(t, 0.6f, 1.0f)) * (1.0f - seg(t, FLASH, FLASH + 0.1f));
  float dazed = seg(t, FLASH + 0.1f, FLASH + 0.2f) * (1.0f - inOut(seg(t, 3.3f, 3.9f)));
  makeRoom(f, k * (1.0f - dazed), 3.0f, 5.0f, 0.04f, 0.06f);
  for (int e = 0; e < 2; e++) {
    EyeParams &p = f.pose.eye[e];
    p.lidB += 0.08f * smile;  // it was already smiling
    p.lidT += 0.30f * dazed;
    p.lidB += 0.20f * dazed;
    f.blink[e] = dazed * (0.45f + 0.45f * sinf(t * 31.0f + e));
  }
  f.dx[0] = f.dx[1] = 2.5f * sinf(t * 3.0f) * dazed;
  f.pose.mouth.w = 0.0f;  // the camera is where the mouth would be
  setEars(f, lerpf(105.0f, 40.0f, dazed), lerpf(105.0f, 40.0f, dazed));
  if (f.crossed(0.9f) || f.crossed(1.4f) || f.crossed(1.9f)) f.chirp = CH_BLIP;
  if (f.crossed(FLASH)) f.chirp = CH_POP;
}

void actPhotoDraw(Canvas &c, const ActFrame &f, const FaceFrame &, uint8_t layer) {
  float t = f.t;
  if (layer == ACT_FRONT) {
    if (t >= FLASH) return;
    float rise = outBack(seg(t, 0.2f, 0.6f));
    float top = GROUND + 8.0f - 22.0f + 46.0f * (1.0f - rise);
    // camera
    c.fillRoundRect(47.0f, top - 1.5f, 34.0f, 25.0f, 4.0f, INK_BLACK);
    c.fillRoundRect(54.0f, top - 4.0f, 10.0f, 5.0f, 1.2f, INK_GRAY);
    c.fillRoundRect(48.5f, top, 31.0f, 22.0f, 3.0f, Paint::gradient(METAL, INK_GRAY, top, top + 22.0f));
    c.fillCircle(64.0f, top + 11.5f, 8.0f, INK_DARK);
    c.ring(64.0f, top + 11.5f, 6.0f, 1.6f, INK_SKY);
    c.fillCircle(62.0f, top + 9.5f, 1.4f, INK_WHITE);
    c.fillRoundRect(73.0f, top + 2.5f, 4.0f, 3.0f, 0.8f, INK_YELLOW);
    // 3, 2, 1
    for (int i = 0; i < 3; i++) {
      float x = 52.0f + i * 12.0f;
      if (t > 0.9f + i * 0.5f) c.fillCircle(x, -12.0f, 3.2f, INK_RED);
      else c.ring(x, -12.0f, 3.0f, 1.0f, INK_GRAY);
    }
    return;
  }
  if (layer != ACT_POST) return;
  if (t >= FLASH && t < FLASH + 0.12f) {
    c.fillRectPx(0, 0, SCREEN_W, SCREEN_H, INK_WHITE);
  } else if (t >= FLASH + 0.12f && t < FLASH + 0.5f) {
    c.fillRectPx(0, 0, SCREEN_W, SCREEN_H, INK_WHITE, 0.9f * (1.0f - seg(t, FLASH + 0.12f, FLASH + 0.5f)));
  }
  if (t >= FLASH + 0.20f && t < FLASH + 1.5f) {
    // spots that stay in the eyes
    float u = seg(t, FLASH + 0.2f, FLASH + 1.5f);
    static const float SX[3] = {30.0f, 78.0f, 100.0f}, SY[3] = {16.0f, 54.0f, 14.0f};
    static const Rgb SPOT[3] = {0x90FF60, 0xFF70D0, 0x70B0FF};
    for (int i = 0; i < 3; i++) {
      c.fillCircle(SX[i] + 6.0f * u * (i - 1), SY[i] - 5.0f * u, (11.0f - i * 2.0f) * (1.0f - u), Paint(SPOT[i], 0.5f * (1.0f - u)));
    }
  }
}
