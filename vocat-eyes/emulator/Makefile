# Windows emulator of the robot.
#
#   make            builds robot_emu.exe (window with the screen and the panel) and facetool.exe
#   make run        builds and starts the emulator
#   make gifs       writes every expression and act as a GIF into ../doc
#   make clean
#
# Needs g++ (w64devkit) and raylib. Change RAYLIB_PATH if yours is elsewhere.
RAYLIB_PATH ?= C:/raylib

CXX = g++
# The mocks in this folder stand in for <Arduino.h>, <SPI.h> and <Wire.h>, so
# "-I." must come first.
CXXFLAGS = -I. -I$(RAYLIB_PATH)/include -O2 -std=c++17 -Wall -Wextra -Wno-missing-field-initializers -MMD -MP \
           -DEMULATOR
LDFLAGS = -L$(RAYLIB_PATH)/lib -lraylib -lopengl32 -lgdi32 -lwinmm -static-libgcc -static-libstdc++

VPATH = ../src/core ../src/face ../src/hw ../src/brain ../src/audio ../src/voice ../src/ui

ENGINE = Canvas.o FaceRenderer.o Decor.o Expressions.o ExpressionGraph.o FaceEngine.o \
         Acts.o ActsMotion.o ActsProps.o ActsGames.o ActsLife.o
ROBOT = VocatLcd.o Touch.o Rtc.o Battery.o Sensors.o Brain.o InfoScreen.o \
        Codec.o AudioIo.o Player.o Listener.o WakeWord.o GeminiLive.o Talk.o WifiSetup.o MicIcon.o Settings.o \
        sketch_wrapper.o Arduino.o EmuBus.o
EMU_OBJS = main.o StateView.o View3D.o $(ROBOT) $(ENGINE)
TOOL_OBJS = facetool.o $(ENGINE)

all: robot_emu.exe facetool.exe

robot_emu.exe: $(EMU_OBJS)
	$(CXX) $(EMU_OBJS) -o $@ $(LDFLAGS)

facetool.exe: $(TOOL_OBJS)
	$(CXX) $(TOOL_OBJS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# the sketch is included by the wrapper
sketch_wrapper.o: ../vocat-eyes.ino ../Config.h

run: robot_emu.exe
	./robot_emu.exe

gifs: facetool.exe
	./facetool.exe all ../doc/expressions
	./facetool.exe acts ../doc/animations

clean:
	-rm -f $(EMU_OBJS) $(TOOL_OBJS) $(EMU_OBJS:.o=.d) $(TOOL_OBJS:.o=.d) robot_emu.exe facetool.exe

.PHONY: all run gifs clean

-include $(EMU_OBJS:.o=.d) $(TOOL_OBJS:.o=.d)
