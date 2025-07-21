/*
 * Custom firmware for Range Finder DIY kit
 * Last Change: 2025 Jul 21
 * License:     https://unlicense.org
 * URL:         https://github.com/matveyt/udm
 * Compiler:    SDCC
 */


#if !defined(UDM_H)
#define UDM_H

#define STC89           89
#define STC15           15
#define N76E003         76
#define CH55x           55
//8051
//8052
typedef __bit boolean;  // my boolean type
#include <stdint.h>
#include <8051.h>
#include <compiler.h>
#include "config.h"
#define HAVE_STC89      (HAVE_MCU == STC89)
#define HAVE_STC15      (HAVE_MCU == STC15)
#define HAVE_NUVOTON    (HAVE_MCU == N76E003)
#define HAVE_WCH        (HAVE_MCU == CH55x)
#define HAVE_8052       (HAVE_MCU == 8052 || HAVE_STC89 || HAVE_NUVOTON || HAVE_WCH)

#if HAVE_STC15
#include <stc12.h>
SFR(T2H, 0xD6);
SFR(T2L, 0xD7);
#elif HAVE_STC89
#include <stc89.h>
#elif HAVE_8052
#include <8052.h>
SFR(T2MOD, 0xC9);
#endif // HAVE_MCU

#if HAVE_NUVOTON
SFR(P3M1, 0xAC);
SFR(P3M2, 0xAD);
SFR(P0M1, 0xB1);
SFR(P0M2, 0xB2);
SFR(P1M1, 0xB3);
SFR(P1M2, 0xB4);
#endif // HAVE_NUVOTON

#if HAVE_WCH
SFR(P1_MOD_OC, 0x92);
SFR(P1_DIR_PU, 0x93);
SFR(P3_MOD_OC, 0x96);
SFR(P3_DIR_PU, 0x97);
#endif // HAVE_WCH

// predefined PCB pinout
#if (HAVE_PINOUT == 1)          // kit with STC89 in DIP-40 package
#define DS1             P3_6    // 3x7seg (common anode)
#define DS2             P3_5
#define DS3             P3_4
#define DS_PORT         P1
#define BUZZER          P2_3    // active buzzer
#define TRIGGER         P3_2    // HC-SR04 sensor
#define ECHO            P3_3
#define BUTTON_MINUS    P2_0    // three push buttons
#define BUTTON_PLUS     P2_1
#define BUTTON_SET      P2_2
// the kit has no programmable LEDs!
#elif (HAVE_PINOUT == 2)        // kit with STC15 in DIP-28 package
#define DS1             P3_4    // 3x7seg (common anode)
#define DS2             P3_5
#define DS3             P3_6
#define DS_PORT         P1
#define BUZZER          P2_5    // active buzzer
#define TRIGGER         P2_6    // HC-SR04 sensor
#define ECHO            P2_7
#define LED1            P3_3    // 5mm indicator LEDs
#define LED2            P3_2
#define LED3            P3_1
#define LED4            P3_0
#define LED5            P2_0
#define LED6            P2_1
#define LED7            P2_2
#define LED8            P2_3
#define LED9            P2_4
#else
//TODO more pinout
#endif // HAVE_PINOUT

// HAVE_BUZZER: make noise
#if !defined(HAVE_BUZZER) && defined(BUZZER)
#define HAVE_BUZZER     1
#endif // HAVE_BUZZER

// HAVE_BUTTONS: Minus, Plus and Set
#if !defined(HAVE_BUTTONS)
#if defined(BUTTON_SET)
#define HAVE_BUTTONS    3
#elif defined(BUTTON_MINUS)
#define HAVE_BUTTONS    2
#endif // BUTTON_SET
#endif // HAVE_BUTTONS

// HAVE_LEDS: use indicator LEDs
#if !defined(HAVE_LEDS) && defined(LED1)
#define HAVE_LEDS       1
#endif // HAVE_LEDS

// HAVE_EEPROM: use EEPROM
#if !defined(HAVE_EEPROM) && HAVE_BUTTONS && (HAVE_STC89 || HAVE_STC15)
#define HAVE_EEPROM     1
#endif // HAVE_EEPROM

// HAVE_LEDS_PROX: proximity or distance indicator
#if !defined(HAVE_LEDS_PROX)
#define HAVE_LEDS_PROX  1
#endif // HAVE_LEDS_PROX

// HAVE_PUSHPULL: configurable I/O ports
#if !defined(HAVE_PUSHPULL) && (HAVE_STC15 || HAVE_NUVOTON || HAVE_WCH)
#define HAVE_PUSHPULL   1
#endif // HAVE_PUSHPULL

#if !defined(HAVE_TIMER)
#if HAVE_MCU == 8051
#define HAVE_TIMER      T1_M1   // Timer1 Mode1
#elif HAVE_STC15
#define HAVE_TIMER      STC15   // Timer2 STC
#else
#define HAVE_TIMER      2       // Timer2 8052
#endif // HAVE_MCU
#endif // HAVE_TIMER

// timer interrupt#
#if HAVE_TIMER == T1_M0 || HAVE_TIMER == T1_M1
#define TFx_VECTOR TF1_VECTOR
#elif HAVE_TIMER == 2
#define TFx_VECTOR 5
#elif HAVE_TIMER == STC15
#define TFx_VECTOR 12
#endif // HAVE_TIMER

// indicator level, cm
enum {
#if HAVE_LEDS_PROX
    LEVEL1 = 35,
    LEVEL2 = 31,
    LEVEL3 = 27,
    LEVEL4 = 23,
    LEVEL5 = 19,
    LEVEL6 = 15,
    LEVEL7 = 11,
    LEVEL8 = 7,
    LEVEL9 = 3,
#else
    LEVEL1 = 30,
    LEVEL2 = 60,
    LEVEL3 = 90,
    LEVEL4 = 120,
    LEVEL5 = 150,
    LEVEL6 = 180,
    LEVEL7 = 210,
    LEVEL8 = 240,
    LEVEL9 = 270,
#endif // HAVE_LEDS_PROX
};

// logical levels
enum {
    DS_ON = HAVE_PUSHPULL ? 1 : 0,  // MCU with PUSHPULL can source DSx directly
    DS_OFF = 1 - DS_ON,
    BUZZER_ON = 0,
    BUZZER_OFF = 1 - BUZZER_ON,
    LED_ON = 0,
    LED_OFF = 1 - LED_ON,
    TRIGGER_ON = 1,
    TRIGGER_OFF = 1 - TRIGGER_ON,
    BUTTON_ON = 0,
    BUTTON_OFF = 1 - BUTTON_ON,
};

// keys to enable EEPROM access
enum {
    IAP_TRIG_KEY1 = HAVE_STC89 ? 0x46 : 0x5a,
    IAP_TRIG_KEY2 = HAVE_STC89 ? 0xb9 : 0xa5,
};

// push button state
enum {
    STATE_NORMAL,       // normal distance display
    STATE_NORMAL_SET,   // same as above but SET button held
    STATE_ALARM,        // alarm distance display
    STATE_ALARM_SET,    // same as above but SET button held
    STATE_ALARM_MINUS,  // same as above but MINUS button held
    STATE_ALARM_PLUS,   // same as above but PLUS button held
};


#endif // UDM_H
