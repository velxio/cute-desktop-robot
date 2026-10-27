#include "Player.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "../../Config.h"
#include "../face/FaceTypes.h"
#include "AudioIo.h"
#include "Codec.h"

#define RING_SAMPLES (1u << 20)  // 64 s of 16 kHz mono in PSRAM
#define TAIL_MS 300

// ---- the chirps, as frequency sweeps {from Hz, to Hz, ms} (0 Hz = silence) ----------------
namespace {
struct Sweep {
  uint16_t f0, f1, ms;
};
const Sweep HELLO[] = {{880, 1320, 90}, {0, 0, 40}, {1320, 1760, 90}, {0, 0, 40}, {1760, 2640, 140}};
const Sweep HAPPY[] = {{1200, 1800, 70}, {0, 0, 30}, {1800, 2400, 70}, {0, 0, 30}, {2400, 3200, 110}};
const Sweep SAD[] = {{900, 600, 220}, {0, 0, 60}, {600, 380, 320}};
const Sweep ANGRY[] = {{320, 260, 120}, {0, 0, 40}, {300, 220, 120}, {0, 0, 40}, {260, 180, 220}};
const Sweep SURPRISE[] = {{700, 2800, 130}};
const Sweep QUESTION[] = {{900, 1100, 90}, {0, 0, 50}, {1100, 1900, 160}};
const Sweep SLEEP[] = {{700, 500, 260}, {0, 0, 120}, {500, 330, 380}};
const Sweep WAKE[] = {{400, 900, 160}, {0, 0, 60}, {900, 1500, 120}};
const Sweep LOVE[] = {{1500, 2000, 100}, {2000, 1500, 100}, {1500, 2500, 180}};
const Sweep SCARED[] = {{2600, 2200, 50}, {2200, 2700, 50}, {2700, 2100, 50}, {2100, 2800, 50}, {2800, 1500, 120}};
const Sweep LAUGH[] = {{1600, 1900, 55}, {0, 0, 45}, {1600, 1900, 55}, {0, 0, 45},
                       {1700, 2000, 55}, {0, 0, 45}, {1800, 2200, 70}};
const Sweep YAWN[] = {{500, 1000, 350}, {1000, 420, 600}};
const Sweep BLIP[] = {{1500, 1900, 45}};
const Sweep DING[] = {{1760, 1760, 90}, {0, 0, 30}, {2640, 2640, 180}};
const Sweep POP[] = {{900, 2600, 35}, {2600, 500, 60}};
const Sweep WHOOSH[] = {{300, 1800, 160}, {1800, 900, 120}};
const Sweep POWER_ON[] = {{200, 600, 180}, {600, 1400, 160}, {0, 0, 40}, {1800, 2200, 120}};
const Sweep POWER_OFF[] = {{1600, 700, 200}, {700, 150, 320}};
const Sweep SNEEZE[] = {{2400, 3400, 60}, {3400, 300, 180}};
const Sweep RAWR[] = {{180, 320, 140}, {320, 240, 90}, {240, 380, 110}, {380, 160, 260}};
const Sweep TUNE[] = {{523, 523, 110}, {0, 0, 20}, {659, 659, 110}, {0, 0, 20}, {784, 784, 110}, {0, 0, 20},
                      {1047, 1047, 170}, {0, 0, 20}, {784, 784, 110}, {0, 0, 20}, {659, 659, 170}};

#define PICK(a)                              \
  count = (int)(sizeof(a) / sizeof(a[0])); \
  return a

const Sweep *sequence(uint8_t chirp, int &count) {
  switch (chirp) {
    case CH_HELLO: PICK(HELLO);
    case CH_HAPPY: PICK(HAPPY);
    case CH_SAD: PICK(SAD);
    case CH_ANGRY: PICK(ANGRY);
    case CH_SURPRISE: PICK(SURPRISE);
    case CH_QUESTION: PICK(QUESTION);
    case CH_SLEEP: PICK(SLEEP);
    case CH_WAKE: PICK(WAKE);
    case CH_LOVE: PICK(LOVE);
    case CH_SCARED: PICK(SCARED);
    case CH_LAUGH: PICK(LAUGH);
    case CH_YAWN: PICK(YAWN);
    case CH_BLIP: PICK(BLIP);
    case CH_DING: PICK(DING);
    case CH_POP: PICK(POP);
    case CH_WHOOSH: PICK(WHOOSH);
    case CH_POWER_ON: PICK(POWER_ON);
    case CH_POWER_OFF: PICK(POWER_OFF);
    case CH_SNEEZE: PICK(SNEEZE);
    case CH_RAWR: PICK(RAWR);
    case CH_TUNE: PICK(TUNE);
    default:
      count = 0;
      return nullptr;
  }
}
}  // namespace

