#!/bin/sh
# Builds the desktop emulator (emulator/main.cpp, raylib window) for the browser:
# web/dist/index.html + index.js + index.wasm. Same sources as robot_emu.exe and the firmware.
#
# Needs: the Emscripten SDK activated (source <emsdk>/emsdk_env.sh) and raylib 5.5 built
# for the web (RAYLIB_WEB = folder with raylib.h and libraylib.a, see the README).
set -e
cd "$(dirname "$0")"
RAYLIB_WEB="${RAYLIB_WEB:-../../../raylib-web}"
SRC="../src"
EMU="../emulator"
mkdir -p dist
em++ -O2 -std=c++17 -DEMULATOR -DPLATFORM_WEB -I"$EMU" -I"$RAYLIB_WEB" \
  "$EMU/main.cpp" "$EMU/StateView.cpp" "$EMU/View3D.cpp" \
  "$EMU/sketch_wrapper.cpp" "$EMU/Arduino.cpp" "$EMU/EmuBus.cpp" \
  $SRC/core/Canvas.cpp \
  $SRC/face/FaceRenderer.cpp $SRC/face/Decor.cpp $SRC/face/Expressions.cpp $SRC/face/ExpressionGraph.cpp \
  $SRC/face/FaceEngine.cpp $SRC/face/Acts.cpp $SRC/face/ActsMotion.cpp $SRC/face/ActsProps.cpp \
  $SRC/face/ActsGames.cpp $SRC/face/ActsLife.cpp \
  $SRC/hw/VocatLcd.cpp $SRC/hw/Touch.cpp $SRC/hw/Rtc.cpp $SRC/hw/Battery.cpp \
  $SRC/brain/Sensors.cpp $SRC/brain/Brain.cpp $SRC/brain/InfoScreen.cpp \
  $SRC/audio/Codec.cpp $SRC/audio/AudioIo.cpp $SRC/audio/Player.cpp $SRC/audio/Listener.cpp $SRC/audio/WakeWord.cpp \
  $SRC/voice/GeminiLive.cpp $SRC/voice/Talk.cpp \
  $SRC/ui/WifiSetup.cpp $SRC/ui/MicIcon.cpp $SRC/ui/Settings.cpp \
  "$RAYLIB_WEB/libraylib.a" \
  -s USE_GLFW=3 -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=67108864 -s STACK_SIZE=1048576 \
  -s EXPORTED_FUNCTIONS='["_main","_web_mouse_scale"]' \
  --preload-file ../assets/3d/vocat_shell.glb@assets/vocat_shell.glb \
  --shell-file shell.html -o dist/index.html
ls -la dist
