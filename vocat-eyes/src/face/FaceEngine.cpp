#include "FaceEngine.h"

#include <string.h>

#include "Decor.h"
#include "FaceMath.h"

namespace {

// Blink timing, seconds
const float BLINK_CLOSE = 0.075f;
const float BLINK_HOLD = 0.035f;
const float BLINK_OPEN = 0.130f;

const float GAZE_PX_X = 12.0f;
const float GAZE_PX_Y = 7.0f;

inline float fract(float v) { return v - floorf(v); }

}  // namespace

// ---------------------------------------------------------------------------------
void FaceEngine::begin(uint32_t seed, uint8_t startExpr) {
  rng_.seed(seed);
  started_ = false;
  time_ = 0.0f;
  gx_ = Spring();
  gy_ = Spring();
  gazeTx_ = gazeTy_ = 0.0f;
  gazeManual_ = false;
  blinkT_[0] = blinkT_[1] = -1.0f;
  blinkNow_[0] = blinkNow_[1] = 0.0f;
  blinkAgain_ = false;
  curtain_ = 0.0f;
  curtainTarget_ = false;
  flinchUntil_[0] = flinchUntil_[1] = 0;
  pendingAct_ = -1;
  act_ = nullptr;
  chirp_ = CH_NONE;
  settle(startExpr < EX_COUNT ? startExpr : (uint8_t)EX_NORMAL);
  const ExpressionDef &d = Expressions::get(cur_);
  for (int i = 0; i < DECOR_COUNT; i++) decor_[i] = (d.decor & (1u << i)) ? 1.0f : 0.0f;
  compose();
}

// Stand exactly on a node, nothing in motion.
void FaceEngine::settle(uint8_t expr) {
  const ExpressionDef &d = Expressions::get(expr);
  cur_ = next_ = target_ = expr;
  moving_ = false;
  p_ = 0.0f;
  blend_ = 1.0f;
  pathLen_ = pathPos_ = 0;
  base_ = from_ = d.pose;
  for (int e = 0; e < 2; e++) {
    baseKind_[e] = fromKind_[e] = d.kind[e];
    baseClose_[e] = 0.0f;
  }
  ears_ = d.ears;
  ink_ = fromInk_ = d.ink;
}

void FaceEngine::snapTo(uint8_t expr) {
  if (expr >= EX_COUNT) return;
  act_ = nullptr;
  pendingAct_ = -1;
  settle(expr);
  const ExpressionDef &d = Expressions::get(expr);
  for (int i = 0; i < DECOR_COUNT; i++) decor_[i] = (d.decor & (1u << i)) ? 1.0f : 0.0f;
}

void FaceEngine::goTo(uint8_t expr, bool urgent) {
  if (expr >= EX_COUNT) return;
  pendingAct_ = -1;
  if (showing()) {
    if (!urgent) {
      // Let the show finish, then go there.
      afterShow_ = expr;
      target_ = expr;
      return;
    }
    stopShow();
  }
  route(expr, urgent);
}

void FaceEngine::route(uint8_t expr, bool urgent) {
  bool enRoute = moving_ || pathPos_ < pathLen_;
  if (enRoute && expr == target_ && urgent == urgent_) return;
  urgent_ = urgent;
  target_ = expr;

  if (!moving_) {
    if (expr == cur_) {
      pathLen_ = pathPos_ = 0;
      return;
    }
    pathLen_ = ExpressionGraph::path(cur_, expr, path_, GRAPH_MAX_PATH);
    pathPos_ = 0;
    nextHop();
    return;
  }

  // Already in the middle of a hop: leave from whichever node is closer and
  // start from the pose that is on screen right now, so nothing jumps.
  uint8_t origin = p_ >= 0.5f ? next_ : cur_;
  cur_ = origin;
  pathLen_ = ExpressionGraph::path(origin, expr, path_, GRAPH_MAX_PATH);
  pathPos_ = 0;
  if (pathLen_ == 0) startHop(origin, 220.0f, EASE_IN_OUT);
  else nextHop();
}

