// A voice conversation with Gemini Live over a WebSocket: the microphone goes up as
// 16 kHz PCM, the voice comes back as 24 kHz PCM and goes to the Player. Runs in its
// own task; the main loop only starts/stops it and reads its events.
#ifndef VOICE_GEMINILIVE_H
#define VOICE_GEMINILIVE_H

#include <stddef.h>
#include <stdint.h>

class Player;
class Listener;

enum TalkEventKind : uint8_t {
  TE_NONE = 0,
  TE_CONNECTING,
  TE_CONNECTED,    // setup done, listening
  TE_AUDIO,        // the first voice chunk of a reply arrived (speaking)
  TE_TURN_DONE,    // the reply is complete (the player may still be draining it)
  TE_INTERRUPTED,  // the user spoke over it
  TE_TOOL,         // name = function, text = its first argument
  TE_TEXT,         // text = a piece of the transcription of what it says
  TE_CLOSED,       // the conversation ended
  TE_FAILED        // text = why
};

// One network seen by a scan.
struct WifiNet {
  char ssid[33];
  int8_t rssi;
  bool open;
};

struct TalkEvent {
  uint8_t kind = TE_NONE;
  char name[24] = {0};
  char text[120] = {0};
};

class GeminiLive {
 public:
  bool begin(Player *player, Listener *listener);
  // Instructions and tools (JSON array of function declarations) used by every session.
  void setSystemPrompt(const char *text) { prompt_ = text; }
  void setTools(const char *json) { tools_ = json; }
  // Used from the next session on.
  void setVoice(const char *name) { voice_ = name; }
  void setIdleEndMs(uint32_t ms) { idleEndMs_ = ms; }

  void start();  // connects (Wi-Fi first, if needed) and listens
  void stop();   // closes the session
  bool active() const { return active_; }
  bool connected() const { return connected_; }
  bool takeEvent(TalkEvent &e);

  bool wifiConnected() const;
  const char *wifiInfo() const;  // IP or the reason it is not up
  const char *ssid() const { return ssid_; }
  // New network: saved in the flash, connects right away.
  void setCredentials(const char *ssid, const char *pass);
  // Scan for networks. The scan runs in the Wi-Fi task: it stops the connection attempts
  // while it listens (an ESP32 refuses to scan while it is busy connecting).
  void requestScan();
  // -1 while the scan runs; then the number of networks copied to `out` (0 = none found).
  int scanResult(WifiNet *out, int max);
  // Console diagnostic: DNS, TLS and the WebSocket handshake, step by step, printed.
  void probe();

 private:
  static void taskEntry(void *arg);
  void task();
  void ensureWifi();
  bool openSession();
  void closeSession();
  void pumpMic();
  void onMessage(const char *data, size_t len);
  void sendAudio(const int16_t *pcm, size_t n);
  void sendToolResponse(const char *id, const char *name);
  void emit(uint8_t kind, const char *name, const char *text);
  static void micSink(void *arg, const int16_t *pcm, size_t n);

  Player *player_ = nullptr;
  Listener *listener_ = nullptr;
  const char *prompt_ = "";
  const char *tools_ = "[]";
  const char *voice_ = "Puck";
  uint32_t idleEndMs_ = 20000;
  volatile bool want_ = false, active_ = false, connected_ = false, setupDone_ = false;
  void *events_ = nullptr;  // queue of TalkEvent
  void *mic_ = nullptr;     // stream buffer with the microphone samples
  uint32_t lastServerMs_ = 0, startedMs_ = 0, lastVoiceMs_ = 0;
  bool wifiStarted_ = false;
  volatile bool reconnect_ = false;
  volatile uint8_t lastReason_ = 0;  // why the router said no (wifi_err_reason_t)
  uint32_t wifiSinceMs_ = 0;
  char wifiInfo_[40] = "no wifi yet";
  void doScan();
  volatile int8_t scanState_ = 0;  // 0 idle, 1 asked, 2 done
  int scanCount_ = 0;
  WifiNet scanNets_[20];
  char closeText_[100] = {0};  // what the server said when it closed the socket
  char ssid_[33] = {0};
  char pass_[65] = {0};
};

#endif
