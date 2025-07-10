/*
 * Custom firmware for Range Finder DIY kit
 * Last Change:  2025 Jul 10
 * License:      https://unlicense.org
 * URL:          https://github.com/matveyt/udm
 *
 * Compiler:    SDCC
 * MCU:         STC15 core in DIP-28 package (2K+ flash)
 *              (e.g. STC15F102 35I-SKDIP28 or better)
 */


#if !defined(CONFIG_H)
#define CONFIG_H

#define HAVE_PxMy       1       // configurable I/O ports (STC12+)
#define HAVE_MODE0      1       // timer MODE0 is 16-bit auto-reload (STC15+)
#define HAVE_LEDS       1       // use LED1..9
#define HAVE_BUZZER     1       // make some noise

#define CPU_FREQ        12000L  // 12 MHz
#define TICK_LEN        50      // us
#define TICKUI_LEN      5000    // us
#define ECHO_RATE       64      // tickui
#define SPEED_OF_SOUND  34655L  // cm/s at 25C

#define ECHO_MAX0       5800    // us
#define ECHO_MIN1       120     // us (ca. 2 cm)
#define ECHO_MAX1       25000   // us (ca. 433 cm)

// PCB pinout
// 3x7seg (common anode)
#define DIG2 P3_4
#define DIG1 P3_5
#define DIG0 P3_6
#define SEG7 P1
// 9 LEDs (cathodes)
#define LED1 P3_3
#define LED2 P3_2
#define LED3 P3_1
#define LED4 P3_0
#define LED5 P2_0
#define LED6 P2_1
#define LED7 P2_2
#define LED8 P2_3
#define LED9 P2_4
// active buzzer (PNP)
#define BUZZ P2_5
// HC-SR04 sensor
#define TRIG P2_6
#define ECHO P2_7

// proximity level, cm
enum {
    LEVEL1 = 35,
    LEVEL2 = 31,
    LEVEL3 = 27,
    LEVEL4 = 23,
    LEVEL5 = 19,
    LEVEL6 = 15,
    LEVEL7 = 11,
    LEVEL8 = 7,
    LEVEL9 = 3,
};

#endif // CONFIG_H
