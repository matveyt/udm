/*
 * Custom firmware for Range Finder DIY kit
 * Last Change: 2025 Jul 21
 * License:     https://unlicense.org
 * URL:         https://github.com/matveyt/udm
 * Compiler:    SDCC
 */


#include "udm.h"


static volatile int_fast8_t echo_status = 0;    // 0=not ready, 1=ok, -1=error
static volatile uint_fast16_t echo_count0;      // ticks while ECHO=0
static volatile uint_fast16_t echo_count1;      // ticks while ECHO=1
#if HAVE_BUZZER
static uint_fast8_t alarm_cm;                   // buzzer alarm distance
#endif // HAVE_BUZZER


// setup fast counter (Timer0 8-bit)
inline void set_fast_counter(void)
{
    const uint8_t fast_counter = (uint8_t)(-F_CPU * TICK_LEN / 12 / 1000);
    const uint8_t us10_counter = (uint8_t)(-F_CPU * 10 / 12 / 1000);
    TH0 = fast_counter;
    TL0 = us10_counter - 1; // 1st tick is 10us
}

// setup slow counter (Timer1 or Timer2 16-bit)
inline void set_slow_counter(void)
{
    const uint16_t slow_counter = (uint16_t)(-F_CPU * TICKUI_LEN / 12 / 1000);
#if HAVE_TIMER == T1_M0 || HAVE_TIMER == T1_M1
    TH1 = (uint8_t)(slow_counter >> 8);
    TL1 = (uint8_t)(slow_counter);
#elif HAVE_TIMER == 2
    TH2 = RCAP2H = (uint8_t)(slow_counter >> 8);
    TL2 = RCAP2L = (uint8_t)(slow_counter);
#elif HAVE_TIMER == STC15
    T2H = (uint8_t)(slow_counter >> 8);
    T2L = (uint8_t)(slow_counter);
#endif // HAVE_TIMER
}

#if HAVE_BUZZER
// read byte from EEPROM
static uint8_t eeprom_read(uint16_t address)
{
    (void)address;      // unused?
#if HAVE_EEPROM
#if HAVE_STC89 || HAVE_STC15
    EA = 0;
    IAP_ADDRH = (uint8_t)(address >> 8);
    IAP_ADDRL = (uint8_t)(address);
    IAP_CMD = 1;        // read byte
    IAP_CONTR = 0x80;   // enable access, max. wait time
    IAP_TRIG = IAP_TRIG_KEY1;
    IAP_TRIG = IAP_TRIG_KEY2;
    NOP();
    uint8_t data = IAP_DATA;
    IAP_CONTR = 0;      // disable access
    IAP_CMD = 0;        // standby
    EA = 1;
    return data;
#else
#error Not implemented.
#endif // HAVE_STCxx
#else
    return 0xFF;
#endif // HAVE_EEPROM
}

// erase EEPROM sector (512 bytes)
static void eeprom_erase(uint16_t address)
{
    (void)address;      // unused?
#if HAVE_EEPROM
#if HAVE_STC89 || HAVE_STC15
    EA = 0;
    IAP_ADDRH = (uint8_t)(address >> 8) & 0xFE;
    IAP_ADDRL = 0;
    IAP_CMD = 3;        // sector erase
    IAP_CONTR = 0x80;   // enable access, max. wait time
    IAP_TRIG = IAP_TRIG_KEY1;
    IAP_TRIG = IAP_TRIG_KEY2;
    NOP();
    IAP_CONTR = 0;      // disable access
    IAP_CMD = 0;        // standby
    EA = 1;
#else
#error Not implemented.
#endif // HAVE_STCxx
#endif // HAVE_EEPROM
}

