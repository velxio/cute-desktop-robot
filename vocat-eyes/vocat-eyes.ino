// Desktop robot with a round color face on the Espressif ESP-VoCat v1.2:
// ESP32-S3, 1.85" 360x360 touch LCD, two microphones, a speaker, an IMU, a battery.
//
//   touch + microphones + battery -> Brain (mood, reactions) -> FaceEngine (emotion graph)
//                                                            -> Canvas -> round LCD
//   microphones -> Listener (voice, direction, bangs) -> Sensors
//               -> WakeWord ("Hi ESP") -> Talk -> GeminiLive <-> Player -> speaker
//
// The same sketch runs on Windows through emulator/ (see README.md).
#include <Wire.h>

#include "Config.h"
#include "src/core/Canvas.h"
#include "src/face/FaceEngine.h"
#include "src/hw/VocatLcd.h"
#include "src/brain/Sensors.h"
#include "src/brain/Brain.h"
#include "src/brain/InfoScreen.h"
#include "src/audio/Codec.h"
#include "src/audio/AudioIo.h"
#include "src/audio/Player.h"
#include "src/audio/Listener.h"
#include "src/audio/WakeWord.h"
#include "src/voice/GeminiLive.h"
#include "src/voice/Talk.h"
#include "src/ui/WifiSetup.h"
#include "src/ui/MicIcon.h"
#include "src/ui/Settings.h"

#ifndef EMULATOR
#include <esp_random.h>
#endif

Canvas canvas;
VocatLcd lcd;
FaceEngine face;
Sensors sensors;
Brain brain;
Codec codec;
AudioIo audio;
Player player;
Listener listener;
WakeWord wakeWord;
GeminiLive gemini;
Talk talk;
WifiSetup wifiSetup;
Settings settings;
SettingsScreen settingsScreen;

static uint32_t nextFrame = 0;
static uint32_t infoSince = 0;
static bool wasInfo = false;
static float light = 0.0f;  // backlight, smoothed

