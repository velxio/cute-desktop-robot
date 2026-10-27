// Windows emulator of the robot.
//
// The sketch (../vocat-eyes.ino) runs unmodified on top of mocked Arduino
// libraries. This window shows what the hardware would do: the round 360 px
// screen of the ESP-VoCat, and the panel lets you play with the sensors.
// Click on the round screen to touch it: a click is a tap, a drag is a stroke,
// keeping it pressed is a long press. Click on the ring around the screen to
// talk to the robot from that side (the two microphones tell where a voice
// comes from), press S for a bang, W for the wake word.
//
//   robot_emu.exe                       interactive
//   robot_emu.exe --script "<steps>"    automated run, e.g. for screenshots:
//        "2:expr=Happy; 4:shot=C:/tmp/happy.png; 5:tap=90,160; 8:quit"
//     steps are time:command[=value]; commands: expr act say tap=x,y stroke
//     hold noise voice=deg wake hour battery charging auto states info
//     shot record quit
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <string>
#include <vector>

#include "raylib.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#include "raymath.h"
#include "rlgl.h"

#include "EmuHw.h"
#include "GifWriter.h"
#include "StateView.h"
#include "View3D.h"

#include "../Config.h"
#include "../src/brain/Brain.h"
#include "../src/brain/Sensors.h"
#include "../src/face/FaceEngine.h"

// ---- the sketch ------------------------------------------------------------------
extern void setup();
extern void loop();
extern FaceEngine face;
extern Brain brain;
extern Sensors sensors;

// ---- layout ------------------------------------------------------------------------
static const int WIN_W = 1380, WIN_H = 900;
static const int PANEL_W = 520;
static const int LCD_W = EMU_LCD_W;

static const Color BG_TOP = {30, 34, 44, 255};
static const Color BG_BOT = {12, 13, 18, 255};
static const Color PANEL = {20, 22, 29, 255};
static const Color PANEL_2 = {30, 33, 43, 255};
static const Color LINE = {52, 58, 74, 255};
static const Color TXT = {214, 222, 236, 255};
static const Color DIM = {128, 138, 158, 255};
static const Color ACC = {96, 200, 255, 255};
static const Color OK_C = {96, 226, 140, 255};
static const Color WARN = {255, 190, 92, 255};
static const Color HOT = {255, 110, 110, 255};
static const Color VOICE = {178, 140, 255, 255};

static const Color MOOD_COLOR[MOOD_COUNT] = {
  {150, 200, 255, 255}, {255, 214, 102, 255}, {122, 150, 255, 255},
  {255, 104, 104, 255}, {255, 160, 84, 255},  {178, 140, 255, 255}};

// ---- state of the emulator -----------------------------------------------------------
struct ScriptTouch {
  int kind = 0;  // 0 none, 1 tap, 2 stroke, 3 hold
  double start = 0.0;
  float x = 180, y = 180;
};

struct Sim {
  double time = 0.0;  // simulated seconds
  float speed = 1.0f;
  bool paused = false;
  float hour = 10.0f;
  float voiceDir = 0.0f;      // where the pretend speaker stands, degrees, + = right of the robot
  double voiceUntil = 0.0;    // the speaker is talking until then (simulated seconds)
  double noiseUntil = 0.0;
  double wakeUntil = 0.0;
  float batteryPct = 80.0f;
  GifWriter gif;
  double gifNext = 0.0;
  std::string gifPath;
  bool stateView = true;
  int showSel = 0;
  int hoverNode = -1;
  bool mouseTouch = false;
  bool view3d = false;   // the 3D enclosure instead of the flat glass
  bool orbiting = false;
  ScriptTouch script;
  std::string toast;
  double toastUntil = 0.0;
};

static Sim sim;
static StateView stateView;
static Texture2D lcdTex;
static Color lcdPixels[LCD_W * LCD_W];
static uint32_t spiLast = 0, spiPerSec = 0, framesLast = 0, fps = 0;
static double spiStamp = 0.0;
static Rectangle lcdRect;  // where the glass is drawn
static float lcdScale = 1.0f;

static void toast(const std::string &s) {
  sim.toast = s;
  sim.toastUntil = GetTime() + 2.5;
}

static double timeOfDay() {
  double s = fmod(emuHw.rtcBase + emuHw.clockMs / 1000.0, 86400.0);
  return s < 0 ? s + 86400.0 : s;
}

static void setHour(float h) {
  sim.hour = h;
  emuHw.rtcBase = h * 3600.0 - emuHw.clockMs / 1000.0;
}

// ---- pretend sounds ------------------------------------------------------------------------
// A voice burst: the audio module would report it every few hundred ms while somebody talks.
static void talk(float dirDeg, double seconds = 1.6) {
  sim.voiceDir = dirDeg;
  sim.voiceUntil = sim.time + seconds;
}
static void bang() {
  sim.noiseUntil = sim.time + 0.15;
  sensors.noteNoise(sim.voiceDir, 1.0f);
}
static void wakeWord() {
  sim.wakeUntil = sim.time + 0.6;
  sensors.noteWakeWord(sim.voiceDir);
}