// write sector to EEPROM (512 bytes or less)
// (address + size) must not cross page boundary!
static void eeprom_write(uint16_t address, uint8_t data[], uint_fast16_t size)
{
    (void)address;      // unused?
    (void)data;         // unused?
    (void)size;         // unused?
#if HAVE_EEPROM
#if HAVE_STC89 || HAVE_STC15
    boolean eeprom_blank = 1, data_blank = 1, equal_data = 1;
    for (uint_fast16_t i = 0; i < size; ++i) {
        uint8_t old_data = eeprom_read(address + i);
        if (old_data != 0xFF)
            eeprom_blank = 0;
        if (data[i] != 0xFF)
            data_blank = 0;
        if (old_data != data[i])
            equal_data = 0;
    }

    if (equal_data)
        return;
    if (!eeprom_blank)
        eeprom_erase(address);
    if (!data_blank) {
        EA = 0;
        IAP_CMD = 2;        // write byte
        IAP_CONTR = 0x80;   // enable access, max. wait time
        for (uint_fast16_t i = 0; i < size; ++i, ++address) {
            IAP_DATA = data[i];
            IAP_ADDRH = (uint8_t)(address >> 8);
            IAP_ADDRL = (uint8_t)(address);
            IAP_TRIG = IAP_TRIG_KEY1;
            IAP_TRIG = IAP_TRIG_KEY2;
            NOP();
        }
        IAP_CONTR = 0;      // disable access
        IAP_CMD = 0;        // standby
        EA = 1;
    }
#else
#error Not implemented.
#endif // HAVE_STCxx
#endif // HAVE_EEPROM
}
#endif // HAVE_BUZZER

#if HAVE_BUTTONS
inline uint_fast8_t next_state(uint_fast8_t last_state)
{
#if HAVE_BUTTONS <= 2
    if (BUTTON_MINUS == BUTTON_ON)
        return (last_state < STATE_ALARM) ? STATE_ALARM_SET : STATE_ALARM_MINUS;
    if (BUTTON_PLUS == BUTTON_ON)
        return (last_state < STATE_ALARM) ? STATE_ALARM_SET : STATE_ALARM_PLUS;
#else
    if (BUTTON_SET == BUTTON_ON)
        return (last_state == STATE_NORMAL || last_state == STATE_ALARM_SET) ?
            STATE_ALARM_SET : STATE_NORMAL_SET;
    if (BUTTON_MINUS == BUTTON_ON)
        return (last_state < STATE_ALARM) ? STATE_NORMAL : STATE_ALARM_MINUS;
    if (BUTTON_PLUS == BUTTON_ON)
        return (last_state < STATE_ALARM) ? STATE_NORMAL : STATE_ALARM_PLUS;
#endif // HAVE_BUTTONS
    return (last_state < STATE_ALARM) ? STATE_NORMAL : STATE_ALARM;
}
#endif // HAVE_BUTTONS

// light LEDs up
static void update_leds(uint_fast8_t m, uint_fast8_t cm)
{
    (void)m;    // unused?
    (void)cm;   // unused?
#if HAVE_LEDS
    uint_fast16_t level = m * 100 + cm;
#if HAVE_LEDS_PROX
    // proximity indicator
    LED1 = (level > LEVEL1) ? LED_OFF : LED_ON;
    LED2 = (level > LEVEL2) ? LED_OFF : LED_ON;
    LED3 = (level > LEVEL3) ? LED_OFF : LED_ON;
    LED4 = (level > LEVEL4) ? LED_OFF : LED_ON;
    LED5 = (level > LEVEL5) ? LED_OFF : LED_ON;
    LED6 = (level > LEVEL6) ? LED_OFF : LED_ON;
    LED7 = (level > LEVEL7) ? LED_OFF : LED_ON;
    LED8 = (level > LEVEL8) ? LED_OFF : LED_ON;
    LED9 = (level > LEVEL9) ? LED_OFF : LED_ON;
#else
    // distance indicator
    LED1 = (level < LEVEL1) ? LED_OFF : LED_ON;
    LED2 = (level < LEVEL2) ? LED_OFF : LED_ON;
    LED3 = (level < LEVEL3) ? LED_OFF : LED_ON;
    LED4 = (level < LEVEL4) ? LED_OFF : LED_ON;
    LED5 = (level < LEVEL5) ? LED_OFF : LED_ON;
    LED6 = (level < LEVEL6) ? LED_OFF : LED_ON;
    LED7 = (level < LEVEL7) ? LED_OFF : LED_ON;
    LED8 = (level < LEVEL8) ? LED_OFF : LED_ON;
    LED9 = (level < LEVEL9) ? LED_OFF : LED_ON;
#endif // HAVE_LEDS_PROX
#endif // HAVE_LEDS
}