// ---- serial console -----------------------------------------------------------------
//   <expression>      go to that face (try: happy, love, furious, asleep ...)
//   act <name>        play an act (try: rainbow, aquarium, pacman, radar ...)
//   say <text>        speech balloon          list    names of faces and acts
//   auto on|off       autonomous behaviour    info    clock and battery screen
//   time HH:MM        set the clock           status  what the robot feels
//   voice <deg>       pretend somebody talks from that side (-90 left .. 90 right)
//   noise             pretend a bang          wake    pretend it heard "Hi ESP"
//   talk / stop       start / end a conversation with Gemini
//   vol <0-100>       speaker volume          mics    level of each microphone slot
//   chirp <n>         play chirp n            wifi    network status
//   wifi setup        choose a network and type its password on the glass
//   settings          open the settings screen
//   scan              list the networks around (name, signal, channel)
//   wifi set <ssid> <password>   save a network from here (connects right away)
//   net               test DNS, TLS and the Gemini handshake, step by step
//   mem               free internal RAM and PSRAM
static void runCommand(char *line) {
  while (*line == ' ') line++;
  if (!*line) return;

  if (!strncmp(line, "act ", 4)) {
    int i = Acts::find(line + 4);
    if (i < 0) Serial.println("unknown act");
    else brain.forceAct(i);
  } else if (!strncmp(line, "say ", 4)) {
    brain.say(line + 4);
  } else if (!strcmp(line, "list")) {
    for (int i = 0; i < EX_COUNT; i++) Serial.println(Expressions::name(i));
    for (int i = 0; i < Acts::count(); i++) {
      Serial.print("act ");
      Serial.println(Acts::get(i).name);
    }
  } else if (!strncmp(line, "auto ", 5)) {
    brain.setAutonomous(!strcmp(line + 5, "on"));
  } else if (!strcmp(line, "info")) {
    brain.requestInfo();
  } else if (!strncmp(line, "voice", 5)) {
    sensors.noteVoice((float)atof(line + 5), 0.7f);
  } else if (!strcmp(line, "noise")) {
    sensors.noteNoise(0.0f, 1.0f);
  } else if (!strcmp(line, "wake")) {
    sensors.noteWakeWord(0.0f);
  } else if (!strcmp(line, "talk")) {
    talk.start("console");
  } else if (!strcmp(line, "stop")) {
    talk.stop();
  } else if (!strncmp(line, "vol ", 4)) {
    settings.volume = (uint8_t)atoi(line + 4);
    settings.save();
    codec.setVolume(settings.volume);
    player.chirp(CH_BLIP);
  } else if (!strncmp(line, "chirp ", 6)) {
    player.chirp((uint8_t)atoi(line + 6));
  } else if (!strcmp(line, "mics")) {
    float l[4];
    listener.slotLevels(l);
    Serial.printf("slots rms: %.4f %.4f %.4f %.4f | level %.4f floor %.4f voice %d dir %.0f\n", l[0], l[1], l[2], l[3],
                  listener.level(), listener.floor(), (int)listener.voice(), listener.direction());
  } else if (!strcmp(line, "net")) {
    gemini.probe();
  } else if (!strcmp(line, "settings")) {
    settingsScreen.open();
  } else if (!strcmp(line, "scan")) {
    gemini.requestScan();  // the list goes to the console
  } else if (!strcmp(line, "wifi setup")) {
    wifiSetup.open();
  } else if (!strncmp(line, "wifi set ", 9)) {
    char *ssid = line + 9;
    char *pass = strchr(ssid, ' ');
    if (pass) *pass++ = 0;
    else pass = (char *)"";
    gemini.setCredentials(ssid, pass);
    Serial.printf("wifi: saved %s, connecting\n", ssid);
  } else if (!strcmp(line, "wifi")) {
    Serial.printf("wifi: %s | talk: %s | wake word: %s\n", gemini.wifiInfo(), talk.stateName(),
                  wakeWord.running() ? "on" : "off");
  } else if (!strncmp(line, "time ", 5)) {
    int h = 0, m = 0;
    if (sscanf(line + 5, "%d:%d", &h, &m) == 2) {
      ClockTime t = sensors.state().clock;
      t.hour = (uint8_t)h;
      t.minute = (uint8_t)m;
      t.second = 0;
      sensors.setClock(t);
    }
  } else if (!strcmp(line, "mem")) {
#ifdef EMULATOR
    Serial.println("mem: not on the emulator");
#else
    Serial.printf("mem: internal %u KB free (largest %u KB), psram %u KB free\n",
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                  (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
#endif
  } else if (!strcmp(line, "status")) {
    const SensorState &s = sensors.state();
    Serial.print("face=");
    Serial.print(Expressions::name(face.current()));
    Serial.print(" mood=");
    Serial.print(Expressions::moodName(brain.mood()));
    Serial.print(" doing=");
    Serial.print(brain.activityName());
    Serial.print(" (");
    Serial.print(brain.reason());
    Serial.print(") social=");
    Serial.print(brain.social());
    Serial.print(" annoy=");
    Serial.print(brain.annoy());
    Serial.print(" | battery=");
    if (s.battery.valid) {
      Serial.print(s.battery.percent);
      Serial.print("% ");
      Serial.print(s.battery.millivolts);
      Serial.print("mV ");
      Serial.print(s.battery.milliamps);
      Serial.print("mA");
      if (s.battery.charging) Serial.print(" charging");
    } else {
      Serial.print("none");
    }
    Serial.print(" sound_dir=");
    Serial.print(s.soundDirValid ? s.soundDir : 0.0f);
    Serial.print(" talk=");
    Serial.print(talk.stateName());
    Serial.print(" wifi=");
    Serial.print(gemini.wifiInfo());
    Serial.print(" clock=");
    Serial.print((int)s.clock.hour);
    Serial.print(":");
    Serial.println((int)s.clock.minute);
  } else {
    uint8_t e = Expressions::find(line);
    if (e == EX_NONE) Serial.println("unknown command (try: list)");
    else brain.force(e, 8000);
  }
}

static void readSerial() {
  static char line[40];
  static uint8_t len = 0;
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      line[len] = 0;
      runCommand(line);
      len = 0;
    } else if (len < sizeof(line) - 1) {
      line[len++] = ch;
    }
  }
}

// Asleep the screen is dim; at night it does not dazzle.
static void updateBacklight(const SensorState &s) {
  float top = BACKLIGHT_MAX * settings.brightness / 100.0f;
  float want = top;
  if (brain.activity() == Brain::SLEEPING) want = fminf(top, BACKLIGHT_ASLEEP);
  else if (brain.night() || s.dark) want = fminf(top, BACKLIGHT_DARK_ROOM);
  light += (want - light) * 0.08f;
  uint8_t level = (uint8_t)(light + 0.5f);
  if (level != lcd.backlight()) lcd.setBacklight(level);
}

