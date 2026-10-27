#include "StateView.h"

#include <math.h>
#include <stdio.h>

#include "raymath.h"

#include "../Config.h"

namespace {

const Color BG = {16, 19, 24, 255};
const Color BG_LINE = {52, 58, 74, 255};
const Color TXT = {214, 222, 236, 255};
const Color DIM = {128, 138, 158, 255};
const Color EDGE = {110, 122, 140, 255};
const Color C_SLEEP = {167, 158, 255, 255};
const Color C_WAKE = {76, 208, 127, 255};
const Color C_REACT = {240, 169, 74, 255};
const Color C_SHOW = {111, 178, 255, 255};
const Color C_INFO = {79, 209, 203, 255};
const Color C_MANUAL = {167, 176, 188, 255};

const int ANY = -1, BOOT = -2;

struct Node {
  float cx, cy, w, h;
  const char *title, *sub;
  Color color;
};

// Indexed by Brain::Activity
const Node NODES[Brain::ACTIVITY_COUNT] = {
  {95, 190, 150, 56, "SLEEPING", "face Asleep, snores", C_SLEEP},
  {275, 70, 140, 44, "WAKING", "face Sleepy", C_SLEEP},
  {470, 190, 180, 76, "AWAKE", "rests on the face of its mood", C_WAKE},
  {690, 70, 150, 44, "REACTING", "one face for 2-5 s", C_REACT},
  {275, 312, 140, 44, "DOZING", "Sleepy, then Yawn", C_SLEEP},
  {775, 190, 150, 44, "SHOW", "an act", C_SHOW},
  {690, 312, 150, 44, "INFO", "clock + weather, 7 s", C_INFO},
  {470, 320, 120, 44, "MANUAL", "requested face", C_MANUAL},
};

enum Align { LEFT, CENTER, RIGHT };

struct Edge {
  int from, to;
  int n;          // 2 = line, 4 = bezier, -4 = path of 4 corners
  Vector2 p[4];
  const char *l1, *l2;
  Vector2 at;
  Align align;
  bool noise;
};

const Edge EDGES[] = {
  {Brain::SLEEPING, Brain::REACTING, -4, {{55, 162}, {55, 28}, {690, 28}, {690, 48}},
   "a noise wakes it up startled", nullptr, {372, 12}, CENTER, true},
  {Brain::SLEEPING, Brain::WAKING, 4, {{120, 162}, {120, 105}, {160, 70}, {205, 70}},
   "a touch on the screen", "or the light is back", {168, 104}, LEFT, false},
  {Brain::WAKING, Brain::AWAKE, 4, {{345, 70}, {400, 70}, {435, 105}, {435, 152}},
   "1.8 s", nullptr, {424, 86}, LEFT, false},
  {Brain::AWAKE, Brain::DOZING, 4, {{410, 228}, {410, 280}, {390, 312}, {345, 312}},
   "dark 8 s, night + alone 3 min,", "or alone for 45 min", {275, 340}, CENTER, false},
  {Brain::DOZING, Brain::SLEEPING, 4, {{205, 312}, {150, 312}, {120, 270}, {120, 218}},
   "falls asleep", nullptr, {108, 288}, RIGHT, false},
  {Brain::DOZING, Brain::AWAKE, 4, {{300, 290}, {300, 250}, {335, 212}, {380, 212}},
   "a touch or light", nullptr, {296, 238}, RIGHT, false},
  {Brain::AWAKE, Brain::REACTING, 4, {{525, 152}, {535, 100}, {575, 68}, {615, 64}},
   "noise, tap, poke, stroke,", "hot or cold, idle face", {568, 40}, RIGHT, false},
  {Brain::REACTING, Brain::AWAKE, 4, {{615, 82}, {585, 88}, {570, 125}, {560, 168}},
   "after 2-5 s", nullptr, {590, 112}, LEFT, false},
  {Brain::AWAKE, Brain::SHOW, 2, {{560, 182}, {700, 182}, {0, 0}, {0, 0}},
   "HOLA!, petting, idle act", nullptr, {630, 166}, CENTER, false},
  {Brain::SHOW, Brain::AWAKE, 2, {{700, 200}, {560, 200}, {0, 0}, {0, 0}},
   "it ends", nullptr, {630, 206}, CENTER, false},
  {ANY, Brain::INFO, 4, {{530, 228}, {540, 285}, {575, 316}, {615, 318}},
   "long press on the glass,", "alone 2+ min: every 2 min", {690, 340}, CENTER, false},
  {Brain::INFO, Brain::AWAKE, 4, {{615, 300}, {585, 296}, {560, 268}, {552, 230}},
   "7 s, a touch or a noise", nullptr, {592, 258}, LEFT, false},
  {ANY, Brain::MANUAL, 2, {{455, 228}, {455, 298}, {0, 0}, {0, 0}}, nullptr, nullptr, {0, 0}, LEFT, false},
  {Brain::MANUAL, Brain::AWAKE, 2, {{485, 298}, {485, 228}, {0, 0}, {0, 0}}, nullptr, nullptr, {0, 0}, LEFT, false},
  {BOOT, Brain::SHOW, 2, {{800, 126}, {800, 168}, {0, 0}, {0, 0}}, "power on", nullptr, {808, 136}, LEFT, false},
  {Brain::SHOW, Brain::REACTING, 2, {{740, 168}, {740, 92}, {0, 0}, {0, 0}}, "noise", nullptr, {734, 126}, RIGHT, true},
};
const int EDGE_COUNT = (int)(sizeof(EDGES) / sizeof(EDGES[0]));

int findEdge(int from, int to) {
  for (int i = 0; i < EDGE_COUNT; i++)
    if (EDGES[i].from == from && EDGES[i].to == to) return i;
  for (int i = 0; i < EDGE_COUNT; i++)
    if (EDGES[i].from == ANY && EDGES[i].to == to) return i;
  return -1;
}

void arrowHead(Vector2 tip, Vector2 dir, float size, Color c) {
  float len = Vector2Length(dir);
  if (len < 0.001f) return;
  dir = Vector2Scale(dir, 1.0f / len);
  Vector2 side = {-dir.y, dir.x};
  Vector2 base = Vector2Subtract(tip, Vector2Scale(dir, size));
  Vector2 a = Vector2Add(base, Vector2Scale(side, size * 0.55f));
  Vector2 b = Vector2Subtract(base, Vector2Scale(side, size * 0.55f));
  DrawTriangle(tip, b, a, c);
  DrawTriangle(tip, a, b, c);  // whatever the winding
}

void drawEdge(const Edge &e, Vector2 o, Color c, float thick) {
  Vector2 p[4];
  for (int i = 0; i < 4; i++) p[i] = Vector2Add(e.p[i], o);
  if (e.n == 2) {
    DrawLineEx(p[0], p[1], thick, c);
    arrowHead(p[1], Vector2Subtract(p[1], p[0]), 9.0f, c);
  } else if (e.n == 4) {
    DrawSplineSegmentBezierCubic(p[0], p[1], p[2], p[3], thick, c);
    arrowHead(p[3], Vector2Subtract(p[3], p[2]), 9.0f, c);
  } else {
    for (int i = 0; i < 3; i++) DrawLineEx(p[i], p[i + 1], thick, c);
    arrowHead(p[3], Vector2Subtract(p[3], p[2]), 9.0f, c);
  }
}

void text(const char *s, float x, float y, Align align, Color c, int size = 10, bool plate = true) {
  int w = MeasureText(s, size);
  int tx = (int)x;
  if (align == CENTER) tx -= w / 2;
  else if (align == RIGHT) tx -= w;
  if (plate) DrawRectangle(tx - 3, (int)y - 1, w + 6, size + 2, BG);
  DrawText(s, tx, (int)y, size, c);
}

const char *clock(uint32_t ms) {
  uint32_t s = ms / 1000;
  if (s >= 60) return TextFormat("%u:%02u", s / 60, s % 60);
  return TextFormat("%.1f s", ms / 1000.0f);
}

// Small labelled bar. `t` is how full it is, `on` tells if that clock is running.
void meter(Rectangle r, const char *name, const char *value, float t, bool on, Color c) {
  DrawText(name, (int)r.x, (int)r.y, 10, on ? TXT : DIM);
  int w = MeasureText(value, 10);
  DrawText(value, (int)(r.x + r.width) - w, (int)r.y, 10, on ? c : DIM);
  Rectangle bar = {r.x, r.y + 14, r.width, 5};
  DrawRectangleRounded(bar, 1.0f, 4, {34, 38, 50, 255});
  if (on && t > 0.005f) {
    bar.width *= Clamp(t, 0.0f, 1.0f);
    DrawRectangleRounded(bar, 1.0f, 4, c);
  }
}

}  // namespace