// 7seg refresh
static void update_display(uint_fast8_t m, uint_fast8_t cm)
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
    static uint_fast8_t current_digit = 1;
    boolean err = (m >= 10 || cm >= 100) ? 1 : 0;

    switch (current_digit) {
    case 1: // meter
        DS3 = DS_OFF;
        DS_PORT = err ? 0b10000110/*E*/ : m ? glyph[m] : 0xFF/*blank*/;
        DS1 = DS_ON;
        ++current_digit;
    break;
    case 2: // decimeter
        DS1 = DS_OFF;
        DS_PORT = err ? 0b10101111/*r*/ : (m || cm >= 10) ? glyph[cm / 10] :
            0xFF/*blank*/;
        DS2 = DS_ON;
        ++current_digit;
    break;
    case 3: // centimeter
        DS2 = DS_OFF;
        DS_PORT = err ? 0b10101111/*r*/ : glyph[cm % 10];
        DS3 = DS_ON;
        current_digit = 1;
    break;
    }
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
INTERRUPT(static isr_tickui, TFx_VECTOR)
{
    static uint_fast8_t distance_m = 0;     // last known distance to obstacle
    static uint_fast8_t distance_cm = 0;
    static uint_fast8_t echo_lost = 0;
    static uint_fast8_t tickui = 0;
#if HAVE_BUTTONS
    static uint_fast8_t last_state = STATE_NORMAL;
    static uint_fast16_t hold_state = 0;
#endif // HAVE_BUTTONS

    ++tickui;

    // check ECHO status
    if (echo_status > 0) {
        // ECHO success
        echo_lost = 0;
        uint_fast16_t t = echo_count1 * (TICK_LEN / 2);
        uint_fast16_t mm = t * SPEED_OF_SOUND / 100000;
        uint_fast16_t cm = mm / 10;
        distance_m = cm / 100;
        distance_cm = (cm % 100) + ((mm % 10 >= 5) ? 1 : 0);
        update_leds(distance_m, distance_cm);
    } else if (echo_status < 0) {
        // ECHO lost for more than 1 second
        if (++echo_lost > 1000000 / TICKUI_LEN / ECHO_RATE) {
            distance_m = 10;
            update_leds(10, 0);
        }
    }
    echo_status = 0;
#if HAVE_BUTTONS
    if (last_state >= STATE_ALARM)
        update_display(0, alarm_cm);
    else
#endif // HAVE_BUTTONS
        update_display(distance_m, distance_cm);

    // restart ECHO
    if ((tickui & ECHO_RATE - 1) == 0 && TR0 == 0 && ECHO == 0) {
        TRIGGER = TRIGGER_ON;
        echo_count0 = echo_count1 = 0;
        for (TR0 = 1; echo_count0 == 0; ) ;  // wait 10 us (fast timer's 1st tick)
        TRIGGER = TRIGGER_OFF;
    }

#if HAVE_BUZZER
    // make noise
    if (distance_m == 0 && 0 < distance_cm && distance_cm <= alarm_cm) {
        static uint_fast8_t last_beep = 0;
        uint_fast8_t delta = tickui - last_beep;
        if (delta > (uint_fast8_t)(distance_cm * (1000000 / TICKUI_LEN / alarm_cm))) {
            // turn buzzer on (every second if distance_cm == alarm_cm)
            BUZZER = BUZZER_ON;
            last_beep = tickui;
        } else if (delta >= 2) {
            // turn buzzer off after 2xTICKUI_LEN
            BUZZER = BUZZER_OFF;
        }
    } else {
        // make sure buzzer is off
        BUZZER = BUZZER_OFF;
    }
#endif // HAVE_BUZZER

#if HAVE_BUTTONS
    // process push button event
    uint_fast8_t state = next_state(last_state);
    if (last_state != state) {
        // state changed
        last_state = state;
        hold_state = 0;
        // state returned to NORMAL
        if (state == STATE_NORMAL)
            eeprom_write(0, &alarm_cm, 1);
    } else {
        // state hold
        ++hold_state;
        if (hold_state >= HOLD_BUTTON && state == STATE_ALARM_MINUS) {
            // hold MINUS
            alarm_cm -= (alarm_cm > 0) ? 1 : 0;
            hold_state = 0;
        } else if (hold_state >= HOLD_BUTTON && state == STATE_ALARM_PLUS) {
            // hold PLUS
            alarm_cm += (alarm_cm < 99) ? 1 : 0;
            hold_state = 0;
        }
#if HAVE_BUTTONS <= 2
        else if (hold_state >= HOLD_ALARM && state == STATE_ALARM) {
            // return from IDLE to NORMAL due to timeout
            last_state = STATE_NORMAL_SET;
            hold_state = 0;
        }
#endif // HAVE_BUTTONS
    }
#endif // HAVE_BUTTONS

#if HAVE_TIMER == T1_M1
    set_slow_counter();
#endif // HAVE_TIMER
}

