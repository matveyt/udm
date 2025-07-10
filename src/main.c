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


#include <stc12.h>                      // STC89/90 < STC12 < STC15 < STC8
#include <stdint.h>
#include "config.h"


static volatile int_fast8_t echo_status = 0;    // 0 not ready, 1 ok, -1 error
static volatile uint_fast16_t echo_count0;      // ticks while ECHO==0
static volatile uint_fast16_t echo_count1;      // ticks while ECHO==1
static volatile uint_fast8_t distance_m = 0;    // last known distance to an object
static volatile uint_fast8_t distance_cm = 0;


// setup fast counter (Timer0 8-bit)
inline void set_fast_counter(void)
{
    const uint8_t fast_counter = (uint8_t)(-CPU_FREQ * TICK_LEN / 12 / 1000);
    const uint8_t us10_counter = (uint8_t)(-CPU_FREQ * 10 / 12 / 1000);
    TH0 = fast_counter;
    TL0 = us10_counter - 1;
}

// setup slow counter (Timer1 16-bit)
inline void set_slow_counter(void)
{
    const uint16_t slow_counter = (uint16_t)(-CPU_FREQ * TICKUI_LEN / 12 / 1000);
    TH1 = (uint8_t)(slow_counter >> 8);
    TL1 = (uint8_t)(slow_counter);
}

// light LEDs up
static void update_leds(void)
{
#if HAVE_LEDS
    uint_fast8_t level = distance_m ? 100 : distance_cm;

    LED1 = (level > LEVEL1) ? 1 : 0;    // inverse logic
    LED2 = (level > LEVEL2) ? 1 : 0;
    LED3 = (level > LEVEL3) ? 1 : 0;
    LED4 = (level > LEVEL4) ? 1 : 0;
    LED5 = (level > LEVEL5) ? 1 : 0;
    LED6 = (level > LEVEL6) ? 1 : 0;
    LED7 = (level > LEVEL7) ? 1 : 0;
    LED8 = (level > LEVEL8) ? 1 : 0;
    LED9 = (level > LEVEL9) ? 1 : 0;
#endif // HAVE_LEDS
}

// 7seg refresh
static void update_display(void)
{
    static const uint8_t glyph[10] = {
        // dp g f e d c b a, inverse logic
        0b11000000, // 0
        0b11111001, // 1
        0b10100100, // 2
        0b10110000, // 3
        0b10011001, // 4
        0b10010010, // 5
        0b10000011, // 6
        0b11111000, // 7
        0b10000000, // 8
        0b10011000, // 9
    };
    static uint_fast8_t current_digit = 0;
    uint_fast8_t err = (distance_m >= 10 || distance_cm >= 100
        || (distance_m == 0 && distance_cm == 0)) ? 1 : 0;

    switch (current_digit) {
    case 0:
        DIG2 = 0;
        SEG7 = err ? 0b10101111/*r*/ : glyph[distance_cm % 10];
        DIG0 = 1;
        ++current_digit;
    break;
    case 1:
        DIG0 = 0;
        SEG7 = err ? 0b10101111/*r*/ : (distance_m || distance_cm >= 10) ?
            glyph[distance_cm / 10] : 0xFF/*blank*/;
        DIG1 = 1;
        ++current_digit;
    break;
    case 2:
        DIG1 = 0;
        SEG7 = err ? 0b10000110/*E*/ : distance_m ? glyph[distance_m] : 0xFF/*blank*/;
        DIG2 = 1;
        current_digit = 0;
    break;
    }
}

// calc (distance_m, distance_cm) from ticks
static void calc_distance(uint_fast16_t ticks)
{
    uint_fast16_t t = ticks * (TICK_LEN / 2);
    uint_fast16_t mm = t * SPEED_OF_SOUND / 100000;
    uint_fast16_t cm = mm / 10;

    distance_m = cm / 100;
    distance_cm = (cm % 100) + ((mm % 10 >= 5) ? 1 : 0);
}

// fast tick timer (polling ECHO pin)
INTERRUPT(static isr_tick, TF0_VECTOR)
{
    if (ECHO) {
        ++echo_count1;
        if (echo_count1 > ECHO_MAX1 / TICK_LEN) {
            // ECHO lost
            echo_status = -1;
            TR0 = 0;
        }
        return;
    }

    if (echo_count1 >= ECHO_MIN1 / TICK_LEN) {
        // ECHO ok
        echo_status = 1;
        TR0 = 0;
        return;
    }

    ++echo_count0;
    if (echo_count0 > 1 + ECHO_MAX0 / TICK_LEN) {
        // no ECHO
        echo_status = -1;
        TR0 = 0;
        return;
    }
}

// slow tick timer (UI update)
INTERRUPT(static isr_tickui, TF1_VECTOR)
{
    static uint_fast8_t tickui = 0;
    static uint_fast8_t echo_lost = 0;

    if (echo_status > 0) {
        // ECHO success
        calc_distance(echo_count1);
        update_leds();
        echo_lost = 0;
        echo_status = 0;
    } else if (echo_status < 0) {
        // ECHO lost for more than 1 second
        if (++echo_lost > 1000000 / TICKUI_LEN / ECHO_RATE) {
            distance_m = 10;
            update_leds();
        }
        echo_status = 0;
    }

    // 7seg refresh
    update_display();
    ++tickui;

#if HAVE_BUZZER
    // make some noise
    if (distance_m == 0 && 0 < distance_cm && distance_cm <= LEVEL1) {
        static uint_fast8_t last_beep = 0;
        uint_fast8_t delta = tickui - last_beep;
        if (delta > (uint_fast8_t)(distance_cm * (1000000 / TICKUI_LEN / LEVEL1))) {
            // turn buzzer on once per second on LEVEL1 approx.
            BUZZ = 0;
            last_beep = tickui;
        } else if (delta >= 2) {
            // turn buzzer off after 2xTICKUI_LEN
            BUZZ = 1;
        }
    } else {
        // make sure buzzer is off
        BUZZ = 1;
    }
#endif // HAVE_BUZZER

    // start measurement
    if ((tickui & ECHO_RATE - 1) == 0 && TR0 == 0 && ECHO == 0) {
        TRIG = 1;
        echo_count0 = echo_count1 = 0;
        for (TR0 = 1; echo_count0 == 0; ) ;  // wait 10 us (fast timer's 1st tick)
        TRIG = 0;
    }

#if !HAVE_MODE0
    // reload 16-bit timer counter
    set_slow_counter();
#endif // !HAVE_MODE0
}

/*noreturn*/ void main(void)
{
    // setup GPIO
    P0 = P1 = P2 = P3 = 0xFF;
    TRIG = 0;

#if HAVE_PxMy
    P0M1 = 0b11111111;  // unused high-z
    P0M0 = 0b00000000;
    P1M1 = 0b11111111;  // 7seg open-drain
    P1M0 = 0b11111111;
    P2M1 = 0b00011111;  // ECHO input-pullup (aka. quasi-bidirectional);
    P2M0 = 0b01111111;  // TRIG, BUZZ push-pull; LEDs open-drain
    P3M1 = 0b10001111;  // unused high-z; DIGs push-pull; LEDs open-drain
    P3M0 = 0b01111111;
#endif // HAVE_PxMy

    IE = 0b10001010;    // EA + ET1 + ET0
    IP = 0b00000010;    // PT0 (fast timer can interrupt slow one)
    TMOD = HAVE_MODE0 ? 0b00000010 : 0b00010010;
    set_fast_counter();
    set_slow_counter();

    for (TR1 = 1; ; PCON = IDL) {
        // nothing
    }
}