// ---- hardware <-> simulation -----------------------------------------------------------
static void applyInputs() {
  // touch from a script
  if (sim.script.kind && !sim.mouseTouch) {
    double u = sim.time - sim.script.start;
    double len = sim.script.kind == 1 ? 0.12 : (sim.script.kind == 2 ? 0.7 : 1.4);
    if (u >= len) {
      sim.script.kind = 0;
      emuHw.touchDown = false;
    } else {
      emuHw.touchDown = true;
      float x = sim.script.x, y = sim.script.y;
      if (sim.script.kind == 2) x = 90.0f + 180.0f * (float)(u / len), y = 225.0f + 18.0f * sinf((float)u * 9.0f);
      emuHw.touchX = (int)x;
      emuHw.touchY = (int)y;
    }
  }
  emuHw.digitalIn[PIN_TOUCH_INT] = emuHw.touchDown ? LOW : HIGH;

  // the voice keeps arriving in bursts while the pretend speaker talks
  static double nextBurst = 0.0;
  if (sim.time < sim.voiceUntil && sim.time >= nextBurst) {
    nextBurst = sim.time + 0.25;
    sensors.noteVoice(sim.voiceDir, 0.6f + 0.3f * (float)sin(sim.time * 7.0));
  }

  // battery
  emuHw.batteryPct = (int)(sim.batteryPct + 0.5f);
  emuHw.batteryMv = 3300 + (int)(sim.batteryPct * 9.0f);
}

static void lcdToRgb(uint32_t *out, int shrink) {
  int size = LCD_W / shrink;
  float half = LCD_W * 0.5f;
  for (int y = 0; y < size; y++)
    for (int x = 0; x < size; x++) {
      int px = x * shrink, py = y * shrink;
      float dx = px + 0.5f - half, dy = py + 0.5f - half;
      uint16_t c = emuHw.lcd[py * LCD_W + px];
      uint32_t rgb = ((uint32_t)((c >> 11) & 31) * 255 / 31) << 16 | ((uint32_t)((c >> 5) & 63) * 255 / 63) << 8 |
                     ((uint32_t)(c & 31) * 255 / 31);
      if (dx * dx + dy * dy > half * half) rgb = 0x101216;  // outside the glass
      out[y * size + x] = rgb;
    }
}

static void stepSim(double seconds) {
  const double step = 0.004;
  double end = sim.time + seconds;
  while (sim.time < end) {
    sim.time += step;
    emuHw.clockMs = sim.time * 1000.0;
    applyInputs();
    loop();
    if (sim.gif.isOpen() && sim.time >= sim.gifNext) {
      static uint32_t rgb[LCD_W * LCD_W / 4];
      lcdToRgb(rgb, 2);
      sim.gif.frame(rgb, 40);
      sim.gifNext += 0.040;
    }
  }
}

static void updateLcdTexture() {
  for (int i = 0; i < LCD_W * LCD_W; i++) {
    uint16_t c = emuHw.lcd[i];
    lcdPixels[i] = {(unsigned char)(((c >> 11) & 31) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63),
                    (unsigned char)((c & 31) * 255 / 31), 255};
  }
  UpdateTexture(lcdTex, lcdPixels);
}

static void toggleRecording() {
  if (sim.gif.isOpen()) {
    int n = sim.gif.frames();
    sim.gif.close();
    toast(TextFormat("saved %s (%d frames)", sim.gifPath.c_str(), n));
    return;
  }
  MakeDirectory("captures");
  time_t t = time(nullptr);
  char name[96];
  strftime(name, sizeof(name), "captures/lcd_%Y%m%d_%H%M%S.gif", localtime(&t));
  sim.gifPath = name;
  if (sim.gif.open(name, LCD_W / 2, LCD_W / 2)) {
    sim.gifNext = sim.time;
    toast("recording the screen... press R to stop");
  }
}