void FaceEngine::nextHop() {
  if (pathPos_ >= pathLen_) return;
  uint8_t node = path_[pathPos_++];
  const GraphEdge *e = ExpressionGraph::find(cur_, node);
  float ms = e ? (float)e->ms : 350.0f;
  uint8_t easing = e ? e->easing : (uint8_t)EASE_IN_OUT;
  // Nodes in the middle of the route are only passed through.
  if (pathPos_ < pathLen_) ms *= 0.6f;
  if (urgent_) ms *= 0.5f;
  startHop(node, ms, easing);
}

void FaceEngine::startHop(uint8_t node, float ms, uint8_t easing) {
  // The pose on screen becomes the origin of the morph.
  from_ = base_;
  for (int e = 0; e < 2; e++) {
    from_.eye[e].h = lerpf(base_.eye[e].h, 2.0f, baseClose_[e]);
    fromKind_[e] = baseKind_[e];
  }
  fromInk_ = ink_;
  next_ = node;
  p_ = 0.0f;
  hopMs_ = ms < 60.0f ? 60.0f : ms;
  hopEase_ = easing;
  moving_ = true;
}

// ---- shows -----------------------------------------------------------------------
bool FaceEngine::playAct(int actIndex, uint8_t thenExpr) {
  if (actIndex < 0 || actIndex >= Acts::count()) return false;
  if (showing()) return false;
  const ActDef &a = Acts::get(actIndex);
  // An act without base face plays over the face the robot is heading to.
  uint8_t base = a.base < EX_COUNT ? a.base : target_;
  pendingAct_ = actIndex;
  afterShow_ = thenExpr < EX_COUNT ? thenExpr : base;
  if (!moving_ && pathPos_ >= pathLen_ && cur_ == base) startPending();
  else route(base, false);
  return true;
}

// The route ended on the face where the show starts.
void FaceEngine::startPending() {
  if (pendingAct_ >= 0) {
    act_ = &Acts::get(pendingAct_);
    pendingAct_ = -1;
    actStartMs_ = nowMs_;
    actPrevT_ = -1.0f;
    actFrame_.t = 0.0f;
    actFrame_.halo = false;
    for (int e = 0; e < 2; e++) {
      actFrame_.eyeOn[e] = true;
      actFrame_.dx[e] = actFrame_.dy[e] = 0.0f;
    }
    gazeManual_ = false;
  }
}

void FaceEngine::stopShow() {
  pendingAct_ = -1;
  if (act_) {
    act_ = nullptr;
    settle(cur_);
  }
}

void FaceEngine::blink() {
  if (blinkT_[0] < 0.0f) blinkT_[0] = 0.0f;
  if (blinkT_[1] < 0.0f) blinkT_[1] = 0.0f;
}

void FaceEngine::wink(bool leftEye) {
  int e = leftEye ? 0 : 1;
  if (blinkT_[e] < 0.0f) blinkT_[e] = 0.0f;
}

void FaceEngine::flinch(bool leftEye, uint32_t holdMs) { flinchUntil_[leftEye ? 0 : 1] = nowMs_ + holdMs; }

void FaceEngine::lookAt(float x, float y, uint32_t holdMs) {
  gazeTx_ = clampf(x, -1.0f, 1.0f);
  gazeTy_ = clampf(y, -1.0f, 1.0f);
  gazeManual_ = true;
  gazeHoldUntil_ = nowMs_ + holdMs;
}

uint8_t FaceEngine::takeChirp() {
  uint8_t c = chirp_;
  chirp_ = CH_NONE;
  return c;
}

