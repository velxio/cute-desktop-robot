#include "Expressions.h"
#include <ctype.h>

// Eye:   x, y, w, h,  nTop, nBot,  lidT, lidTAng, lidTBend,  lidB, lidBAng, lidBBend
#define EYE(x, y, w, h, nt, nb, lt, lta, ltb, lb, lba, lbb) \
  { x, y, w, h, nt, nb, lt, lta, ltb, lb, lba, lbb, 0.0f, 1.0f, 0.0f, 0.0f }
// Same eye with a pupil:   ..., size, width factor, px, py
#define EYE_P(x, y, w, h, nt, nb, lt, lta, ltb, lb, lba, lbb, pu, puw, px, py) \
  { x, y, w, h, nt, nb, lt, lta, ltb, lb, lba, lbb, pu, puw, px, py }
#define NO_MOUTH { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }
#define MOUTH(w, curve, open, y) { w, curve, open, y, 0.0f }
#define MOUTH_T(w, curve, open, y, tilt) { w, curve, open, y, tilt }
#define BOTH(kind) { kind, kind }
#define WOB(kind, amp, hz) { kind, amp, hz }
#define EARS(l, r, gesture, amp, hz) { l, r, gesture, amp, hz }

// Reusable eye shapes
#define E_NORMAL EYE(0, 0, 34, 36, 4.0f, 4.0f, 0, 0, 0, 0, 0, 0)
// Thin arc, ends pointing down: the "closed and smiling" eye.
#define E_ARC_UP(y, w) EYE(0, y, w, 18, 9.0f, 9.0f, 0.22f, 0, -0.75f, 0.58f, 0, 0.75f)
// Thin arc, ends pointing up: the "closed and resting" eye.
#define E_ARC_DOWN(y, w) EYE(0, y, w, 16, 9.0f, 9.0f, 0.55f, 0, 0.7f, 0.22f, 0, -0.7f)

