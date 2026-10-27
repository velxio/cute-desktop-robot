// Command line companion of the face engine (no window, no hardware).
// It runs the very same code that goes into the robot and writes what the
// round screen would show, as PNG pages or GIF animations.
//
//   facetool list                        expressions, acts and graph size
//   facetool path  <from> <to>           route over the emotion graph
//   facetool face  <expr> <out.png> [t]  one face, full size
//   facetool sheet <out.png> [t]         every expression on one page
//   facetool strip <act> <out.png> [n]   n moments of an act side by side
//   facetool gif   <expr> <out.gif> [from] [hold_s]
//   facetool trip  <out.gif> <expr> <expr> ...   a walk through several faces
//   facetool act   <name> <out.gif>      an act (computed animation)
//   facetool say   <text> <out.gif>      the speech balloon
//   facetool all   <dir>                 one GIF per expression + sheet.png
//   facetool acts  <dir>                 one GIF per act
//   facetool cost                        rough time per frame of every act
//
// GIFs are written at half size (120 x 120) and 20 frames per second unless
// the environment variable GIF_FULL is set.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir(p, 0755)
#endif

#include "../src/face/FaceEngine.h"
#include "GifWriter.h"
#include "Picture.h"

static const int FRAME_MS = 33;

static bool gifFull() { return getenv("GIF_FULL") != nullptr; }

static uint8_t exprOrDie(const char *name) {
  uint8_t id = Expressions::find(name);
  if (id == EX_NONE) {
    fprintf(stderr, "unknown expression '%s' (see: facetool list)\n", name);
    exit(1);
  }
  return id;
}

// ---- GIF recording --------------------------------------------------------------------
struct Recorder {
  GifWriter gif;
  int shrink = 2, every = 2, tick = 0;

  bool open(const char *path) {
    if (gifFull()) shrink = 1, every = 1;
    return gif.open(path, SCREEN_W / shrink, SCREEN_H / shrink);
  }
  void add(const Canvas &c) {
    if (tick++ % every) return;
    Picture p(SCREEN_W / shrink, SCREEN_H / shrink);
    p.screen(c, 0, 0, shrink, 0x000000);
    gif.frame(p.rgb.data(), FRAME_MS * every);
  }
};

// Runs the engine until it is idle (plus `holdMs`), writing every frame.
static void record(FaceEngine &face, Canvas &c, Recorder &rec, uint32_t &now, uint32_t holdMs, uint32_t maxMs = 60000) {
  uint32_t idleSince = 0;
  bool idle = false;
  uint32_t start = now;
  while (now - start < maxMs) {
    now += FRAME_MS;
    face.update(now);
    face.render(c);
    rec.add(c);
    if (!face.busy()) {
      if (!idle) {
        idle = true;
        idleSince = now;
      }
      if (now - idleSince >= holdMs) break;
    } else {
      idle = false;
    }
  }
}

static int cmdList() {
  printf("Expressions (%d):\n", (int)EX_COUNT);
  for (int i = 0; i < EX_COUNT; i++) {
    const ExpressionDef &d = Expressions::get((uint8_t)i);
    uint8_t nb[16];
    int n = ExpressionGraph::neighbours((uint8_t)i, nb, 16);
    printf("  %-12s %-7s #%06X ->", d.name, Expressions::moodName(d.mood), (unsigned)d.ink);
    for (int k = 0; k < n; k++) printf(" %s", Expressions::name(nb[k]));
    printf("\n");
  }
  printf("Graph: %d nodes, %d edges\n", (int)EX_COUNT, ExpressionGraph::edgeCount());
  printf("Acts (%d):\n", Acts::count());
  for (int i = 0; i < Acts::count(); i++) {
    const ActDef &a = Acts::get(i);
    printf("  %-14s %4.1f s  on %s\n", a.name, a.duration, a.base < EX_COUNT ? Expressions::name(a.base) : "any face");
  }
  return 0;
}

static int cmdPath(const char *a, const char *b) {
  uint8_t from = exprOrDie(a), to = exprOrDie(b);
  uint8_t p[GRAPH_MAX_PATH];
  int n = ExpressionGraph::path(from, to, p, GRAPH_MAX_PATH);
  printf("%s", Expressions::name(from));
  uint8_t prev = from;
  int ms = 0;
  for (int i = 0; i < n; i++) {
    const GraphEdge *e = ExpressionGraph::find(prev, p[i]);
    ms += e ? e->ms : 0;
    printf(" -> %s", Expressions::name(p[i]));
    prev = p[i];
  }
  printf("   (%d hops, %d ms at full length)\n", n, ms);
  return 0;
}

// The engine standing on a face for `t` seconds, eyes to the front.
static void settle(FaceEngine &face, uint8_t expr, float t, int seed) {
  face.begin((uint32_t)seed, expr);
  face.setAutoGaze(false);
  face.setAutoBlink(false);
  uint32_t now = 0;
  for (int k = 0; k < (int)(t * 1000 / FRAME_MS) + 1; k++) {
    now += FRAME_MS;
    face.update(now);
  }
}