// ---- the queue --------------------------------------------------------------------------
void Player::push16k(const int16_t *pcm, size_t n) {
  if (!ring_) return;
  for (size_t i = 0; i < n; i++) {
    uint32_t next = (head_ + 1) & (size_ - 1);
    if (next == tail_) return;  // full: drop the rest
    ring_[head_] = pcm[i];
    head_ = next;
  }
}

// 3 samples in, 2 out: the first as is, the second halfway between the next two.
void Player::push24k(const int16_t *pcm, size_t n) {
  int16_t out[256];
  size_t o = 0;
  for (size_t i = 0; i < n; i++) {
    grp_[grpN_++] = pcm[i];
    if (grpN_ == 3) {
      out[o++] = grp_[0];
      out[o++] = (int16_t)(((int32_t)grp_[1] + grp_[2]) / 2);
      grpN_ = 0;
      if (o >= 254) {
        push16k(out, o);
        o = 0;
      }
    }
  }
  if (o) push16k(out, o);
}

void Player::clear() {
  tail_ = head_;
  grpN_ = 0;
}

size_t Player::pop(int16_t *out, size_t n) {
  size_t k = 0;
  while (k < n && tail_ != head_) {
    out[k++] = ring_[tail_];
    tail_ = (tail_ + 1) & (size_ - 1);
  }
  return k;
}

bool Player::playing() const {
  if (head_ != tail_) return true;
  return (uint32_t)(millis() - lastSoundMs_) < TAIL_MS;
}

float Player::queuedSeconds() const {
  uint32_t n = (head_ - tail_) & (size_ - 1);
  return n / (float)AUDIO_RATE;
}

// ---- chirps ----------------------------------------------------------------------------
void Player::sweep(float f0, float f1, uint32_t ms) {
  const float amp = 0.22f * 32767.0f;
  uint32_t n = ms * AUDIO_RATE / 1000;
  int16_t buf[160];
  size_t o = 0;
  for (uint32_t i = 0; i < n; i++) {
    float t = (float)i / (float)n;
    float f = f0 + (f1 - f0) * t;
    phase_ += f / (float)AUDIO_RATE;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    float env = 1.0f;
    uint32_t edge = AUDIO_RATE * 4 / 1000;  // 4 ms fades
    if (i < edge) env = (float)i / edge;
    else if (n - i < edge) env = (float)(n - i) / edge;
    float s = sinf(phase_ * 6.2831853f);
    s = s * 0.75f + (s > 0 ? 0.25f : -0.25f);  // a little square, like a piezo
    buf[o++] = (int16_t)(s * env * amp);
    if (o == 160) {
      push16k(buf, o);
      o = 0;
    }
  }
  if (o) push16k(buf, o);
}

void Player::silence(uint32_t ms) {
  int16_t z[160] = {0};
  uint32_t n = ms * AUDIO_RATE / 1000;
  while (n > 0) {
    size_t k = n > 160 ? 160 : n;
    push16k(z, k);
    n -= k;
  }
}

void Player::chirp(uint8_t ch) {
  if (!ring_ || playing()) return;
  int count = 0;
  const Sweep *seq = sequence(ch, count);
  if (!seq) return;
  phase_ = 0.0f;
  silence(15);
  for (int i = 0; i < count; i++) {
    if (seq[i].f0 == 0) silence(seq[i].ms);
    else sweep(seq[i].f0, seq[i].f1, seq[i].ms);
  }
  silence(15);
}

// ---- the task ---------------------------------------------------------------------------
#ifdef EMULATOR
bool Player::begin(AudioIo *io, Codec *codec) {
  io_ = io;
  codec_ = codec;
  size_ = 1u << 14;
  ring_ = new int16_t[size_];
  return true;
}
void Player::taskEntry(void *) {}
void Player::task() {}

#else
bool Player::begin(AudioIo *io, Codec *codec) {
  io_ = io;
  codec_ = codec;
  size_ = RING_SAMPLES;
  ring_ = (int16_t *)ps_malloc(size_ * sizeof(int16_t));
  if (!ring_) return false;
  xTaskCreatePinnedToCore(taskEntry, "player", 4096, this, 6, nullptr, 1);
  return true;
}

void Player::taskEntry(void *arg) { ((Player *)arg)->task(); }

void Player::task() {
  int16_t buf[320];
  for (;;) {
    size_t k = pop(buf, 320);
    if (k > 0) {
      if (codec_) codec_->enablePa(true);
      lastSoundMs_ = millis();
      if (k < 320) memset(buf + k, 0, (320 - k) * sizeof(int16_t));
      io_->writeMono(buf, 320);
    } else {
      // keep the DMA fed with silence; the amplifier goes off after a while
      memset(buf, 0, sizeof(buf));
      io_->writeMono(buf, 320);
      if (codec_ && (uint32_t)(millis() - lastSoundMs_) > 1500) codec_->enablePa(false);
    }
  }
}
#endif
