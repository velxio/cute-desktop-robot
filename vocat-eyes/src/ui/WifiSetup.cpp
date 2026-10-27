#include "WifiSetup.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "../../Config.h"
#include "../core/MathUtil.h"
#include "../voice/GeminiLive.h"

// Everything in face units: the glass is a circle of radius 64 around (64, 40).
namespace {
const float CX = DISC_CX, CY = DISC_CY;
const float KEY_W = 10.8f, KEY_H = 10.6f, ROW_PITCH = 12.2f, ROW0_Y = 25.0f;
const Rgb KEY_BG = 0x1E2534, KEY_HOT = 0x35E0FF, FIELD_BG = 0x0E1218;

const char *LAYER_ROWS[3][3] = {
    {"1234567890", "qwertyuiop", "asdfghjkl"},
    {"1234567890", "QWERTYUIOP", "ASDFGHJKL"},
    {"!@#$%^&*()", "-_=+[]{}\\|", ";:'\",.<>/?"},
};
const char *LAYER_ROW3[3] = {"zxcvbnm", "ZXCVBNM", "~`"};

bool inDisc(float x, float y) {
  float dx = x - CX, dy = y - CY;
  return dx * dx + dy * dy <= 64.0f * 64.0f;
}
}  // namespace

// ---- open / close ---------------------------------------------------------------------------
void WifiSetup::open() {
  if (active()) return;
  passLen_ = 0;
  pass_[0] = 0;
  layer_ = 0;
  picked_ = -1;
  page_ = 0;
  hotRow_ = hotCol_ = hotItem_ = -1;
  msg_[0] = 0;
  mode_ = SCANNING;
  since_ = millis();
  startScan();
}

void WifiSetup::close() {
  mode_ = OFF;
  hotRow_ = hotCol_ = hotItem_ = -1;
}

void WifiSetup::sortNets() {
  for (int i = 1; i < netCount_; i++)
    for (int j = i; j > 0 && nets_[j].rssi > nets_[j - 1].rssi; j--) {
      Net t = nets_[j];
      nets_[j] = nets_[j - 1];
      nets_[j - 1] = t;
    }
}

// ---- the keyboard geometry -------------------------------------------------------------------
// rows 0-2: the layer's rows; row 3: shift + letters + backspace; row 4: symbols, space, OK
int WifiSetup::rowKeys(int row) const {
  if (row < 3) return (int)strlen(LAYER_ROWS[layer_][row]);
  if (row == 3) return (int)strlen(LAYER_ROW3[layer_]) + 2;
  return 3;
}

float WifiSetup::keyW(int row, int col) const {
  if (row == 4) return col == 0 ? KEY_W * 1.4f : (col == 1 ? KEY_W * 4.4f : KEY_W * 1.8f);
  if (row == 3 && (col == 0 || col == rowKeys(3) - 1)) return KEY_W * 1.3f;
  return KEY_W;
}

float WifiSetup::keyX(int row, int col) const {
  float total = 0.0f;
  int n = rowKeys(row);
  for (int k = 0; k < n; k++) total += keyW(row, k);
  float x = CX - total * 0.5f;
  for (int k = 0; k < col; k++) x += keyW(row, k);
  return x;
}

bool WifiSetup::keyAt(float x, float y, int &row, int &col) const {
  for (int r = 0; r < 5; r++) {
    float y0 = ROW0_Y + r * ROW_PITCH;
    if (y < y0 || y > y0 + KEY_H + 1.0f) continue;
    int n = rowKeys(r);
    for (int k = 0; k < n; k++) {
      float x0 = keyX(r, k);
      if (x >= x0 && x < x0 + keyW(r, k)) {
        row = r;
        col = k;
        return true;
      }
    }
  }
  return false;
}

const char *WifiSetup::keyLabel(int row, int col, char *buf) const {
  if (row < 3) {
    buf[0] = LAYER_ROWS[layer_][row][col];
    buf[1] = 0;
    return buf;
  }
  if (row == 3) {
    int n = rowKeys(3);
    if (col == 0) return layer_ == 1 ? "a" : "^";
    if (col == n - 1) return "<";
    buf[0] = LAYER_ROW3[layer_][col - 1];
    buf[1] = 0;
    return buf;
  }
  if (col == 0) return layer_ == 2 ? "a" : "#";
  if (col == 1) return "";
  return "OK";
}

