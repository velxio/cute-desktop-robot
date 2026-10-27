#include "Settings.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../Config.h"

// ---- the values ---------------------------------------------------------------------------
const char *const Settings::VOICES[] = {"Puck", "Charon", "Kore", "Fenrir", "Aoede", "Leda", "Orus", "Zephyr"};
const int Settings::VOICE_COUNT = 8;
const uint8_t Settings::TALK_END_S[] = {10, 20, 30, 60};
const int Settings::TALK_END_COUNT = 4;

#ifdef EMULATOR
void Settings::load() {
  volume = SPEAKER_VOLUME;
  wakeWord = WAKE_WORD != 0;
  for (int i = 0; i < VOICE_COUNT; i++)
    if (!strcmp(VOICES[i], GEMINI_VOICE)) voice = (uint8_t)i;
}
void Settings::save() const { Serial.println("settings: saved (emulator: in memory only)"); }
#else
#include <Preferences.h>

void Settings::load() {
  volume = SPEAKER_VOLUME;
  wakeWord = WAKE_WORD != 0;
  for (int i = 0; i < VOICE_COUNT; i++)
    if (!strcmp(VOICES[i], GEMINI_VOICE)) voice = (uint8_t)i;
  Preferences p;
  if (!p.begin("cfg", true)) return;  // nothing saved yet
  volume = p.getUChar("vol", volume);
  brightness = p.getUChar("bright", brightness);
  voice = p.getUChar("voice", voice) % VOICE_COUNT;
  wakeWord = p.getBool("wake", wakeWord);
  sounds = p.getBool("sounds", sounds);
  autonomous = p.getBool("auto", autonomous);
  talkEnd = p.getUChar("talkend", talkEnd) % TALK_END_COUNT;
  p.end();
}

void Settings::save() const {
  Preferences p;
  if (!p.begin("cfg", false)) return;
  p.putUChar("vol", volume);
  p.putUChar("bright", brightness);
  p.putUChar("voice", voice);
  p.putBool("wake", wakeWord);
  p.putBool("sounds", sounds);
  p.putBool("auto", autonomous);
  p.putUChar("talkend", talkEnd);
  p.end();
  Serial.println("settings: saved");
}
#endif

// ---- the screen ---------------------------------------------------------------------------
namespace {
const float CX = DISC_CX, CY = DISC_CY;
const Rgb ROW_BG = 0x1E2534, ROW_HOT = 0x2F6F8A;
const float ROW0_Y = 15.0f, ROW_PITCH = 10.0f, ROW_H = 9.0f;
const char *const LABELS[] = {"Volumen", "Brillo", "Voz", "Wi-Fi", "Hi ESP", "Sonidos", "Autonomo", "Fin charla"};
const int PAGES = (SettingsScreen::ITEM_COUNT + 4) / 5;

float rowHalf(float y) {
  float d = y + ROW_H * 0.5f - CY;
  return sqrtf(64.0f * 64.0f - d * d) - 5.0f;
}
}  // namespace

void SettingsScreen::open() {
  open_ = true;
  page_ = 0;
  hot_ = -1;
  flashItem_ = -1;
  lastTouchMs_ = millis();
}

void SettingsScreen::close() {
  if (!open_) return;
  open_ = false;
  if (cfg_) cfg_->save();
}

int SettingsScreen::rowAt(float x, float y) const {
  (void)x;
  if (y >= 65.0f && y < 76.0f) return -2;
  for (int i = 0; i < PER_PAGE; i++) {
    float y0 = ROW0_Y + i * ROW_PITCH;
    if (y >= y0 - 0.5f && y < y0 + ROW_H + 0.5f) {
      int item = page_ * PER_PAGE + i;
      return item < ITEM_COUNT ? item : -1;
    }
  }
  return -1;
}

void SettingsScreen::change(int item, int dir) {
  Settings &c = *cfg_;
  switch (item) {
    case VOLUME: c.volume = (uint8_t)constrain((int)c.volume + dir * 10, 0, 100); break;
    case BRIGHTNESS: c.brightness = (uint8_t)constrain((int)c.brightness + dir * 10, 20, 100); break;
    case VOICE: c.voice = (uint8_t)((c.voice + Settings::VOICE_COUNT + dir) % Settings::VOICE_COUNT); break;
    case WAKE: c.wakeWord = !c.wakeWord; break;
    case SOUNDS: c.sounds = !c.sounds; break;
    case AUTONOMOUS: c.autonomous = !c.autonomous; break;
    case TALK_END: c.talkEnd = (uint8_t)((c.talkEnd + Settings::TALK_END_COUNT + dir) % Settings::TALK_END_COUNT); break;
    default: break;
  }
  flashItem_ = item;
  flashUntil_ = millis() + 400;
  if (onChange_) onChange_((Item)item);
}