// ---- tiny immediate mode widgets ----------------------------------------------------------
static void label(const char *s, int x, int y, Color c = TXT, int size = 10) { DrawText(s, x, y, size, c); }
static bool hit(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

static bool button(Rectangle r, const char *text, bool active = false, Color accent = ACC) {
  bool over = hit(r);
  Color bg = active ? Fade(accent, 0.30f) : (over ? Color{44, 49, 64, 255} : PANEL_2);
  DrawRectangleRounded(r, 0.25f, 6, bg);
  DrawRectangleRoundedLines(r, 0.25f, 6, active ? accent : LINE);
  int w = MeasureText(text, 10);
  DrawText(text, (int)(r.x + (r.width - w) / 2), (int)(r.y + (r.height - 10) / 2), 10, active ? WHITE : TXT);
  return over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int dragId = 0;
static bool slider(int id, Rectangle r, float *v, float lo, float hi, Color accent = ACC) {
  bool over = hit(r);
  if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) dragId = id;
  if (dragId == id && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) dragId = 0;
  bool changed = false;
  if (dragId == id) {
    float t = Clamp((GetMousePosition().x - r.x) / r.width, 0.0f, 1.0f);
    *v = lo + (hi - lo) * t;
    changed = true;
  }
  float t = Clamp((*v - lo) / (hi - lo), 0.0f, 1.0f);
  float cy = r.y + r.height / 2;
  DrawRectangleRounded({r.x, cy - 3, r.width, 6}, 1.0f, 4, PANEL_2);
  DrawRectangleRounded({r.x, cy - 3, r.width * t, 6}, 1.0f, 4, Fade(accent, 0.8f));
  DrawCircle((int)(r.x + r.width * t), (int)cy, (over || dragId == id) ? 7.0f : 6.0f, accent);
  return changed;
}

static void bar(Rectangle r, float t, Color c) {
  DrawRectangleRounded(r, 1.0f, 4, PANEL_2);
  if (t > 0.01f) DrawRectangleRounded({r.x, r.y, r.width * Clamp(t, 0.0f, 1.0f), r.height}, 1.0f, 4, c);
}

static void led(int x, int y, bool on, Color c) {
  DrawCircle(x, y, 6, on ? c : Color{46, 50, 62, 255});
  if (on) DrawCircle(x, y, 9, Fade(c, 0.25f));
}

static void section(const char *title, int x, int y, int w) {
  DrawText(title, x, y, 10, DIM);
  int tw = MeasureText(title, 10);
  DrawLine(x + tw + 8, y + 5, x + w, y + 5, LINE);
}

// ---- graph map --------------------------------------------------------------------------------
static Vector2 nodePos[EX_COUNT];

static void layoutGraph(Rectangle r) {
  static const Vector2 CENTER[MOOD_COUNT] = {{0.50f, 0.40f}, {0.13f, 0.32f}, {0.87f, 0.30f},
                                             {0.85f, 0.80f}, {0.15f, 0.82f}, {0.50f, 0.92f}};
  static const Vector2 RAD[MOOD_COUNT] = {{0.21f, 0.34f}, {0.115f, 0.27f}, {0.105f, 0.24f},
                                          {0.09f, 0.17f}, {0.10f, 0.15f},  {0.10f, 0.0f}};
  static const float START[MOOD_COUNT] = {-90, -70, -110, -60, -120, 0};
  for (int m = 0; m < MOOD_COUNT; m++) {
    uint8_t hub = ExpressionGraph::hub((uint8_t)m);
    std::vector<int> members;
    for (int i = 0; i < EX_COUNT; i++)
      if (Expressions::get((uint8_t)i).mood == m && i != hub) members.push_back(i);
    nodePos[hub] = {r.x + CENTER[m].x * r.width, r.y + CENTER[m].y * r.height};
    int n = (int)members.size();
    for (int k = 0; k < n; k++) {
      float x, y;
      if (m == MOOD_SLEEPY) {
        int step = k / 2 + 1;
        x = CENTER[m].x + ((k & 1) ? RAD[m].x : -RAD[m].x) * step;
        y = CENTER[m].y;
      } else {
        float ring = (n > 9 && (k & 1)) ? 0.6f : 1.0f;
        float a = (START[m] + 360.0f * k / n) * DEG2RAD;
        x = CENTER[m].x + cosf(a) * RAD[m].x * ring;
        y = CENTER[m].y + sinf(a) * RAD[m].y * ring;
      }
      nodePos[members[k]] = {r.x + x * r.width, r.y + y * r.height};
    }
  }
}

static bool onRoute(uint8_t a, uint8_t b) {
  if (!face.busy()) return false;
  uint8_t prev = face.current();
  if (face.moving()) {
    uint8_t h = face.heading();
    if ((prev == a && h == b) || (prev == b && h == a)) return true;
    prev = h;
  }
  for (int i = face.pathPosition(); i < face.pathLength(); i++) {
    uint8_t n = face.pathNode(i);
    if ((prev == a && n == b) || (prev == b && n == a)) return true;
    prev = n;
  }
  return false;
}

static void drawGraph(Rectangle r) {
  DrawRectangleRounded(r, 0.03f, 6, {14, 15, 20, 255});
  layoutGraph({r.x + 14, r.y + 10, r.width - 28, r.height - 22});
  for (int i = 0; i < ExpressionGraph::edgeCount(); i++) {
    const GraphEdge &e = ExpressionGraph::edge(i);
    bool hubs = Expressions::get(e.a).mood != Expressions::get(e.b).mood;
    DrawLineEx(nodePos[e.a], nodePos[e.b], 1.0f, hubs ? Color{60, 66, 84, 255} : Color{44, 49, 64, 255});
  }
  for (int i = 0; i < ExpressionGraph::edgeCount(); i++) {
    const GraphEdge &e = ExpressionGraph::edge(i);
    if (onRoute(e.a, e.b)) DrawLineEx(nodePos[e.a], nodePos[e.b], 2.5f, ACC);
  }
  if (face.moving()) {
    Vector2 a = nodePos[face.current()], b = nodePos[face.heading()];
    float t = Clamp(face.progress(), 0.0f, 1.0f);
    DrawCircleV({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}, 4.0f, WHITE);
  }
  sim.hoverNode = -1;
  Vector2 mp = GetMousePosition();
  for (int i = 0; i < EX_COUNT; i++)
    if (Vector2Distance(mp, nodePos[i]) < 8.0f) sim.hoverNode = i;
  for (int i = 0; i < EX_COUNT; i++) {
    const ExpressionDef &d = Expressions::get((uint8_t)i);
    bool hub = ExpressionGraph::hub(d.mood) == i;
    Color c = {(unsigned char)(d.ink >> 16), (unsigned char)(d.ink >> 8), (unsigned char)d.ink, 255};
    bool cur = face.heading() == i;
    bool tgt = face.busy() && face.target() == i;
    float rad = hub ? 6.0f : 4.0f;
    if (cur) DrawCircleV(nodePos[i], rad + 6.0f, Fade(c, 0.3f));
    DrawCircleV(nodePos[i], rad, cur ? WHITE : Fade(c, hub ? 1.0f : 0.75f));
    if (tgt || sim.hoverNode == i) DrawRing(nodePos[i], rad + 3.0f, rad + 4.5f, 0, 360, 24, tgt ? ACC : WHITE);
    if (hub || cur || tgt || sim.hoverNode == i) {
      int w = MeasureText(d.name, 10);
      int tx = (int)nodePos[i].x - w / 2, ty = (int)nodePos[i].y + (int)rad + 4;
      if (tx < r.x + 2) tx = (int)r.x + 2;
      if (tx + w > r.x + r.width - 2) tx = (int)(r.x + r.width) - w - 2;
      DrawRectangle(tx - 2, ty - 1, w + 4, 12, {14, 15, 20, 220});
      DrawText(d.name, tx, ty, 10, (cur || tgt) ? WHITE : (hub ? c : TXT));
    }
  }
  if (sim.hoverNode >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hit(r)) brain.force((uint8_t)sim.hoverNode, 12000);
}

// ---- the robot: its glass and the ring of directions ---------------------------------------------
// The glass is drawn as big as the view allows; the ring around it stands for the room:
// click on it and somebody talks to the robot from that side (0 = in front, right = right).
static float ringR() { return lcdRect.width * 0.5f + 42.0f; }

static Vector2 ringPoint(float dirDeg, float r) {
  // the robot looks at the viewer: its right is the viewer's left; "in front" is at the bottom
  float a = (90.0f + dirDeg) * DEG2RAD;
  return {lcdRect.x + lcdRect.width * 0.5f + cosf(a) * r, lcdRect.y + lcdRect.height * 0.5f + sinf(a) * r};
}

static void drawLabels(int viewW, int viewH);

static void drawRobot(int viewW, int viewH) {
  if (sim.view3d && View3D::available()) {
    float light = 0.15f + 0.85f * emuHw.backlight / 255.0f;
    View3D::draw({0, 0, (float)viewW, (float)viewH}, lcdTex, light, sim.voiceDir, sim.time < sim.voiceUntil);
    drawLabels(viewW, viewH);
    label("drag the screen = touch   drag around = turn   wheel = zoom   M = flat view", 18, viewH - 36, DIM);
    if (button({(float)viewW - 128, 12, 112, 24}, "flat view [M]")) sim.view3d = false;
    return;
  }
  float size = (float)fminf(viewW - 150.0f, viewH - 120.0f);
  if (size < 200.0f) size = 200.0f;
  lcdScale = size / LCD_W;
  lcdRect = {(viewW - size) * 0.5f, (viewH - size) * 0.5f + 8.0f, size, size};
  Vector2 c = {lcdRect.x + size * 0.5f, lcdRect.y + size * 0.5f};

  // the ring of directions
  float rr = ringR();
  DrawRing(c, rr - 14.0f, rr + 14.0f, 0, 360, 96, Fade(PANEL_2, 0.8f));
  DrawRing(c, rr - 14.0f, rr - 13.0f, 0, 360, 96, LINE);
  DrawRing(c, rr + 13.0f, rr + 14.0f, 0, 360, 96, LINE);
  for (int d = -180; d < 180; d += 30) {
    Vector2 p0 = ringPoint((float)d, rr - 10.0f), p1 = ringPoint((float)d, rr + (d % 90 == 0 ? 10.0f : 4.0f));
    DrawLineEx(p0, p1, 1.0f, d % 90 == 0 ? DIM : LINE);
  }
  {
    Vector2 p = ringPoint(0.0f, rr + 26.0f);
    label("front", (int)p.x - 12, (int)p.y - 5, DIM);
    p = ringPoint(180.0f, rr + 26.0f);
    label("behind", (int)p.x - 16, (int)p.y - 5, DIM);
    p = ringPoint(90.0f, rr + 26.0f);
    label("robot's right", (int)p.x - 60, (int)p.y - 5, DIM);
    p = ringPoint(-90.0f, rr + 26.0f);
    label("robot's left", (int)p.x + 6, (int)p.y - 5, DIM);
  }
  // the pretend speaker
  {
    bool talking = sim.time < sim.voiceUntil;
    Vector2 p = ringPoint(sim.voiceDir, rr);
    DrawCircleV(p, talking ? 13.0f : 9.0f, talking ? VOICE : Fade(VOICE, 0.5f));
    if (talking) {
      float k = (float)fmod(sim.time * 1.6, 1.0);
      DrawRing(p, 14.0f + 16.0f * k, 16.0f + 16.0f * k, 0, 360, 32, Fade(VOICE, 1.0f - k));
    }
    if (sim.time < sim.wakeUntil) label("\"Hi ESP\"", (int)p.x - 20, (int)p.y - 30, WHITE);
    label(TextFormat("%+.0f deg", sim.voiceDir), (int)p.x - 18, (int)p.y + 16, talking ? WHITE : DIM);
  }
  if (sim.time < sim.noiseUntil) {
    Vector2 p = ringPoint(sim.voiceDir, rr);
    DrawRing(p, 18.0f, 30.0f, 0, 360, 12, WARN);
  }

  // the glass, lit by the backlight
  DrawCircleV(c, size * 0.5f + 10.0f, {8, 9, 12, 255});
  float light = 0.15f + 0.85f * emuHw.backlight / 255.0f;
  unsigned char k = (unsigned char)(255 * light);
  rlSetTexture(lcdTex.id);
  rlBegin(RL_QUADS);
  rlColor4ub(k, k, k, 255);
  const int seg = 96;
  for (int i = 0; i < seg; i++) {
    float a0 = i * 2 * PI / seg, a1 = (i + 1) * 2 * PI / seg;
    rlTexCoord2f(0.5f, 0.5f);
    rlVertex2f(c.x, c.y);
    rlTexCoord2f(0.5f, 0.5f);
    rlVertex2f(c.x, c.y);
    rlTexCoord2f(0.5f + 0.5f * cosf(a1), 0.5f + 0.5f * sinf(a1));
    rlVertex2f(c.x + cosf(a1) * size * 0.5f, c.y + sinf(a1) * size * 0.5f);
    rlTexCoord2f(0.5f + 0.5f * cosf(a0), 0.5f + 0.5f * sinf(a0));
    rlVertex2f(c.x + cosf(a0) * size * 0.5f, c.y + sinf(a0) * size * 0.5f);
  }
  rlEnd();
  rlSetTexture(0);
  if (emuHw.touchDown) {
    Vector2 p = {lcdRect.x + emuHw.touchX * lcdScale, lcdRect.y + emuHw.touchY * lcdScale};
    DrawCircleV(p, 16, Fade(WHITE, 0.18f));
    DrawCircleLinesV(p, 16, Fade(WHITE, 0.6f));
  }
  if (sim.gif.isOpen()) {
    DrawCircle((int)lcdRect.x + 8, (int)lcdRect.y + 8, 5, fmod(GetTime(), 1.0) < 0.5 ? HOT : Fade(HOT, 0.3f));
    label(TextFormat("REC %d", sim.gif.frames()), (int)lcdRect.x + 18, (int)lcdRect.y + 3, HOT);
  }

  drawLabels(viewW, viewH);
  label("click glass = tap   drag = pet   hold = clock   click ring = talk from there", 18, viewH - 36, DIM);
  if (View3D::available() && button({(float)viewW - 128, 12, 112, 24}, "3D view [M]")) sim.view3d = true;
}

// The face and mood at the top, the panel bus at the bottom: both views share them.
static void drawLabels(int viewW, int viewH) {
  (void)viewW;
  Rgb ink = face.ink();
  Color inkC = {(unsigned char)(ink >> 16), (unsigned char)(ink >> 8), (unsigned char)ink, 255};
  label(TextFormat("%s   %s", Expressions::name(face.heading()), Expressions::moodName(brain.mood())), 18, 14, inkC, 20);
  float ms = fps ? spiPerSec / (float)fps * 8.0f / (LCD_QSPI_HZ / 1000.0f) / 4.0f : 0.0f;
  label(TextFormat("ST77916 360x360 QSPI   %u KB/s   ~%.1f ms per frame at %d MHz x4   backlight %d%%", spiPerSec / 1024,
                   ms, LCD_QSPI_HZ / 1000000, emuHw.backlight * 100 / 255),
        18, viewH - 22, DIM);
}

// ---- panel ----------------------------------------------------------------------------------
static void drawPanel(int x0, int winH) {
  DrawRectangle(x0, 0, PANEL_W, winH, PANEL);
  DrawLine(x0, 0, x0, winH, LINE);
  const int x = x0 + 16, w = PANEL_W - 32;
  int y = 12;

  DrawText("VOCAT EYES", x, y, 20, TXT);
  DrawText("ESP-VoCat v1.2 (ESP32-S3) emulator", x + 140, y + 8, 10, DIM);
  const char *clock = TextFormat("%s  t=%.1fs  x%.0f", sim.paused ? "PAUSED" : "running", sim.time, sim.speed);
  label(clock, x + w - MeasureText(clock, 10), y + 8, sim.paused ? WARN : DIM);
  y += 34;

  // ---- state
  section("STATE", x, y, w);
  y += 14;
  uint8_t mood = brain.mood();
  label("mood", x, y, DIM);
  label(Expressions::moodName(mood), x + 40, y, MOOD_COLOR[mood]);
  label("doing", x + 130, y, DIM);
  label(brain.autonomous() ? brain.activityName() : "manual (auto off)", x + 170, y, brain.autonomous() ? TXT : WARN);
  label("alone", x + 300, y, DIM);
  label(TextFormat("%us", brain.aloneMs() / 1000), x + 340, y, TXT);
  if (brain.night()) label("NIGHT", x + 420, y, VOICE);
  y += 14;
  label("face", x, y, DIM);
  if (face.actActive()) {
    label(TextFormat("act %s  %.1f / %.1f s", face.act()->name, face.actTime(), face.act()->duration), x + 40, y, ACC);
  } else {
    std::string route = Expressions::name(face.current());
    if (face.busy()) {
      if (face.moving()) route += std::string(" > ") + Expressions::name(face.heading());
      for (int i = face.pathPosition(); i < face.pathLength(); i++) route += std::string(" > ") + Expressions::name(face.pathNode(i));
    }
    label(route.c_str(), x + 40, y, face.busy() ? ACC : TXT);
  }
  y += 14;
  label("why", x, y, DIM);
  label(brain.reason(), x + 40, y, DIM);
  y += 14;
  label("social", x, y, DIM);
  bar({(float)x + 40, (float)y + 2, 160, 6}, brain.social(), MOOD_COLOR[MOOD_HAPPY]);
  label("annoy", x + 230, y, DIM);
  bar({(float)x + 270, (float)y + 2, 160, 6}, brain.annoy() / 1.2f, MOOD_COLOR[MOOD_ANGRY]);
  y += 16;

  // ---- graph
  drawGraph({(float)x, (float)y, (float)w, 200});
  y += 206;

  // ---- acts
  {
    int total = Acts::count();
    label("act", x, y + 7, DIM);
    if (button({(float)x + 30, (float)y, 24, 22}, "<")) sim.showSel = (sim.showSel + total - 1) % total;
    const ActDef &a = Acts::get(sim.showSel);
    label(TextFormat("%d/%d  %s", sim.showSel + 1, total, a.name), x + 62, y + 1, TXT);
    label(TextFormat("%.1f s, starts on %s", a.duration, a.base < EX_COUNT ? Expressions::name(a.base) : "any face"),
          x + 62, y + 12, DIM);
    if (button({(float)x + w - 100, (float)y, 24, 22}, ">")) sim.showSel = (sim.showSel + 1) % total;
    if (button({(float)x + w - 70, (float)y, 70, 22}, "PLAY [0]")) brain.forceAct(sim.showSel);
    y += 30;
  }

  // ---- sensors
  section("SENSORS", x, y, w);
  y += 14;
  const SensorState &s = sensors.state();
  const int row = 24;

  led(x + 8, y + 10, sim.time < sim.voiceUntil, VOICE);
  label("Microphones", x + 22, y + 5, TXT);
  label("ES7210 x2", x + 90, y + 5, DIM);
  if (button({(float)x + 160, (float)y, 70, 20}, "talk [V]", false, VOICE)) talk(sim.voiceDir);
  if (button({(float)x + 236, (float)y, 70, 20}, "bang [S]", false, WARN)) bang();
  if (button({(float)x + 312, (float)y, 76, 20}, "Hi ESP [W]", false, OK_C)) wakeWord();
  label(s.soundDirValid ? TextFormat("%+.0f deg", s.soundDir) : "-", x + 396, y + 5, s.soundDirValid ? TXT : DIM);
  y += row;
  label("from", x + 22, y + 5, DIM);
  slider(1, {(float)x + 160, (float)y, 160, 20}, &sim.voiceDir, -180.0f, 180.0f, VOICE);
  label(TextFormat("%+.0f deg   level %.2f", sim.voiceDir, s.soundLevel), x + 332, y + 5, TXT);
  y += row;

  led(x + 8, y + 10, s.touching, OK_C);
  label("Touch", x + 22, y + 5, TXT);
  label("CST816S", x + 90, y + 5, DIM);
  label(s.touching ? TextFormat("%.0f,%.0f  (face units)", s.touchX, s.touchY) : "none", x + 160, y + 5, s.touching ? TXT : DIM);
  if (button({(float)x + 332, (float)y, 80, 20}, "clock  [I]")) brain.requestInfo();
  y += row;

  led(x + 8, y + 10, s.battery.valid, s.batteryLow ? HOT : OK_C);
  label("Battery", x + 22, y + 5, TXT);
  label("BQ27220", x + 90, y + 5, DIM);
  slider(2, {(float)x + 160, (float)y, 160, 20}, &sim.batteryPct, 0.0f, 100.0f,
         sim.batteryPct <= BATTERY_LOW_PCT ? HOT : OK_C);
  if (button({(float)x + 332, (float)y, 80, 20}, emuHw.charging ? "charging [C]" : "on battery [C]", emuHw.charging, ACC))
    emuHw.charging = !emuHw.charging;
  label(s.battery.valid ? TextFormat("%d%% %.2fV", s.battery.percent, s.battery.millivolts / 1000.0f) : "-", x + 420, y + 5, DIM);
  y += row;

  double tod = timeOfDay();
  label("Clock", x + 22, y + 5, TXT);
  label("ESP32", x + 90, y + 5, DIM);
  float h = (float)(tod / 3600.0);
  if (slider(3, {(float)x + 160, (float)y, 160, 20}, &h, 0.0f, 23.99f, VOICE)) setHour(h);
  label(TextFormat("%02d:%02d", (int)(tod / 3600), (int)fmod(tod / 60, 60)), x + 332, y + 5, TXT);
  y += row + 2;

  DrawLine(x, y, x0 + PANEL_W - 16, y, LINE);
  y += 8;
  label("1-9 faces  TAB next  [ ] act  0 play  Y yes  U no  H hola  A auto  V talk  S bang  W wake word", x, y, DIM);
  y += 13;
  label("I clock  C charger  Q states  R record GIF  P shot  F fast  ENTER pause", x, y, DIM);
}

// ---- mouse: touch the glass, place the speaker on the ring ------------------------------------
static void mouse3d() {
  Vector2 m = GetMousePosition();
  int viewW = GetScreenWidth() - PANEL_W;
  int px = 0, py = 0;
  bool onScreen = View3D::screenPixel(m, px, py);
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m.x < viewW && m.y > 44) {
    if (onScreen) sim.mouseTouch = true;
    else sim.orbiting = true;
  }
  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && m.x < viewW) sim.orbiting = true;
  if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) sim.orbiting = false;
  if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    if (sim.mouseTouch) emuHw.touchDown = false;
    sim.mouseTouch = false;
  }
  if (sim.mouseTouch) {
    emuHw.touchDown = true;
    if (onScreen) {  // off the glass the finger stays where it left it
      emuHw.touchX = px;
      emuHw.touchY = py;
    }
  }
  if (sim.orbiting) View3D::orbit(GetMouseDelta());
  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f && m.x < viewW) View3D::zoom(wheel);
}

