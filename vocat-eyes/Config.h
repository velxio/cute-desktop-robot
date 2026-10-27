// Hardware map and behaviour tuning of the robot. Everything that depends on
// the board lives here.
//
// Board: Espressif ESP-VoCat v1.2 (before: EchoEar). ESP32-S3-WROOM-1-N16R16VA,
// 1.85" round 360x360 LCD (ST77916, QSPI) with a CST816S touch, two microphones
// on an ES7210, a 3 W speaker on an ES8311 + NS4150B, BMI270 IMU, BQ27220 fuel
// gauge, microSD, a green LED, two touch pads. Pins from the esp-bsp package
// and the xiaozhi-esp32 board driver; see doc/circuito.html.
#ifndef ROBOT_CONFIG_H
#define ROBOT_CONFIG_H

// ---- Power ---------------------------------------------------------------------
#define PIN_LCD_EN 9          // LCD and SD rail: LOW = on (BSP_LCD_EN / BSP_SD_EN)
#define PIN_CODEC_PWR 48      // codec rail on v1.2: HIGH = on (v1.0 has no switch)

// ---- Display (ST77916 over QSPI) -------------------------------------------------
#define PIN_LCD_PCLK 18
#define PIN_LCD_D0 46
#define PIN_LCD_D1 13
#define PIN_LCD_D2 11
#define PIN_LCD_D3 12
#define PIN_LCD_CS 14
#define PIN_LCD_RST 47        // reset is active HIGH on v1.2
#define PIN_LCD_BL 44         // backlight (PWM)
#define LCD_QSPI_HZ 40000000
#define LCD_ROTATION 0        // 0..3 quarters; 2 if the flex cable ends up at the top
#define LCD_RESET_ACTIVE_HIGH 1

// ---- Touch (CST816S) -------------------------------------------------------------
#define PIN_I2C_SDA 2
#define PIN_I2C_SCL 1
#define I2C_HZ 400000
#define TOUCH_ADDR 0x15
#define PIN_TOUCH_INT 10      // LOW while a finger is on the glass

// ---- Audio (ES7210 microphones in, ES8311 speaker out, one I2S bus) --------------
#define PIN_I2S_MCLK 42
#define PIN_I2S_BCLK 40
#define PIN_I2S_WS 39
#define PIN_I2S_DOUT 41       // to the ES8311
#define PIN_I2S_DIN 3         // from the ES7210 (4 TDM slots: MIC1, MIC3, MIC2, MIC4)
#define PIN_PA_EN 15          // NS4150B enable
#define ES8311_ADDR 0x18
#define ES7210_ADDR 0x40
#define AUDIO_RATE 16000      // ESP-SR wants 16 kHz
#define MIC_SPACING_MM 60.0f  // between the two microphones: measure it on the MicBoard

// ---- Other hardware ---------------------------------------------------------------
#define PIN_LED 43            // green LED on the MicBoard
#define PIN_BOOT 0
#define PIN_TOUCH_PAD1 7      // capacitive pads on the shell
#define PIN_TOUCH_PAD2 6
#define BATTERY_ADDR 0x55     // BQ27220 fuel gauge
#define IMU_ADDR 0x68         // BMI270
#define PIN_SD_CLK 16
#define PIN_SD_CMD 38
#define PIN_SD_D0 17

// ---- Screen ---------------------------------------------------------------------
#define FRAME_MS 33                // 30 frames per second
#define BACKLIGHT_MAX 255
#define BACKLIGHT_ASLEEP 18        // dim while it sleeps
#define BACKLIGHT_DARK_ROOM 90     // (no light sensor: used at night)

// ---- Touch --------------------------------------------------------------------
#define TAP_MAX_MS 280             // shorter than this, and still: a tap
#define HOLD_MS 900                // longer than this, and still: a long press
#define MOVE_PX 21                 // more than this is a stroke, not a tap (360 px glass)
#define POKES_TO_ANNOY 3           // taps on the eyes in a short time

// ---- Behaviour ----------------------------------------------------------------
#define T_DARK_TO_SLEEP_MS 8000UL      // (no light sensor; kept for the brain)
#define T_BORED_MS (2UL * 60 * 1000)   // nobody around -> bored
#define T_LONELY_MS (10UL * 60 * 1000) // -> sad
#define T_SLEEP_MS (45UL * 60 * 1000)  // -> falls asleep
#define IDLE_MIN_MS 5000UL             // time between idle expressions
#define IDLE_MAX_MS 13000UL
#define ACT_CHANCE 0.35f               // chance that an idle moment plays an act
#define HOT_ABOVE_C 31.0f              // (no climate sensor; kept for the brain)
#define COLD_BELOW_C 12.0f
#define INFO_SCREEN_MS 7000UL          // how long the clock / battery screen stays
#define NIGHT_FROM_HOUR 23             // at night it falls asleep sooner
#define NIGHT_TO_HOUR 7
#define T_NIGHT_SLEEP_MS (3UL * 60 * 1000)
#define BATTERY_LOW_PCT 15             // below this it shows the battery screen and goes to sleep sooner

// ---- Sound -------------------------------------------------------------------
#define MIC_SLOT_RIGHT 0           // TDM slot of the microphone on the robot's right (slots: MIC1, MIC3, MIC2, MIC4)
#define MIC_SLOT_LEFT 2            // ...and on its left. Swap them if the eyes turn the wrong way.
#define MIC_GAIN_DB 30             // analog gain of the ES7210: 0, 3, 6 ... 30, 33, 34.5, 36, 37.5
#define SPEAKER_VOLUME 60          // 0..100 at power up ("vol N" on the console)
#define VOICE_LEVEL_MIN 0.004f     // rms (0..1) under which nothing counts as a voice
#define NOISE_LEVEL_MIN 0.12f      // a sudden jump above this rms is a bang (it startles)
#define WAKE_WORD 1                // 1 = listens for "Hi ESP" all the time (ESP-SR)
#define PAD_TOUCH_RATIO 1.25f      // a shell pad counts as touched above baseline x this

// ---- Talking (Gemini Live) --------------------------------------------------------
#define ROBOT_NAME "Voki"
#define GEMINI_MODEL "gemini-2.5-flash-native-audio-latest"
#define GEMINI_VOICE "Puck"        // Puck, Charon, Kore, Fenrir, Aoede, Leda, Orus, Zephyr
// language: not sent (the native-audio models refuse languageCode); the prompt in Talk.cpp sets Spanish
#define TALK_IDLE_END_MS 20000UL   // nobody talks for this long -> the conversation ends
#define TALK_MAX_MS (8UL * 60 * 1000)
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"  // for the clock, once Wi-Fi is up (POSIX TZ string)

#define SERIAL_BAUD 115200

#endif
