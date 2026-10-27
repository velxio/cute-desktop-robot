#include "Talk.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "../../Config.h"
#include "../audio/Listener.h"
#include "../audio/Player.h"
#include "../audio/WakeWord.h"
#include "../brain/Brain.h"
#include "../brain/Sensors.h"
#include "../face/FaceEngine.h"
#include "GeminiLive.h"

// Who the robot is, for Gemini. One line, JSON-safe (no double quotes).
static const char *PROMPT =
    "Eres " ROBOT_NAME ", un pequeno robot de escritorio con una cara redonda de ojos expresivos. "
    "Hablas en espanol, con frases cortas y naturales, simpatico y un poco juguetón, como una mascota curiosa. "
    "No tienes brazos ni piernas: solo ojos, dos microfonos, un altavoz y una bateria. "
    "Cuando algo te emocione, usa la herramienta set_face para poner la cara que toca (por ejemplo happy, "
    "surprised, love, sad, thinking, laugh, curious) y play_act para gestos (yes, no). "
    "Sabes la hora y la fecha porque te las digo al empezar; si te preguntan la bateria, di que mantengan el dedo en tu pantalla para verla. "
    "Responde siempre con voz, breve.";

// Tools Gemini may call (JSON array of function declarations).
static const char *TOOLS =
    "[{\"name\":\"set_face\",\"description\":\"Pone una expresion en la cara del robot.\","
    "\"parameters\":{\"type\":\"OBJECT\",\"properties\":{\"name\":{\"type\":\"STRING\","
    "\"description\":\"happy, glee, excited, love, laugh, proud, shy, surprised, curious, thinking, "
    "attentive, listening, sad, worried, crying, angry, annoyed, scared, dizzy, sleepy, bored, confused, wink\"}},"
    "\"required\":[\"name\"]}},"
    "{\"name\":\"play_act\",\"description\":\"Reproduce un gesto o animacion corta.\","
    "\"parameters\":{\"type\":\"OBJECT\",\"properties\":{\"name\":{\"type\":\"STRING\","
    "\"description\":\"yes, no, rainbow, hearts, pacman, aquarium, radar, snore\"}},\"required\":[\"name\"]}},"
    "{\"name\":\"look\",\"description\":\"Mira hacia un lado.\","
    "\"parameters\":{\"type\":\"OBJECT\",\"properties\":{\"direction\":{\"type\":\"STRING\","
    "\"description\":\"left, right, up, down, center\"}},\"required\":[\"direction\"]}},"
    "{\"name\":\"show_text\",\"description\":\"Muestra un texto muy corto (max 12 letras) en un bocadillo.\","
    "\"parameters\":{\"type\":\"OBJECT\",\"properties\":{\"text\":{\"type\":\"STRING\"}},\"required\":[\"text\"]}}]";

void Talk::begin(Brain *brain, FaceEngine *face, Sensors *sensors, Player *player, Listener *listener,
                 GeminiLive *gemini, WakeWord *wake) {
  brain_ = brain;
  face_ = face;
  sensors_ = sensors;
  player_ = player;
  listener_ = listener;
  gemini_ = gemini;
  wake_ = wake;
  gemini_->setSystemPrompt(PROMPT);
  gemini_->setTools(TOOLS);
}

const char *Talk::stateName() const {
  static const char *N[] = {"off", "connecting", "listening", "speaking"};
  return N[state_];
}

void Talk::setFace(uint8_t expr, uint32_t holdMs) {
  if (expr < EX_COUNT) brain_->force(expr, holdMs);
}

void Talk::start(const char *why) {
  if (active()) return;
  Serial.printf("talk: start (%s)\n", why);
  state_ = CONNECTING;
  text_[0] = 0;
  wake_->pause();
  setFace(EX_ATTENTIVE, 15000);
  gemini_->start();
}

void Talk::stop() {
  if (!active()) return;
  gemini_->stop();
}

void Talk::onTool(const char *name, const char *arg) {
  Serial.printf("talk: tool %s(%s)\n", name, arg);
  if (!strcmp(name, "set_face")) {
    uint8_t e = Expressions::find(arg);
    if (e != EX_NONE) {
      setFace(e, 8000);
      toolFaceUntil_ = millis() + 8000;
    }
  } else if (!strcmp(name, "play_act")) {
    int a = Acts::find(arg);
    if (a >= 0) {
      brain_->forceAct(a);
      toolFaceUntil_ = millis() + 4000;
    }
  } else if (!strcmp(name, "look")) {
    float x = 0, y = 0;
    if (!strcmp(arg, "left")) x = -1;
    else if (!strcmp(arg, "right")) x = 1;
    else if (!strcmp(arg, "up")) y = 1;
    else if (!strcmp(arg, "down")) y = -1;
    face_->lookAt(x, y, 2500);
  } else if (!strcmp(name, "show_text")) {
    brain_->say(arg);
  }
}

