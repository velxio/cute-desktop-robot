// A plain RGB picture in memory: pages of faces for facetool, frames for the
// GIF recorder. Saved as PNG through raylib.
#ifndef EMU_PICTURE_H
#define EMU_PICTURE_H

#include <stdint.h>
#include <string.h>

#include <vector>

#include "raylib.h"

#include "../src/core/Canvas.h"
#include "../src/core/Font5x7.h"

struct Picture {
  int w, h;
  std::vector<uint32_t> rgb;  // 0xRRGGBB

  Picture(int w_, int h_, uint32_t fill = 0x14161C) : w(w_), h(h_), rgb((size_t)w_ * h_, fill) {}

  void set(int x, int y, uint32_t c) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    rgb[(size_t)y * w + x] = c;
  }

  // What the round glass shows, reduced `shrink` times. Outside the glass
  // goes `frame`, so what the round edge cuts is seen as cut.
  void screen(const Canvas &c, int ox, int oy, int shrink = 1, uint32_t frame = 0x262A33) {
    const int size = SCREEN_W / shrink;
    const float r = SCREEN_W * 0.5f;
    for (int y = 0; y < size; y++) {
      for (int x = 0; x < size; x++) {
        int sr = 0, sg = 0, sb = 0, inside = 0;
        for (int j = 0; j < shrink; j++) {
          for (int i = 0; i < shrink; i++) {
            int px = x * shrink + i, py = y * shrink + j;
            float dx = px + 0.5f - r, dy = py + 0.5f - r;
            if (dx * dx + dy * dy > r * r) continue;
            Rgb v = c.getPx(px, py);
            sr += (v >> 16) & 255, sg += (v >> 8) & 255, sb += v & 255;
            inside++;
          }
        }
        int all = shrink * shrink, out = all - inside;
        sr += out * ((frame >> 16) & 255), sg += out * ((frame >> 8) & 255), sb += out * (frame & 255);
        set(ox + x, oy + y, (uint32_t)((sr / all) << 16 | (sg / all) << 8 | (sb / all)));
      }
    }
  }

  void text(int x, int y, const char *s, uint32_t color = 0xD6DEEC, int scale = 1) {
    for (; *s; s++, x += 6 * scale) {
      unsigned char ch = (unsigned char)*s;
      if (ch < 0x20 || ch > 0x7E) ch = '?';
      for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
          if (FONT5X7[ch - 0x20][col] & (1 << row))
            for (int j = 0; j < scale; j++)
              for (int i = 0; i < scale; i++) set(x + col * scale + i, y + row * scale + j, color);
    }
  }

  bool savePng(const char *path) const {
    Image img;
    img.width = w;
    img.height = h;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    std::vector<unsigned char> bytes((size_t)w * h * 3);
    for (size_t i = 0; i < rgb.size(); i++) {
      bytes[i * 3 + 0] = (unsigned char)(rgb[i] >> 16);
      bytes[i * 3 + 1] = (unsigned char)(rgb[i] >> 8);
      bytes[i * 3 + 2] = (unsigned char)rgb[i];
    }
    img.data = bytes.data();
    return ExportImage(img, path);
  }
};

#endif
