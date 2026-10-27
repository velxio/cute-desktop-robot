#include "Sensors.h"

#include <Wire.h>

#include "../../Config.h"
#include "../core/Canvas.h"

void Sensors::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_HZ);
  touch_.begin();
  rtc_.begin();
  battery_.begin();
}

void Sensors::poll() {}

void Sensors::noteNoise(float dirDeg, float level) {
  latchDir_ = dirDeg;
  latchLevel_ = level;
  noiseLatch_ = true;
}

void Sensors::noteVoice(float dirDeg, float level) {
  latchDir_ = dirDeg;
  latchLevel_ = level;
  voiceLatch_ = true;
}

void Sensors::noteWakeWord(float dirDeg) {
  latchDir_ = dirDeg;
  wakeLatch_ = true;
}

void Sensors::updateTouch(uint32_t nowMs) {
  TouchPoint p = touch_.read();
  st_.gesture = G_NONE;
  st_.touching = p.down;
  if (p.down) {
    float x = Canvas::fromPxX((float)p.x), y = Canvas::fromPxY((float)p.y);
    st_.touchX = x;
    st_.touchY = y;
    if (!down_) {
      down_ = true;
      moved_ = held_ = false;
      downAt_ = nowMs;
      x0_ = lastX_ = x;
      y0_ = lastY_ = y;
      travel_ = 0.0f;
    }
    float dx = x - lastX_, dy = y - lastY_;
    travel_ += sqrtf(dx * dx + dy * dy);
    lastX_ = x;
    lastY_ = y;
    float fx = x - x0_, fy = y - y0_;
    float limit = MOVE_PX / FU_PX;
    if (fx * fx + fy * fy > limit * limit) moved_ = true;
    if (!moved_ && !held_ && nowMs - downAt_ >= HOLD_MS) {
      held_ = true;
      st_.gesture = G_HOLD;
      st_.gestureX = x0_;
      st_.gestureY = y0_;
    }
    return;
  }
  if (!down_) return;
  // the finger left the glass
  down_ = false;
  st_.gestureX = x0_;
  st_.gestureY = y0_;
  st_.strokeLength = travel_;
  if (moved_) st_.gesture = G_STROKE;
  else if (!held_) st_.gesture = G_TAP;
}

void Sensors::update(uint32_t nowMs) {
  if (!started_) {
    started_ = true;
    nextClock_ = nowMs;
    nextBattery_ = nowMs + 500;
  }

  updateTouch(nowMs);

  // ---- sound: the audio module left its notes in the latches
  st_.sound = st_.voice = st_.wakeWord = false;
  if (noiseLatch_) {
    noiseLatch_ = false;
    if ((int32_t)(nowMs - noiseMuteUntil_) >= 0) {
      st_.sound = true;
      noiseMuteUntil_ = nowMs + 700;  // one event per bang
      st_.soundDir = latchDir_;
      st_.soundDirValid = true;
      st_.soundLevel = latchLevel_;
    }
  }
  if (voiceLatch_) {
    voiceLatch_ = false;
    if ((int32_t)(nowMs - voiceMuteUntil_) >= 0) {
      st_.voice = true;
      voiceMuteUntil_ = nowMs + 400;  // a direction update at most every 0.4 s
      st_.soundDir = latchDir_;
      st_.soundDirValid = true;
    }
    st_.soundLevel += (latchLevel_ - st_.soundLevel) * 0.5f;
  } else {
    st_.soundLevel *= 0.9f;
  }
  if (wakeLatch_) {
    wakeLatch_ = false;
    st_.wakeWord = true;
    st_.soundDir = latchDir_;
    st_.soundDirValid = true;
  }

  // ---- clock, once per second
  if ((int32_t)(nowMs - nextClock_) >= 0) {
    nextClock_ = nowMs + 1000;
    st_.clock = rtc_.read();
  }

  // ---- battery, every 2 seconds
  if ((int32_t)(nowMs - nextBattery_) >= 0) {
    nextBattery_ = nowMs + 2000;
    BatteryState b = battery_.read();
    if (b.valid || !st_.battery.valid) st_.battery = b;
    st_.batteryLow = st_.battery.valid && !st_.battery.charging && st_.battery.percent <= BATTERY_LOW_PCT;
  }
}
