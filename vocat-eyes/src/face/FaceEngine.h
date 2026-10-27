// Runtime of the face. It owns the current pose, walks the emotion graph,
// and stacks the "life" layers (blink, gaze, wobble, decorations) on top.
//
//   goTo(EX_GLEE)  -> route over the graph -> hop by hop morph -> layers -> pixels
//
// Besides faces it can put on a show: an act (computed animation, see Acts.h).
// Acts hang from a face of the graph.
//
// No Arduino dependency: time comes in through update(nowMs).
#ifndef FACE_FACEENGINE_H
#define FACE_FACEENGINE_H

#include "../core/Canvas.h"
#include "../core/MathUtil.h"
#include "Acts.h"
#include "ExpressionGraph.h"
#include "Expressions.h"
#include "FaceRenderer.h"

class FaceEngine {
 public:
  void begin(uint32_t seed, uint8_t startExpr = EX_ASLEEP);
  void update(uint32_t nowMs);
  void render(Canvas &c);

  // ---- commands ---------------------------------------------------------------
  // Walk the graph to `expr`. `urgent` makes the trip twice as fast and cuts
  // a running show (used for startle reactions).
  void goTo(uint8_t expr, bool urgent = false);
  void snapTo(uint8_t expr);

  // Goes to the base face of the act, plays it and continues to `thenExpr`
  // (EX_NONE = stays on the face where the act ended).
  bool playAct(int actIndex, uint8_t thenExpr = EX_NONE);
  // Cuts the act that is running or waiting.
  void stopShow();

  void blink();
  void wink(bool leftEye);
  // x: -1 left .. +1 right, y: -1 down .. +1 up. Overrides the wandering gaze
  // for `holdMs`.
  void lookAt(float x, float y, uint32_t holdMs = 1500);
  // An eye flinches shut for a moment (somebody poked it).
  void flinch(bool leftEye, uint32_t holdMs = 450);
  void setAutoGaze(bool on) { autoGaze_ = on; }
  void setAutoBlink(bool on) { autoBlink_ = on; }
  // Closes the eyes and keeps them shut, e.g. to show an info screen.
  void setCurtain(bool closed) { curtainTarget_ = closed; }
  float curtain() const { return curtain_; }

  // ---- state ------------------------------------------------------------------
  uint8_t current() const { return cur_; }                    // last node reached
  uint8_t heading() const { return moving_ ? next_ : cur_; }  // node being approached
  uint8_t target() const { return target_; }                  // end of the route
  bool moving() const { return moving_; }
  bool showing() const { return act_ != nullptr; }
  bool busy() const { return moving_ || pathPos_ < pathLen_ || showing() || pendingAct_ >= 0; }
  float progress() const { return moving_ ? p_ : 1.0f; }
  uint8_t mood() const { return Expressions::get(heading()).mood; }

  int pathLength() const { return pathLen_; }
  int pathPosition() const { return pathPos_; }
  uint8_t pathNode(int i) const { return path_[i]; }

  bool actActive() const { return act_ != nullptr; }
  const ActDef *act() const { return act_; }
  float actTime() const { return act_ ? actFrame_.t : 0.0f; }

  // Color of the eyes right now.
  Rgb ink() const { return ink_; }
  // Where the eyes are on the screen, face units (for the touch screen).
  float eyeX(int eye) const { return eyeX_[eye]; }
  float eyeY(int eye) const { return eyeY_[eye]; }
  float eyeW(int eye) const { return eye_[eye].w; }
  float eyeH(int eye) const { return eye_[eye].h; }

  // What the ears should be doing right now.
  EarDef ears() const { return ears_; }
  // Sound to play, reported once.
  uint8_t takeChirp();

  float gazeX() const { return gx_.pos; }
  float gazeY() const { return gy_.pos; }
  float blinkAmount(int eye) const { return blinkNow_[eye]; }

 private:
  struct Layer {
    float dx, dy;  // screen offset shared by both eyes
    float sw, sh;  // size multipliers
    float lid;     // extra upper lid closure
  };

  void route(uint8_t expr, bool urgent);
  void startHop(uint8_t node, float ms, uint8_t easing);
  void nextHop();
  void settle(uint8_t expr);
  void startPending();
  void updateAct(float dt);
  void updateBase();
  void updateDecor(float dt);
  void updateGaze(float dt);
  void updateBlink(float dt);
  void updateCurtain(float dt);
  void compose();
  Layer wobble(const WobbleDef &w) const;

  Rng rng_;
  uint32_t nowMs_ = 0, lastMs_ = 0;
  float time_ = 0.0f;
  bool started_ = false;

  // graph walk
  uint8_t cur_ = EX_NORMAL, next_ = EX_NORMAL, target_ = EX_NORMAL;
  uint8_t path_[GRAPH_MAX_PATH];
  int pathLen_ = 0, pathPos_ = 0;
  bool moving_ = false, urgent_ = false;
  float p_ = 0.0f, hopMs_ = 300.0f;
  uint8_t hopEase_ = EASE_IN_OUT;

  // pose being shown, before the life layers
  FacePose from_;
  uint8_t fromKind_[2];
  FacePose base_;
  uint8_t baseKind_[2];
  float baseClose_[2];  // 0..1 squash used while an eye swaps its kind
  float blend_ = 1.0f;  // eased progress, used to blend wobble / ears

  float decor_[DECOR_COUNT];
  Rgb ink_ = INK_CYAN, fromInk_ = INK_CYAN;
  EarDef ears_;
  uint8_t chirp_ = CH_NONE;

  // gaze
  Spring gx_, gy_;
  float gazeTx_ = 0.0f, gazeTy_ = 0.0f;
  uint32_t gazeNextMs_ = 0, gazeHoldUntil_ = 0;
  bool autoGaze_ = true, gazeManual_ = false;

  // blink
  bool autoBlink_ = true;
  float blinkT_[2];  // seconds since the blink of each eye started, <0 = idle
  float blinkNow_[2];
  uint32_t blinkNextMs_ = 0;
  bool blinkAgain_ = false;
  float curtain_ = 0.0f;
  bool curtainTarget_ = false;
  uint32_t flinchUntil_[2] = {0, 0};

  // shows
  const ActDef *act_ = nullptr;
  ActFrame actFrame_;
  uint32_t actStartMs_ = 0;
  float actPrevT_ = -1.0f;
  int pendingAct_ = -1;
  uint8_t afterShow_ = EX_NORMAL;

  // final frame, filled by update()
  EyeParams eye_[2];
  float eyeX_[2], eyeY_[2];
  MouthParams mouth_;
  float mouthX_ = 64.0f, mouthY_ = 32.0f;
};

#endif