static int cmdFace(const char *expr, const char *out, float t) {
  FaceEngine face;
  Canvas c;
  settle(face, exprOrDie(expr), t, 5);
  face.render(c);
  Picture page(SCREEN_W + 16, SCREEN_H + 16);
  page.screen(c, 8, 8);
  return page.savePng(out) ? 0 : 1;
}

static int cmdSheet(const char *out, float t) {
  const int cols = 8, size = SCREEN_W / 2, cellW = size + 8, cellH = size + 16;
  int rows = (EX_COUNT + cols - 1) / cols;
  Picture page(cols * cellW + 8, rows * cellH + 8);
  Canvas c;
  for (int i = 0; i < EX_COUNT; i++) {
    FaceEngine face;
    settle(face, (uint8_t)i, t, 5 + i);
    face.render(c);
    int ox = 8 + (i % cols) * cellW, oy = 8 + (i / cols) * cellH;
    page.screen(c, ox, oy, 2);
    page.text(ox + 2, oy + size + 3, Expressions::name((uint8_t)i));
  }
  if (!page.savePng(out)) return 1;
  printf("%s: %dx%d\n", out, page.w, page.h);
  return 0;
}

// Moments of an act, evenly spaced, on one page: enough to judge it at a glance.
static int cmdStrip(const char *name, const char *out, int n, int shrink) {
  int idx = Acts::find(name);
  if (idx < 0) {
    fprintf(stderr, "unknown act '%s'\n", name);
    return 1;
  }
  const ActDef &a = Acts::get(idx);
  if (n < 2) n = 2;
  if (shrink < 1) shrink = 1;
  const int cols = n < 6 ? n : 6, size = SCREEN_W / shrink, cellW = size + 6, cellH = size + 14;
  int rows = (n + cols - 1) / cols;
  Picture page(cols * cellW + 6, rows * cellH + 6);
  FaceEngine face;
  Canvas c;
  face.begin(11, a.base < EX_COUNT ? a.base : (uint8_t)EX_NORMAL);
  face.setAutoBlink(false);
  face.setAutoGaze(false);
  uint32_t now = 1000;
  face.update(now);
  face.playAct(idx);
  int shot = 0;
  uint32_t start = now;
  while (shot < n) {
    now += FRAME_MS;
    face.update(now);
    face.render(c);  // every frame: acts may keep things on the canvas
    float t = (now - start) * 0.001f;
    float want = a.duration * (shot + 0.5f) / n;
    if (t < want) continue;
    int ox = 6 + (shot % cols) * cellW, oy = 6 + (shot / cols) * cellH;
    page.screen(c, ox, oy, shrink);
    char label[32];
    snprintf(label, sizeof(label), "%.2f s", t);
    page.text(ox + 2, oy + size + 3, label, 0x808AA0);
    shot++;
  }
  return page.savePng(out) ? 0 : 1;
}

static int cmdGif(const char *expr, const char *out, const char *from, float hold) {
  FaceEngine face;
  Canvas c;
  face.begin(1234, exprOrDie(from));
  Recorder rec;
  if (!rec.open(out)) {
    fprintf(stderr, "cannot write %s\n", out);
    return 1;
  }
  uint32_t now = 1000;
  record(face, c, rec, now, 600);
  face.goTo(exprOrDie(expr));
  record(face, c, rec, now, (uint32_t)(hold * 1000));
  printf("%s: %d frames\n", out, rec.gif.frames());
  return 0;
}

static int cmdTrip(const char *out, int n, char **names) {
  FaceEngine face;
  Canvas c;
  face.begin(99, exprOrDie(names[0]));
  Recorder rec;
  if (!rec.open(out)) return 1;
  uint32_t now = 1000;
  record(face, c, rec, now, 800);
  for (int i = 1; i < n; i++) {
    face.goTo(exprOrDie(names[i]));
    record(face, c, rec, now, 1800);
  }
  printf("%s: %d frames\n", out, rec.gif.frames());
  return 0;
}

static int cmdAct(const char *name, const char *out) {
  int idx = Acts::find(name);
  if (idx < 0) {
    fprintf(stderr, "unknown act '%s' (see: facetool list)\n", name);
    return 1;
  }
  const ActDef &a = Acts::get(idx);
  FaceEngine face;
  Canvas c;
  face.begin(11, a.base < EX_COUNT ? a.base : (uint8_t)EX_NORMAL);
  Recorder rec;
  if (!rec.open(out)) return 1;
  uint32_t now = 1000;
  record(face, c, rec, now, 400);
  face.playAct(idx);
  record(face, c, rec, now, 600);
  printf("%s: %d frames\n", out, rec.gif.frames());
  return 0;
}

