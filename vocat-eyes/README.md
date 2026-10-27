# VoCat Eyes

A cute desk robot on the **Espressif ESP-VoCat v1.2** (ESP32-S3, 1.85" round 360×360 touch
screen, two microphones, speaker, battery). It has 56 facial expressions and 47 animations.
Its eyes turn toward whoever speaks, and it holds a spoken conversation through the
Gemini Live API.

The same C++ runs in three places:

| Where | What | How |
|-------|------|-----|
| On the robot | the firmware | `arduino-cli`, Arduino core for ESP32 3.3 |
| On your desk | a Windows emulator with the screen, a 3D model of the robot, the state machine and every sensor | `g++` + raylib |
| In a web page | the same emulator compiled to WebAssembly | Emscripten + raylib |

[Leer en español](README.es.md)

## Try it without the board

**Desktop (Windows):** install [w64devkit](https://github.com/skeeto/w64devkit) and raylib 5.5
(`C:/raylib`, or set `RAYLIB_PATH`), then:

```
cd emulator
mingw32-make
./robot_emu.exe
```

**Browser:** with the [Emscripten SDK](https://emscripten.org) activated and raylib 5.5 built for
the web (see below):

```
sh web/build.sh            # writes web/dist/index.html, index.js, index.wasm, index.data
cd web/dist && python -m http.server 8000
```

Building raylib for the web, once:

```
git clone --depth 1 -b 5.5 https://github.com/raysan5/raylib.git
cd raylib/src
for f in rcore rshapes rtextures rtext rmodels utils raudio; do
  emcc -c $f.c -Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -o $f.o
done
emar rcs libraylib.a *.o
# then point RAYLIB_WEB at a folder with libraylib.a, raylib.h, raymath.h and rlgl.h
```

### Emulator controls

- **The robot's screen:** click to tap, drag to pet, hold for the clock and battery screen.
  The small microphone at the bottom of the screen starts a pretend conversation. There is no
  network in the emulator.
- **3D view** (the default when the model is found): drag on the screen to touch it, drag
  anywhere else to turn the robot, and use the mouse wheel to zoom. `M` switches to the flat view.
- **The flat view:** click the ring around the glass to talk from that direction. The eyes look there.
- **Keys:** `1`-`9` faces, `TAB` next face, `[` `]` pick an act and `0` play it, `Y` yes,
  `U` no, `H` "HOLA!", `R` autonomous on/off, `V` talk, `S` bang, `W` wake word,
  `I` clock, `C` charger, `Q` state machine view, `P` screenshot, `F` fast, `ENTER` pause.
- **Scripted runs** for screenshots and tests:
  `robot_emu.exe --script "2:expr=Happy; 3:act=Rain; 5:shot=out.png; 6:quit"`.
- **`facetool`** renders every expression and act as a GIF: `mingw32-make gifs`.

## Every expression and animation

Rendered by `facetool` from the same engine as the firmware. Each GIF loops a few seconds of every face at once. Rebuild them with `mingw32-make gifs` and `tools/make_grid.py`.

### 56 expressions, by mood

**Calm and thinking** (16)

![Calm and thinking expressions of VoCat Eyes, animated](doc/faces-normal.gif)

**Happy** (15)

![Happy expressions of VoCat Eyes, animated](doc/faces-happy.gif)

**Sad** (8)

![Sad expressions of VoCat Eyes, animated](doc/faces-sad.gif)

**Angry** (6)

![Angry expressions of VoCat Eyes, animated](doc/faces-angry.gif)

**Surprised and alert** (7)

![Surprised and alert expressions of VoCat Eyes, animated](doc/faces-alert.gif)

**Sleepy** (4)

![Sleepy expressions of VoCat Eyes, animated](doc/faces-sleepy.gif)

### 47 animations, by family

**Body language** (9)

![Body language: VoCat Eyes animations](doc/acts-body.gif)

**Screen effects** (5)

![Screen effects: VoCat Eyes animations](doc/acts-screen.gif)

**With props** (18)

![With props: VoCat Eyes animations](doc/acts-props.gif)

**Everyday things** (9)

![Everyday things: VoCat Eyes animations](doc/acts-everyday.gif)

**Games and round-screen effects** (6)

![Games and round-screen effects: VoCat Eyes animations](doc/acts-games.gif)

## Build and flash the firmware

Libraries: Arduino core for ESP32 **3.3.x**, Arduino_GFX 1.6.5, ArduinoWebsockets 0.5.4,
ArduinoJson 7.

```
cp secrets.example.h secrets.h      # your Wi-Fi and Gemini key; never committed
arduino-cli compile --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=esp_sr_16,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio" \
  --build-property upload.maximum_size=6291456 --build-path build .
cp <arduino data>/packages/esp32/tools/esp32s3-libs/3.3.*/esp_sr/srmodels.bin build/
arduino-cli upload -p <port> --fqbn "<same as above>" --input-dir build .
```

`partitions.csv` gives the app 6 MB and keeps the ESP-SR model partition where the
`esp_sr_16` scheme expects it. The wake word engine does not fit the stock table. The upload
writes `srmodels.bin` (the "Hi ESP" model) at `0xC10000`.

## On the robot

| Touch | What it does |
|-------|--------------|
| Tap | next animation |
| Tap the small microphone | start or end a conversation (gray off, blue connecting, cyan listening, orange speaking) |
| Hold | clock and battery screen, with the Wi-Fi state |
| Hold, then tap | settings: volume, brightness, Gemini voice, Wi-Fi, wake word, sounds, autonomy, how long a silence ends a talk |
| Drag | petting |

The Wi-Fi setup lists the networks around and has a keyboard with capitals, numbers and symbols.
The network is saved in flash. The ESP32-S3 only has a 2.4 GHz radio.

**Talking to it:** say "Hi ESP", tap the microphone, or type `talk` on the serial console
(115200 baud). The microphone streams to Gemini while the robot listens and is muted while it
speaks. Gemini can change the face (`set_face`), play an animation (`play_act`), look somewhere
(`look`) and show a short text (`show_text`). The console prints what Gemini heard (`you: ...`)
and what it answered.

Serial console commands: `list`, `<face>`, `act <name>`, `say <text>`, `info`, `status`,
`auto on|off`, `talk`, `stop`, `vol <0-100>`, `mics`, `wifi`, `wifi set <ssid> <password>`,
`scan`, `settings`, `mem`, `net`, `voice <deg>`, `noise`, `wake`.

## How it works

```
src/core    Canvas: 360x360 frame in PSRAM, antialiased shapes, 24 px dirty tiles
src/face    FaceEngine, 56 expressions on an emotion graph, 47 acts (animations)
src/brain   Sensors, Brain (state machine: awake, show, reacting, dozing, sleeping...), clock screen
src/hw      ST77916 QSPI LCD, CST816S touch, BQ27220 gauge, clock
src/audio   ES8311/ES7210 codecs, I2S 4-slot TDM both ways, Player, Listener (voice direction), WakeWord
src/voice   GeminiLive (WebSocket, TLS with Google's roots), Talk (faces and tools)
src/ui      Wi-Fi setup, settings, microphone icon
emulator/   mocked Arduino layer, raylib window, 3D view, facetool
web/        Emscripten build of the emulator
assets/3d/  the enclosure model and the script that makes it
```

Two things that took a while:

- **RAM:** ESP-SR keeps about 100 KB of internal RAM, and mbedTLS needs about 45 KB of it for the
  connection to Gemini. The firmware stops ESP-SR when a conversation starts and loads it
  again when it ends, which takes about 1.2 s.
- **TLS:** `setInsecure()` in ArduinoWebsockets does nothing on the ESP32. The firmware embeds
  Google's GTS Root R1 and R4 certificates and waits for NTP before connecting.

## 3D model

`assets/3d/vocat_shell.glb` is made from the ESP-VoCat / EchoEar enclosure that Espressif's
ESP-College published on [OSHWHub](https://oshwhub.com/esp-college/echoear) under **GPL-3.0**.
The source STLs are in `assets/3d/source/`. `assets/3d/make_glb.py` rebuilds the GLB, with
your own colours if you want them.

## License

The code is MIT (see `LICENSE` at the root of the repository). The enclosure model in
`assets/3d/` keeps Espressif's GPL-3.0.
