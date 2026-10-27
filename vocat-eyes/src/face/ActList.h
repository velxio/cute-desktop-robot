// Functions of every act (see Acts.cpp for the table).
#ifndef FACE_ACTLIST_H
#define FACE_ACTLIST_H

#include "Acts.h"

#define ACT_DECL(name)            \
  void act##name##Pose(ActFrame &f); \
  void act##name##Draw(Canvas &c, const ActFrame &f, const FaceFrame &face, uint8_t layer)

// body language
ACT_DECL(Yes);
ACT_DECL(No);
ACT_DECL(EyeRoll);
ACT_DECL(Peek);
ACT_DECL(Dash);
ACT_DECL(Exclaim);
ACT_DECL(Thinking);
ACT_DECL(Read);
ACT_DECL(Sneeze);
// screen effects
ACT_DECL(Glitch);
ACT_DECL(PowerOn);
ACT_DECL(Reboot);
ACT_DECL(Scanner);
ACT_DECL(Matrix);
// props
ACT_DECL(Notify);
ACT_DECL(Orbit);
ACT_DECL(Swirl);
ACT_DECL(Burst);
ACT_DECL(Heartbeat);
ACT_DECL(Rain);
ACT_DECL(ShootingStar);
ACT_DECL(Fireworks);
ACT_DECL(Snore);
ACT_DECL(Loading);
ACT_DECL(Bubbles);
ACT_DECL(Say);
ACT_DECL(Fly);
ACT_DECL(Ball);
ACT_DECL(Hula);
ACT_DECL(Dance);
ACT_DECL(Plant);
ACT_DECL(Drink);
// everyday things
ACT_DECL(Book);
ACT_DECL(Headphones);
ACT_DECL(Butterfly);
ACT_DECL(Coffee);
ACT_DECL(Sunglasses);
ACT_DECL(Rocket);
ACT_DECL(Snow);
ACT_DECL(Search);
ACT_DECL(Photo);
// games
ACT_DECL(Pacman);
ACT_DECL(Pong);
// made for the round color screen
ACT_DECL(Rainbow);
ACT_DECL(Radar);
ACT_DECL(Aquarium);
ACT_DECL(Spin);

#endif
