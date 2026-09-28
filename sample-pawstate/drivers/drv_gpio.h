/**
 * @file drv_gpio.h
 * @brief GPIO Driver — Buttons, Speaker, Temperature
 *
 * Handles micro:bit v2 on-board peripherals accessed via GPIO:
 *   - Button A (P0.14) and Button B (P0.23)
 *   - On-board speaker (P0.00) via PWM
 *   - On-chip temperature sensor
 */

#ifndef DRV_GPIO_H
#define DRV_GPIO_H

#include <stdint.h>
#include <stdbool.h>

/** Initialise buttons as inputs with pull-ups, speaker as output */
void gpio_periph_init(void);

/** Read Button A state. Returns true if pressed (active low). */
bool gpio_button_a_pressed(void);

/** Read Button B state. Returns true if pressed (active low). */
bool gpio_button_b_pressed(void);

/** Play a tone on the speaker at given frequency for given duration.
 *  Uses PWM0. Blocking call (busy-waits for duration). */
void gpio_speaker_tone(uint32_t freq_hz, uint32_t duration_ms);

/** Play the anxiety alert beep pattern (multiple short beeps) */
void gpio_speaker_anxiety_alert(void);

/** Stop any active speaker output */
void gpio_speaker_stop(void);

/** Read on-chip temperature sensor. Returns temperature in °C. */
int8_t gpio_read_temperature(void);

#endif /* DRV_GPIO_H */
