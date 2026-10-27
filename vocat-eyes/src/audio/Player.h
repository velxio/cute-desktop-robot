// Plays sound through the speaker: a queue of 16 kHz mono samples in PSRAM that its own
// task drains into the I2S bus. Gemini's voice (24 kHz) is resampled on the way in, and the
// chirps of the face are synthesized here (the speaker is the buzzer of this robot).
#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stddef.h>
#include <stdint.h>

class AudioIo;
class Codec;

class Player {
 public:
  bool begin(AudioIo *io, Codec *codec);
  // Queues mono 16 kHz samples. Safe from any task.
  void push16k(const int16_t *pcm, size_t n);
  // Queues mono 24 kHz samples (what Gemini sends), resampled to 16 kHz.
  void push24k(const int16_t *pcm, size_t n);
  // Drops what is queued (somebody interrupted the robot).
  void clear();
  // True while sound comes out and for a short tail after.
  bool playing() const;
  // Queued seconds.
  float queuedSeconds() const;
  // A chirp of the face (CH_*), if nothing else is playing.
  void chirp(uint8_t chirp);

 private:
  static void taskEntry(void *arg);
  void task();
  size_t pop(int16_t *out, size_t n);
  void sweep(float f0, float f1, uint32_t ms);
  void silence(uint32_t ms);

  AudioIo *io_ = nullptr;
  Codec *codec_ = nullptr;
  int16_t *ring_ = nullptr;
  uint32_t size_ = 0;
  volatile uint32_t head_ = 0, tail_ = 0;  // head: next write, tail: next read
  volatile uint32_t lastSoundMs_ = 0;
  // 24 -> 16 kHz: the group of three being collected
  int16_t grp_[3] = {0, 0, 0};
  uint8_t grpN_ = 0;
  float phase_ = 0.0f;
};

#endif