static void mouse() {
  if (sim.view3d && View3D::available()) {
    mouse3d();
    return;
  }
  Vector2 m = GetMousePosition();
  float cx = lcdRect.x + lcdRect.width * 0.5f, cy = lcdRect.y + lcdRect.height * 0.5f;
  float dx = m.x - cx, dy = m.y - cy, r = lcdRect.width * 0.5f;
  float d2 = dx * dx + dy * dy;
  bool inside = d2 <= r * r;
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inside) sim.mouseTouch = true;
  if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    if (sim.mouseTouch) emuHw.touchDown = false;
    sim.mouseTouch = false;
  }
  if (sim.mouseTouch) {
    emuHw.touchDown = true;
    emuHw.touchX = (int)Clamp((m.x - lcdRect.x) / lcdScale, 0.0f, LCD_W - 1.0f);
    emuHw.touchY = (int)Clamp((m.y - lcdRect.y) / lcdScale, 0.0f, LCD_W - 1.0f);
  }
  float rr = ringR();
  bool onRing = d2 >= (rr - 16.0f) * (rr - 16.0f) && d2 <= (rr + 16.0f) * (rr + 16.0f);
  if (onRing && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && dragId == 0) {
    float dir = atan2f(dy, dx) * RAD2DEG - 90.0f;
    while (dir > 180.0f) dir -= 360.0f;
    while (dir < -180.0f) dir += 360.0f;
    talk(dir);
  }
}