void SettingsScreen::update(const SensorState &s, uint32_t nowMs) {
  if (!open_ || !cfg_) return;
  if (s.touching || s.gesture != G_NONE) lastTouchMs_ = nowMs;
  if (s.gesture == G_HOLD || nowMs - lastTouchMs_ > 30000) {
    close();
    return;
  }
  hot_ = s.touching ? rowAt(s.touchX, s.touchY) : -1;
  if (s.gesture != G_TAP) return;
  int item = rowAt(s.gestureX, s.gestureY);
  if (item == -2) {
    page_ = (page_ + 1) % PAGES;
  } else if (item == WIFI) {
    if (onChange_) onChange_(WIFI);  // the caller closes this screen and opens the Wi-Fi one
  } else if (item >= 0) {
    // left half: less / previous, right half: more / next
    change(item, s.gestureX < CX ? -1 : 1);
  }
}

void SettingsScreen::valueText(int item, char *out, int n) const {
  const Settings &c = *cfg_;
  switch (item) {
    case VOLUME: snprintf(out, n, "%d%%", c.volume); break;
    case BRIGHTNESS: snprintf(out, n, "%d%%", c.brightness); break;
    case VOICE: snprintf(out, n, "%s", c.voiceName()); break;
    case WIFI: snprintf(out, n, "%s", wifiOk_ ? "ok >" : "no >"); break;
    case WAKE: snprintf(out, n, "%s", c.wakeWord ? "si" : "no"); break;
    case SOUNDS: snprintf(out, n, "%s", c.sounds ? "si" : "no"); break;
    case AUTONOMOUS: snprintf(out, n, "%s", c.autonomous ? "si" : "no"); break;
    case TALK_END: snprintf(out, n, "%d s", Settings::TALK_END_S[c.talkEnd % Settings::TALK_END_COUNT]); break;
    default: out[0] = 0; break;
  }
}

void SettingsScreen::draw(Canvas &c, uint32_t nowMs) {
  c.clear();
  if (!cfg_) return;
  c.textCentered(CX, 3.5f, "AJUSTES", 1, INK_GRAY);
  for (int i = 0; i < PER_PAGE; i++) {
    int item = page_ * PER_PAGE + i;
    if (item >= ITEM_COUNT) break;
    float y = ROW0_Y + i * ROW_PITCH;
    float half = rowHalf(y);
    c.fillRoundRect(CX - half, y, half * 2.0f, ROW_H, 2.5f, Paint(hot_ == item ? ROW_HOT : ROW_BG));
    c.text(CX - half + 3.5f, y + 0.8f, LABELS[item], 1, INK_WHITE);
    char v[16];
    valueText(item, v, sizeof(v));
    bool flash = item == flashItem_ && (int32_t)(flashUntil_ - nowMs) > 0;
    bool toggle = item == WAKE || item == SOUNDS || item == AUTONOMOUS;
    Rgb color = flash ? INK_CYAN : (toggle ? (strcmp(v, "si") ? INK_ORANGE : INK_MINT) : INK_SKY);
    if (item == WIFI) color = wifiOk_ ? INK_MINT : INK_ORANGE;
    float vw = Canvas::textWidth(v, 1);
    c.text(CX + half - 3.5f - vw, y + 0.8f, v, 1, color);
  }
  // page button and hints
  c.fillRoundRect(CX - 20.0f, 66.0f, 40.0f, 9.0f, 3.0f, Paint(ROW_BG));
  char t[12];
  snprintf(t, sizeof(t), "%d/%d >", page_ + 1, PAGES);
  c.textCentered(CX, 66.8f, t, 1, INK_SKY);
  c.textCentered(CX, 77.0f, "izq - / der +", 1, INK_GRAY);
  c.textCentered(CX, 84.5f, "largo: salir", 1, INK_GRAY);
}