void WifiSetup::pressKey(int row, int col) {
  char buf[2];
  if (row == 3 && col == 0) {
    layer_ = layer_ == 0 ? 1 : 0;
    return;
  }
  if (row == 3 && col == rowKeys(3) - 1) {
    if (passLen_ > 0) pass_[--passLen_] = 0;
    return;
  }
  if (row == 4) {
    if (col == 0) layer_ = layer_ == 2 ? 0 : 2;
    else if (col == 1) {
      if (passLen_ < (int)sizeof(pass_) - 1) {
        pass_[passLen_++] = ' ';
        pass_[passLen_] = 0;
      }
    } else {
      // OK: connect
      mode_ = CONNECTING;
      since_ = millis();
      ok_ = false;
      if (gemini_) gemini_->setCredentials(nets_[picked_].ssid, pass_);
    }
    return;
  }
  const char *l = keyLabel(row, col, buf);
  if (passLen_ < (int)sizeof(pass_) - 1) {
    pass_[passLen_++] = l[0];
    pass_[passLen_] = 0;
  }
}

// ---- the list geometry -------------------------------------------------------------------------
int WifiSetup::listRowAt(float x, float y) const {
  if (!inDisc(x, y)) return -1;
  // rows of the page at y = 16 + i * 10; the last row holds the buttons
  if (y >= 16.0f && y < 16.0f + PER_PAGE * 10.0f) {
    int i = (int)((y - 16.0f) / 10.0f);
    int idx = page_ * PER_PAGE + i;
    return idx < netCount_ ? idx : -1;
  }
  if (y >= 68.0f && y < 80.0f) return x < CX ? -2 : -3;  // -2 rescan, -3 next page
  return -1;
}

// ---- touch --------------------------------------------------------------------------------------
void WifiSetup::update(const SensorState &s, uint32_t nowMs) {
  if (!active()) return;
  if (s.gesture == G_HOLD) {
    close();
    return;
  }
  switch (mode_) {
    case SCANNING:
      if (pollScan()) {
        sortNets();
        mode_ = LIST;
        page_ = 0;
      }
      break;
    case LIST: {
      hotItem_ = s.touching ? listRowAt(s.touchX, s.touchY) : -1;
      if (s.gesture == G_TAP) {
        int i = listRowAt(s.gestureX, s.gestureY);
        if (i >= 0) {
          picked_ = i;
          passLen_ = 0;
          pass_[0] = 0;
          if (nets_[i].open) {
            mode_ = CONNECTING;
            since_ = nowMs;
            if (gemini_) gemini_->setCredentials(nets_[i].ssid, "");
          } else {
            mode_ = KEYBOARD;
          }
        } else if (i == -2) {
          mode_ = SCANNING;
          since_ = nowMs;
          startScan();
        } else if (i == -3 && netCount_ > PER_PAGE) {
          page_ = (page_ + 1) % ((netCount_ + PER_PAGE - 1) / PER_PAGE);
        }
      }
      break;
    }
    case KEYBOARD: {
      int r, k;
      if (s.touching && keyAt(s.touchX, s.touchY, r, k)) {
        hotRow_ = r;
        hotCol_ = k;
      } else {
        hotRow_ = hotCol_ = -1;
      }
      if (s.gesture == G_TAP && keyAt(s.gestureX, s.gestureY, r, k)) pressKey(r, k);
      break;
    }
    case CONNECTING:
      if (gemini_ && gemini_->wifiConnected()) {
        ok_ = true;
        snprintf(msg_, sizeof(msg_), "%s", gemini_->wifiInfo());
        mode_ = DONE;
        since_ = nowMs;
      } else if (nowMs - since_ > 3000 && gemini_ && !strcmp(gemini_->wifiInfo(), "clave erronea")) {
        ok_ = false;  // the router already said no: back to the keyboard sooner
        strcpy(msg_, "clave incorrecta");
        mode_ = DONE;
        since_ = nowMs;
      } else if (nowMs - since_ > 25000) {
        ok_ = false;
        snprintf(msg_, sizeof(msg_), "%s", gemini_ ? gemini_->wifiInfo() : "no se pudo conectar");
        mode_ = DONE;
        since_ = nowMs;
      }
      break;
    case DONE:
      if (s.gesture == G_TAP || (ok_ && nowMs - since_ > 4000)) {
        if (ok_) close();
        else mode_ = nets_[picked_ >= 0 ? picked_ : 0].open ? LIST : KEYBOARD;
      }
      break;
    default:
      break;
  }
}