void StateView::draw(Rectangle area, const Brain &brain, const FaceEngine &face, const SensorState &sensors) {
  DrawRectangleRec(area, BG);
  DrawLine((int)area.x, (int)area.y, (int)(area.x + area.width), (int)area.y, BG_LINE);

  int now = brain.activity();
  double t = GetTime();
  if (first_ || brain.changes() != changes_) {
    // the first change of all is the power on
    from_ = brain.changes() <= 1 ? BOOT : (int)brain.previous();
    first_ = false;
    changes_ = brain.changes();
    flashEdge_ = findEdge(from_, now);
    flashAt_ = t;
  }
  float flash = (float)Clamp(1.0 - (t - flashAt_) / 2.5, 0.0, 1.0);

  // ---- header: what just happened
  const float x0 = area.x + 16;
  DrawText("STATE MACHINE", (int)x0, (int)area.y + 9, 10, DIM);
  float hx = x0 + 110;
  const Color nowColor = NODES[now].color;
  if (from_ >= 0 && from_ != now) {
    const char *a = Brain::activityName((uint8_t)from_);
    DrawText(a, (int)hx, (int)area.y + 9, 10, DIM);
    hx += MeasureText(a, 10) + 6;
    DrawText(">", (int)hx, (int)area.y + 9, 10, DIM);
    hx += 12;
  }
  const char *b = Brain::activityName((uint8_t)now);
  DrawText(b, (int)hx, (int)area.y + 9, 10, nowColor);
  hx += MeasureText(b, 10) + 14;
  DrawText(TextFormat("because: %s", brain.reason()), (int)hx, (int)area.y + 9, 10, TXT);
  const char *ago = TextFormat("%s ago", clock(brain.stateAgeMs()));
  DrawText(ago, (int)(area.x + area.width) - MeasureText(ago, 10) - 16, (int)area.y + 9, 10, DIM);

  // ---- diagram, centered in the panel
  Vector2 o = {area.x + (area.width - 880.0f) * 0.5f, area.y + 26.0f};
  if (o.x < area.x) o.x = area.x;

  for (int i = 0; i < EDGE_COUNT; i++) {
    const Edge &e = EDGES[i];
    Color c = e.noise ? Fade(C_REACT, 0.75f) : EDGE;
    drawEdge(e, o, c, 1.5f);
  }
  DrawCircleV(Vector2Add({800, 120}, o), 5.0f, TXT);
  if (flashEdge_ >= 0 && flash > 0.0f) {
    drawEdge(EDGES[flashEdge_], o, Fade(WHITE, flash), 2.0f + 2.0f * flash);
  }

  for (int i = 0; i < EDGE_COUNT; i++) {
    const Edge &e = EDGES[i];
    if (!e.l1) continue;
    bool hot = i == flashEdge_ && flash > 0.0f;
    Color c = hot ? WHITE : (e.noise ? C_REACT : TXT);
    text(e.l1, o.x + e.at.x, o.y + e.at.y, e.align, c);
    if (e.l2) text(e.l2, o.x + e.at.x, o.y + e.at.y + 12, e.align, c);
  }

  Brain::Timers tm = brain.timers();
  for (int i = 0; i < Brain::ACTIVITY_COUNT; i++) {
    const Node &n = NODES[i];
    Rectangle r = {o.x + n.cx - n.w / 2, o.y + n.cy - n.h / 2, n.w, n.h};
    bool active = i == now;
    float round = 0.3f;
    if (active) {
      float pulse = 0.5f + 0.5f * sinf((float)t * 4.0f);
      Rectangle glow = {r.x - 5, r.y - 5, r.width + 10, r.height + 10};
      DrawRectangleRounded(glow, round, 8, Fade(n.color, 0.10f + 0.10f * pulse + 0.25f * flash));
    }
    DrawRectangleRounded(r, round, 8, active ? ColorAlphaBlend(BG, Fade(n.color, 0.34f), WHITE) : ColorAlphaBlend(BG, Fade(n.color, 0.10f), WHITE));
    DrawRectangleRoundedLinesEx(r, round, 8, active ? 2.5f : 1.2f, active ? n.color : Fade(n.color, 0.55f));

    bool tall = n.h > 60;
    float ty = r.y + (tall ? 14 : 9);
    text(n.title, r.x + r.width / 2, ty, CENTER, active ? WHITE : n.color, 10, false);
    const char *sub = n.sub;
    if (active) {
      // what it is doing right now
      if (i == Brain::SHOW) {
        if (face.actActive()) sub = TextFormat("act %s  %.1f / %.1f s", face.act()->name, face.actTime(), face.act()->duration);
        else sub = TextFormat("going to %s", Expressions::name(face.target()));
      } else if (i == Brain::INFO) {
        sub = sensors.battery.valid ? TextFormat("battery %d %%  %.2f V", sensors.battery.percent, sensors.battery.millivolts / 1000.0f) : "clock";
      } else if (i != Brain::AWAKE) {
        sub = TextFormat("face %s", Expressions::name(face.heading()));
      }
    }
    text(sub, r.x + r.width / 2, ty + 14, CENTER, active ? TXT : DIM, 10, false);
    if (i == Brain::AWAKE) {
      const char *m = active ? TextFormat("mood %s, face %s", Expressions::moodName(brain.mood()),
                                          Expressions::name(face.heading()))
                             : TextFormat("mood %s", Expressions::moodName(brain.mood()));
      text(m, r.x + r.width / 2, ty + 28, CENTER, active ? WHITE : DIM, 10, false);
    }

    // time left in a timed state
    float left = -1.0f;
    if (active && tm.stateTotal > 0) left = (float)tm.stateLeft / (float)tm.stateTotal;
    if (active && i == Brain::SHOW && face.actActive()) left = 1.0f - face.actTime() / face.act()->duration;
    if (left >= 0.0f) {
      Rectangle bar = {r.x + 10, r.y + r.height - 8, r.width - 20, 3};
      DrawRectangleRec(bar, Fade(BLACK, 0.45f));
      bar.width *= Clamp(1.0f - left, 0.0f, 1.0f);
      DrawRectangleRec(bar, WHITE);
    }
  }

  // ---- clocks
  float y = area.y + 26 + 366 + 4;
  const float gap = 22.0f;
  float w4 = (area.width - 32.0f - gap * 3.0f) / 4.0f;
  bool awake = now == Brain::AWAKE;
  bool asleep = now == Brain::SLEEPING;

  // how long it has been alone, on a scale that gives room to the first minutes
  {
    Rectangle r = {x0, y, area.width - 32.0f - w4 - gap, 24};
    uint32_t alone = brain.aloneMs();
    DrawText("alone for", (int)r.x, (int)r.y, 10, TXT);
    DrawText(clock(alone), (int)r.x + 62, (int)r.y, 10, C_WAKE);
    Rectangle bar = {r.x, r.y + 14, r.width, 5};
    DrawRectangleRounded(bar, 1.0f, 4, {34, 38, 50, 255});
    auto place = [&](float minutes) {
      float f;
      if (minutes < 2.0f) f = 0.2f * minutes / 2.0f;
      else if (minutes < 10.0f) f = 0.2f + 0.3f * (minutes - 2.0f) / 8.0f;
      else f = 0.5f + 0.5f * (minutes - 10.0f) / 35.0f;
      return Clamp(f, 0.0f, 1.0f);
    };
    float f = place(alone / 60000.0f);
    Color c = alone > T_LONELY_MS ? C_SHOW : (alone > T_BORED_MS ? C_MANUAL : C_WAKE);
    if (f > 0.003f) DrawRectangleRounded({bar.x, bar.y, bar.width * f, bar.height}, 1.0f, 4, c);
    static const float MARK[3] = {2.0f, 10.0f, 45.0f};
    static const char *NAME[3] = {"2:00 bored", "10:00 sad", "45:00 sleeps"};
    for (int i = 0; i < 3; i++) {
      float mx = bar.x + bar.width * place(MARK[i]);
      DrawLine((int)mx, (int)bar.y - 3, (int)mx, (int)bar.y + 8, TXT);
      int tw = MeasureText(NAME[i], 10);
      float tx = i == 2 ? mx - tw : mx - tw / 2;
      DrawText(NAME[i], (int)tx, (int)bar.y + 10, 10, alone > MARK[i] * 60000.0f ? TXT : DIM);
    }
  }
  {
    Rectangle r = {x0 + (w4 + gap) * 3.0f, y, w4, 24};
    bool dark = sensors.dark;
    meter(r, "dark and still", dark ? TextFormat("%s of 8 s", clock(tm.darkFor)) : "no light sensor",
          (float)tm.darkFor / (float)T_DARK_TO_SLEEP_MS, dark && !asleep, C_SLEEP);
  }

  y += 40;
  meter({x0, y, w4, 24}, "does something on its own", awake ? clock(tm.idleIn) : "-",
        1.0f - (float)tm.idleIn / (float)IDLE_MAX_MS, awake, C_WAKE);
  meter({x0 + (w4 + gap), y, w4, 24}, "hot / cold check", awake ? clock(tm.climateIn) : "-",
        1.0f - (float)tm.climateIn / 45000.0f, awake, C_REACT);
  bool lonely = brain.aloneMs() > T_BORED_MS && now != Brain::SLEEPING && now != Brain::DOZING;
  meter({x0 + (w4 + gap) * 2.0f, y, w4, 24}, "clock screen", lonely ? clock(tm.infoIn) : "not alone yet",
        1.0f - (float)tm.infoIn / 120000.0f, lonely, C_INFO);
  meter({x0 + (w4 + gap) * 3.0f, y, w4, 24}, "next snore", asleep ? clock(tm.snoreIn) : "not asleep",
        1.0f - (float)tm.snoreIn / 70000.0f, asleep, C_SLEEP);
}
