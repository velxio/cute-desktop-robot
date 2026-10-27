// Data model of the procedural face.
//
// An eye is a superellipse cut by an upper and a lower eyelid. Every number is
// a plain float so any two faces can be blended (morphed) parameter by
// parameter; that is what makes transitions between expressions smooth.
//
// Sizes and positions are in face units (see core/Canvas.h): the eyes live on
// a stage of 128 x 64 units in the middle of the round screen.
#ifndef FACE_FACETYPES_H
#define FACE_FACETYPES_H

#include <stdint.h>

#include "../core/Color.h"

// ---- Expressions (nodes of the emotion graph) --------------------------------
// The table in Expressions.cpp follows this order.
enum ExprId : uint8_t {
  // calm cluster
  EX_NORMAL = 0,
  EX_CURIOUS,
  EX_FOCUSED,
  EX_SKEPTIC,
  EX_SUSPICIOUS,
  EX_SQUINT,
  EX_UNIMPRESSED,
  EX_BORED,
  EX_CONFUSED,
  EX_OVAL,  // tall oval eyes
  EX_ATTENTIVE,
  EX_THINKING,
  EX_LISTENING,
  EX_DETERMINED,
  // happy cluster
  EX_HAPPY,
  EX_GLEE,
  EX_EXCITED,
  EX_AWE,
  EX_LOVE,
  EX_WINK,
  EX_PROUD,
  EX_SHY,
  EX_LAUGH,
  EX_SINGING,
  EX_SMUG,
  EX_MISCHIEF,
  EX_STARSTRUCK,
  EX_CHEEKY,
  EX_RELIEVED,
  // sad cluster
  EX_SAD,
  EX_WORRIED,
  EX_PLEADING,
  EX_CRYING,
  EX_LONELY,
  EX_GUILTY,
  EX_DESPAIR,
  EX_EMBARRASSED,
  // angry cluster
  EX_ANNOYED,
  EX_FRUSTRATED,
  EX_ANGRY,
  EX_FURIOUS,
  EX_DISGUST,
  EX_FERAL,
  // surprise / fear cluster
  EX_SURPRISED,
  EX_SCARED,
  EX_DIZZY,
  EX_KO,
  EX_HORRIFIED,
  EX_CROSSEYED,
  EX_HYPNO,
  // sleep cluster
  EX_SLEEPY,
  EX_YAWN,
  EX_ASLEEP,
  EX_TIRED,
  // body feelings (temperature sensor)
  EX_HOT,
  EX_COLD,
  EX_COUNT,
  EX_NONE = 0xFF
};

// Mood = cluster of the graph. Each mood has a hub expression.
enum Mood : uint8_t {
  MOOD_NORMAL = 0,
  MOOD_HAPPY,
  MOOD_SAD,
  MOOD_ANGRY,
  MOOD_ALERT,
  MOOD_SLEEPY,
  MOOD_COUNT
};

// ---- Eye ----------------------------------------------------------------------
enum EyeKind : uint8_t {
  EK_SHAPE = 0,  // superellipse + eyelids (the normal eye)
  EK_HEART,
  EK_SPIRAL,
  EK_CROSS,    // X
  EK_CHEVRON,  // > <  (tightly shut, laughing)
  EK_STAR,
  EK_RING      // rings growing from the middle (hypnotized)
};

struct EyeParams {
  float x, y;  // offset from the eye anchor. +x = away from the nose, +y = down
  float w, h;  // full size
  // Superellipse exponent of the upper / lower half: 2 = oval, 3..4 = rounded,
  // 8+ = almost a box.
  float nTop, nBot;
  // Upper eyelid: closure (0..1 of the height), slope in degrees (+ = lower on
  // the nose side, the angry look) and bend (+ = sags in the middle).
  float lidT, lidTAng, lidTBend;
  // Lower eyelid: closure, slope (+ = higher on the nose side) and bend
  // (+ = arches up in the middle, the smiling look).
  float lidB, lidBAng, lidBBend;
  // Pupil, a dark spot carved in the eye. 0 = none (the usual look).
  float pupil;   // diameter as a fraction of the smaller side of the eye
  float pupilW;  // width factor: 1 = round, 0.25 = cat slit
  float px, py;  // rest position inside the eye, -1..1. +px = towards the nose, +py = down
};