#ifdef EMULATOR
void Talk::checkPads(uint32_t) {}
#else
// The two capacitive pads on the shell: a touch starts or ends the conversation.
void Talk::checkPads(uint32_t nowMs) {
  const uint8_t pins[2] = {PIN_TOUCH_PAD1, PIN_TOUCH_PAD2};
  uint32_t v[2] = {touchRead(pins[0]), touchRead(pins[1])};
  if (padSamples_ < 20) {
    // learn the untouched value during the first second
    for (int i = 0; i < 2; i++) padBase_[i] = padSamples_ == 0 ? v[i] : (padBase_[i] * 7 + v[i]) / 8;
    padSamples_++;
    return;
  }
  for (int i = 0; i < 2; i++) {
    bool down = v[i] > padBase_[i] * PAD_TOUCH_RATIO;
    if (down && !padDown_[i] && nowMs - padAt_ > 600) {
      padAt_ = nowMs;
      if (active()) stop();
      else start(i == 0 ? "pad 1" : "pad 2");
    }
    if (!down) padBase_[i] = (padBase_[i] * 63 + v[i]) / 64;  // drifts with temperature
    padDown_[i] = down;
  }
}
#endif

void Talk::update(uint32_t nowMs) {
  static uint32_t padNext = 0;
  if ((int32_t)(nowMs - padNext) >= 0) {
    padNext = nowMs + 50;
    checkPads(nowMs);
  }

  // the robot must not listen to itself
  listener_->setMuted(player_->playing());

  // somebody said its name
  if (wake_->takeDetected()) sensors_->noteWakeWord(listener_->direction());
  if (sensors_->state().wakeWord && !active()) start("wake word");

  // what Gemini's task reports
  TalkEvent e;
  while (gemini_->takeEvent(e)) {
    switch (e.kind) {
      case TE_CONNECTED:
        state_ = LISTENING;
        setFace(EX_LISTENING, 5000);
        break;
      case TE_AUDIO:
        state_ = SPEAKING;
        if (nowMs > toolFaceUntil_) setFace(EX_ATTENTIVE, 5000);
        break;
      case TE_INTERRUPTED:
        state_ = LISTENING;
        setFace(EX_SURPRISED, 1200);
        break;
      case TE_TURN_DONE:
        break;
      case TE_TOOL:
        onTool(e.name, e.text);
        break;
      case TE_TEXT:
        strncpy(text_, e.text, sizeof(text_) - 1);
        Serial.printf("talk: \"%s\"\n", e.text);
        break;
      case TE_CLOSED:
        state_ = OFF;
        brain_->release();
        wake_->resume();
        Serial.println("talk: ended");
        break;
      case TE_FAILED:
        state_ = OFF;
        Serial.printf("talk: failed: %s\n", e.text);
        brain_->say(!strcmp(e.name, "wifi") ? "SIN WIFI" : "ERROR GEMINI");
        setFace(EX_SAD, 3000);
        wake_->resume();
        break;
      default:
        break;
    }
  }

  if (!active()) {
    // the chirps of the face come out of the speaker
    uint8_t c = brain_->takeChirp();
    if (c != CH_NONE && sounds_) player_->chirp(c);
    return;
  }
  brain_->takeChirp();

  // it finished talking: back to listening
  if (state_ == SPEAKING && !player_->playing()) {
    state_ = LISTENING;
    if (nowMs > toolFaceUntil_) setFace(EX_LISTENING, 5000);
  }
  // keep the face while the conversation lasts (a forced face expires)
  if ((int32_t)(nowMs - faceRefresh_) >= 0) {
    faceRefresh_ = nowMs + 3000;
    if (nowMs > toolFaceUntil_ && !face_->actActive()) {
      if (state_ == LISTENING) setFace(EX_LISTENING, 5000);
      else if (state_ == SPEAKING) setFace(EX_ATTENTIVE, 5000);
      else if (state_ == CONNECTING) setFace(EX_THINKING, 5000);
    }
  }
}
