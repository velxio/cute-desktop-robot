// "Hi ESP": the wake word of ESP-SR (WakeNet 9, model in the esp_sr partition), fed with
// the mono stream of the Listener. While the robot talks it is stopped altogether: ESP-SR
// keeps ~100 KB of internal RAM that the TLS connection to Gemini needs; pause() frees
// it and resume() loads the engine again (about a second).
#ifndef AUDIO_WAKEWORD_H
#define AUDIO_WAKEWORD_H

#include <stddef.h>
#include <stdint.h>

class Listener;

class WakeWord {
 public:
  bool begin(Listener *listener);
  bool running() const { return running_; }
  // True once after each detection.
  bool takeDetected();
  void pause();
  void resume();
  // Off: stays stopped (and its RAM free) until switched on again.
  void setEnabled(bool on);
  bool enabled() const { return enabled_; }

 private:
  bool launch();

  Listener *listener_ = nullptr;
  bool running_ = false;
  bool paused_ = false;
  bool enabled_ = true;
};

#endif