// ---- script (automated runs) --------------------------------------------------------------------
struct Step {
  double t;
  std::string cmd, val;
  bool done = false;
};

static std::vector<Step> parseScript(const char *s) {
  std::vector<Step> out;
  std::string all = s;
  size_t pos = 0;
  while (pos < all.size()) {
    size_t end = all.find(';', pos);
    if (end == std::string::npos) end = all.size();
    std::string item = all.substr(pos, end - pos);
    pos = end + 1;
    size_t a = item.find_first_not_of(" \t");
    if (a == std::string::npos) continue;
    item = item.substr(a);
    size_t colon = item.find(':');
    if (colon == std::string::npos) continue;
    Step st;
    st.t = atof(item.substr(0, colon).c_str());
    std::string rest = item.substr(colon + 1);
    size_t eq = rest.find('=');
    st.cmd = rest.substr(0, eq);
    st.val = eq == std::string::npos ? "" : rest.substr(eq + 1);
    while (!st.val.empty() && st.val.back() == ' ') st.val.pop_back();
    while (!st.cmd.empty() && st.cmd.back() == ' ') st.cmd.pop_back();
    out.push_back(st);
  }
  std::stable_sort(out.begin(), out.end(), [](const Step &a, const Step &b) { return a.t < b.t; });
  return out;
}

