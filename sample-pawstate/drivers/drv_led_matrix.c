/**
 * @file drv_led_matrix.c
 * @brief micro:bit v2 5x5 LED Matrix Implementation
 *
 * DESIGN DECISION: Row-scanning multiplexing — only one row is
 * driven HIGH at a time while the corresponding column pins are
 * driven LOW to illuminate desired LEDs. At >60 Hz refresh rate,
 * persistence of vision creates a stable image.
 *
 * Each behavioural state has a distinct, recognisable pattern:
 *   RESTING:        Zzz (sleep symbol)
 *   WALKING:        Arrow pointing right (movement)
 *   PLAYING:        Star (energy/fun)
 *   ANXIOUS_PACING: Exclamation mark (warning)
 *   ALERT_FREEZE:   Square (frozen/stopped)
 */

#include "drv_led_matrix.h"
#include "nrf52833_hal.h"
#include <string.h>

/* --- Pin lookup tables --- */
static const uint8_t row_port[5] = {
    LED_ROW1_PORT, LED_ROW2_PORT, LED_ROW3_PORT,
    LED_ROW4_PORT, LED_ROW5_PORT
};
static const uint8_t row_pin[5] = {
    LED_ROW1_PIN, LED_ROW2_PIN, LED_ROW3_PIN,
    LED_ROW4_PIN, LED_ROW5_PIN
};
static const uint8_t col_port[5] = {
    LED_COL1_PORT, LED_COL2_PORT, LED_COL3_PORT,
    LED_COL4_PORT, LED_COL5_PORT
};
static const uint8_t col_pin[5] = {
    LED_COL1_PIN, LED_COL2_PIN, LED_COL3_PIN,
    LED_COL4_PIN, LED_COL5_PIN
};

/* Current display pattern and scan state */
static led_pattern_t current_pattern;
static uint8_t current_row = 0;

/* --- Pre-defined behavioural state patterns --- */

/* RESTING: "Zzz" sleep symbol (clean 5x5 Z)
 *  Z Z Z Z Z
 *  . . . Z .
 *  . . Z . .
 *  . Z . . .
 *  Z Z Z Z Z
 */
static const led_pattern_t pattern_resting = {
    .rows = { 0x1F, 0x08, 0x04, 0x02, 0x1F }
};

/* WALKING: Right arrow (points towards right edge)
 *  . . 1 . .
 *  . . . 1 .
 *  1 1 1 1 1
 *  . . . 1 .
 *  . . 1 . .
 */
static const led_pattern_t pattern_walking = {
    .rows = { 0x04, 0x08, 0x1F, 0x08, 0x04 }
};

/* PLAYING: Star (energy/fun)
 *  . . 1 . .
 *  . 1 1 1 .
 *  1 1 1 1 1
 *  . 1 1 1 .
 *  . 1 . 1 .
 */
static const led_pattern_t pattern_playing = {
    .rows = { 0x04, 0x0E, 0x1F, 0x0E, 0x0A }
};

/* ANXIOUS_PACING: Exclamation mark (warning/alert)
 *  . . 1 . .
 *  . . 1 . .
 *  . . 1 . .
 *  . . . . .
 *  . . 1 . .
 */
static const led_pattern_t pattern_anxious = {
    .rows = { 0x04, 0x04, 0x04, 0x00, 0x04 }
};

/* ALERT_FREEZE: Solid square (frozen/stopped)
 *  1 1 1 1 1
 *  1 . . . 1
 *  1 . . . 1
 *  1 . . . 1
 *  1 1 1 1 1
 */
static const led_pattern_t pattern_freeze = {
    .rows = { 0x1F, 0x11, 0x11, 0x11, 0x1F }
};

/* UNKNOWN: Question mark (standard right-curving '?')
 *  . 1 1 1 .
 *  . . . 1 .
 *  . . 1 . .
 *  . . . . .
 *  . . 1 . .
 */
static const led_pattern_t pattern_unknown = {
    .rows = { 0x0E, 0x08, 0x04, 0x00, 0x04 }
};

