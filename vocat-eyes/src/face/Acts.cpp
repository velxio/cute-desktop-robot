#include "Acts.h"

#include <ctype.h>
#include <string.h>

#include "ActList.h"

#define ACT(name, seconds, base, moods) \
  { #name, seconds, base, (uint8_t)(moods), act##name##Pose, act##name##Draw }

#define CALM IN_MOOD(MOOD_NORMAL)
#define GLAD IN_MOOD(MOOD_HAPPY)
#define BLUE IN_MOOD(MOOD_SAD)
#define MAD IN_MOOD(MOOD_ANGRY)
#define DOZE IN_MOOD(MOOD_SLEEPY)
// Never chosen by the robot on its own, only on request.
#define ON_REQUEST 0

static const ActDef TABLE[] = {
  // body language
  ACT(Yes, 1.9f, EX_NONE, ON_REQUEST),
  ACT(No, 1.9f, EX_NONE, ON_REQUEST),
  ACT(EyeRoll, 2.2f, EX_UNIMPRESSED, CALM | MAD),
  ACT(Peek, 4.6f, EX_NORMAL, CALM | GLAD),
  ACT(Dash, 2.2f, EX_NORMAL, CALM | GLAD),
  ACT(Exclaim, 2.0f, EX_NORMAL, ON_REQUEST),
  ACT(Thinking, 3.4f, EX_NORMAL, CALM),
  ACT(Read, 4.4f, EX_FOCUSED, CALM),
  ACT(Sneeze, 3.4f, EX_NORMAL, CALM | GLAD | BLUE),
  // screen effects
  ACT(Glitch, 1.8f, EX_NONE, CALM | MAD),
  ACT(PowerOn, 1.8f, EX_NORMAL, ON_REQUEST),
  ACT(Reboot, 2.7f, EX_NONE, ON_REQUEST),
  ACT(Scanner, 2.4f, EX_NONE, CALM),
  ACT(Matrix, 6.5f, EX_NORMAL, CALM),
  // props
  ACT(Notify, 2.8f, EX_NORMAL, ON_REQUEST),
  ACT(Orbit, 3.6f, EX_NORMAL, CALM | GLAD),
  ACT(Swirl, 1.5f, EX_NONE, GLAD),
  ACT(Burst, 2.8f, EX_NORMAL, CALM | GLAD),
  ACT(Heartbeat, 3.6f, EX_HAPPY, GLAD),
  ACT(Rain, 5.0f, EX_SAD, BLUE),
  ACT(ShootingStar, 3.2f, EX_NORMAL, CALM | GLAD | BLUE),
  ACT(Fireworks, 4.4f, EX_HAPPY, GLAD),
  ACT(Snore, 5.0f, EX_ASLEEP, DOZE),
  ACT(Loading, 3.6f, EX_NORMAL, CALM),
  ACT(Bubbles, 4.6f, EX_HAPPY, GLAD),
  ACT(Say, 3.4f, EX_HAPPY, ON_REQUEST),
  ACT(Fly, 5.5f, EX_NORMAL, CALM | GLAD),
  ACT(Ball, 4.5f, EX_NORMAL, CALM | GLAD),
  ACT(Hula, 4.4f, EX_HAPPY, GLAD),
  ACT(Dance, 5.0f, EX_HAPPY, GLAD),
  ACT(Plant, 5.2f, EX_NORMAL, CALM | GLAD),
  ACT(Drink, 5.4f, EX_HAPPY, GLAD),
  // everyday things
  ACT(Book, 6.2f, EX_NORMAL, CALM),
  ACT(Headphones, 6.0f, EX_HAPPY, GLAD | CALM),
  ACT(Butterfly, 7.0f, EX_NORMAL, CALM | GLAD),
  ACT(Coffee, 5.6f, EX_NORMAL, CALM),
  ACT(Sunglasses, 4.2f, EX_NORMAL, CALM | GLAD),
  ACT(Rocket, 5.6f, EX_NORMAL, CALM | GLAD),
  ACT(Snow, 6.0f, EX_NORMAL, CALM | BLUE),
  ACT(Search, 5.2f, EX_CURIOUS, CALM),
  ACT(Photo, 4.2f, EX_HAPPY, GLAD),
  // games
  ACT(Pacman, 5.6f, EX_NORMAL, CALM | GLAD),
  ACT(Pong, 5.8f, EX_NORMAL, CALM | GLAD),
  // made for the round color screen
  ACT(Rainbow, 4.6f, EX_HAPPY, GLAD | CALM),
  ACT(Radar, 5.0f, EX_NORMAL, CALM),
  ACT(Aquarium, 7.0f, EX_NORMAL, CALM | GLAD),
  ACT(Spin, 5.4f, EX_NORMAL, CALM | GLAD),
};

static char sayText[21] = "HOLA!";

namespace Acts {

int count() { return (int)(sizeof(TABLE) / sizeof(TABLE[0])); }

const ActDef &get(int i) { return TABLE[i]; }

int find(const char *name) {
  for (int i = 0; i < count(); i++) {
    const char *a = TABLE[i].name, *b = name;
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
      a++;
      b++;
    }
    if (!*a && !*b) return i;
  }
  return -1;
}

void setText(const char *text) {
  size_t n = strlen(text);
  if (n > sizeof(sayText) - 1) n = sizeof(sayText) - 1;
  memcpy(sayText, text, n);
  sayText[n] = 0;
}

const char *text() { return sayText; }

}  // namespace Acts
