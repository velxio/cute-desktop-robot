// Animated GIF writer for color frames. Every frame carries its own palette
// of up to 256 colors, picked from what the frame actually uses, so the eyes
// keep their gradients.
#ifndef EMU_GIFWRITER_H
#define EMU_GIFWRITER_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <vector>

class GifWriter {
 public:
  ~GifWriter() { close(); }

  bool open(const char *path, int w, int h) {
    close();
    f_ = fopen(path, "wb");
    if (!f_) return false;
    w_ = w;
    h_ = h;
    fwrite("GIF89a", 1, 6, f_);
    put16(w_);
    put16(h_);
    fputc(0x70, f_);  // no global color table
    fputc(0, f_);
    fputc(0, f_);
    // loop forever
    fputc(0x21, f_);
    fputc(0xFF, f_);
    fputc(11, f_);
    fwrite("NETSCAPE2.0", 1, 11, f_);
    fputc(3, f_);
    fputc(1, f_);
    put16(0);
    fputc(0, f_);
    frames_ = 0;
    return true;
  }

  bool isOpen() const { return f_ != nullptr; }
  int frames() const { return frames_; }
  int width() const { return w_; }
  int height() const { return h_; }

  // `rgb`: w * h pixels, 0xRRGGBB.
  void frame(const uint32_t *rgb, int delayMs) {
    if (!f_) return;
    const int n = w_ * h_;
    std::vector<uint8_t> index((size_t)n);
    uint8_t palette[256 * 3];
    quantize(rgb, n, index.data(), palette);

    int cs = (delayMs + 5) / 10;
    if (cs < 2) cs = 2;
    fputc(0x21, f_);
    fputc(0xF9, f_);
    fputc(4, f_);
    fputc(0x04, f_);
    put16(cs);
    fputc(0, f_);
    fputc(0, f_);
    fputc(0x2C, f_);
    put16(0);
    put16(0);
    put16(w_);
    put16(h_);
    fputc(0x87, f_);  // local color table, 256 entries
    fwrite(palette, 1, sizeof(palette), f_);

    const int minCode = 8;
    fputc(minCode, f_);
    const int clearCode = 1 << minCode;
    tree_.assign(4096 * 256, 0);
    int codeSize = minCode + 1, maxCode = clearCode + 1, cur = -1;
    bitBuf_ = 0;
    bitCount_ = 0;
    chunkLen_ = 0;
    code(clearCode, codeSize);
    for (int i = 0; i < n; i++) {
      int v = index[(size_t)i];
      if (cur < 0) {
        cur = v;
      } else if (tree_[(size_t)cur * 256 + v]) {
        cur = tree_[(size_t)cur * 256 + v];
      } else {
        code(cur, codeSize);
        tree_[(size_t)cur * 256 + v] = (uint16_t)(++maxCode);
        if (maxCode >= (1 << codeSize)) codeSize++;
        if (maxCode == 4095) {
          code(clearCode, codeSize);
          tree_.assign(4096 * 256, 0);
          codeSize = minCode + 1;
          maxCode = clearCode + 1;
        }
        cur = v;
      }
    }
    code(cur, codeSize);
    code(clearCode, codeSize);
    code(clearCode + 1, minCode + 1);
    while (bitCount_ > 0) flushByte();
    flushChunk();
    fputc(0, f_);
    frames_++;
  }

  void close() {
    if (!f_) return;
    fputc(0x3B, f_);
    fclose(f_);
    f_ = nullptr;
  }

 private:
  // Picks the colors of the frame. Few colors: all of them. Many: the most
  // used ones, on a grid of 32 levels per channel.
  void quantize(const uint32_t *rgb, int n, uint8_t *index, uint8_t *palette) {
    count_.assign(32768, 0);
    for (int i = 0; i < n; i++) count_[key(rgb[i])]++;
    std::vector<int> used;
    for (int k = 0; k < 32768; k++)
      if (count_[(size_t)k]) used.push_back(k);
    std::sort(used.begin(), used.end(), [&](int a, int b) { return count_[(size_t)a] > count_[(size_t)b]; });
    int colors = (int)used.size() < 256 ? (int)used.size() : 256;
    memset(palette, 0, 256 * 3);
    for (int i = 0; i < colors; i++) {
      int k = used[(size_t)i];
      palette[i * 3 + 0] = (uint8_t)(((k >> 10) & 31) * 255 / 31);
      palette[i * 3 + 1] = (uint8_t)(((k >> 5) & 31) * 255 / 31);
      palette[i * 3 + 2] = (uint8_t)((k & 31) * 255 / 31);
    }
    map_.assign(32768, -1);
    for (int i = 0; i < colors; i++) map_[(size_t)used[(size_t)i]] = i;
    for (size_t u = (size_t)colors; u < used.size(); u++) {
      int k = used[u];
      int r = (k >> 10) & 31, g = (k >> 5) & 31, b = k & 31;
      int best = 0, bestD = 1 << 30;
      for (int i = 0; i < colors; i++) {
        int q = used[(size_t)i];
        int dr = r - ((q >> 10) & 31), dg = g - ((q >> 5) & 31), db = b - (q & 31);
        int d = dr * dr * 2 + dg * dg * 4 + db * db;
        if (d < bestD) bestD = d, best = i;
      }
      map_[(size_t)k] = best;
    }
    for (int i = 0; i < n; i++) index[i] = (uint8_t)map_[key(rgb[i])];
  }

  static size_t key(uint32_t c) { return (size_t)(((c >> 19) & 31) << 10 | ((c >> 11) & 31) << 5 | ((c >> 3) & 31)); }

  void put16(int v) {
    fputc(v & 0xFF, f_);
    fputc((v >> 8) & 0xFF, f_);
  }
  void flushChunk() {
    if (!chunkLen_) return;
    fputc(chunkLen_, f_);
    fwrite(chunk_, 1, (size_t)chunkLen_, f_);
    chunkLen_ = 0;
  }
  void flushByte() {
    chunk_[chunkLen_++] = (uint8_t)(bitBuf_ & 0xFF);
    bitBuf_ >>= 8;
    bitCount_ = bitCount_ > 8 ? bitCount_ - 8 : 0;
    if (chunkLen_ == 255) flushChunk();
  }
  void code(int c, int size) {
    bitBuf_ |= (uint32_t)c << bitCount_;
    bitCount_ += size;
    while (bitCount_ >= 8) flushByte();
  }

  FILE *f_ = nullptr;
  int w_ = 0, h_ = 0, frames_ = 0;
  std::vector<uint16_t> tree_;
  std::vector<int> count_, map_;
  uint32_t bitBuf_ = 0;
  int bitCount_ = 0;
  uint8_t chunk_[256];
  int chunkLen_ = 0;
};

#endif