struct MouthParams {
  float w;      // width, < 2 = no mouth
  float curve;  // + = smile, - = frown (sagitta)
  float open;   // 0 = line, 1 = fully open
  float y;      // vertical position, from the row of the eyes
  float tilt;   // + = right end higher (smirk)
};

struct FacePose {
  EyeParams eye[2];  // [0] = left eye on screen, [1] = right eye on screen
  MouthParams mouth;
};

// ---- Decorations (bit flags) --------------------------------------------------
enum DecorFlag : uint32_t {
  DC_NONE = 0,
  DC_ZZZ = 1u << 0,
  DC_TEARS = 1u << 1,
  DC_SWEAT = 1u << 2,
  DC_ANGER = 1u << 3,
  DC_HEARTS = 1u << 4,
  DC_SPARKLE = 1u << 5,
  DC_QUESTION = 1u << 6,
  DC_EXCLAIM = 1u << 7,
  DC_BLUSH = 1u << 8,
  DC_NOTES = 1u << 9,
  DC_STARS = 1u << 10,
  DC_SHIVER = 1u << 11,
  DC_STEAM = 1u << 12,
  DC_GLINT = 1u << 13,
  DC_DOTS = 1u << 14,    // "..." while thinking
  DC_WAVES = 1u << 15,   // sound waves at both sides
  DC_TONGUE = 1u << 16,
  DC_FANGS = 1u << 17
};
#define DECOR_COUNT 18

// ---- Life motion layered on top of a pose -------------------------------------
enum WobbleKind : uint8_t {
  WB_NONE = 0,
  WB_BREATHE,  // slow height change
  WB_BOUNCE,   // vertical hop
  WB_SHAKE,    // fast horizontal shake
  WB_TREMBLE,  // small nervous jitter
  WB_SWAY,     // slow side to side
  WB_PULSE,    // size beat
  WB_NOD       // slow sinking and jerking up (falling asleep)
};

struct WobbleDef {
  uint8_t kind;
  float amp;  // face units (or fraction for PULSE / BREATHE)
  float hz;
};

// ---- Ears (two servos) --------------------------------------------------------
// Logical ear angle in degrees: 0 = lying back, 90 = straight up, 150 = forward.
enum EarGesture : uint8_t {
  EG_STILL = 0,
  EG_WIGGLE,  // both ears together
  EG_FLAP,    // ears in opposite phase
  EG_TWITCH,  // short random flicks
  EG_TREMBLE  // fast and tiny
};

struct EarDef {
  float left, right;  // rest angles
  uint8_t gesture;
  float amp;  // degrees
  float hz;
};

// ---- Sounds (buzzer chirps) ---------------------------------------------------
enum ChirpId : uint8_t {
  CH_NONE = 0,
  CH_HELLO,
  CH_HAPPY,
  CH_SAD,
  CH_ANGRY,
  CH_SURPRISE,
  CH_QUESTION,
  CH_SLEEP,
  CH_WAKE,
  CH_LOVE,
  CH_SCARED,
  CH_LAUGH,
  CH_YAWN,
  CH_BLIP,
  CH_DING,
  CH_POP,
  CH_WHOOSH,
  CH_POWER_ON,
  CH_POWER_OFF,
  CH_SNEEZE,
  CH_RAWR,
  CH_TUNE,
  CH_COUNT
};

// ---- Full definition of one expression ---------------------------------------
struct ExpressionDef {
  const char *name;
  uint8_t mood;
  FacePose pose;
  uint8_t kind[2];  // EyeKind of each eye
  uint32_t decor;   // DecorFlag bits
  WobbleDef wobble;
  EarDef ears;
  float gazeRange;  // 0 = eyes locked to the front, 1 = free to look around
  float blinkRate;  // 0 = never blinks, 1 = normal
  uint8_t chirp;    // sound played when the expression is reached
  Rgb ink;          // color of the eyes and the mouth
};

// Anchors of the eyes on the stage.
#define EYE_ANCHOR_LX 40.0f
#define EYE_ANCHOR_RX 88.0f
#define EYE_ANCHOR_Y 32.0f

#endif
