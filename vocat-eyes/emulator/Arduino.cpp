#include "Arduino.h"

#include "EmuHw.h"

EmuHw emuHw;
HardwareSerial Serial;

static bool pinOk(int pin) { return pin >= 0 && pin < EMU_PINS; }

// ---- time: the simulated clock, so the emulator can pause or fast-forward -------
unsigned long millis() { return (unsigned long)emuHw.clockMs; }
unsigned long micros() { return (unsigned long)(emuHw.clockMs * 1000.0); }
void delay(unsigned long ms) { emuHw.clockMs += (double)ms; }
void delayMicroseconds(unsigned int us) { emuHw.clockMs += us / 1000.0; }
void yield() {}

// ---- pins ---------------------------------------------------------------------------
void pinMode(int, int) {}
int digitalRead(int pin) { return pinOk(pin) ? emuHw.digitalIn[pin] : LOW; }
void digitalWrite(int pin, int value) {
  if (pinOk(pin)) emuHw.digitalOut[pin] = value;
}
int analogRead(int pin) {
  if (!pinOk(pin)) return 0;
  // a real ADC is never perfectly still
  int v = emuHw.analogIn[pin] + (rand() % 25) - 12;
  return v < 0 ? 0 : (v > 4095 ? 4095 : v);
}

// ---- misc ---------------------------------------------------------------------------
long random(long howbig) { return howbig > 0 ? rand() % howbig : 0; }
long random(long howsmall, long howbig) { return howbig > howsmall ? howsmall + rand() % (howbig - howsmall) : howsmall; }
void randomSeed(unsigned long seed) { srand((unsigned)seed); }
long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ---- LEDC: servos (50 Hz) and the buzzer (tone) -----------------------------------------
bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution) {
  if (!pinOk(pin)) return false;
  EmuPwm &p = emuHw.pwm[pin];
  p.attached = p.everUsed = true;
  p.freq = freq;
  p.bits = resolution;
  p.duty = 0;
  return true;
}
bool ledcDetach(uint8_t pin) {
  if (!pinOk(pin)) return false;
  emuHw.pwm[pin].attached = false;
  emuHw.pwm[pin].tone = 0;
  return true;
}
bool ledcWrite(uint8_t pin, uint32_t duty) {
  if (!pinOk(pin) || !emuHw.pwm[pin].attached) return false;
  emuHw.pwm[pin].duty = duty;
  if (duty == 0) emuHw.pwm[pin].tone = 0;
  return true;
}
uint32_t ledcWriteTone(uint8_t pin, uint32_t freq) {
  if (!pinOk(pin) || !emuHw.pwm[pin].attached) return 0;
  emuHw.pwm[pin].tone = freq;
  return freq;
}

int emuServoAngle(int pin) {
  if (!pinOk(pin) || !emuHw.pwm[pin].everUsed || emuHw.pwm[pin].freq != 50) return -1;
  const EmuPwm &p = emuHw.pwm[pin];
  double us = (double)p.duty / (double)((1u << p.bits) - 1u) * 20000.0;
  // SG90: 500 us = 0 degrees, 2400 us = 180 degrees (same numbers as Config.h)
  int a = (int)lround((us - 500.0) * 180.0 / 1900.0);
  return a < 0 ? 0 : (a > 180 ? 180 : a);
}

// ---- Serial -------------------------------------------------------------------------
void HardwareSerial::begin(unsigned long) {}
int HardwareSerial::available() { return (int)emuHw.serialIn.size(); }
int HardwareSerial::read() {
  if (emuHw.serialIn.empty()) return -1;
  int c = (unsigned char)emuHw.serialIn[0];
  emuHw.serialIn.erase(0, 1);
  return c;
}
void HardwareSerial::out(const char *s) {
  fputs(s, stdout);
  emuHw.serialOut += s;
  if (emuHw.serialOut.size() > 4000) emuHw.serialOut.erase(0, emuHw.serialOut.size() - 2000);
}
size_t HardwareSerial::print(const char *s) {
  out(s);
  return strlen(s);
}
size_t HardwareSerial::print(char c) {
  char b[2] = {c, 0};
  out(b);
  return 1;
}
size_t HardwareSerial::print(int v) {
  char b[24];
  snprintf(b, sizeof(b), "%d", v);
  return print(b);
}
size_t HardwareSerial::print(unsigned int v) {
  char b[24];
  snprintf(b, sizeof(b), "%u", v);
  return print(b);
}
size_t HardwareSerial::print(long v) {
  char b[24];
  snprintf(b, sizeof(b), "%ld", v);
  return print(b);
}
size_t HardwareSerial::print(unsigned long v) {
  char b[24];
  snprintf(b, sizeof(b), "%lu", v);
  return print(b);
}
size_t HardwareSerial::print(float v, int digits) { return print((double)v, digits); }
size_t HardwareSerial::print(double v, int digits) {
  char b[40];
  snprintf(b, sizeof(b), "%.*f", digits, v);
  return print(b);
}
size_t HardwareSerial::println() {
  out("\n");
  fflush(stdout);
  return 1;
}

#include <stdarg.h>
size_t HardwareSerial::printf(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n < 0) return 0;
  out(buf);
  return (size_t)n;
}
