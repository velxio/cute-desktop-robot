// Settings on the glass: volume, brightness, Gemini's voice, Wi-Fi, the wake word, the
// sounds of the face, whether the robot does things on its own, and how long a silence
// ends a conversation. Opened with a tap on the clock screen; a long press (or 30 s
// without touching) closes it and saves everything in the flash (NVS).
#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <stdint.h>

#include "../brain/Sensors.h"
#include "../core/Canvas.h"

struct Settings {
  uint8_t volume = 60;      // 0..100
  uint8_t brightness = 100; // 20..100 %
  uint8_t voice = 0;        // index in Settings::VOICES
  bool wakeWord = true;     // listens for "Hi ESP"
  bool sounds = true;       // the chirps of the face
  bool autonomous = true;   // looks around, plays, sleeps on its own
  uint8_t talkEnd = 1;      // index in Settings::TALK_END_S

  static const char *const VOICES[];
  static const int VOICE_COUNT;
  static const uint8_t TALK_END_S[];
  static const int TALK_END_COUNT;

  const char *voiceName() const { return VOICES[voice % VOICE_COUNT]; }
  uint32_t talkEndMs() const { return TALK_END_S[talkEnd % TALK_END_COUNT] * 1000UL; }
  void load();  // defaults from Config.h, then what was saved
  void save() const;
};

class SettingsScreen {
 public:
  enum Item : uint8_t { VOLUME, BRIGHTNESS, VOICE, WIFI, WAKE, SOUNDS, AUTONOMOUS, TALK_END, ITEM_COUNT };
  // Called after an item changed (apply it) - WIFI means "open the Wi-Fi setup".
  typedef void (*ChangeFn)(Item item);

  void begin(Settings *settings, ChangeFn onChange) {
    cfg_ = settings;
    onChange_ = onChange;
  }
  void open();
  void close();  // saves
  bool active() const { return open_; }
  // Shown on the Wi-Fi row.
  void setWifiState(bool ok) { wifiOk_ = ok; }
  void update(const SensorState &s, uint32_t nowMs);
  void draw(Canvas &c, uint32_t nowMs);

 private:
  static const int PER_PAGE = 5;
  int rowAt(float x, float y) const;  // item index, -2 page button, -1 nothing
  void change(int item, int dir);
  void valueText(int item, char *out, int n) const;

  Settings *cfg_ = nullptr;
  ChangeFn onChange_ = nullptr;
  bool open_ = false, wifiOk_ = false;
  int page_ = 0, hot_ = -1;
  uint32_t lastTouchMs_ = 0, flashUntil_ = 0;
  int flashItem_ = -1;
};

#endif
