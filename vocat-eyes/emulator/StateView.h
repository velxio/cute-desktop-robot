// Live drawing of the state machine of the Brain: the state it is in, the
// transition it just took and why, and the clocks that will trigger the next.
#ifndef EMU_STATEVIEW_H
#define EMU_STATEVIEW_H

#include "raylib.h"

#include "../src/brain/Brain.h"

class StateView {
 public:
  static const int HEIGHT = 470;

  void draw(Rectangle area, const Brain &brain, const FaceEngine &face, const SensorState &sensors);

 private:
  int from_ = -1;          // state it came from, -1 = power on
  int flashEdge_ = -1;     // edge of the last transition
  double flashAt_ = -100;  // when it happened, real seconds
  uint32_t changes_ = 0;
  bool first_ = true;
};

#endif
