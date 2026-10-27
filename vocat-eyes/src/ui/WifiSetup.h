// Wi-Fi setup on the glass: a list of the networks around, a keyboard to type the
// password (letters, capitals, numbers, symbols), and the result. Opened with a tap on
// the clock screen (long press, then tap) or with "wifi setup" on the console; a long
// press closes it. The credentials go to the flash (NVS) and survive reboots.
#ifndef UI_WIFISETUP_H
#define UI_WIFISETUP_H

#include <stdint.h>

#include "../brain/Sensors.h"
#include "../core/Canvas.h"

class GeminiLive;

class WifiSetup {
 public:
  void begin(GeminiLive *gemini) { gemini_ = gemini; }
  void open();
  void close();
  bool active() const { return mode_ != OFF; }
  // Touch and time, once per frame while active.
  void update(const SensorState &s, uint32_t nowMs);
  void draw(Canvas &c, uint32_t nowMs);

 private:
  enum Mode : uint8_t { OFF, SCANNING, LIST, KEYBOARD, CONNECTING, DONE };
  struct Net {
    char ssid[33];
    int8_t rssi;
    bool open;
  };
  static const int MAX_NETS = 12;
  static const int PER_PAGE = 5;

  void startScan();
  bool pollScan();  // true when the list is ready
  void sortNets();
  int listRowAt(float x, float y) const;
  bool keyAt(float x, float y, int &row, int &col) const;
  void pressKey(int row, int col);
  int rowKeys(int row) const;
  float keyX(int row, int col) const;
  float keyW(int row, int col) const;
  const char *keyLabel(int row, int col, char *buf) const;
  void drawList(Canvas &c, uint32_t nowMs);
  void drawKeyboard(Canvas &c, uint32_t nowMs);
  void drawStatus(Canvas &c, uint32_t nowMs);

  GeminiLive *gemini_ = nullptr;
  Mode mode_ = OFF;
  Net nets_[MAX_NETS];
  int netCount_ = 0, page_ = 0, picked_ = -1;
  char pass_[64] = {0};
  int passLen_ = 0;
  uint8_t layer_ = 0;  // 0 small letters, 1 capitals, 2 symbols
  uint32_t since_ = 0;
  int hotRow_ = -1, hotCol_ = -1, hotItem_ = -1;
  char msg_[48] = {0};
  bool ok_ = false;
};

#endif