// ---------------------------------------------------------------------------------
void FaceEngine::update(uint32_t nowMs) {
  nowMs_ = nowMs;
  if (!started_) {
    started_ = true;
    lastMs_ = nowMs;
    blinkNextMs_ = nowMs + 1800;
    gazeNextMs_ = nowMs + 1200;
  }
  uint32_t dms = nowMs - lastMs_;
  lastMs_ = nowMs;
  if (dms > 100) dms = 100;
  float dt = dms * 0.001f;
  time_ += dt;

  if (act_) {
    float t = (float)(nowMs - actStartMs_) * 0.001f;
    if (t < act_->duration) {
      actFrame_.t = t;
      updateAct(dt);
      return;
    }
    // Act finished on its base face, carry on.
    act_ = nullptr;
    settle(cur_);
    blinkNextMs_ = nowMs + 1500;
    gazeNextMs_ = nowMs + 600;
    route(afterShow_, false);
  }

  if (moving_) {
    p_ += (float)dms / hopMs_;
    if (p_ >= 1.0f) {
      // Reached a node.
      int len = pathLen_, pos = pathPos_;
      uint8_t tgt = target_;
      settle(next_);
      pathLen_ = len;
      pathPos_ = pos;
      target_ = tgt;
      if (pathPos_ < pathLen_) {
        nextHop();
      } else {
        target_ = cur_;
        chirp_ = Expressions::get(cur_).chirp;
        if (pendingAct_ >= 0) {
          chirp_ = CH_NONE;  // the show brings its own sounds
          startPending();
          if (act_) {
            updateAct(dt);
            return;
          }
        }
      }
    }
  }

  updateBase();
  updateDecor(dt);
  updateGaze(dt);
  updateBlink(dt);
  compose();
}

// One frame of the running act.
void FaceEngine::updateAct(float dt) {
  ActFrame &f = actFrame_;
  const ExpressionDef &d = Expressions::get(cur_);
  f.tPrev = actPrevT_;
  f.duration = act_->duration;
  f.pose = d.pose;
  for (int e = 0; e < 2; e++) {
    f.kind[e] = d.kind[e];
    f.eyeOn[e] = true;
    f.dx[e] = f.dy[e] = 0.0f;
    f.blink[e] = 0.0f;
  }
  f.gazeX = f.gazeY = 0.0f;
  f.halo = false;
  f.ears = d.ears;
  f.chirp = CH_NONE;
  f.ink = d.ink;

  act_->pose(f);
  actPrevT_ = f.t;
  if (f.chirp != CH_NONE) chirp_ = f.chirp;

  base_ = f.pose;
  for (int e = 0; e < 2; e++) {
    baseKind_[e] = f.kind[e];
    baseClose_[e] = 0.0f;
  }
  ears_ = f.ears;
  ink_ = f.ink;

  gx_.update(clampf(f.gazeX, -1.0f, 1.0f), 38.0f, dt);
  gy_.update(clampf(f.gazeY, -1.0f, 1.0f), 38.0f, dt);

  updateCurtain(dt);
  for (int e = 0; e < 2; e++) {
    blinkT_[e] = -1.0f;
    blinkNow_[e] = clamp01(f.blink[e]);
    if (curtain_ > blinkNow_[e]) blinkNow_[e] = curtain_;
  }
  updateDecor(dt);
  compose();
}

