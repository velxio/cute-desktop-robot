// The robot in 3D: Espressif's own ESP-VoCat enclosure (assets/3d/vocat_shell.glb, from
// the STL files Espressif published on OSHWHub, GPL-3.0) with the live screen of the
// emulator set into the round window of the lid. Drag on the screen to touch it, drag
// elsewhere to turn around the robot, mouse wheel to come closer.
#ifndef EMU_VIEW3D_H
#define EMU_VIEW3D_H

#include "raylib.h"

namespace View3D {

// Loads the model; false when the file is missing (the 3D view is then unavailable).
bool load();
bool available();
void unload();

// Draws the robot inside `area` of the window. `lcd` is the screen texture, `light` the
// backlight 0..1, `voiceDeg` where the pretend speaker stands (+ = robot's right) and
// whether it talks.
void draw(Rectangle area, Texture2D lcd, float light, float voiceDeg, bool talking);

// Mouse in window pixels -> pixel of the 360 x 360 screen, when the mouse is on it.
bool screenPixel(Vector2 mouse, int &px, int &py);
// Orbit with a mouse movement in pixels, zoom with the wheel.
void orbit(Vector2 delta);
void zoom(float wheel);

}  // namespace View3D

#endif