/* Helper: set a GPIO pin high or low */
static void gpio_set(uint8_t port, uint8_t pin, uint8_t value)
{
    if (port == 0) {
        if (value) {
            P0_OUTSET = (1UL << pin);
        } else {
            P0_OUTCLR = (1UL << pin);
        }
    } else {
        if (value) {
            P1_OUTSET = (1UL << pin);
        } else {
            P1_OUTCLR = (1UL << pin);
        }
    }
}

/* Helper: configure pin as output */
static void gpio_cfg_output(uint8_t port, uint8_t pin)
{
    if (port == 0) {
        P0_PIN_CNF(pin) = PIN_CNF_DIR_OUTPUT | PIN_CNF_INPUT_DISCONNECT |
                          PIN_CNF_DRIVE_S0S1;
    } else {
        P1_PIN_CNF(pin) = PIN_CNF_DIR_OUTPUT | PIN_CNF_INPUT_DISCONNECT |
                          PIN_CNF_DRIVE_S0S1;
    }
}

void led_matrix_init(void)
{
    /* Configure all row and column pins as outputs */
    for (int i = 0; i < 5; i++) {
        gpio_cfg_output(row_port[i], row_pin[i]);
        gpio_cfg_output(col_port[i], col_pin[i]);

        /* Rows LOW (inactive), Columns HIGH (inactive — LEDs off) */
        gpio_set(row_port[i], row_pin[i], 0);
        gpio_set(col_port[i], col_pin[i], 1);
    }

    memset(&current_pattern, 0, sizeof(current_pattern));
    current_row = 0;
}

void led_matrix_set_pattern(const led_pattern_t *pattern)
{
    current_pattern = *pattern;
}

void led_matrix_clear(void)
{
    memset(&current_pattern, 0, sizeof(current_pattern));
}

void led_matrix_scan_tick(void)
{
    /* Turn off previous row */
    gpio_set(row_port[current_row], row_pin[current_row], 0);

    /* Advance to next row */
    current_row = (current_row + 1) % 5;

    /* Set column pins: LOW = LED on, HIGH = LED off.
     * Bit 0 of rows[r] → COL1, Bit 4 → COL5. */
    uint8_t row_data = current_pattern.rows[current_row];

    for (int c = 0; c < 5; c++) {
        if (row_data & (1 << c)) {
            gpio_set(col_port[c], col_pin[c], 0); /* LED ON */
        } else {
            gpio_set(col_port[c], col_pin[c], 1); /* LED OFF */
        }
    }

    /* Drive current row HIGH */
    gpio_set(row_port[current_row], row_pin[current_row], 1);
}

const led_pattern_t *led_matrix_get_state_pattern(behaviour_state_t state)
{
    switch (state) {
        case STATE_RESTING:        return &pattern_resting;
        case STATE_WALKING:        return &pattern_walking;
        case STATE_PLAYING:        return &pattern_playing;
        case STATE_ANXIOUS_PACING: return &pattern_anxious;
        case STATE_ALERT_FREEZE:   return &pattern_freeze;
        default:                   return &pattern_unknown;
    }
}

void led_matrix_show_state(behaviour_state_t state)
{
    const led_pattern_t *pat = led_matrix_get_state_pattern(state);
    led_matrix_set_pattern(pat);
}

void led_matrix_show_boot_animation(void)
{
    /* Progressive fill animation — each row fills sequentially.
     * This runs during init before the scheduler starts, so we
     * use simple busy-wait delays. */
    led_pattern_t boot = { .rows = {0, 0, 0, 0, 0} };

    for (int r = 0; r < 5; r++) {
        boot.rows[r] = 0x1F; /* Fill entire row */
        led_matrix_set_pattern(&boot);

        /* Simple delay — ~200ms at 64 MHz.
         * In pre-scheduler context, we can't use tk_dly_tsk. */
        for (volatile uint32_t d = 0; d < 800000; d++) {
            /* Perform a few scan ticks to show the pattern */
            if ((d % 5000) == 0) {
                led_matrix_scan_tick();
            }
        }
    }

    /* Brief all-on hold, then clear */
    for (volatile uint32_t d = 0; d < 400000; d++) {
        if ((d % 5000) == 0) {
            led_matrix_scan_tick();
        }
    }
}