// A setting changed on the glass (or at start, for all of them).
static void applySetting(SettingsScreen::Item item) {
  switch (item) {
    case SettingsScreen::VOLUME:
      codec.setVolume(settings.volume);
      player.chirp(CH_BLIP);  // to hear the new level
      break;
    case SettingsScreen::VOICE: gemini.setVoice(settings.voiceName()); break;
    case SettingsScreen::WIFI:
      settingsScreen.close();
      wifiSetup.open();
      break;
    case SettingsScreen::WAKE: wakeWord.setEnabled(settings.wakeWord); break;
    case SettingsScreen::SOUNDS: talk.setSounds(settings.sounds); break;
    case SettingsScreen::AUTONOMOUS: brain.setAutonomous(settings.autonomous); break;
    case SettingsScreen::TALK_END: gemini.setIdleEndMs(settings.talkEndMs()); break;
    default: break;  // brightness: updateBacklight reads it every frame
  }
}

// -------------------------------------------------------------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  if (!canvas.begin()) Serial.println("no memory for the frame buffer");
  if (!lcd.begin()) Serial.println("the display did not answer");
  lcd.setBacklight(0);

  // A different personality on every start.
#ifdef EMULATOR
  uint32_t seed = (uint32_t)micros() * 2654435761u;
#else
  uint32_t seed = esp_random();
#endif

  sensors.begin();
  face.begin(seed, EX_NORMAL);
  brain.begin(&face, seed ^ 0xB4A1u);

  // sound: codecs, I2S, the speaker queue, the microphones, the wake word, Gemini
  settings.load();
  codec.begin();
  codec.setVolume(settings.volume);
  wakeWord.setEnabled(settings.wakeWord);  // before begin: off means ESP-SR is never loaded
  if (!audio.begin()) Serial.println("I2S did not start");
  player.begin(&audio, &codec);
  listener.begin(&audio, &sensors);
  wakeWord.begin(&listener);
  gemini.begin(&player, &listener);
  talk.begin(&brain, &face, &sensors, &player, &listener, &gemini, &wakeWord);
  wifiSetup.begin(&gemini);
  gemini.setVoice(settings.voiceName());
  gemini.setIdleEndMs(settings.talkEndMs());
  talk.setSounds(settings.sounds);
  brain.setAutonomous(settings.autonomous);
  settingsScreen.begin(&settings, applySetting);

  nextFrame = millis();
  Serial.println("robot ready (type 'list')");
}

void loop() {
  sensors.poll();
  readSerial();

  uint32_t now = millis();
  if ((int32_t)(now - nextFrame) < 0) return;
  nextFrame += FRAME_MS;
  if ((int32_t)(now - nextFrame) > 200) nextFrame = now;  // fell behind, resync

  sensors.update(now);
  const SensorState &s = sensors.state();

  // a tap on the clock screen opens the settings; they own the glass until they close
  if (!wifiSetup.active() && !settingsScreen.active() && brain.showInfo() && s.gesture == G_TAP) {
    settingsScreen.open();
  } else if (settingsScreen.active()) {
    settingsScreen.setWifiState(gemini.wifiConnected());
    settingsScreen.update(s, now);  // may close it, or switch to the Wi-Fi setup
  }
  if (settingsScreen.active()) {
    face.update(now);
    settingsScreen.draw(canvas, now);
    lcd.push(canvas);
    updateBacklight(s);
    talk.update(now);
    return;
  }
  if (wifiSetup.active()) {
    wifiSetup.update(s, now);
    face.update(now);
    if (wifiSetup.active()) {
      wifiSetup.draw(canvas, now);
      lcd.push(canvas);
      updateBacklight(s);
      talk.update(now);  // a conversation keeps running behind the setup screen
      return;
    }
  }

  // a tap on the microphone icon starts or ends the conversation; the Brain does not see it
  if (s.gesture == G_TAP && !brain.showInfo() && MicIcon::hit(s.gestureX, s.gestureY)) {
    if (talk.active()) talk.stop();
    else talk.start("the microphone icon");
    SensorState quiet = s;
    quiet.gesture = G_NONE;
    brain.update(quiet, now);
  } else {
    brain.update(s, now);
  }
  face.update(now);

  bool info = brain.showInfo();
  if (info && !wasInfo) infoSince = now;
  wasInfo = info;
  if (info) InfoScreen::draw(canvas, s, (now - infoSince) * 0.001f, gemini.wifiInfo(), gemini.wifiConnected());
  else {
    face.render(canvas);
    MicIcon::draw(canvas, talk.state(), now);
  }
  lcd.push(canvas);
  updateBacklight(s);

  talk.update(now);  // conversation, wake word, pads; plays the chirps of the face
}