static std::string pendingShot;
static bool wantQuit = false;

static void runStep(const Step &st) {
  const char *v = st.val.c_str();
  if (st.cmd == "expr") {
    uint8_t e = Expressions::find(v);
    if (e != EX_NONE) brain.force(e, 600000);
    else fprintf(stderr, "script: unknown expression %s\n", v);
  } else if (st.cmd == "act") {
    int a = Acts::find(v);
    if (a >= 0) brain.forceAct(a);
    else fprintf(stderr, "script: unknown act %s\n", v);
  } else if (st.cmd == "say") brain.say(v);
  else if (st.cmd == "tap" || st.cmd == "hold") {
    sim.script.kind = st.cmd == "tap" ? 1 : 3;
    sim.script.start = sim.time;
    sim.script.x = 180, sim.script.y = 180;
    sscanf(v, "%f,%f", &sim.script.x, &sim.script.y);
  } else if (st.cmd == "stroke") {
    sim.script.kind = 2;
    sim.script.start = sim.time;
  } else if (st.cmd == "noise" || st.cmd == "sound") bang();
  else if (st.cmd == "voice") talk((float)atof(v));
  else if (st.cmd == "wake") wakeWord();
  else if (st.cmd == "hour") setHour((float)atof(v));
  else if (st.cmd == "battery") sim.batteryPct = (float)atof(v);
  else if (st.cmd == "charging") emuHw.charging = atoi(v) != 0;
  else if (st.cmd == "auto") brain.setAutonomous(atoi(v) != 0);
  else if (st.cmd == "states") sim.stateView = atoi(v) != 0;
  else if (st.cmd == "view3d") sim.view3d = atoi(v) != 0;
  else if (st.cmd == "orbit") View3D::orbit({(float)atof(v), 0.0f});
  else if (st.cmd == "info") brain.requestInfo();
  else if (st.cmd == "shot") pendingShot = st.val;
  else if (st.cmd == "record") toggleRecording();
  else if (st.cmd == "quit") wantQuit = true;
  else fprintf(stderr, "script: unknown command %s\n", st.cmd.c_str());
}

