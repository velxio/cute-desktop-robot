// Reads and cleans the sensors. The rest of the code only sees events and
// normalized values, never raw pins.
//
//   touch screen -> gestures (tap, stroke, long press)
//   microphones  -> noise, voice and where it comes from (fed by the audio module)
//   fuel gauge   -> battery        clock (kept by the ESP32, set by NTP or serial)
//
// The ESP-VoCat has no light or climate sensor; those fields stay at "bright" and
// "unknown" so the Brain, shared with the other robots, keeps working.
#ifndef BRAIN_SENSORS_H
#define BRAIN_SENSORS_H

#include <Arduino.h>

#include "../hw/Battery.h"
#include "../hw/Rtc.h"
#include "../hw/Touch.h"

enum TouchGesture : uint8_t {
  G_NONE = 0,
  G_TAP,     // short touch without moving
  G_HOLD,    // long touch without moving (reported once, while still pressed)
  G_STROKE   // the finger moved over the glass: petting
};

struct SensorState {
  // ambient light: none on this board, the Brain sees daylight all the time
  int lightRaw = 0;
  float light = 1.0f;       // 0 = dark, 1 = bright
  bool dark = false;
  bool lightEdge = false;   // dark -> bright, one frame
  bool darkEdge = false;    // bright -> dark, one frame
  // sound (two microphones, see src/audio)
  bool sound = false;       // a sudden loud noise since the last frame: it startles
  bool voice = false;       // somebody is talking (one frame per burst): it looks there
  bool wakeWord = false;    // "Hi ESP" heard, one frame
  bool soundDirValid = false;
  float soundDir = 0.0f;    // degrees, 0 = in front, positive = to the robot's right
  float soundLevel = 0.0f;  // 0..1, how loud, smoothed
  // climate: none on this board
  bool climateValid = false;
  float tempC = 0.0f;
  float humidity = 0.0f;
  // touch, in face units (see core/Canvas.h)
  bool touching = false;
  float touchX = 0.0f, touchY = 0.0f;
  uint8_t gesture = G_NONE;  // one frame
  float gestureX = 0.0f, gestureY = 0.0f;  // where it started
  float strokeLength = 0.0f;               // face units travelled
  // battery
  BatteryState battery;
  bool batteryLow = false;
  // clock
  ClockTime clock;
};

class Sensors {
 public:
  void begin();
  // Call as often as possible (cheap).
  void poll();
  // Call once per frame.
  void update(uint32_t nowMs);
  const SensorState &state() const { return st_; }
  void setClock(const ClockTime &t) { rtc_.set(t); }

  // The audio module reports here (it may run in another task): a loud noise that
  // stands out of the background, a voice burst, and the wake word, each with the
  // direction it came from (degrees, 0 = front, + = right) and its level 0..1.
  void noteNoise(float dirDeg, float level);
  void noteVoice(float dirDeg, float level);
  void noteWakeWord(float dirDeg);

 private:
  void updateTouch(uint32_t nowMs);

  SensorState st_;
  Touch touch_;
  Rtc rtc_;
  Battery battery_;
  bool started_ = false;
  // latches written by the audio module, read once per frame
  volatile bool noiseLatch_ = false, voiceLatch_ = false, wakeLatch_ = false;
  volatile float latchDir_ = 0.0f, latchLevel_ = 0.0f;
  uint32_t noiseMuteUntil_ = 0, voiceMuteUntil_ = 0;
  uint32_t nextClock_ = 0, nextBattery_ = 0;
  // touch
  bool down_ = false, moved_ = false, held_ = false;
  uint32_t downAt_ = 0;
  float x0_ = 0, y0_ = 0, lastX_ = 0, lastY_ = 0, travel_ = 0;
};

#endif
