/*
 *  microbit_periph.h
 *
 *  BBC micro:bit v2.21 — Peripheral Pin Map & Register Definitions
 *  Hardware: nRF52833 (ARM Cortex-M4F, 64 MHz)
 *
 *  This header centralises every on-board peripheral's GPIO pin
 *  assignments and control-register addresses so that drivers and
 *  application code never hard-code magic numbers.
 */

#ifndef __MICROBIT_PERIPH_H__
#define __MICROBIT_PERIPH_H__

#include <sys/sysdef.h>        /* GPIO(), GPIO_P0_BASE, GPIO_P1_BASE macros */

/* ======================================================================
 *  1. LED Matrix  (5×5, active-high rows, active-low columns)
 *      ROW  HIGH + COL LOW  →  LED ON
 * ====================================================================== */
/* Row pins (accent: active-HIGH drives current through the LED) */
#define LED_ROW1_PORT   0       /* P0 */
#define LED_ROW1_PIN    21
#define LED_ROW2_PORT   0
#define LED_ROW2_PIN    22
#define LED_ROW3_PORT   0
#define LED_ROW3_PIN    15
#define LED_ROW4_PORT   0
#define LED_ROW4_PIN    24
#define LED_ROW5_PORT   0
#define LED_ROW5_PIN    19

/* Column pins (active-LOW sinks current through the LED) */
#define LED_COL1_PORT   0
#define LED_COL1_PIN    28
#define LED_COL2_PORT   0
#define LED_COL2_PIN    11
#define LED_COL3_PORT   0
#define LED_COL3_PIN    31
#define LED_COL4_PORT   1       /* P1 */
#define LED_COL4_PIN    5
#define LED_COL5_PORT   0
#define LED_COL5_PIN    30

/* Convenience: number of rows/columns */
#define LED_ROWS        5
#define LED_COLS        5

/* ======================================================================
 *  2. Buttons  (active-LOW with internal pull-up)
 * ====================================================================== */
#define BTN_A_PORT      0
#define BTN_A_PIN       14
#define BTN_B_PORT      0
#define BTN_B_PIN       23

/* PIN_CNF value: Input, pull-up, sense disabled */
#define BTN_PIN_CNF_VAL 0x0000000C  /* DIR=input(0), INPUT=connect(0), PULL=pullup(3<<2) */

/* ======================================================================
 *  3. Touch Logo  (capacitive, active-LOW when touched)
 * ====================================================================== */
#define TOUCH_LOGO_PORT 1
#define TOUCH_LOGO_PIN  4

/* ======================================================================
 *  4. Speaker  (on-board magnetic speaker)
 * ====================================================================== */
#define SPEAKER_PORT    0
#define SPEAKER_PIN     0

/* ======================================================================
 *  5. Microphone — MEMS via PDM interface
 * ====================================================================== */
#define MIC_PDM_CLK_PIN     26      /* P0.26 */
#define MIC_PDM_DIN_PIN     25      /* P0.25 */

/* PDM Peripheral Registers (nRF52833) */
#define PDM_BASE            0x4001D000
#define PDM_TASKS_START     (PDM_BASE + 0x000)
#define PDM_TASKS_STOP      (PDM_BASE + 0x004)
#define PDM_EVENTS_STARTED  (PDM_BASE + 0x100)
#define PDM_EVENTS_STOPPED  (PDM_BASE + 0x104)
#define PDM_EVENTS_END      (PDM_BASE + 0x108)
#define PDM_ENABLE          (PDM_BASE + 0x500)
#define PDM_PDMCLKCTRL      (PDM_BASE + 0x504)
#define PDM_MODE            (PDM_BASE + 0x508)
#define PDM_PSEL_CLK        (PDM_BASE + 0x540)
#define PDM_PSEL_DIN        (PDM_BASE + 0x544)

/* ======================================================================
 *  6. LSM303AGR — Combined Accelerometer + Magnetometer (I2C)
 * ====================================================================== */
/* I2C slave addresses */
#define LSM303_ACCEL_ADDR   0x19
#define LSM303_MAG_ADDR     0x1E

/* WHO_AM_I registers and expected values */
#define LSM303_WHO_AM_I_A   0x0F
#define LSM303_WHO_AM_I_A_VAL  0x33

#define LSM303_WHO_AM_I_M   0x4F
#define LSM303_WHO_AM_I_M_VAL  0x40

/* Accelerometer control registers */
#define LSM303_CTRL_REG1_A  0x20
#define LSM303_OUT_X_L_A    0x28    /* First of 6-byte burst (| 0x80 for auto-inc) */

/* Magnetometer control registers */
#define LSM303_CFG_REG_A_M  0x60
#define LSM303_OUTX_L_REG_M 0x68

/* ======================================================================
 *  7. nRF52833 Internal Temperature Sensor  (TEMP peripheral)
 * ====================================================================== */
#define TEMP_BASE               0x4000C000
#define TEMP_TASKS_START        (TEMP_BASE + 0x000)
#define TEMP_TASKS_STOP         (TEMP_BASE + 0x004)
#define TEMP_EVENTS_DATARDY     (TEMP_BASE + 0x100)
#define TEMP_INTENSET           (TEMP_BASE + 0x304)
#define TEMP_INTENCLR           (TEMP_BASE + 0x308)
#define TEMP_VALUE              (TEMP_BASE + 0x508)  /* Result in 0.25 °C units */

/* ======================================================================
 *  8. I2C (TWIM0) — Internal sensor bus
 *     (Pin assignments also in i2c_nrf5.c; repeated here for reference)
 * ====================================================================== */
#define I2C_INT_SCL_PIN     8       /* P0.08 */
#define I2C_INT_SDA_PIN     16      /* P0.16 */

/* ======================================================================
 *  9. GPIO Helper — Shorthand to build full register address
 * ====================================================================== */
#define PERIPH_GPIO_BASE(port)   ((port) == 0 ? GPIO_P0_BASE : GPIO_P1_BASE)
#define PERIPH_GPIO_DIRSET(port) (PERIPH_GPIO_BASE(port) + GPIO_DIRSET)
#define PERIPH_GPIO_OUTSET(port) (PERIPH_GPIO_BASE(port) + GPIO_OUTSET)
#define PERIPH_GPIO_OUTCLR(port) (PERIPH_GPIO_BASE(port) + GPIO_OUTCLR)
#define PERIPH_GPIO_IN(port)     (PERIPH_GPIO_BASE(port) + GPIO_IN)
#define PERIPH_GPIO_PIN_CNF(port, pin) (PERIPH_GPIO_BASE(port) + GPIO_PIN_CNF(pin))

#endif /* __MICROBIT_PERIPH_H__ */