// ---- drawing --------------------------------------------------------------------------------------
void WifiSetup::drawList(Canvas &c, uint32_t nowMs) {
  c.textCentered(CX, 4.0f, "REDES WIFI", 1, INK_GRAY);
  if (netCount_ == 0) {
    c.textCentered(CX, 36.0f, "ninguna red", 1, INK_GRAY);
  }
  for (int i = 0; i < PER_PAGE; i++) {
    int idx = page_ * PER_PAGE + i;
    if (idx >= netCount_) break;
    float y = 16.0f + i * 10.0f;
    float half = sqrtf(64.0f * 64.0f - (y + 4.0f - CY) * (y + 4.0f - CY)) - 4.0f;
    bool hot = hotItem_ == idx;
    c.fillRoundRect(CX - half, y, half * 2.0f, 9.0f, 2.5f, hot ? Paint(KEY_HOT, 0.35f) : Paint(KEY_BG));
    char name[20];
    strncpy(name, nets_[idx].ssid, 13);
    name[13] = 0;
    c.text(CX - half + 4.0f, y + 0.8f, name, 1, INK_WHITE);
    // signal: 4 bars
    int bars = nets_[idx].rssi > -55 ? 4 : (nets_[idx].rssi > -65 ? 3 : (nets_[idx].rssi > -75 ? 2 : 1));
    for (int b = 0; b < 4; b++) {
      float bx = CX + half - 14.0f + b * 3.0f, bh = 1.5f + b * 1.5f;
      c.fillRect(bx, y + 7.0f - bh, 2.0f, bh, b < bars ? Paint(INK_MINT) : Paint(INK_DARK));
    }
    if (nets_[idx].open) c.text(CX + half - 22.0f, y + 0.8f, "o", 1, INK_GRAY);
  }
  // buttons
  c.fillRoundRect(CX - 50.0f, 67.0f, 46.0f, 10.0f, 3.0f, Paint(KEY_BG));
  c.textCentered(CX - 27.0f, 68.2f, "buscar", 1, INK_SKY);
  if (netCount_ > PER_PAGE) {
    c.fillRoundRect(CX + 4.0f, 67.0f, 46.0f, 10.0f, 3.0f, Paint(KEY_BG));
    char t[16];
    snprintf(t, sizeof(t), "%d/%d >", page_ + 1, (netCount_ + PER_PAGE - 1) / PER_PAGE);
    c.textCentered(CX + 27.0f, 68.2f, t, 1, INK_SKY);
  }
  c.textCentered(CX, 79.0f, "largo: salir", 1, INK_DARK);
  (void)nowMs;
}

void WifiSetup::drawKeyboard(Canvas &c, uint32_t nowMs) {
  char title[24];
  snprintf(title, sizeof(title), "%.14s", picked_ >= 0 ? nets_[picked_].ssid : "");
  c.textCentered(CX, 3.0f, title, 1, INK_GRAY);
  // the password field
  c.fillRoundRect(CX - 46.0f, 11.0f, 92.0f, 11.0f, 2.5f, Paint(FIELD_BG));
  c.rect(CX - 46.0f, 11.0f, 92.0f, 11.0f, 0.8f, Paint(INK_DARK));
  char shown[24];
  int n = passLen_;
  const char *tail = n > 13 ? pass_ + n - 13 : pass_;
  snprintf(shown, sizeof(shown), "%s", tail);
  float tw = Canvas::textWidth(shown, 1);
  c.text(CX - 44.0f, 14.0f, shown, 1, INK_WHITE);
  if ((nowMs / 500) % 2 == 0) c.fillRect(CX - 44.0f + tw + 0.5f, 13.5f, 1.2f, 6.0f, Paint(INK_CYAN));
  // the keys
  char buf[2];
  for (int r = 0; r < 5; r++) {
    int nk = rowKeys(r);
    float y = ROW0_Y + r * ROW_PITCH;
    for (int k = 0; k < nk; k++) {
      float x = keyX(r, k), w = keyW(r, k);
      bool hot = r == hotRow_ && k == hotCol_;
      bool special = (r == 3 && (k == 0 || k == nk - 1)) || r == 4;
      bool lit = (r == 3 && k == 0 && layer_ == 1) || (r == 4 && k == 0 && layer_ == 2);
      Rgb bg = hot ? KEY_HOT : (lit ? 0x2F6F8A : (special ? 0x2A3650 : KEY_BG));
      if (r == 4 && k == 2) bg = hot ? INK_MINT : 0x1F6B4A;
      c.fillRoundRect(x + 0.6f, y, w - 1.2f, KEY_H, 2.2f, Paint(bg));
      const char *l = keyLabel(r, k, buf);
      if (*l) c.textCentered(x + w * 0.5f, y + (KEY_H - Canvas::textHeight(1)) * 0.5f, l, 1, hot ? INK_DARK : INK_WHITE);
    }
  }
}