// ---- input --------------------------------------------------------------------------------------
static void handleKeys() {
  static const uint8_t FAV[9] = {EX_NORMAL, EX_HAPPY, EX_SAD, EX_ANGRY, EX_SURPRISED,
                                 EX_ASLEEP, EX_LOVE,  EX_DIZZY, EX_LAUGH};
  for (int i = 0; i < 9; i++)
    if (IsKeyPressed(KEY_ONE + i)) brain.force(FAV[i], 12000);
  int total = Acts::count();
  if (IsKeyPressed(KEY_ZERO)) brain.forceAct(sim.showSel);
  if (IsKeyPressed(KEY_LEFT_BRACKET)) sim.showSel = (sim.showSel + total - 1) % total;
  if (IsKeyPressed(KEY_RIGHT_BRACKET)) sim.showSel = (sim.showSel + 1) % total;
  if (IsKeyPressed(KEY_Y)) brain.forceAct(Acts::find("Yes"));
  if (IsKeyPressed(KEY_U)) brain.forceAct(Acts::find("No"));
  if (IsKeyPressed(KEY_H)) brain.say("HOLA!");
  if (IsKeyPressed(KEY_TAB)) {
    int d = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? EX_COUNT - 1 : 1;
    brain.force((uint8_t)((face.target() + d) % EX_COUNT), 12000);
  }
  if (IsKeyPressed(KEY_A)) {
    brain.setAutonomous(!brain.autonomous());
    toast(brain.autonomous() ? "autonomous behaviour ON" : "autonomous behaviour OFF");
  }
  if (IsKeyPressed(KEY_V)) talk(sim.voiceDir);
  if (IsKeyPressed(KEY_S)) bang();
  if (IsKeyPressed(KEY_W)) wakeWord();
  if (IsKeyPressed(KEY_I)) brain.requestInfo();
  if (IsKeyPressed(KEY_C)) emuHw.charging = !emuHw.charging;
  if (IsKeyPressed(KEY_Q)) sim.stateView = !sim.stateView;
  if (IsKeyPressed(KEY_M) && View3D::available()) sim.view3d = !sim.view3d;
  if (IsKeyPressed(KEY_R)) toggleRecording();
  if (IsKeyPressed(KEY_P)) {
    MakeDirectory("captures");
    time_t t = time(nullptr);
    char name[96];
    strftime(name, sizeof(name), "captures/shot_%Y%m%d_%H%M%S.png", localtime(&t));
    pendingShot = name;
  }
  if (IsKeyPressed(KEY_F)) sim.speed = sim.speed >= 8.0f ? 1.0f : sim.speed * 2.0f;
  if (IsKeyPressed(KEY_ENTER)) sim.paused = !sim.paused;
  if (IsKeyPressed(KEY_LEFT)) talk(sim.voiceDir - 30.0f < -180.0f ? sim.voiceDir + 330.0f : sim.voiceDir - 30.0f, 0.8);
  if (IsKeyPressed(KEY_RIGHT)) talk(sim.voiceDir + 30.0f > 180.0f ? sim.voiceDir - 330.0f : sim.voiceDir + 30.0f, 0.8);

}

