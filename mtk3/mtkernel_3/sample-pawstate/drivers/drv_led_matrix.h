/**
 * @file drv_led_matrix.h
 * @brief micro:bit v2 5x5 LED Matrix Display Driver
 *
 * Row-scanning display driver with pre-defined patterns for each
 * behavioural state. Uses μT-Kernel cyclic handler for refresh.
 */

#ifndef DRV_LED_MATRIX_H
#define DRV_LED_MATRIX_H

#include <stdint.h>
#include "pawstate_types.h"

/** Initialise LED matrix GPIO pins as outputs */
void led_matrix_init(void);

/** Set the current display pattern (copied internally) */
void led_matrix_set_pattern(const led_pattern_t *pattern);

/** Clear the display (all LEDs off) */
void led_matrix_clear(void);

/** Row-scan tick — call from cyclic handler at LED_REFRESH_RATE_HZ.
 *  Activates the next row in the scan sequence. */
void led_matrix_scan_tick(void);

/** Set display to the pattern for a given behavioural state */
void led_matrix_show_state(behaviour_state_t state);

/** Show a brief animation (e.g., for startup or anxiety alert) */
void led_matrix_show_boot_animation(void);

/** Get the pre-defined pattern for a given state */
const led_pattern_t *led_matrix_get_state_pattern(behaviour_state_t state);

#endif /* DRV_LED_MATRIX_H */