static int cmdActs(const char *dir) {
  std::string d = dir;
  MAKE_DIR(dir);
  for (int i = 0; i < Acts::count(); i++) {
    std::string out = d + "/" + Acts::get(i).name + ".gif";
    if (cmdAct(Acts::get(i).name, out.c_str())) return 1;
  }
  return 0;
}

static int cmdAll(const char *dir) {
  std::string d = dir;
  MAKE_DIR(dir);
  for (int i = 0; i < EX_COUNT; i++) {
    std::string out = d + "/" + Expressions::name((uint8_t)i) + ".gif";
    const char *from = (i == EX_NORMAL) ? "Asleep" : "Normal";
    if (cmdGif(Expressions::name((uint8_t)i), out.c_str(), from, 3.0f)) return 1;
  }
  return cmdSheet((d + "/_sheet.png").c_str(), 1.0f);
}

// How heavy every act is to draw, measured on this PC. The numbers only
// compare acts with each other: the robot is some 100 times slower.
static int cmdCost() {
  Canvas c;
  c.begin();
  printf("%-14s %9s %9s %9s\n", "act", "avg us", "worst us", "tiles");
  for (int i = -1; i < Acts::count(); i++) {
    FaceEngine face;
    uint8_t base = EX_NORMAL;
    float dur = 3.0f;
    if (i >= 0) {
      const ActDef &a = Acts::get(i);
      if (a.base < EX_COUNT) base = a.base;
      dur = a.duration;
    }
    face.begin(11, base);
    uint32_t now = 1000;
    face.update(now);
    if (i >= 0) face.playAct(i);
    double total = 0, worst = 0;
    long tiles = 0;
    int frames = 0;
    while ((now - 1000) * 0.001f < dur) {
      now += FRAME_MS;
      face.update(now);
      auto t0 = std::chrono::steady_clock::now();
      face.render(c);
      auto t1 = std::chrono::steady_clock::now();
      double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
      total += us;
      if (us > worst) worst = us;
      for (int ty = 0; ty < TILES; ty++) {
        uint16_t m = c.changedTiles(ty);
        while (m) tiles += m & 1, m >>= 1;
      }
      frames++;
    }
    printf("%-14s %9.0f %9.0f %9ld\n", i < 0 ? "(plain face)" : Acts::get(i).name, total / frames, worst, tiles / frames);
  }
  return 0;
}

int main(int argc, char **argv) {
  SetTraceLogLevel(LOG_WARNING);
  const char *cmd = argc > 1 ? argv[1] : "";
  if (!strcmp(cmd, "list")) return cmdList();
  if (!strcmp(cmd, "cost")) return cmdCost();
  if (!strcmp(cmd, "path") && argc >= 4) return cmdPath(argv[2], argv[3]);
  if (!strcmp(cmd, "face") && argc >= 4) return cmdFace(argv[2], argv[3], argc > 4 ? (float)atof(argv[4]) : 1.0f);
  if (!strcmp(cmd, "gif") && argc >= 4)
    return cmdGif(argv[2], argv[3], argc > 4 ? argv[4] : "Normal", argc > 5 ? (float)atof(argv[5]) : 3.0f);
  if (!strcmp(cmd, "trip") && argc >= 5) return cmdTrip(argv[2], argc - 3, argv + 3);
  if (!strcmp(cmd, "act") && argc >= 4) return cmdAct(argv[2], argv[3]);
  if (!strcmp(cmd, "say") && argc >= 4) {
    Acts::setText(argv[2]);
    return cmdAct("Say", argv[3]);
  }
  if (!strcmp(cmd, "strip") && argc >= 4)
    return cmdStrip(argv[2], argv[3], argc > 4 ? atoi(argv[4]) : 12, argc > 5 ? atoi(argv[5]) : 2);
  if (!strcmp(cmd, "acts") && argc >= 3) return cmdActs(argv[2]);
  if (!strcmp(cmd, "sheet") && argc >= 3) return cmdSheet(argv[2], argc > 3 ? (float)atof(argv[3]) : 1.0f);
  if (!strcmp(cmd, "all") && argc >= 3) return cmdAll(argv[2]);
  fprintf(stderr,
          "usage:\n"
          "  facetool list\n"
          "  facetool path  <from> <to>\n"
          "  facetool face  <expr> <out.png> [t]\n"
          "  facetool sheet <out.png> [t]\n"
          "  facetool strip <act> <out.png> [n] [shrink]\n"
          "  facetool gif   <expr> <out.gif> [from] [hold_s]\n"
          "  facetool trip  <out.gif> <expr> <expr> ...\n"
          "  facetool act   <name> <out.gif>\n"
          "  facetool say   <text> <out.gif>\n"
          "  facetool all   <dir>\n"
          "  facetool acts  <dir>\n"
          "  facetool cost\n");
  return 1;
}