void WifiSetup::drawStatus(Canvas &c, uint32_t nowMs) {
  const char *ssid = picked_ >= 0 ? nets_[picked_].ssid : "";
  if (mode_ == SCANNING) {
    c.textCentered(CX, 30.0f, "buscando redes", 1, INK_WHITE);
  } else if (mode_ == CONNECTING) {
    c.textCentered(CX, 26.0f, "conectando a", 1, INK_GRAY);
    c.textCentered(CX, 34.0f, ssid, 1, INK_WHITE);
  } else {
    c.textCentered(CX, 26.0f, ok_ ? "conectado" : "fallo", 1, ok_ ? INK_MINT : INK_ORANGE);
    c.textCentered(CX, 34.0f, msg_, 1, INK_WHITE);
    c.textCentered(CX, 60.0f, ok_ ? "" : "toca para volver", 1, INK_GRAY);
  }
  if (mode_ != DONE) {
    // a spinner
    float a = (nowMs % 1200) / 1200.0f * 6.2831853f;
    c.arc(CX, 52.0f, 6.0f, a, a + 4.0f, 2.0f, Paint(INK_CYAN));
  }
}

void WifiSetup::draw(Canvas &c, uint32_t nowMs) {
  c.clear();
  switch (mode_) {
    case LIST: drawList(c, nowMs); break;
    case KEYBOARD: drawKeyboard(c, nowMs); break;
    default: drawStatus(c, nowMs); break;
  }
}

// ---- the scan ------------------------------------------------------------------------------------
#ifdef EMULATOR
void WifiSetup::startScan() {}
bool WifiSetup::pollScan() {
  if (millis() - since_ < 1500) return false;
  static const char *N[] = {"HomeNet_5G", "Casa de al lado", "MOVISTAR_2F3A", "cafeteria", "iPhone de Ana", "lab", "vecino"};
  static const int8_t R[] = {-48, -70, -62, -80, -58, -85, -90};
  netCount_ = 7;
  for (int i = 0; i < netCount_; i++) {
    strncpy(nets_[i].ssid, N[i], 32);
    nets_[i].ssid[32] = 0;
    nets_[i].rssi = R[i];
    nets_[i].open = i == 3;
  }
  return true;
}
#else
#include <WiFi.h>

// The Wi-Fi task owns the radio: it does the scan (see GeminiLive::doScan).
void WifiSetup::startScan() {
  if (gemini_) gemini_->requestScan();
}

bool WifiSetup::pollScan() {
  if (!gemini_) return true;
  WifiNet found[MAX_NETS];
  int n = gemini_->scanResult(found, MAX_NETS);
  if (n < 0) {
    if (millis() - since_ > 20000) {  // a conversation is holding the radio
      netCount_ = 0;
      return true;
    }
    return false;
  }
  netCount_ = n;
  for (int i = 0; i < n; i++) {
    memcpy(nets_[i].ssid, found[i].ssid, sizeof(nets_[i].ssid));
    nets_[i].rssi = found[i].rssi;
    nets_[i].open = found[i].open;
  }
  return true;
}
#endif
