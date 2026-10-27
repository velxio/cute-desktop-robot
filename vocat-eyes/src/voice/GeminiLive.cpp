#include "GeminiLive.h"

#include <Arduino.h>
#include <string.h>

#include "../../Config.h"
#include "../audio/Listener.h"
#include "../audio/Player.h"

#ifdef EMULATOR
// On Windows there is no network: a session "connects" and listens, nothing more.
bool GeminiLive::begin(Player *player, Listener *listener) {
  player_ = player;
  listener_ = listener;
  return true;
}
void GeminiLive::start() {
  if (active_) return;
  active_ = connected_ = true;
  startedMs_ = millis();
  emit(TE_CONNECTED, "", "");
}
void GeminiLive::stop() {
  if (!active_) return;
  active_ = connected_ = false;
  emit(TE_CLOSED, "", "");
}
bool GeminiLive::takeEvent(TalkEvent &e) {
  TalkEvent *q = (TalkEvent *)events_;
  if (!q || q->kind == TE_NONE) return false;
  e = *q;
  q->kind = TE_NONE;
  return true;
}
void GeminiLive::emit(uint8_t kind, const char *name, const char *text) {
  if (!events_) events_ = new TalkEvent();
  TalkEvent *q = (TalkEvent *)events_;
  q->kind = kind;
  strncpy(q->name, name, sizeof(q->name) - 1);
  strncpy(q->text, text, sizeof(q->text) - 1);
}
bool GeminiLive::wifiConnected() const { return reconnect_ && millis() - wifiSinceMs_ > 2500; }
const char *GeminiLive::wifiInfo() const { return wifiConnected() ? "192.168.1.42" : "sin red (emu)"; }
void GeminiLive::requestScan() {}
int GeminiLive::scanResult(WifiNet *, int) { return 0; }
void GeminiLive::doScan() {}
void GeminiLive::setCredentials(const char *ssid, const char *pass) {
  strncpy(ssid_, ssid, sizeof(ssid_) - 1);
  strncpy(pass_, pass, sizeof(pass_) - 1);
  reconnect_ = true;
  wifiSinceMs_ = millis();
  Serial.printf("wifi: would join '%s' (%d chars of password)\n", ssid_, (int)strlen(pass_));
}
void GeminiLive::taskEntry(void *) {}
void GeminiLive::task() {}
void GeminiLive::ensureWifi() {}
bool GeminiLive::openSession() { return false; }
void GeminiLive::closeSession() {}
void GeminiLive::pumpMic() {}
void GeminiLive::onMessage(const char *, size_t) {}
void GeminiLive::sendAudio(const int16_t *, size_t) {}
void GeminiLive::sendToolResponse(const char *, const char *) {}
void GeminiLive::micSink(void *, const int16_t *, size_t) {}
void GeminiLive::probe() { Serial.println("probe: not on the emulator"); }

#else
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/stream_buffer.h>
#include <mbedtls/base64.h>
#include <time.h>

#include "../../secrets.h"
#include "google_roots.h"

using namespace websockets;