void FaceEngine::updateBase() {
  if (!moving_) return;  // settle() left the exact pose of the node
  const ExpressionDef &to = Expressions::get(next_);
  float t = ease((Ease)hopEase_, p_);  // may overshoot a little (squash & stretch)
  blend_ = clamp01(t);
  ink_ = mixRgb(fromInk_, to.ink, blend_);

  for (int e = 0; e < 2; e++) {
    if (fromKind_[e] == to.kind[e]) {
      base_.eye[e] = lerpEye(from_.eye[e], to.pose.eye[e], t);
      baseKind_[e] = to.kind[e];
      baseClose_[e] = 0.0f;
      continue;
    }
    // Different kinds of eye cannot be morphed: squash the old one to a
    // line, swap, and open the new one.
    EyeParams place = lerpEye(from_.eye[e], to.pose.eye[e], ease(EASE_IN_OUT, p_));
    if (p_ < 0.5f) {
      base_.eye[e] = from_.eye[e];
      baseKind_[e] = fromKind_[e];
      baseClose_[e] = ease(EASE_IN_OUT, p_ * 2.0f);
    } else {
      base_.eye[e] = to.pose.eye[e];
      baseKind_[e] = to.kind[e];
      baseClose_[e] = 1.0f - ease(EASE_OUT, (p_ - 0.5f) * 2.0f);
    }
    base_.eye[e].x = place.x;
    base_.eye[e].y = place.y;
    base_.eye[e].w = place.w;
  }
  base_.mouth = lerpMouth(from_.mouth, to.pose.mouth, blend_);

  const EarDef &a = Expressions::get(cur_).ears;
  const EarDef &b = to.ears;
  ears_.left = lerpf(a.left, b.left, blend_);
  ears_.right = lerpf(a.right, b.right, blend_);
  if (blend_ < 0.5f) {
    ears_.gesture = a.gesture;
    ears_.hz = a.hz;
    ears_.amp = a.amp * (1.0f - blend_ * 2.0f);
  } else {
    ears_.gesture = b.gesture;
    ears_.hz = b.hz;
    ears_.amp = b.amp * (blend_ - 0.5f) * 2.0f;
  }
}

void FaceEngine::updateDecor(float dt) {
  uint32_t flags = Expressions::get((moving_ && p_ >= 0.4f) ? next_ : cur_).decor;
  float step = dt / 0.22f;
  for (int i = 0; i < DECOR_COUNT; i++) {
    float goal = (flags & (1u << i)) ? 1.0f : 0.0f;
    if (decor_[i] < goal) decor_[i] = fminf(goal, decor_[i] + step);
    else if (decor_[i] > goal) decor_[i] = fmaxf(goal, decor_[i] - step);
  }
}

void FaceEngine::updateGaze(float dt) {
  float range = Expressions::get(cur_).gazeRange;
  if (moving_) range = lerpf(range, Expressions::get(next_).gazeRange, blend_);

  if (gazeManual_) {
    if ((int32_t)(nowMs_ - gazeHoldUntil_) >= 0) {
      gazeManual_ = false;
      gazeNextMs_ = nowMs_;
    }
  } else if (autoGaze_) {
    if ((int32_t)(nowMs_ - gazeNextMs_) >= 0) {
      float ox = gazeTx_, oy = gazeTy_;
      if (rng_.chance(0.35f)) {
        gazeTx_ = gazeTy_ = 0.0f;
      } else {
        gazeTx_ = rng_.range(-0.95f, 0.95f);
        gazeTy_ = rng_.range(-0.75f, 0.75f);
      }
      gazeNextMs_ = nowMs_ + (uint32_t)rng_.rangeInt(900, 4200);
      float dx = gazeTx_ - ox, dy = gazeTy_ - oy;
      // People blink when they move the eyes far.
      if (autoBlink_ && range > 0.3f && dx * dx + dy * dy > 0.8f && rng_.chance(0.6f)) blink();
    }
  } else {
    gazeTx_ = gazeTy_ = 0.0f;
  }

  float k = gazeManual_ ? 1.0f : range;
  gx_.update(gazeTx_ * k, 24.0f, dt);
  gy_.update(gazeTy_ * k, 24.0f, dt);
}

void FaceEngine::updateCurtain(float dt) {
  float step = dt / 0.12f;
  if (curtainTarget_) curtain_ = fminf(1.0f, curtain_ + step);
  else curtain_ = fmaxf(0.0f, curtain_ - step);
}

