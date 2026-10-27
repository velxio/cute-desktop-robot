// The I2S bus shared by both codecs: a standard stereo stream out to the ES8311 and a
// 4-slot TDM stream in from the ES7210, both at AUDIO_RATE (16 kHz) and 16 bit.
#ifndef AUDIO_AUDIOIO_H
#define AUDIO_AUDIOIO_H

#include <stddef.h>
#include <stdint.h>

class AudioIo {
 public:
  bool begin();
  // Reads `frames` frames of 4 samples (one per TDM slot). Blocks until they arrive.
  size_t read4(int16_t *out, size_t frames);
  // Plays mono samples (copied to both channels). Blocks while the DMA queue is full.
  size_t writeMono(const int16_t *mono, size_t n);
  bool ok() const { return ok_; }

 private:
  bool ok_ = false;
};

#endif
