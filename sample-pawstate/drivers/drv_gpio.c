/**
 * @file drv_gpio.c
 * @brief GPIO Peripheral Driver Implementation
 *
 * Buttons use internal pull-ups (active low on micro:bit v2).
 * Speaker uses PWM0 for tone generation.
 * Temperature reads the nRF52833 on-chip sensor (0.25°C resolution).
 */

#include "drv_gpio.h"
#include "nrf52833_hal.h"
#include "pawstate_config.h"
#include <tk/tkernel.h>

/* PWM duty-cycle buffer (must be in RAM for EasyDMA, 32-bit aligned) */
static uint16_t pwm_seq_buf[2] __attribute__((aligned(4)));

void gpio_periph_init(void)
{
    /* Button A (P0.14): input, pull-up, connect input buffer */
    P0_PIN_CNF(BUTTON_A_PIN) = PIN_CNF_DIR_INPUT |
                                PIN_CNF_INPUT_CONNECT |
                                PIN_CNF_PULL_UP;

    /* Button B (P0.23): input, pull-up, connect input buffer */
    P0_PIN_CNF(BUTTON_B_PIN) = PIN_CNF_DIR_INPUT |
                                PIN_CNF_INPUT_CONNECT |
                                PIN_CNF_PULL_UP;

    /* Speaker (P0.00): output, initially low */
    P0_PIN_CNF(SPEAKER_PIN) = PIN_CNF_DIR_OUTPUT |
                               PIN_CNF_INPUT_DISCONNECT |
                               PIN_CNF_DRIVE_S0S1;
    P0_OUTCLR = (1UL << SPEAKER_PIN);
}

bool gpio_button_a_pressed(void)
{
    /* Active low: pressed = pin reads 0 */
    return (P0_IN & (1UL << BUTTON_A_PIN)) == 0;
}

bool gpio_button_b_pressed(void)
{
    return (P0_IN & (1UL << BUTTON_B_PIN)) == 0;
}

void gpio_speaker_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz == 0 || duration_ms == 0) return;

    /* Configure PWM0 for the desired frequency.
     * PWM base clock = 16 MHz / (2^PRESCALER).
     * With PRESCALER=0: base = 16 MHz.
     * COUNTERTOP = base_clock / frequency = 16000000 / freq_hz.
     * Duty cycle = 50% → compare value = COUNTERTOP / 2. */
    uint32_t countertop = 16000000UL / freq_hz;
    if (countertop > 32767) countertop = 32767; /* 15-bit max */

    /* Stop PWM first if it is running */
    if (PWM0_ENABLE != 0) {
        PWM0_EVENTS_STOPPED = 0;
        PWM0_TASKS_STOP = 1;
        for (volatile uint32_t t = 0; t < 100000 && PWM0_EVENTS_STOPPED == 0; t++) {}
        PWM0_ENABLE = 0;
    }

    /* Configure PWM */
    PWM0_PRESCALER = 0;  /* 16 MHz */
    PWM0_COUNTERTOP = countertop;
    PWM0_MODE = 0;       /* Up counter */
    PWM0_DECODER = 0;    /* Common mode, loaded from RAM */
    PWM0_LOOP = 0xFFFF;  /* Loop sequence continuously until stopped */
    PWM0_SHORTS = (1UL << 3); /* LOOPSDONE_SEQSTART0: auto-restart sequence */

    /* Set output pin to speaker, and disconnect others */
    PWM0_PSELOUT0 = (SPEAKER_PORT << 5) | SPEAKER_PIN;
    PWM0_PSELOUT1 = 0xFFFFFFFF;
    PWM0_PSELOUT2 = 0xFFFFFFFF;
    PWM0_PSELOUT3 = 0xFFFFFFFF;

    /* 12.5% duty cycle to reduce current draw and avoid brownout
     * Bit 15 = polarity (0 = rising edge first). */
    pwm_seq_buf[0] = (uint16_t)(countertop / 8);

    /* Point sequence to our buffer */
    PWM0_SEQ0_PTR = (uint32_t)pwm_seq_buf;
    PWM0_SEQ0_CNT = 1;
    PWM0_SEQ0_REFRESH = 0;

    /* Enable and start */
    PWM0_ENABLE = 1;
    PWM0_TASKS_SEQSTART0 = 1;

    /* Busy-wait for duration replaced with RTOS delay to avoid watchdog timeouts. */
    tk_dly_tsk(duration_ms);

    /* Stop PWM */
    gpio_speaker_stop();
}

void gpio_speaker_anxiety_alert(void)
{
    /* Play repeated short beeps to audibly alert anxiety detection.
     * Pattern: BEEP - gap - BEEP - gap - BEEP */
    for (uint32_t i = 0; i < SPEAKER_ALERT_REPEAT; i++) {
        gpio_speaker_tone(SPEAKER_ALERT_FREQ_HZ, SPEAKER_ALERT_DURATION_MS);

        if (i < SPEAKER_ALERT_REPEAT - 1) {
            /* Silence gap between beeps */
            tk_dly_tsk(SPEAKER_ALERT_GAP_MS);
        }
    }
}

void gpio_speaker_stop(void)
{
    if (PWM0_ENABLE != 0) {
        PWM0_EVENTS_STOPPED = 0;
        PWM0_TASKS_STOP = 1;
        
        /* Wait for hardware to finish stopping before disabling */
        for (volatile uint32_t t = 0; t < 100000 && PWM0_EVENTS_STOPPED == 0; t++) {}
        
        PWM0_ENABLE = 0;
    }

    /* Ensure speaker pin is low (silent) */
    P0_OUTCLR = (1UL << SPEAKER_PIN);
}

int8_t gpio_read_temperature(void)
{
    /* Start temperature measurement */
    TEMP_EVENTS_DATARDY = 0;
    TEMP_TASKS_START = 1;

    /* Wait for measurement to complete (~36 μs on nRF52833) */
    while (TEMP_EVENTS_DATARDY == 0) {}
    TEMP_EVENTS_DATARDY = 0;

    /* Stop the sensor */
    TEMP_TASKS_STOP = 1;

    /* TEMP register value is in units of 0.25°C.
     * Divide by 4 to get integer °C. */
    int32_t raw_temp = (int32_t)TEMP_VALUE;
    return (int8_t)(raw_temp / 4);
}