void FaceEngine::updateBlink(float dt) {
  const float total = BLINK_CLOSE + BLINK_HOLD + BLINK_OPEN;
  bool idle = true;
  bool finished = false;
  for (int e = 0; e < 2; e++) {
    float b = 0.0f;
    if (blinkT_[e] >= 0.0f) {
      idle = false;
      blinkT_[e] += dt;
      float t = blinkT_[e];
      if (t < BLINK_CLOSE) b = ease(EASE_IN_OUT, t / BLINK_CLOSE);
      else if (t < BLINK_CLOSE + BLINK_HOLD) b = 1.0f;
      else if (t < total) b = 1.0f - ease(EASE_OUT, (t - BLINK_CLOSE - BLINK_HOLD) / BLINK_OPEN);
      else {
        blinkT_[e] = -1.0f;
        finished = true;
      }
    }
    blinkNow_[e] = b;
  }

  if (finished && blinkAgain_) {
    blinkAgain_ = false;
    blink();
  }

  float rate = Expressions::get(cur_).blinkRate;
  if (moving_) rate = lerpf(rate, Expressions::get(next_).blinkRate, blend_);
  if (autoBlink_ && rate > 0.05f && idle && (int32_t)(nowMs_ - blinkNextMs_) >= 0) {
    blink();
    blinkAgain_ = rng_.chance(0.15f);
    blinkNextMs_ = nowMs_ + (uint32_t)(rng_.range(2200.0f, 6000.0f) / rate);
  }

  updateCurtain(dt);
  for (int e = 0; e < 2; e++) {
    if ((int32_t)(flinchUntil_[e] - nowMs_) > 0) blinkNow_[e] = fmaxf(blinkNow_[e], 0.93f);
    if (curtain_ > blinkNow_[e]) blinkNow_[e] = curtain_;
  }
}

FaceEngine::Layer FaceEngine::wobble(const WobbleDef &w) const {
  Layer l = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f};
  float ph = time_ * w.hz;
  switch (w.kind) {
    case WB_BREATHE: {
      float s = sinf(M_TAU * ph);
      l.sh = 1.0f + w.amp * s;
      l.dy = -w.amp * s * 8.0f;
      break;
    }
    case WB_BOUNCE: {
      float s = fabsf(sinf(3.14159265f * ph));
      l.dy = -w.amp * s;
      // squash on the ground, stretch in the air
      l.sh = 0.96f + 0.08f * s;
      l.sw = 1.03f - 0.05f * s;
      break;
    }
    case WB_SHAKE:
      l.dx = w.amp * sinf(M_TAU * ph);
      break;
    case WB_TREMBLE:
      l.dx = w.amp * sinf(M_TAU * ph) * 0.8f;
      l.dy = w.amp * sinf(M_TAU * ph * 1.37f + 1.3f) * 0.6f;
      break;
    case WB_SWAY:
      l.dx = w.amp * sinf(M_TAU * ph);
      break;
    case WB_PULSE: {
      float s = sinf(M_TAU * ph);
      float beat = s > 0.0f ? s * s : 0.0f;
      l.sw = l.sh = 1.0f + w.amp * beat;
      break;
    }
    case WB_NOD: {
      float f = fract(ph);
      float sink = f < 0.78f ? smoothstepf(0.0f, 0.78f, f) : 1.0f - smoothstepf(0.78f, 0.86f, f);
      l.dy = w.amp * sink;
      l.lid = 0.2f * sink;
      break;
    }
    default:
      break;
  }
  return l;
}

