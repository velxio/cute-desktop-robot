// Watch face shown instead of the eyes: clock hands, time, temperature and
// humidity on arcs around the round glass.
#ifndef BRAIN_INFOSCREEN_H
#define BRAIN_INFOSCREEN_H

#include "../core/Canvas.h"
#include "Sensors.h"

namespace InfoScreen {
// `net`: the Wi-Fi line under the battery (IP or why not); `netOk` paints it green.
void draw(Canvas &c, const SensorState &s, float time, const char *net = nullptr, bool netOk = false);
}

#endif