namespace {
WebsocketsClient *ws = nullptr;
GeminiLive *self = nullptr;
StaticStreamBuffer_t micStatic;

// ArduinoJson keeps the parsed message in PSRAM (the audio chunks are big)
struct PsramAllocator : ArduinoJson::Allocator {
  void *allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  void deallocate(void *p) override { heap_caps_free(p); }
  void *reallocate(void *p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
};
PsramAllocator psram;

const char *HOST = "generativelanguage.googleapis.com";
const char *PATH = "/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent?key=";

// scratch buffers in PSRAM
char *txBuf = nullptr;      // JSON with one audio chunk
uint8_t *pcmBuf = nullptr;  // decoded reply audio
const size_t TX_BUF = 16 * 1024;
const size_t PCM_BUF = 96 * 1024;
const size_t CHUNK = 1600;  // 100 ms of microphone per message
}  // namespace

void GeminiLive::setCredentials(const char *ssid, const char *pass) {
  strncpy(ssid_, ssid, sizeof(ssid_) - 1);
  strncpy(pass_, pass, sizeof(pass_) - 1);
  Preferences prefs;
  if (prefs.begin("wifi", false)) {
    prefs.putString("ssid", ssid_);
    prefs.putString("pass", pass_);
    prefs.end();
  }
  reconnect_ = true;
}

bool GeminiLive::begin(Player *player, Listener *listener) {
  player_ = player;
  listener_ = listener;
  self = this;
  // the network: what was typed on the glass, or else secrets.h
  strncpy(ssid_, WIFI_SSID, sizeof(ssid_) - 1);
  strncpy(pass_, WIFI_PASS, sizeof(pass_) - 1);
  Preferences prefs;
  if (prefs.begin("wifi", true)) {
    String s = prefs.getString("ssid", "");
    if (s.length()) {
      strncpy(ssid_, s.c_str(), sizeof(ssid_) - 1);
      strncpy(pass_, prefs.getString("pass", "").c_str(), sizeof(pass_) - 1);
    }
    prefs.end();
  }
  events_ = xQueueCreate(8, sizeof(TalkEvent));
  const size_t bytes = 32000 * sizeof(int16_t);  // two seconds of microphone
  uint8_t *storage = (uint8_t *)ps_malloc(bytes);
  if (storage) mic_ = xStreamBufferCreateStatic(bytes, 1, storage, &micStatic);
  txBuf = (char *)ps_malloc(TX_BUF);
  pcmBuf = (uint8_t *)ps_malloc(PCM_BUF);
  listener_->setSink(micSink, this);
  listener_->setSinkEnabled(true);  // micSink itself drops everything while no session is open
  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) {
    if (self) self->lastReason_ = info.wifi_sta_disconnected.reason;
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  xTaskCreatePinnedToCore(taskEntry, "gemini", 24 * 1024, this, 3, nullptr, 0);
  return true;
}

void GeminiLive::start() {
  if (active_) return;
  want_ = true;
}

void GeminiLive::stop() { want_ = false; }

bool GeminiLive::takeEvent(TalkEvent &e) {
  if (!events_) return false;
  return xQueueReceive((QueueHandle_t)events_, &e, 0) == pdTRUE;
}

void GeminiLive::emit(uint8_t kind, const char *name, const char *text) {
  TalkEvent e;
  e.kind = kind;
  strncpy(e.name, name, sizeof(e.name) - 1);
  strncpy(e.text, text, sizeof(e.text) - 1);
  xQueueSend((QueueHandle_t)events_, &e, 0);
}

bool GeminiLive::wifiConnected() const { return WiFi.status() == WL_CONNECTED; }
const char *GeminiLive::wifiInfo() const { return wifiInfo_; }

// ---- the microphone comes in from the Listener's task -------------------------------------
void GeminiLive::micSink(void *arg, const int16_t *pcm, size_t n) {
  GeminiLive *g = (GeminiLive *)arg;
  if (!g->mic_ || !g->connected_) return;
  xStreamBufferSend((StreamBufferHandle_t)g->mic_, pcm, n * sizeof(int16_t), 0);
}

// ---- Wi-Fi --------------------------------------------------------------------------------
void GeminiLive::ensureWifi() {
  if (reconnect_) {
    reconnect_ = false;
    wifiStarted_ = false;
    WiFi.disconnect(false, true);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  if (WiFi.status() == WL_CONNECTED) return;
  if (strlen(ssid_) == 0) {
    strcpy(wifiInfo_, "sin red");
    return;
  }
  // a failed attempt is not always retried by the driver (wrong password, router asleep):
  // start again every 15 s
  if (wifiStarted_ && millis() - wifiSinceMs_ > 15000) {
    WiFi.disconnect(false, false);
    vTaskDelay(pdMS_TO_TICKS(200));
    wifiStarted_ = false;
  }
  if (!wifiStarted_) {
    wifiStarted_ = true;
    wifiSinceMs_ = millis();
    lastReason_ = 0;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    // routers in WPA/WPA2 mixed mode are refused with the default minimum (WPA2): reason 210
    WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
    // several access points with the same name (mesh, repeater): take the strongest
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    WiFi.begin(ssid_, pass_);
    Serial.printf("wifi: joining %s\n", ssid_);
  }
  // what the router answered last time
  uint8_t r = lastReason_;
  // 210 is what a WPA/WPA2 mixed router gives for a wrong password (seen on David's router)
  if (r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || r == WIFI_REASON_HANDSHAKE_TIMEOUT ||
      r == WIFI_REASON_MIC_FAILURE || r == WIFI_REASON_802_1X_AUTH_FAILED ||
      r == WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY)
    strcpy(wifiInfo_, "clave erronea");
  else if (r == WIFI_REASON_NO_AP_FOUND ||
           r == WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD || r == WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD)
    strcpy(wifiInfo_, "no veo la red");
  else if (r)
    snprintf(wifiInfo_, sizeof(wifiInfo_), "falla (%d)", (int)r);
  else
    strcpy(wifiInfo_, "conectando");
}

void GeminiLive::requestScan() {
  scanCount_ = 0;
  scanState_ = 1;
}

int GeminiLive::scanResult(WifiNet *out, int max) {
  if (scanState_ != 2) return -1;
  int n = scanCount_ < max ? scanCount_ : max;
  memcpy(out, scanNets_, n * sizeof(WifiNet));
  return n;
}

// Runs in the Wi-Fi task. Stops the connection attempts while it scans, then resumes them.
void GeminiLive::doScan() {
  bool wasUp = WiFi.status() == WL_CONNECTED;
  if (!wasUp) {
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    vTaskDelay(pdMS_TO_TICKS(300));
  }
  WiFi.mode(WIFI_STA);
  int n = -1;
  for (int attempt = 0; attempt < 3 && n < 0; attempt++) {
    WiFi.scanDelete();
    n = WiFi.scanNetworks(false, false, false, 300);
    Serial.printf("wifi: scan %d -> %d networks\n", attempt + 1, n);
    if (n < 0) vTaskDelay(pdMS_TO_TICKS(500));
  }
  int count = 0;
  for (int i = 0; i < n && count < 20; i++) {
    String ssid = WiFi.SSID(i);
    Serial.printf("wifi:   %-24s %4d dBm  ch %d  auth %d  %s\n", ssid.c_str(), (int)WiFi.RSSI(i), (int)WiFi.channel(i), (int)WiFi.encryptionType(i), WiFi.BSSIDstr(i).c_str());
    if (ssid.length() == 0) continue;
    bool dup = false;  // the same name on several access points: keep the strongest
    for (int k = 0; k < count; k++)
      if (ssid == scanNets_[k].ssid) {
        dup = true;
        if (WiFi.RSSI(i) > scanNets_[k].rssi) scanNets_[k].rssi = (int8_t)WiFi.RSSI(i);
      }
    if (dup) continue;
    strncpy(scanNets_[count].ssid, ssid.c_str(), 32);
    scanNets_[count].ssid[32] = 0;
    scanNets_[count].rssi = (int8_t)WiFi.RSSI(i);
    scanNets_[count].open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    count++;
  }
  WiFi.scanDelete();
  scanCount_ = count;
  scanState_ = 2;
  if (!wasUp) {
    WiFi.setAutoReconnect(true);
    wifiStarted_ = false;  // connect again right away
  }
}

// ---- the session --------------------------------------------------------------------------
void GeminiLive::probe() {
  Serial.printf("probe: time=%ld wifi=%s ip=%s heap=%u\n", (long)time(nullptr), WiFi.isConnected() ? "up" : "down",
                WiFi.localIP().toString().c_str(), (unsigned)ESP.getFreeHeap());
  IPAddress ip;
  bool dns = WiFi.hostByName(HOST, ip);
  Serial.printf("probe: dns %s -> %s\n", HOST, dns ? ip.toString().c_str() : "FAILED");
  if (!dns) return;
  WiFiClientSecure c;
  c.setCACert(GOOGLE_ROOTS);
  c.setTimeout(10);
  uint32_t t0 = millis();
  bool ok = c.connect(HOST, 443);
  Serial.printf("probe: tls %s in %lu ms\n", ok ? "ok" : "FAILED", (unsigned long)(millis() - t0));
  if (!ok) {
    char buf[120];
    int e = c.lastError(buf, sizeof(buf));
    Serial.printf("probe: error %d %s\n", e, buf);
    return;
  }
  String req = String("GET ") + PATH + GEMINI_API_KEY + " HTTP/1.1\r\n";
  req += String("Host: ") + HOST + "\r\n";
  req += "Upgrade: websocket\r\nConnection: Upgrade\r\n";
  req += "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
  c.print(req);
  t0 = millis();
  while (!c.available() && millis() - t0 < 8000) delay(10);
  int n = 0;
  while (c.available() && n < 14) {
    String l = c.readStringUntil('\n');
    l.trim();
    if (l.length()) Serial.printf("probe: < %s\n", l.c_str());
    n++;
  }
  c.stop();
}

bool GeminiLive::openSession() {
  if (!ws) {
    ws = new WebsocketsClient();
    ws->setCACert(GOOGLE_ROOTS);  // Google's roots; needs the clock set (NTP) to check them
    ws->onMessage([](WebsocketsMessage m) {
      if (self) self->onMessage(m.data().c_str(), m.data().length());
    });
    ws->onEvent([](WebsocketsEvent ev, String data) {
      if (ev != WebsocketsEvent::ConnectionClosed || !self) return;
      self->connected_ = false;
      // a close frame carries a 2-byte code and then the reason, e.g. "quota exceeded"
      if (data.length() > 2) {
        strncpy(self->closeText_, data.c_str() + 2, sizeof(self->closeText_) - 1);
        Serial.printf("gemini: closed: %s\n", self->closeText_);
      }
    });
  }
  String url = String("wss://") + HOST + PATH + GEMINI_API_KEY;
  emit(TE_CONNECTING, "", "");
  closeText_[0] = 0;
  // the certificate check needs the real date: wait for NTP a little, if it is not there yet
  for (int i = 0; i < 50 && time(nullptr) < 1700000000; i++) vTaskDelay(pdMS_TO_TICKS(100));
  if (time(nullptr) < 1700000000) Serial.println("gemini: clock not set yet, the certificate may fail");
  if (!ws->connect(url)) {
    emit(TE_FAILED, "", "could not reach Gemini");
    return false;
  }
  // the setup message
  JsonDocument doc(&psram);
  JsonObject setup = doc["setup"].to<JsonObject>();
  setup["model"] = String("models/") + GEMINI_MODEL;
  JsonObject gen = setup["generationConfig"].to<JsonObject>();
  gen["responseModalities"].add("AUDIO");
  gen["speechConfig"]["voiceConfig"]["prebuiltVoiceConfig"]["voiceName"] = voice_;
  // no languageCode: the native-audio models refuse it (1007) and follow the prompt
  setup["systemInstruction"]["parts"][0]["text"] = prompt_;
  JsonDocument toolsDoc(&psram);
  if (deserializeJson(toolsDoc, tools_) == DeserializationError::Ok) setup["tools"][0]["functionDeclarations"] = toolsDoc.as<JsonArray>();
  setup["outputAudioTranscription"].to<JsonObject>();
  setup["inputAudioTranscription"].to<JsonObject>();
  String msg;
  serializeJson(doc, msg);
  setupDone_ = false;
  ws->send(msg);
  uint32_t t0 = millis();
  while (!setupDone_ && millis() - t0 < 10000 && ws->available()) {
    ws->poll();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (!setupDone_) {
    if (closeText_[0]) {
      char why[120];
      snprintf(why, sizeof(why), "Gemini: %.100s", closeText_);
      emit(TE_FAILED, "", why);
    } else {
      emit(TE_FAILED, "", "Gemini did not accept the setup");
    }
    ws->close();
    return false;
  }
  xStreamBufferReset((StreamBufferHandle_t)mic_);
  connected_ = true;
  startedMs_ = lastServerMs_ = lastVoiceMs_ = millis();
  emit(TE_CONNECTED, "", "");
  // a first turn so it greets right away (a text, as if the user had said it)
  JsonDocument hello(&psram);
  JsonObject turn = hello["clientContent"]["turns"].add<JsonObject>();
  turn["role"] = "user";
  char first[200];
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  if (now > 1700000000) {
    static const char *DAYS[] = {"domingo", "lunes", "martes", "miercoles", "jueves", "viernes", "sabado"};
    snprintf(first, sizeof(first),
             "(te acaban de activar. Ahora son las %02d:%02d del %s %d/%d/%d. "
             "Saluda en una frase corta y pregunta que quiero)",
             lt.tm_hour, lt.tm_min, DAYS[lt.tm_wday], lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900);
  } else {
    snprintf(first, sizeof(first), "(te acaban de activar: saluda en una frase corta y pregunta que quiero)");
  }
  turn["parts"][0]["text"] = first;
  hello["clientContent"]["turnComplete"] = true;
  String hi;
  serializeJson(hello, hi);
  ws->send(hi);
  return true;
}

void GeminiLive::closeSession() {
  if (ws && ws->available()) ws->close();
  connected_ = false;
  player_->clear();
  emit(TE_CLOSED, "", "");
}

void GeminiLive::sendAudio(const int16_t *pcm, size_t n) {
  size_t olen = 0;
  const char *head = "{\"realtimeInput\":{\"audio\":{\"mimeType\":\"audio/pcm;rate=16000\",\"data\":\"";
  const char *tail = "\"}}}";
  size_t hl = strlen(head);
  memcpy(txBuf, head, hl);
  if (mbedtls_base64_encode((unsigned char *)txBuf + hl, TX_BUF - hl - 8, &olen, (const unsigned char *)pcm, n * 2) != 0) return;
  memcpy(txBuf + hl + olen, tail, strlen(tail) + 1);
  ws->send(txBuf, hl + olen + strlen(tail));
}

void GeminiLive::sendToolResponse(const char *id, const char *name) {
  JsonDocument doc(&psram);
  JsonObject r = doc["toolResponse"]["functionResponses"].add<JsonObject>();
  r["id"] = id;
  r["name"] = name;
  r["response"]["result"] = "ok";
  String msg;
  serializeJson(doc, msg);
  ws->send(msg);
}

// Sends the microphone up, 100 ms at a time, unless the robot is talking (it would hear itself).
void GeminiLive::pumpMic() {
  static int16_t chunk[CHUNK];
  StreamBufferHandle_t sb = (StreamBufferHandle_t)mic_;
  while (xStreamBufferBytesAvailable(sb) >= CHUNK * sizeof(int16_t)) {
    xStreamBufferReceive(sb, chunk, CHUNK * sizeof(int16_t), 0);
    if (player_->playing()) continue;  // half duplex: dropped while it speaks
    sendAudio(chunk, CHUNK);
  }
  if (listener_->voice()) lastVoiceMs_ = millis();
}

// ---- what Gemini sends ----------------------------------------------------------------------
void GeminiLive::onMessage(const char *data, size_t len) {
  JsonDocument doc(&psram);
  if (deserializeJson(doc, data, len) != DeserializationError::Ok) return;
  if (doc["setupComplete"].is<JsonObject>()) {
    setupDone_ = true;
    return;
  }
  JsonObject sc = doc["serverContent"];
  if (sc) {
    lastServerMs_ = millis();
    if (sc["interrupted"] | false) {
      player_->clear();
      emit(TE_INTERRUPTED, "", "");
    }
    JsonArray parts = sc["modelTurn"]["parts"];
    bool first = !player_->playing();
    for (JsonObject p : parts) {
      const char *b64 = p["inlineData"]["data"];
      if (!b64) continue;
      size_t olen = 0;
      if (mbedtls_base64_decode(pcmBuf, PCM_BUF, &olen, (const unsigned char *)b64, strlen(b64)) != 0) continue;
      if (first) {
        emit(TE_AUDIO, "", "");
        first = false;
      }
      player_->push24k((const int16_t *)pcmBuf, olen / 2);
    }
    const char *heard = sc["inputTranscription"]["text"];
    if (heard && *heard) Serial.printf("you: %s\n", heard);
    const char *tr = sc["outputTranscription"]["text"];
    if (tr && *tr) emit(TE_TEXT, "", tr);
    if (sc["turnComplete"] | false) emit(TE_TURN_DONE, "", "");
    return;
  }
  JsonArray calls = doc["toolCall"]["functionCalls"];
  if (calls) {
    for (JsonObject c : calls) {
      const char *name = c["name"] | "";
      const char *id = c["id"] | "";
      char arg[120] = {0};
      JsonObject args = c["args"];
      for (JsonPair kv : args) {
        if (kv.value().is<const char *>()) {
          strncpy(arg, kv.value().as<const char *>(), sizeof(arg) - 1);
          break;
        }
        if (kv.value().is<float>()) {
          snprintf(arg, sizeof(arg), "%g", kv.value().as<float>());
          break;
        }
      }
      emit(TE_TOOL, name, arg);
      sendToolResponse(id, name);
    }
    return;
  }
  if (doc["goAway"].is<JsonObject>()) want_ = false;
}

// ---- the task ---------------------------------------------------------------------------------
void GeminiLive::taskEntry(void *arg) { ((GeminiLive *)arg)->task(); }

void GeminiLive::task() {
  bool clockSet = false;
  for (;;) {
    if (scanState_ == 1 && !active_) doScan();
    ensureWifi();
    if (WiFi.status() == WL_CONNECTED) {
      if (wifiInfo_[0] != '1' && wifiInfo_[0] != '2') snprintf(wifiInfo_, sizeof(wifiInfo_), "%s", WiFi.localIP().toString().c_str());
      if (!clockSet) {
        clockSet = true;
        configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
      }
    }

    if (want_ && !active_) {
      active_ = true;
      if (WiFi.status() != WL_CONNECTED) {
        emit(TE_FAILED, "wifi", wifiInfo_);
        active_ = want_ = false;
      } else if (!openSession()) {
        active_ = want_ = false;
      }
    }

    if (active_) {
      if (!want_ || !ws->available()) {
        closeSession();
        active_ = want_ = false;
      } else {
        ws->poll();
        pumpMic();
        uint32_t now = millis();
        bool quiet = now - lastServerMs_ > idleEndMs_ && now - lastVoiceMs_ > idleEndMs_ && !player_->playing();
        if (quiet || now - startedMs_ > TALK_MAX_MS) want_ = false;
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}
#endif