// Stack the life layers on the base pose and leave the frame ready to draw.
void FaceEngine::compose() {
  Layer l = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f};
  if (!act_) {
    // (an act moves the face by itself)
    l = wobble(Expressions::get(cur_).wobble);
    if (moving_) {
      Layer b = wobble(Expressions::get(next_).wobble);
      l.dx = lerpf(l.dx, b.dx, blend_);
      l.dy = lerpf(l.dy, b.dy, blend_);
      l.sw = lerpf(l.sw, b.sw, blend_);
      l.sh = lerpf(l.sh, b.sh, blend_);
      l.lid = lerpf(l.lid, b.lid, blend_);
    }
  }

  float gx = gx_.pos, gy = gy_.pos;
  for (int e = 0; e < 2; e++) {
    EyeParams p = base_.eye[e];
    float side = e == 0 ? -1.0f : 1.0f;
    float close = baseClose_[e] > blinkNow_[e] ? baseClose_[e] : blinkNow_[e];

    // The eye on the side we look at is seen smaller (fake perspective).
    float par = 1.0f - 0.13f * gx * side;
    float h0 = p.h * l.sh * par * (1.0f - 0.16f * fabsf(gy));
    p.w = p.w * l.sw * (1.0f + 0.10f * close);
    p.h = lerpf(h0, 2.0f, close);
    p.lidT = p.lidT + l.lid;

    // The pupil rests where the face says (measured towards the nose) and
    // moves with the gaze; the renderer wants it in screen terms.
    p.px = clampf(-side * p.px + gx * 0.9f, -1.0f, 1.0f);
    p.py = clampf(p.py - gy * 0.9f, -1.0f, 1.0f);
    p.pupil *= 1.0f - close;

    float ax = e == 0 ? EYE_ANCHOR_LX - p.x : EYE_ANCHOR_RX + p.x;
    eyeX_[e] = ax + gx * GAZE_PX_X + l.dx;
    // Lids meet below the middle of the eye.
    eyeY_[e] = EYE_ANCHOR_Y + p.y - gy * GAZE_PX_Y + l.dy + close * h0 * 0.10f;
    if (act_) {
      eyeX_[e] += actFrame_.dx[e];
      eyeY_[e] += actFrame_.dy[e];
    }
    eye_[e] = p;
  }

  mouth_ = base_.mouth;
  mouth_.w *= (1.0f - curtain_);
  mouthX_ = STAGE_W * 0.5f + gx * GAZE_PX_X * 0.5f + l.dx * 0.7f;
  mouthY_ = EYE_ANCHOR_Y + mouth_.y - gy * GAZE_PX_Y * 0.4f + l.dy * 0.5f;
  if (act_) {
    mouthX_ += (actFrame_.dx[0] + actFrame_.dx[1]) * 0.35f;
    mouthY_ += (actFrame_.dy[0] + actFrame_.dy[1]) * 0.35f;
  }
}

void FaceEngine::render(Canvas &c) {
  c.clear();

  FaceFrame f;
  for (int e = 0; e < 2; e++) {
    f.cx[e] = eyeX_[e];
    f.cy[e] = eyeY_[e];
    f.w[e] = eye_[e].w;
    f.h[e] = eye_[e].h;
  }
  f.gazeX = gx_.pos;
  f.gazeY = gy_.pos;
  f.mouthX = mouthX_;
  f.mouthY = mouthY_;
  f.mouthW = mouth_.w;
  f.time = time_;
  f.ink = ink_;

  bool show = act_ != nullptr && act_->draw != nullptr;
  if (show) act_->draw(c, actFrame_, f, ACT_BACK);

  bool on[2] = {true, true};
  if (act_) on[0] = actFrame_.eyeOn[0], on[1] = actFrame_.eyeOn[1];
  if (act_ && actFrame_.halo) {
    for (int e = 0; e < 2; e++) {
      if (on[e])
        FaceRenderer::drawEye(c, eyeX_[e], eyeY_[e], eye_[e], baseKind_[e], e == 0, time_, INK_BLACK, 1.8f, false);
    }
  }
  for (int e = 0; e < 2; e++) {
    if (on[e]) FaceRenderer::drawEye(c, eyeX_[e], eyeY_[e], eye_[e], baseKind_[e], e == 0, time_, ink_);
  }

  FaceRenderer::drawMouth(c, mouthX_, mouthY_, mouth_, ink_);

  float open = 1.0f - (blinkNow_[0] > blinkNow_[1] ? blinkNow_[0] : blinkNow_[1]);
  for (int i = 0; i < DECOR_COUNT; i++) {
    float w = decor_[i];
    uint32_t flag = 1u << i;
    if (flag == DC_GLINT) w *= open;  // no sparkle on a closed eye
    w *= (1.0f - curtain_);
    if (w > 0.02f) Decor::draw(c, flag, f, w);
  }

  if (show) {
    act_->draw(c, actFrame_, f, ACT_FRONT);
    act_->draw(c, actFrame_, f, ACT_POST);
  }
}