static const ExpressionDef TABLE[EX_COUNT] = {
  // ------------------------------------------------------------------ calm
  /* EX_NORMAL */
  { "Normal", MOOD_NORMAL,
    { { E_NORMAL, E_NORMAL }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_BREATHE, 0.035f, 0.25f),
    EARS(80, 80, EG_TWITCH, 10, 0.2f), 1.0f, 1.0f, CH_NONE, INK_CYAN },

  /* EX_CURIOUS */
  { "Curious", MOOD_NORMAL,
    { { EYE(0, -3, 36, 40, 3.6f, 3.6f, 0, 0, 0, 0, 0, 0),
        EYE(-1, 2, 30, 30, 3.6f, 3.6f, 0.10f, -6, 0, 0.05f, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_BREATHE, 0.03f, 0.4f),
    EARS(112, 72, EG_TWITCH, 14, 0.5f), 0.8f, 0.9f, CH_QUESTION, INK_CYAN },

  /* EX_FOCUSED */
  { "Focused", MOOD_NORMAL,
    { { EYE(-1, 1, 36, 26, 5.0f, 5.0f, 0.16f, 7, 0, 0.12f, 0, 0),
        EYE(-1, 1, 36, 26, 5.0f, 5.0f, 0.16f, 7, 0, 0.12f, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(118, 118, EG_STILL, 0, 0), 0.25f, 0.5f, CH_NONE, INK_CYAN },

  /* EX_SKEPTIC */
  { "Skeptic", MOOD_NORMAL,
    { { EYE(0, 3, 34, 26, 4.5f, 4.0f, 0.38f, 4, 0, 0.05f, 0, 0),
        EYE(0, -4, 34, 38, 4.0f, 4.0f, 0, 0, 0, 0.08f, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(60, 105, EG_STILL, 0, 0), 0.4f, 0.7f, CH_NONE, INK_CYAN },

  /* EX_SUSPICIOUS */
  { "Suspicious", MOOD_NORMAL,
    { { EYE(0, 0, 36, 16, 6.0f, 6.0f, 0.20f, 5, 0, 0.18f, 0, 0),
        EYE(0, 0, 36, 20, 6.0f, 6.0f, 0.22f, 5, 0, 0.18f, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_SWAY, 5.0f, 0.35f),
    EARS(100, 100, EG_STILL, 0, 0), 0.0f, 0.4f, CH_NONE, INK_CYAN },

  /* EX_SQUINT */
  { "Squint", MOOD_NORMAL,
    { { EYE(0, 0, 38, 14, 7.0f, 7.0f, 0.12f, 0, 0, 0.12f, 0, 0),
        EYE(0, 0, 38, 14, 7.0f, 7.0f, 0.12f, 0, 0, 0.12f, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(95, 95, EG_STILL, 0, 0), 0.3f, 0.5f, CH_NONE, INK_CYAN },

  /* EX_UNIMPRESSED */
  { "Unimpressed", MOOD_NORMAL,
    { { EYE(0, -4, 34, 32, 4.0f, 4.0f, 0.48f, 0, 0, 0, 0, 0),
        EYE(0, -4, 34, 32, 4.0f, 4.0f, 0.48f, 0, 0, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(55, 55, EG_STILL, 0, 0), 0.5f, 0.8f, CH_NONE, 0x6FB4CC },

  /* EX_BORED */
  { "Bored", MOOD_NORMAL,
    { { EYE(0, -2, 34, 32, 4.0f, 3.2f, 0.55f, -3, 0.05f, 0, 0, 0),
        EYE(0, -2, 34, 32, 4.0f, 3.2f, 0.55f, -3, 0.05f, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_SWAY, 3.0f, 0.12f),
    EARS(35, 35, EG_STILL, 0, 0), 0.9f, 1.3f, CH_NONE, 0x6FA6C0 },

  /* EX_CONFUSED */
  { "Confused", MOOD_NORMAL,
    { { EYE(0, -2, 30, 36, 3.4f, 3.4f, 0.05f, 0, 0, 0, 0, 0),
        EYE(0, 3, 34, 24, 4.0f, 4.0f, 0.22f, -10, 0, 0.06f, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_QUESTION, WOB(WB_SWAY, 2.0f, 0.5f),
    EARS(115, 50, EG_TWITCH, 12, 0.6f), 0.5f, 1.0f, CH_QUESTION, 0xB9A2FF },

  /* EX_OVAL: matches the first frame of the GIF clips */
  { "Oval", MOOD_NORMAL,
    { { EYE(-3, -3, 14, 30, 2.0f, 2.0f, 0, 0, 0, 0, 0, 0),
        EYE(-3, -3, 14, 30, 2.0f, 2.0f, 0, 0, 0, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(80, 80, EG_STILL, 0, 0), 0.0f, 0.0f, CH_NONE, INK_CYAN },

  /* EX_ATTENTIVE */
  { "Attentive", MOOD_NORMAL,
    { { EYE(0, -2, 32, 40, 3.6f, 3.6f, 0, 0, 0, 0, 0, 0),
        EYE(0, -2, 32, 40, 3.6f, 3.6f, 0, 0, 0, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(104, 104, EG_STILL, 0, 0), 0.15f, 0.6f, CH_NONE, INK_CYAN },

  /* EX_THINKING: looks up and to the right */
  { "Thinking", MOOD_NORMAL,
    { { EYE(-7, -7, 34, 34, 4.0f, 4.0f, 0.06f, 0, 0, 0.10f, 0, 0.1f),
        EYE(7, -7, 30, 24, 4.0f, 4.0f, 0.18f, -6, 0, 0.10f, 0, 0.1f) }, MOUTH_T(10, 0, 0.0f, 22, 3) },
    BOTH(EK_SHAPE), DC_DOTS, WOB(WB_NONE, 0, 0),
    EARS(108, 62, EG_TWITCH, 8, 0.4f), 0.1f, 0.5f, CH_NONE, INK_CYAN },

  /* EX_LISTENING */
  { "Listening", MOOD_NORMAL,
    { { EYE(0, -3, 34, 38, 3.8f, 3.8f, 0, 0, 0, 0.05f, 0, 0),
        EYE(0, 0, 34, 34, 3.8f, 3.8f, 0.05f, 0, 0, 0.05f, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_WAVES, WOB(WB_NONE, 0, 0),
    EARS(125, 125, EG_TWITCH, 10, 1.2f), 0.1f, 0.6f, CH_NONE, INK_MINT },

  /* EX_DETERMINED */
  { "Determined", MOOD_NORMAL,
    { { EYE(0, 1, 36, 28, 5.0f, 5.0f, 0.28f, 12, 0, 0.14f, 0, 0.1f),
        EYE(0, 1, 36, 28, 5.0f, 5.0f, 0.28f, 12, 0, 0.14f, 0, 0.1f) }, MOUTH(14, -1, 0.0f, 22) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(132, 132, EG_STILL, 0, 0), 0.2f, 0.4f, CH_NONE, INK_CYAN },

  // ----------------------------------------------------------------- happy
  /* EX_HAPPY */
  { "Happy", MOOD_HAPPY,
    { { EYE(0, -3, 36, 30, 4.0f, 4.0f, 0, 0, 0, 0.42f, 0, 0.42f),
        EYE(0, -3, 36, 30, 4.0f, 4.0f, 0, 0, 0, 0.42f, 0, 0.42f) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_BOUNCE, 1.0f, 0.9f),
    EARS(95, 95, EG_WIGGLE, 12, 1.4f), 0.8f, 1.0f, CH_HAPPY, INK_YELLOW },

  /* EX_GLEE */
  { "Glee", MOOD_HAPPY,
    { { E_ARC_UP(-2, 36), E_ARC_UP(-2, 36) }, MOUTH(16, 4, 0.0f, 22) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_BOUNCE, 2.2f, 2.4f),
    EARS(100, 100, EG_FLAP, 26, 2.6f), 0.2f, 0.0f, CH_LAUGH, INK_YELLOW },

  /* EX_EXCITED */
  { "Excited", MOOD_HAPPY,
    { { EYE(0, -3, 38, 40, 3.4f, 5.0f, 0, 0, 0, 0.20f, 0, 0.16f),
        EYE(0, -3, 38, 40, 3.4f, 5.0f, 0, 0, 0, 0.20f, 0, 0.16f) }, MOUTH(12, 3, 0.5f, 25) },
    BOTH(EK_SHAPE), DC_SPARKLE, WOB(WB_BOUNCE, 2.6f, 3.0f),
    EARS(105, 105, EG_WIGGLE, 30, 3.6f), 0.6f, 0.7f, CH_HAPPY, INK_AMBER },

  /* EX_AWE */
  { "Awe", MOOD_HAPPY,
    { { EYE(0, -1, 38, 44, 3.0f, 3.0f, 0, 0, 0, 0, 0, 0),
        EYE(0, -1, 38, 44, 3.0f, 3.0f, 0, 0, 0, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_GLINT | DC_SPARKLE, WOB(WB_PULSE, 0.03f, 0.8f),
    EARS(92, 92, EG_STILL, 0, 0), 0.3f, 0.4f, CH_SURPRISE, INK_ICE },

  /* EX_LOVE */
  { "Love", MOOD_HAPPY,
    { { EYE(0, -1, 36, 34, 2, 2, 0, 0, 0, 0, 0, 0),
        EYE(0, -1, 36, 34, 2, 2, 0, 0, 0, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_HEART), DC_HEARTS, WOB(WB_PULSE, 0.10f, 1.3f),
    EARS(100, 100, EG_WIGGLE, 14, 1.3f), 0.15f, 0.0f, CH_LOVE, INK_PINK },

  /* EX_WINK */
  { "Wink", MOOD_HAPPY,
    { { EYE(0, -3, 36, 32, 4.0f, 4.0f, 0, 0, 0, 0.30f, 0, 0.35f),
        E_ARC_UP(0, 36) }, MOUTH(14, 3, 0.0f, 22) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(95, 60, EG_STILL, 0, 0), 0.0f, 0.0f, CH_HAPPY, INK_YELLOW },

  /* EX_PROUD */
  { "Proud", MOOD_HAPPY,
    { { EYE(0, -6, 36, 22, 5.0f, 5.0f, 0.10f, 0, 0, 0.45f, 0, 0.35f),
        EYE(0, -6, 36, 22, 5.0f, 5.0f, 0.10f, 0, 0, 0.45f, 0, 0.35f) }, MOUTH(18, 3, 0.0f, 18) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_BREATHE, 0.04f, 0.3f),
    EARS(110, 110, EG_STILL, 0, 0), 0.2f, 0.5f, CH_HAPPY, INK_YELLOW },

  /* EX_SHY */
  { "Shy", MOOD_HAPPY,
    { { EYE(-2, 0, 26, 26, 3.4f, 3.4f, 0.18f, -4, 0, 0.10f, 0, 0.2f),
        EYE(-2, 0, 26, 26, 3.4f, 3.4f, 0.18f, -4, 0, 0.10f, 0, 0.2f) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_BLUSH, WOB(WB_SWAY, 2.0f, 0.4f),
    EARS(45, 45, EG_STILL, 0, 0), 0.0f, 1.4f, CH_NONE, INK_ROSE },

  /* EX_LAUGH */
  { "Laugh", MOOD_HAPPY,
    { { EYE(0, -5, 30, 24, 2, 2, 0, 0, 0, 0, 0, 0),
        EYE(0, -5, 30, 24, 2, 2, 0, 0, 0, 0, 0, 0) }, MOUTH(24, 5, 0.75f, 21) },
    BOTH(EK_CHEVRON), DC_NONE, WOB(WB_BOUNCE, 3.0f, 4.5f),
    EARS(100, 100, EG_FLAP, 30, 4.0f), 0.0f, 0.0f, CH_LAUGH, INK_YELLOW },

  /* EX_SINGING */
  { "Singing", MOOD_HAPPY,
    { { E_ARC_UP(-4, 34), E_ARC_UP(-4, 34) }, MOUTH(11, 0, 1.0f, 19) },
    BOTH(EK_SHAPE), DC_NOTES, WOB(WB_SWAY, 4.0f, 0.8f),
    EARS(90, 90, EG_FLAP, 18, 0.8f), 0.0f, 0.0f, CH_HAPPY, INK_MINT },

  /* EX_SMUG */
  { "Smug", MOOD_HAPPY,
    { { EYE(0, 2, 36, 24, 5.0f, 4.0f, 0.42f, 0, 0, 0.12f, 0, 0.25f),
        EYE(0, -2, 36, 28, 5.0f, 4.0f, 0.30f, -7, 0, 0.12f, 0, 0.25f) }, MOUTH_T(18, 3, 0.0f, 21, 5) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(112, 72, EG_STILL, 0, 0), 0.3f, 0.6f, CH_NONE, INK_YELLOW },

  /* EX_MISCHIEF: angry brows over a smile */
  { "Mischief", MOOD_HAPPY,
    { { EYE(0, -1, 36, 30, 4.0f, 4.0f, 0.30f, 18, 0, 0.28f, 0, 0.35f),
        EYE(0, -1, 36, 30, 4.0f, 4.0f, 0.30f, 18, 0, 0.28f, 0, 0.35f) }, MOUTH(24, 5, 0.0f, 20) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_TREMBLE, 0.4f, 7.0f),
    EARS(138, 138, EG_TWITCH, 10, 1.0f), 0.4f, 0.5f, CH_LAUGH, INK_PURPLE },

  /* EX_STARSTRUCK */
  { "Starstruck", MOOD_HAPPY,
    { { EYE(0, -3, 38, 38, 2, 2, 0, 0, 0, 0, 0, 0),
        EYE(0, -3, 38, 38, 2, 2, 0, 0, 0, 0, 0, 0) }, MOUTH(12, 3, 0.6f, 25) },
    BOTH(EK_STAR), DC_SPARKLE, WOB(WB_PULSE, 0.08f, 1.4f),
    EARS(100, 100, EG_WIGGLE, 18, 2.0f), 0.0f, 0.0f, CH_SURPRISE, INK_YELLOW },

  /* EX_CHEEKY */
  { "Cheeky", MOOD_HAPPY,
    { { EYE(0, -5, 36, 30, 4.0f, 4.0f, 0, 0, 0, 0.30f, 0, 0.35f),
        E_ARC_UP(-3, 36) }, MOUTH(18, 4, 0.0f, 16) },
    BOTH(EK_SHAPE), DC_TONGUE, WOB(WB_BOUNCE, 1.2f, 1.6f),
    EARS(100, 60, EG_WIGGLE, 10, 1.6f), 0.0f, 0.0f, CH_LAUGH, INK_YELLOW },

  /* EX_RELIEVED */
  { "Relieved", MOOD_HAPPY,
    { { E_ARC_DOWN(0, 34), E_ARC_DOWN(0, 34) }, MOUTH(10, 2, 0.5f, 22) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_BREATHE, 0.06f, 0.4f),
    EARS(62, 62, EG_STILL, 0, 0), 0.0f, 0.0f, CH_NONE, INK_MINT },

  // ------------------------------------------------------------------- sad
  /* EX_SAD */
  { "Sad", MOOD_SAD,
    { { EYE(0, 5, 34, 28, 5.0f, 3.0f, 0.32f, -20, 0, 0, 0, 0),
        EYE(0, 5, 34, 28, 5.0f, 3.0f, 0.32f, -20, 0, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_BREATHE, 0.03f, 0.18f),
    EARS(28, 28, EG_STILL, 0, 0), 0.5f, 0.8f, CH_SAD, INK_BLUE },

  /* EX_WORRIED */
  { "Worried", MOOD_SAD,
    { { EYE(0, 1, 32, 34, 4.0f, 3.6f, 0.20f, -13, 0, 0, 0, 0),
        EYE(0, 1, 32, 32, 4.0f, 3.6f, 0.24f, -13, 0, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_TREMBLE, 0.6f, 9.0f),
    EARS(50, 50, EG_TWITCH, 10, 0.8f), 0.9f, 1.4f, CH_NONE, 0x7FA8FF },

  /* EX_PLEADING */
  { "Pleading", MOOD_SAD,
    { { EYE(0, 1, 38, 42, 3.0f, 3.0f, 0.10f, -12, 0, 0, 0, 0),
        EYE(0, 1, 38, 42, 3.0f, 3.0f, 0.10f, -12, 0, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_GLINT, WOB(WB_TREMBLE, 0.5f, 7.0f),
    EARS(40, 40, EG_TREMBLE, 4, 6.0f), 0.1f, 0.4f, CH_SAD, INK_SKY },

  /* EX_CRYING */
  { "Crying", MOOD_SAD,
    { { EYE(0, -2, 36, 22, 6.0f, 4.0f, 0.34f, -16, 0, 0.18f, 0, 0.25f),
        EYE(0, -2, 36, 22, 6.0f, 4.0f, 0.34f, -16, 0, 0.18f, 0, 0.25f) }, MOUTH(16, -4, 0.35f, 24) },
    BOTH(EK_SHAPE), DC_TEARS, WOB(WB_TREMBLE, 0.8f, 6.0f),
    EARS(15, 15, EG_TREMBLE, 5, 5.0f), 0.0f, 0.0f, CH_SAD, INK_BLUE },

  /* EX_LONELY */
  { "Lonely", MOOD_SAD,
    { { EYE(0, 7, 32, 26, 5.0f, 3.2f, 0.46f, -9, 0, 0, 0, 0),
        EYE(0, 7, 32, 26, 5.0f, 3.2f, 0.46f, -9, 0, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_SWAY, 2.0f, 0.1f),
    EARS(20, 20, EG_STILL, 0, 0), 0.4f, 0.7f, CH_SAD, 0x5F7AE0 },

  /* EX_GUILTY */
  { "Guilty", MOOD_SAD,
    { { EYE(0, 7, 34, 26, 4.0f, 3.0f, 0.50f, 0, 0.5f, 0, 0, 0),
        EYE(0, 7, 34, 26, 4.0f, 3.0f, 0.50f, 0, 0.5f, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(30, 30, EG_STILL, 0, 0), 0.0f, 0.6f, CH_NONE, INK_BLUE },

  /* EX_DESPAIR */
  { "Despair", MOOD_SAD,
    { { EYE(0, 6, 36, 26, 6.0f, 3.0f, 0.40f, -27, 0, 0, 0, 0),
        EYE(0, 6, 36, 26, 6.0f, 3.0f, 0.40f, -27, 0, 0, 0, 0) }, MOUTH(18, -5, 0.25f, 25) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_TREMBLE, 0.7f, 5.0f),
    EARS(6, 6, EG_TREMBLE, 3, 5.0f), 0.0f, 0.3f, CH_SAD, 0x3F6FE8 },

  /* EX_EMBARRASSED: looks away */
  { "Embarrassed", MOOD_SAD,
    { { EYE(6, 5, 30, 20, 5.0f, 5.0f, 0.38f, -4, 0, 0.06f, 0, 0),
        EYE(-6, 5, 30, 20, 5.0f, 5.0f, 0.38f, -4, 0, 0.06f, 0, 0) }, MOUTH(10, -1, 0.0f, 22) },
    BOTH(EK_SHAPE), DC_BLUSH | DC_SWEAT, WOB(WB_NONE, 0, 0),
    EARS(36, 36, EG_STILL, 0, 0), 0.0f, 1.5f, CH_NONE, INK_ROSE },

  // ----------------------------------------------------------------- angry
  /* EX_ANNOYED */
  { "Annoyed", MOOD_ANGRY,
    { { EYE(0, 1, 36, 24, 5.0f, 5.0f, 0.36f, 9, 0, 0.08f, 0, 0),
        EYE(0, 3, 36, 18, 5.0f, 5.0f, 0.40f, 11, 0, 0.10f, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(125, 125, EG_TWITCH, 8, 0.7f), 0.4f, 0.6f, CH_NONE, INK_ORANGE },

  /* EX_FRUSTRATED */
  { "Frustrated", MOOD_ANGRY,
    { { EYE(0, 3, 36, 20, 6.0f, 6.0f, 0.34f, 5, 0, 0.22f, 0, 0),
        EYE(0, 3, 36, 20, 6.0f, 6.0f, 0.34f, 5, 0, 0.22f, 0, 0) }, MOUTH(16, -2, 0.0f, 22) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_TREMBLE, 0.5f, 8.0f),
    EARS(130, 130, EG_STILL, 0, 0), 0.3f, 0.5f, CH_ANGRY, 0xFF6A30 },

  /* EX_ANGRY */
  { "Angry", MOOD_ANGRY,
    { { EYE(0, 3, 36, 32, 4.0f, 4.0f, 0.34f, 21, 0, 0.06f, 0, 0),
        EYE(0, 3, 36, 32, 4.0f, 4.0f, 0.34f, 21, 0, 0.06f, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_TREMBLE, 0.5f, 10.0f),
    EARS(140, 140, EG_STILL, 0, 0), 0.3f, 0.4f, CH_ANGRY, INK_RED },

  /* EX_FURIOUS */
  { "Furious", MOOD_ANGRY,
    { { EYE(1, 3, 38, 34, 4.0f, 5.0f, 0.38f, 28, 0, 0.14f, -6, 0),
        EYE(1, 3, 38, 34, 4.0f, 5.0f, 0.38f, 28, 0, 0.14f, -6, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_ANGER, WOB(WB_SHAKE, 1.6f, 11.0f),
    EARS(150, 150, EG_TREMBLE, 6, 11.0f), 0.0f, 0.2f, CH_ANGRY, 0xFF2020 },

  /* EX_DISGUST */
  { "Disgust", MOOD_ANGRY,
    { { EYE(0, 3, 36, 18, 5.0f, 5.0f, 0.36f, 15, 0, 0.22f, 0, 0),
        EYE(0, 0, 36, 28, 5.0f, 5.0f, 0.30f, 7, 0, 0.10f, 0, 0) }, MOUTH_T(16, -3, 0.0f, 22, -4) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NONE, 0, 0),
    EARS(122, 142, EG_STILL, 0, 0), 0.3f, 0.5f, CH_NONE, 0xA8D84A },

  /* EX_FERAL: cat eyes and fangs */
  { "Feral", MOOD_ANGRY,
    { { EYE_P(0, -6, 30, 36, 2.4f, 2.4f, 0.10f, 16, 0, 0, 0, 0, 0.62f, 0.26f, 0, 0),
        EYE_P(0, -6, 30, 36, 2.4f, 2.4f, 0.10f, 16, 0, 0, 0, 0, 0.62f, 0.26f, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_FANGS, WOB(WB_TREMBLE, 0.5f, 12.0f),
    EARS(150, 150, EG_TREMBLE, 5, 10.0f), 0.5f, 0.2f, CH_RAWR, INK_AMBER },

  // ------------------------------------------------------- surprise / fear
  /* EX_SURPRISED */
  { "Surprised", MOOD_ALERT,
    { { EYE(1, -7, 36, 42, 3.2f, 3.2f, 0, 0, 0, 0, 0, 0),
        EYE(1, -7, 36, 42, 3.2f, 3.2f, 0, 0, 0, 0, 0, 0) }, MOUTH(9, 0, 1.0f, 24) },
    BOTH(EK_SHAPE), DC_EXCLAIM, WOB(WB_NONE, 0, 0),
    EARS(90, 90, EG_TREMBLE, 3, 9.0f), 0.1f, 0.2f, CH_SURPRISE, 0xE6F6FF },

  /* EX_SCARED */
  { "Scared", MOOD_ALERT,
    { { EYE(-1, -5, 30, 38, 3.4f, 3.4f, 0.12f, -14, 0, 0, 0, 0),
        EYE(-1, -5, 30, 38, 3.4f, 3.4f, 0.12f, -14, 0, 0, 0, 0) }, MOUTH(14, -3, 0.25f, 24) },
    BOTH(EK_SHAPE), DC_SWEAT, WOB(WB_TREMBLE, 1.3f, 14.0f),
    EARS(18, 18, EG_TREMBLE, 7, 12.0f), 0.7f, 1.6f, CH_SCARED, INK_LILAC },

  /* EX_DIZZY */
  { "Dizzy", MOOD_ALERT,
    { { EYE(0, 0, 32, 32, 2, 2, 0, 0, 0, 0, 0, 0),
        EYE(0, 0, 32, 32, 2, 2, 0, 0, 0, 0, 0, 0) }, MOUTH(16, -1, 0.0f, 24) },
    BOTH(EK_SPIRAL), DC_STARS, WOB(WB_SWAY, 4.0f, 0.7f),
    EARS(70, 100, EG_FLAP, 20, 0.7f), 0.0f, 0.0f, CH_SAD, INK_PURPLE },

  /* EX_KO */
  { "KO", MOOD_ALERT,
    { { EYE(0, 0, 26, 26, 2, 2, 0, 0, 0, 0, 0, 0),
        EYE(0, 0, 26, 26, 2, 2, 0, 0, 0, 0, 0, 0) }, MOUTH(14, -2, 0.0f, 24) },
    BOTH(EK_CROSS), DC_STARS, WOB(WB_NONE, 0, 0),
    EARS(10, 10, EG_STILL, 0, 0), 0.0f, 0.0f, CH_SAD, 0xB0B8C8 },

  /* EX_HORRIFIED */
  { "Horrified", MOOD_ALERT,
    { { EYE_P(0, -6, 32, 42, 3.0f, 3.0f, 0.08f, -16, 0, 0, 0, 0, 0.30f, 1.0f, 0, 0),
        EYE_P(0, -6, 32, 42, 3.0f, 3.0f, 0.08f, -16, 0, 0, 0, 0, 0.30f, 1.0f, 0, 0) }, MOUTH(12, 0, 1.0f, 25) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_TREMBLE, 1.0f, 16.0f),
    EARS(14, 14, EG_TREMBLE, 6, 13.0f), 0.3f, 0.0f, CH_SCARED, 0xE6F6FF },

  /* EX_CROSSEYED */
  { "Crosseyed", MOOD_ALERT,
    { { EYE_P(0, -3, 34, 36, 3.4f, 3.4f, 0, 0, 0, 0, 0, 0, 0.46f, 1.0f, 0.9f, 0.1f),
        EYE_P(0, -3, 34, 36, 3.4f, 3.4f, 0, 0, 0, 0, 0, 0, 0.46f, 1.0f, 0.9f, 0.1f) }, MOUTH(8, 0, 0.8f, 24) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_SWAY, 1.5f, 0.6f),
    EARS(70, 110, EG_TWITCH, 10, 1.0f), 0.0f, 0.5f, CH_QUESTION, INK_CYAN },

  /* EX_HYPNO */
  { "Hypno", MOOD_ALERT,
    { { EYE(0, -2, 36, 36, 2, 2, 0, 0, 0, 0, 0, 0),
        EYE(0, -2, 36, 36, 2, 2, 0, 0, 0, 0, 0, 0) }, MOUTH(8, 0, 0.7f, 25) },
    BOTH(EK_RING), DC_NONE, WOB(WB_SWAY, 3.0f, 0.5f),
    EARS(80, 80, EG_FLAP, 12, 0.5f), 0.0f, 0.0f, CH_NONE, 0xE070FF },

  // ----------------------------------------------------------------- sleep
  /* EX_SLEEPY */
  { "Sleepy", MOOD_SLEEPY,
    { { EYE(0, -3, 34, 30, 4.0f, 3.4f, 0.50f, -2, 0.08f, 0, 0, 0),
        EYE(0, -3, 34, 30, 4.0f, 3.4f, 0.56f, -2, 0.08f, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NOD, 5.0f, 0.22f),
    EARS(38, 38, EG_STILL, 0, 0), 0.2f, 1.8f, CH_NONE, INK_INDIGO },

  /* EX_YAWN */
  { "Yawn", MOOD_SLEEPY,
    { { E_ARC_DOWN(-8, 32), E_ARC_DOWN(-8, 32) }, MOUTH(20, 0, 1.0f, 16) },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_BREATHE, 0.05f, 0.5f),
    EARS(60, 60, EG_STILL, 0, 0), 0.0f, 0.0f, CH_YAWN, INK_INDIGO },

  /* EX_ASLEEP */
  { "Asleep", MOOD_SLEEPY,
    { { E_ARC_DOWN(6, 34), E_ARC_DOWN(6, 34) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_ZZZ, WOB(WB_BREATHE, 0.10f, 0.22f),
    EARS(10, 10, EG_STILL, 0, 0), 0.0f, 0.0f, CH_SLEEP, 0x6A60C8 },

  /* EX_TIRED */
  { "Tired", MOOD_SLEEPY,
    { { EYE(0, 3, 36, 12, 6.0f, 6.0f, 0.30f, -12, 0, 0, 0, 0),
        EYE(0, 3, 36, 12, 6.0f, 6.0f, 0.30f, -12, 0, 0, 0, 0) }, NO_MOUTH },
    BOTH(EK_SHAPE), DC_NONE, WOB(WB_NOD, 3.0f, 0.18f),
    EARS(28, 28, EG_STILL, 0, 0), 0.2f, 1.6f, CH_NONE, 0x7A82C0 },

  // ------------------------------------------------------------------ body
  /* EX_HOT */
  { "Hot", MOOD_NORMAL,
    { { EYE(0, 0, 34, 28, 4.0f, 3.4f, 0.42f, -7, 0.05f, 0, 0, 0),
        EYE(0, 0, 34, 28, 4.0f, 3.4f, 0.42f, -7, 0.05f, 0, 0, 0) }, MOUTH(14, -1, 0.6f, 24) },
    BOTH(EK_SHAPE), DC_SWEAT | DC_STEAM, WOB(WB_BREATHE, 0.06f, 0.7f),
    EARS(30, 30, EG_STILL, 0, 0), 0.4f, 1.2f, CH_SAD, 0xFF6A30 },

  /* EX_COLD */
  { "Cold", MOOD_NORMAL,
    { { EYE(-2, 0, 30, 20, 5.0f, 5.0f, 0.22f, -6, 0, 0.18f, 0, 0),
        EYE(-2, 0, 30, 20, 5.0f, 5.0f, 0.22f, -6, 0, 0.18f, 0, 0) }, MOUTH(18, 0, 0.0f, 22) },
    BOTH(EK_SHAPE), DC_SHIVER, WOB(WB_SHAKE, 1.0f, 15.0f),
    EARS(25, 25, EG_TREMBLE, 5, 14.0f), 0.2f, 1.0f, CH_SCARED, INK_ICE },
};

static const char *const MOOD_NAMES[MOOD_COUNT] = {"Normal", "Happy", "Sad", "Angry", "Alert", "Sleepy"};

namespace Expressions {

const ExpressionDef &get(uint8_t id) { return TABLE[id < EX_COUNT ? id : (uint8_t)EX_NORMAL]; }

const char *name(uint8_t id) { return id < EX_COUNT ? TABLE[id].name : "-"; }

const char *moodName(uint8_t mood) { return mood < MOOD_COUNT ? MOOD_NAMES[mood] : "-"; }

uint8_t find(const char *name) {
  for (uint8_t i = 0; i < EX_COUNT; i++) {
    const char *a = TABLE[i].name, *b = name;
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
      a++;
      b++;
    }
    if (!*a && !*b) return i;
  }
  return EX_NONE;
}

}  // namespace Expressions
