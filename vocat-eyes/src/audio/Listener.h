// Listens through the two microphones, all the time, in its own task:
//   - how loud it is, whether somebody is talking, sudden bangs
//   - from which side the voice comes (delay between the two microphones)
//   - a mono 16 kHz copy of the right microphone for the wake word and for Gemini
// It tells the Sensors what it hears; the Brain does the rest.
#ifndef AUDIO_LISTENER_H
#define AUDIO_LISTENER_H

#include <stddef.h>
#include <stdint.h>

class AudioIo;
class Sensors;

typedef void (*MicSink)(void *arg, const int16_t *mono16k, size_t n);

class Listener {
 public:
  bool begin(AudioIo *io, Sensors *sensors);
  // Gets every 20 ms frame of the microphone (320 samples) while enabled and not muted.
  void setSink(MicSink fn, void *arg);
  void setSinkEnabled(bool on) { sinkOn_ = on; }
  // While the speaker plays the robot does not listen to itself.
  void setMuted(bool on) { muted_ = on; }
  bool muted() const { return muted_; }

  float level() const { return level_; }        // rms 0..1 of the last frame
  float floor() const { return floor_; }        // background level
  bool voice() const { return voiceFrames_ >= 3; }
  float direction() const { return dir_; }      // degrees, + = robot's right
  // rms of the four TDM slots, to find out which slot is which microphone
  void slotLevels(float out[4]) const;
  // FreeRTOS stream buffer (as void*) with the mono stream for the wake word, or null.
  void *wakeStream() const { return wake_; }

 private:
  static void taskEntry(void *arg);
  void task();
  void frame(const int16_t *in4, size_t n);

  AudioIo *io_ = nullptr;
  Sensors *sensors_ = nullptr;
  MicSink sink_ = nullptr;
  void *sinkArg_ = nullptr;
  volatile bool sinkOn_ = false, muted_ = false;
  void *wake_ = nullptr;

  volatile float level_ = 0.0f, floor_ = 0.01f, prevLevel_ = 0.0f;
  volatile float slot_[4] = {0, 0, 0, 0};
  volatile int voiceFrames_ = 0;
  volatile float dir_ = 0.0f;
  float corr_[11];
  int corrFrames_ = 0;
  uint32_t lastVoiceNoteMs_ = 0;
};

#endif
