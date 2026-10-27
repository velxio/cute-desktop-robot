// The conversation, seen from the face: when to start it (wake word, a shell pad, the
// console), what the eyes do while it listens and talks, and what Gemini's tools change.
#ifndef VOICE_TALK_H
#define VOICE_TALK_H

#include <stdint.h>

class Brain;
class FaceEngine;
class Sensors;
class Player;
class Listener;
class GeminiLive;
class WakeWord;

class Talk {
 public:
  enum State : uint8_t { OFF, CONNECTING, LISTENING, SPEAKING };

  void begin(Brain *brain, FaceEngine *face, Sensors *sensors, Player *player, Listener *listener, GeminiLive *gemini,
             WakeWord *wake);
  void update(uint32_t nowMs);
  void start(const char *why);
  void stop();
  State state() const { return state_; }
  const char *stateName() const;
  bool active() const { return state_ != OFF; }
  const char *lastText() const { return text_; }
  void setSounds(bool on) { sounds_ = on; }

 private:
  void onTool(const char *name, const char *arg);
  void setFace(uint8_t expr, uint32_t holdMs);
  void checkPads(uint32_t nowMs);

  Brain *brain_ = nullptr;
  FaceEngine *face_ = nullptr;
  Sensors *sensors_ = nullptr;
  Player *player_ = nullptr;
  Listener *listener_ = nullptr;
  GeminiLive *gemini_ = nullptr;
  WakeWord *wake_ = nullptr;
  State state_ = OFF;
  bool sounds_ = true;
  uint32_t toolFaceUntil_ = 0, faceRefresh_ = 0;
  char text_[120] = {0};
  // shell pads
  uint32_t padBase_[2] = {0, 0};
  uint8_t padSamples_ = 0;
  bool padDown_[2] = {false, false};
  uint32_t padAt_ = 0;
};

#endif
