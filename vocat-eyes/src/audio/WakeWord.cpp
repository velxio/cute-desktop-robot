#include "WakeWord.h"

#include <Arduino.h>
#include <string.h>

#include "../../Config.h"
#include "Listener.h"

namespace {
volatile bool detected = false;
}

bool WakeWord::takeDetected() {
  if (!detected) return false;
  detected = false;
  return true;
}

#if defined(EMULATOR) || !WAKE_WORD
bool WakeWord::begin(Listener *) { return false; }
bool WakeWord::launch() { return false; }
void WakeWord::pause() {}
void WakeWord::resume() {}
void WakeWord::setEnabled(bool on) { enabled_ = on; }

#else
#include <esp32-hal-sr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

namespace {
volatile bool stopping = false;  // set before sr_stop(): the feed task must not wait for audio

// ESP-SR asks for `len` bytes of 16 kHz mono; they come from the Listener's stream.
esp_err_t fill(void *arg, void *out, size_t len, size_t *bytesRead, uint32_t timeoutMs) {
  StreamBufferHandle_t sb = (StreamBufferHandle_t)arg;
  size_t got = 0;
  uint32_t start = millis();
  while (got < len && !stopping) {
    got += xStreamBufferReceive(sb, (uint8_t *)out + got, len - got, pdMS_TO_TICKS(100));
    if (got < len && timeoutMs != portMAX_DELAY && millis() - start > timeoutMs) break;
  }
  if (got < len) memset((uint8_t *)out + got, 0, len - got);
  *bytesRead = len;
  return ESP_OK;
}

void onEvent(void *, sr_event_t event, int, int) {
  if (event == SR_EVENT_WAKEWORD) detected = true;
}
}  // namespace

bool WakeWord::begin(Listener *listener) {
  listener_ = listener;
  if (!enabled_) {
    paused_ = true;  // resume() will not start it while it is off
    return false;
  }
  return launch();
}

void WakeWord::setEnabled(bool on) {
  if (on == enabled_) return;
  enabled_ = on;
  if (on) resume();
  else pause();
}

bool WakeWord::launch() {
  void *sb = listener_ ? listener_->wakeStream() : nullptr;
  if (!sb) return false;
  stopping = false;
  xStreamBufferReset((StreamBufferHandle_t)sb);
  uint32_t t0 = millis();
  esp_err_t err = sr_start(fill, sb, SR_CHANNELS_MONO, SR_MODE_WAKEWORD, "M", nullptr, 0, onEvent, nullptr);
  running_ = err == ESP_OK;
  if (!running_) Serial.printf("wake word: sr_start failed (%d)\n", (int)err);
  else Serial.printf("wake word: on (%lu ms, %u KB internal free)\n", (unsigned long)(millis() - t0),
                     (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
  return running_;
}

void WakeWord::pause() {
  if (!running_ || paused_) return;
  paused_ = true;
  stopping = true;
  sr_stop();
  running_ = false;
  detected = false;
  Serial.printf("wake word: off (%u KB internal free)\n", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
}

void WakeWord::resume() {
  if (!paused_ || !enabled_) return;
  paused_ = false;
  launch();
}
#endif
