// The "hardware" of the emulator: what the mocked Arduino libraries read and
// write. The window (main.cpp) shows it and lets you move the inputs.
#ifndef EMU_EMUHW_H
#define EMU_EMUHW_H

#include <stdint.h>
#include <string>

#define EMU_PINS 64
#define EMU_LCD_W 360
#define EMU_LCD_H 360

struct EmuPwm {
  bool attached = false;
  bool everUsed = false;
  uint32_t freq = 0;
  uint8_t bits = 8;
  uint32_t duty = 0;
  uint32_t tone = 0;  // Hz written with ledcWriteTone, 0 = silent
};

struct EmuHw {
  // simulated clock, advanced by the window
  double clockMs = 0.0;

  // ST77916 panel memory, RGB565
  uint16_t lcd[EMU_LCD_W * EMU_LCD_H] = {0};
  uint8_t backlight = 0;
  uint32_t lcdUpdates = 0;
  uint32_t spiBytes = 0;  // total bytes sent to the panel

  // pins
  int digitalIn[EMU_PINS] = {0};
  int digitalOut[EMU_PINS] = {0};
  int analogIn[EMU_PINS] = {0};
  EmuPwm pwm[EMU_PINS];

  // CST816S touch, in screen pixels
  bool touchDown = false;
  int touchX = 180, touchY = 180;

  // clock of the ESP32: seconds since midnight at clockMs = 0, and the date
  double rtcBase = 10 * 3600.0;
  bool rtcValid = true;
  int rtcDay = 22, rtcMonth = 10, rtcYear = 2026;

  // BQ27220 fuel gauge
  bool batteryPresent = true;
  int batteryPct = 80;
  int batteryMv = 3900;
  bool charging = false;

  // characters typed into the "serial monitor"
  std::string serialIn;
  std::string serialOut;
};

extern EmuHw emuHw;

// Servo angle 0..180 read from the PWM of a pin, -1 when it never got pulses.
int emuServoAngle(int pin);

#endif