// -------------------------------------------------------------------------------------------------
#ifdef __EMSCRIPTEN__
// Kept for pages that need to correct the mouse by hand (the default page does not:
// Emscripten's GLFW already maps CSS pixels back to canvas pixels).
extern "C" EMSCRIPTEN_KEEPALIVE void web_mouse_scale(float sx, float sy) { SetMouseScale(sx, sy); }
#endif

static std::vector<Step> script;  // --script steps
static bool scripted = false;

// One frame of the window: input, simulation, drawing. The desktop calls it in a loop,
// the browser once per animation frame (emscripten_set_main_loop).
static void frameOnce() {
  int winW = GetScreenWidth(), winH = GetScreenHeight();
  int viewW = winW - PANEL_W;
  int statesH = sim.stateView ? StateView::HEIGHT : 0;
  if (statesH > winH - 380) statesH = winH - 380;
  int viewH = winH - statesH;

  if (scripted) {
    double until = sim.time + 1.0 / 30.0;
    for (auto &st : script) {
      if (!st.done) {
        until = st.t;
        break;
      }
    }
    if (until > sim.time) stepSim(until - sim.time);
    for (auto &st : script) {
      if (!st.done && st.t <= sim.time + 1e-6) {
        st.done = true;
        runStep(st);
        if (!pendingShot.empty() || wantQuit) break;
      }
    }
    bool left = false;
    for (auto &st : script) left |= !st.done;
    if (!left && pendingShot.empty() && sim.script.kind == 0) wantQuit = true;
  } else {
    handleKeys();
    mouse();
    if (!sim.paused) stepSim(fmin(GetFrameTime(), 0.05f) * sim.speed);
  }

  if (sim.time - spiStamp >= 0.5) {
    spiPerSec = (uint32_t)((emuHw.spiBytes - spiLast) / (sim.time - spiStamp));
    fps = (uint32_t)((emuHw.lcdUpdates - framesLast) / (sim.time - spiStamp));
    spiLast = emuHw.spiBytes;
    framesLast = emuHw.lcdUpdates;
    spiStamp = sim.time;
  }
  updateLcdTexture();

  BeginDrawing();
  ClearBackground(BG_BOT);
  DrawRectangleGradientV(0, 0, viewW, winH, BG_TOP, BG_BOT);
  drawRobot(viewW, viewH);
  if (GetTime() < sim.toastUntil) {
    int tw = MeasureText(sim.toast.c_str(), 20);
    DrawRectangleRounded({(float)(viewW - tw) / 2 - 14, (float)viewH - 70, (float)tw + 28, 34}, 0.4f, 6, Fade(BLACK, 0.7f));
    label(sim.toast.c_str(), (viewW - tw) / 2, viewH - 63, WHITE, 20);
  }
  if (statesH > 0) stateView.draw({0, (float)viewH, (float)viewW, (float)statesH}, brain, face, sensors.state());
  drawPanel(viewW, winH);

  if (!pendingShot.empty()) {
    rlDrawRenderBatchActive();
    Image shot = LoadImageFromScreen();
    ExportImage(shot, pendingShot.c_str());
    UnloadImage(shot);
    if (!scripted) toast("saved " + pendingShot);
    pendingShot.clear();
  }
  EndDrawing();
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--script") && i + 1 < argc) script = parseScript(argv[++i]);
  }
  scripted = !script.empty();

  SetTraceLogLevel(LOG_WARNING);
#ifdef __EMSCRIPTEN__
  SetConfigFlags(FLAG_MSAA_4X_HINT);  // fixed 1380 x 900: the page scales the canvas with CSS
#else
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
#endif
  InitWindow(WIN_W, WIN_H, "VoCat Eyes - ESP-VoCat emulator");
  SetWindowMinSize(1100, 760);
  SetTargetFPS(60);
  SetExitKey(KEY_NULL);
#ifndef __EMSCRIPTEN__
  ChangeDirectory(GetApplicationDirectory());
#else
  sim.stateView = true;  // in the browser the state view starts open
#endif

  Image img = GenImageColor(LCD_W, LCD_W, BLACK);
  lcdTex = LoadTextureFromImage(img);
  UnloadImage(img);
  SetTextureFilter(lcdTex, TEXTURE_FILTER_BILINEAR);
  if (View3D::load()) sim.view3d = true;  // the enclosure is there: start in 3D

  // the clock of the robot starts at the time of the PC
  {
    time_t t = time(nullptr);
    struct tm *tmv = localtime(&t);
    setHour(tmv->tm_hour + tmv->tm_min / 60.0f + tmv->tm_sec / 3600.0f);
    emuHw.rtcDay = tmv->tm_mday;
    emuHw.rtcMonth = tmv->tm_mon + 1;
    emuHw.rtcYear = tmv->tm_year + 1900;
  }
  applyInputs();
  setup();
  sim.time = emuHw.clockMs / 1000.0;

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(frameOnce, 0, 1);
#else
  while (!WindowShouldClose() && !wantQuit) frameOnce();
#endif

  if (sim.gif.isOpen()) sim.gif.close();
  View3D::unload();
  UnloadTexture(lcdTex);
  CloseWindow();
  return 0;
}
