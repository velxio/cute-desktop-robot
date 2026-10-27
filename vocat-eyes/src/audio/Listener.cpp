#include "Listener.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "../../Config.h"
#include "../brain/Sensors.h"
#include "AudioIo.h"

#define FRAME 320  // 20 ms at 16 kHz
#define MAX_LAG 5  // samples: 60 mm between the mics is 2.8 samples, with margin

void Listener::setSink(MicSink fn, void *arg) {
  sink_ = fn;
  sinkArg_ = arg;
}

void Listener::slotLevels(float out[4]) const {
  for (int i = 0; i < 4; i++) out[i] = slot_[i];
}

#ifdef EMULATOR
bool Listener::begin(AudioIo *io, Sensors *sensors) {
  io_ = io;
  sensors_ = sensors;
  return true;
}
void Listener::taskEntry(void *) {}
void Listener::task() {}
void Listener::frame(const int16_t *, size_t) {}

#else
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

namespace {
int16_t micR[FRAME], micL[FRAME];
StaticStreamBuffer_t wakeStatic;
}  // namespace

bool Listener::begin(AudioIo *io, Sensors *sensors) {
  io_ = io;
  sensors_ = sensors;
  memset(corr_, 0, sizeof(corr_));
#if WAKE_WORD
  const size_t bytes = 16000 * sizeof(int16_t);  // one second
  uint8_t *storage = (uint8_t *)ps_malloc(bytes);
  if (storage) wake_ = xStreamBufferCreateStatic(bytes, 1, storage, &wakeStatic);
#endif
  xTaskCreatePinnedToCore(taskEntry, "listener", 6144, this, 7, nullptr, 0);
  return true;
}

void Listener::taskEntry(void *arg) { ((Listener *)arg)->task(); }

void Listener::task() {
  static int16_t in4[FRAME * 4];
  for (;;) {
    size_t n = io_->read4(in4, FRAME);
    if (n == 0) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    frame(in4, n);
  }
}

void Listener::frame(const int16_t *in4, size_t n) {
  // split the slots, measure each
  double acc[4] = {0, 0, 0, 0};
  for (size_t i = 0; i < n; i++) {
    for (int s = 0; s < 4; s++) {
      float v = in4[i * 4 + s];
      acc[s] += v * v;
    }
    micR[i] = in4[i * 4 + MIC_SLOT_RIGHT];
    micL[i] = in4[i * 4 + MIC_SLOT_LEFT];
  }
  for (int s = 0; s < 4; s++) slot_[s] = sqrtf((float)(acc[s] / n)) / 32768.0f;
  float level = slot_[MIC_SLOT_RIGHT];
  prevLevel_ = level_;
  level_ = level;

  // the background: follows the quiet quickly down, loud sounds slowly up
  if (level < floor_) floor_ = level;
  else floor_ += (level - floor_) * 0.002f;
  if (floor_ < 0.0005f) floor_ = 0.0005f;

  bool talking = !muted_ && level > fmaxf(floor_ * 3.5f, VOICE_LEVEL_MIN);
  voiceFrames_ = talking ? voiceFrames_ + 1 : 0;

  // a bang: much louder than the frame before, out of nowhere
  if (!muted_ && level > NOISE_LEVEL_MIN && prevLevel_ < level * 0.2f && sensors_) sensors_->noteNoise(dir_, level);

  // where does it come from: cross-correlation of the two microphones over a few lags,
  // accumulated over 100 ms of voice, peak with parabolic interpolation
  if (talking) {
    for (int lag = -MAX_LAG; lag <= MAX_LAG; lag++) {
      float r = 0.0f;
      int i0 = lag < 0 ? -lag : 0, i1 = lag > 0 ? (int)n - lag : (int)n;
      for (int i = i0; i < i1; i++) r += (float)micR[i] * (float)micL[i + lag];
      corr_[lag + MAX_LAG] += r;
    }
    corrFrames_++;
    if (corrFrames_ >= 5) {
      int best = 0;
      for (int k = 1; k < 2 * MAX_LAG + 1; k++)
        if (corr_[k] > corr_[best]) best = k;
      float tau = (float)(best - MAX_LAG);
      if (best > 0 && best < 2 * MAX_LAG) {
        float a = corr_[best - 1], b = corr_[best], c = corr_[best + 1];
        float den = a - 2.0f * b + c;
        if (fabsf(den) > 1e-3f) tau += 0.5f * (a - c) / den;
      }
      // positive lag: the left mic lags the right one -> the sound came from the right
      float s = tau * 343.0f / ((float)AUDIO_RATE * MIC_SPACING_MM / 1000.0f);
      if (s > 1.0f) s = 1.0f;
      if (s < -1.0f) s = -1.0f;
      float deg = asinf(s) * 57.2958f;
      dir_ += (deg - dir_) * 0.4f;
      memset(corr_, 0, sizeof(corr_));
      corrFrames_ = 0;
      if (sensors_ && voiceFrames_ >= 3) sensors_->noteVoice(dir_, fminf(1.0f, level * 8.0f));
    }
  } else {
    memset(corr_, 0, sizeof(corr_));
    corrFrames_ = 0;
  }

  // the mono copy: for the wake word and for whoever listens (Gemini)
  if (!muted_) {
    if (wake_) xStreamBufferSend((StreamBufferHandle_t)wake_, micR, n * sizeof(int16_t), 0);
    if (sinkOn_ && sink_) sink_(sinkArg_, micR, n);
  }
}
#endif