/*noreturn*/ void main(void)
{
    // setup GPIO
    P0 = P1 = P2 = P3 = 0xFF;
    DS1 = DS_OFF;
    DS2 = DS_OFF;
    DS3 = DS_OFF;
    TRIGGER = TRIGGER_OFF;
#if HAVE_BUZZER
    BUZZER = BUZZER_OFF;
    alarm_cm = eeprom_read(0);
    if (alarm_cm == 0xFF)
        alarm_cm = LEVEL1;
#endif // HAVE_BUZZER

    // switch DSx into push-pull mode
#if HAVE_PUSHPULL
#if HAVE_PINOUT == 1 || HAVE_PINOUT == 2
    // DSx pins are P3.4, P3.5 and P3.6
#if HAVE_STC15
    P3M1 &= 0b10001111;
    P3M0 |= 0b01110000;
#elif HAVE_NUVOTON
    P3M1 &= 0b10001111;
    P3M2 |= 0b01110000;
#elif HAVE_WCH
    P3_MOD_OC &= 0b10001111;
    P3_DIR_PU |= 0b01110000;
#else
#error Not implemented.
#endif // HAVE_STC15
#else
    //TODO more pinout
#endif // HAVE_PINOUT
#endif // HAVE_PUSHPULL

    set_fast_counter();
    set_slow_counter();
    IP = 0b00000010;        // PT0 (fast timer can interrupt slow one)
#if HAVE_TIMER == T1_M0     // Timer0 Mode2, Timer1 Mode0
    TMOD = 0x02;
    IE = 0b10001010;        // EA + ET1 + ET0
    TR1 = 1;
#elif HAVE_TIMER == T1_M1   // Timer0 Mode2, Timer1 Mode1
    TMOD = 0x12;
    IE = 0b10001010;        // EA + ET1 + ET0
    TR1 = 1;
#elif HAVE_TIMER == 2       // Timer0 Mode2, Timer2 8052
    TMOD = 0x02;
    IE = 0b10100010;        // EA + ET2 + ET0
    T2MOD = HAVE_NUVOTON ? 0b10000000 : 0;  // LDEN (Nuvoton only)
    T2CON = 0b00000100;     // TR2
#elif HAVE_TIMER == STC15   // Timer0 Mode2, Timer2 STC
    TMOD = 0x02;
    IE = 0b10000010;        // EA + ET0
    IE2 = 0b00000100;       // ET2
    AUXR = 0b00010000;      // T2R
#endif // HAVE_TIMER

    for (;;) {
        PCON |= IDL;        // enter Idle Mode
    }
}
