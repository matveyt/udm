// Kit features
#define HAVE_MCU        STC15   // STC89, STC15, N76E003, CH55x, 8051, 8052
#define HAVE_PINOUT     2       // pinout#
//#define HAVE_BUZZER     1       // make noise
//#define HAVE_BUTTONS    0       // number of push buttons (Minus, Plus and Set)
//#define HAVE_LEDS       1       // use indicator LEDs
//#define HAVE_LEDS_PROX  1       // proximity or distance indicator
//#define HAVE_EEPROM     1       // use EEPROM (only if HAVE_BUTTONS)
//#define HAVE_PUSHPULL   1       // configurable GPIO
//#define HAVE_TIMER      STC15   // T1_M0, T1_M1, STC15, 2

// CPU timings
#define F_CPU           11059L  // kHz (core frequency)
#define TICK_LEN        50      // us (fast tick to poll ECHO pin)
#define TICKUI_LEN      5000    // us (slow tick to update UI)
#define ECHO_RATE       64      // tickui (rate of sending ECHOs, must be power of two)
#define HOLD_BUTTON     100     // tickui (Plus/Minus button hold delay)
#define HOLD_ALARM      1000    // tickui (Alarm display idle time if HAVE_BUTTONS <= 2)
#define SPEED_OF_SOUND  34655L  // cm/s at 25C
#define ECHO_MAX0       5800    // us (max. time from TRIGGER=1 to ECHO=1)
#define ECHO_MIN1       120     // us (ca. 2 cm, min. time when ECHO=1)
#define ECHO_MAX1       25000   // us (ca. 433 cm, max. time when ECHO=1)
