#include "Rtc.h"

#include <Arduino.h>

#ifdef EMULATOR
#include <math.h>

#include "EmuHw.h"

void Rtc::begin() {}

ClockTime Rtc::read() {
  ClockTime t;
  double s = fmod(emuHw.rtcBase + emuHw.clockMs / 1000.0, 86400.0);
  if (s < 0) s += 86400.0;
  int sec = (int)s;
  t.valid = emuHw.rtcValid;
  t.hour = (uint8_t)(sec / 3600);
  t.minute = (uint8_t)((sec / 60) % 60);
  t.second = (uint8_t)(sec % 60);
  t.day = (uint8_t)emuHw.rtcDay;
  t.month = (uint8_t)emuHw.rtcMonth;
  t.year = (uint16_t)emuHw.rtcYear;
  return t;
}

void Rtc::set(const ClockTime &t) {
  emuHw.rtcBase = t.hour * 3600.0 + t.minute * 60.0 + t.second - emuHw.clockMs / 1000.0;
  emuHw.rtcDay = t.day;
  emuHw.rtcMonth = t.month;
  emuHw.rtcYear = t.year;
  emuHw.rtcValid = true;
}

#else
#include <sys/time.h>
#include <time.h>

void Rtc::begin() {}

ClockTime Rtc::read() {
  ClockTime t;
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  t.valid = set_ || tm.tm_year + 1900 >= 2024;  // NTP sets it without going through set()
  t.hour = (uint8_t)tm.tm_hour;
  t.minute = (uint8_t)tm.tm_min;
  t.second = (uint8_t)tm.tm_sec;
  t.day = (uint8_t)tm.tm_mday;
  t.month = (uint8_t)(tm.tm_mon + 1);
  t.year = (uint16_t)(tm.tm_year + 1900);
  return t;
}

void Rtc::set(const ClockTime &t) {
  struct tm tm = {};
  tm.tm_year = t.year - 1900;
  tm.tm_mon = t.month - 1;
  tm.tm_mday = t.day;
  tm.tm_hour = t.hour;
  tm.tm_min = t.minute;
  tm.tm_sec = t.second;
  struct timeval tv = {mktime(&tm), 0};
  settimeofday(&tv, nullptr);
  set_ = true;
}
#endif
