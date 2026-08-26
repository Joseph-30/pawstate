/**
 * @file nrf52833_hal.h
 * @brief nRF52833 Hardware Abstraction — Register-Level Definitions
 *
 * Direct register addresses for GPIO, TWIM (I2C), TIMER, GPIOTE, and NVIC
 * on the nRF52833 SoC (micro:bit v2). Used by PawState low-level drivers.
 *
 * Reference: nRF52833 Product Specification v1.5
 * Target: BBC micro:bit v2, μT-Kernel 3.0
 */

#ifndef NRF52833_HAL_H
#define NRF52833_HAL_H

#include <stdint.h>

/* =========================================================================
 * Register access macros
 * ========================================================================= */
#define REG32(addr)     (*(volatile uint32_t *)(addr))
#define REG_SET(r,b)    ((r) |=  (b))
#define REG_CLR(r,b)    ((r) &= ~(b))

/* =========================================================================
 * GPIO — Port 0 (P0) and Port 1 (P1)
 * ========================================================================= */
#define NRF_P0_BASE             0x50000000UL
#define NRF_P1_BASE             0x50000300UL

/* Register offsets from port base */
#define GPIO_OUT_OFFSET         0x504
#define GPIO_OUTSET_OFFSET      0x508
#define GPIO_OUTCLR_OFFSET      0x50C
#define GPIO_IN_OFFSET          0x510
#define GPIO_DIR_OFFSET         0x514
#define GPIO_DIRSET_OFFSET      0x518
#define GPIO_DIRCLR_OFFSET      0x51C
#define GPIO_PIN_CNF_OFFSET(n)  (0x700 + ((n) * 4))

/* P0 convenience registers */
#define P0_OUT                  REG32(NRF_P0_BASE + GPIO_OUT_OFFSET)
#define P0_OUTSET               REG32(NRF_P0_BASE + GPIO_OUTSET_OFFSET)
#define P0_OUTCLR               REG32(NRF_P0_BASE + GPIO_OUTCLR_OFFSET)
#define P0_IN                   REG32(NRF_P0_BASE + GPIO_IN_OFFSET)
#define P0_DIR                  REG32(NRF_P0_BASE + GPIO_DIR_OFFSET)
#define P0_DIRSET               REG32(NRF_P0_BASE + GPIO_DIRSET_OFFSET)
#define P0_PIN_CNF(n)           REG32(NRF_P0_BASE + GPIO_PIN_CNF_OFFSET(n))

/* P1 convenience registers */
#define P1_OUT                  REG32(NRF_P1_BASE + GPIO_OUT_OFFSET)
#define P1_OUTSET               REG32(NRF_P1_BASE + GPIO_OUTSET_OFFSET)
#define P1_OUTCLR               REG32(NRF_P1_BASE + GPIO_OUTCLR_OFFSET)
#define P1_IN                   REG32(NRF_P1_BASE + GPIO_IN_OFFSET)
#define P1_DIRSET               REG32(NRF_P1_BASE + GPIO_DIRSET_OFFSET)
#define P1_PIN_CNF(n)           REG32(NRF_P1_BASE + GPIO_PIN_CNF_OFFSET(n))

/* PIN_CNF field values */
#define PIN_CNF_DIR_OUTPUT      (1UL << 0)
#define PIN_CNF_DIR_INPUT       (0UL << 0)
#define PIN_CNF_INPUT_CONNECT   (0UL << 1)
#define PIN_CNF_INPUT_DISCONNECT (1UL << 1)
#define PIN_CNF_PULL_NONE       (0UL << 2)
#define PIN_CNF_PULL_DOWN       (1UL << 2)
#define PIN_CNF_PULL_UP         (3UL << 2)
#define PIN_CNF_DRIVE_S0S1      (0UL << 8)
#define PIN_CNF_DRIVE_H0S1      (1UL << 8)
#define PIN_CNF_DRIVE_S0H1      (2UL << 8)
#define PIN_CNF_DRIVE_H0H1      (3UL << 8)

/* =========================================================================
 * micro:bit v2 — LED Matrix Pin Mapping (from schematic)
 * ========================================================================= */
#define LED_ROW1_PORT   0
#define LED_ROW1_PIN    21
#define LED_ROW2_PORT   0
#define LED_ROW2_PIN    22
#define LED_ROW3_PORT   0
#define LED_ROW3_PIN    15
#define LED_ROW4_PORT   0
#define LED_ROW4_PIN    24
#define LED_ROW5_PORT   0
#define LED_ROW5_PIN    19

#define LED_COL1_PORT   0
#define LED_COL1_PIN    28
#define LED_COL2_PORT   0
#define LED_COL2_PIN    11
#define LED_COL3_PORT   0
#define LED_COL3_PIN    31
#define LED_COL4_PORT   1
#define LED_COL4_PIN    5
#define LED_COL5_PORT   0
#define LED_COL5_PIN    30

/* =========================================================================
 * micro:bit v2 — Button Pins
 * ========================================================================= */
#define BUTTON_A_PORT   0
#define BUTTON_A_PIN    14
#define BUTTON_B_PORT   0
#define BUTTON_B_PIN    23

/* =========================================================================
 * micro:bit v2 — Speaker Pin
 * ========================================================================= */
#define SPEAKER_PORT    0
#define SPEAKER_PIN     0

/* =========================================================================
 * micro:bit v2 — Internal I2C Bus (on-board sensors)
 * ========================================================================= */
#define I2C_INT_SCL_PORT  0
#define I2C_INT_SCL_PIN   8
#define I2C_INT_SDA_PORT  0
#define I2C_INT_SDA_PIN   16

/* =========================================================================
 * TWIM0 — Two-Wire Interface Master with EasyDMA
 * Base address: 0x40003000
 * ========================================================================= */
#define NRF_TWIM0_BASE           0x40003000UL

#define TWIM0_TASKS_STARTRX      REG32(NRF_TWIM0_BASE + 0x000)
#define TWIM0_TASKS_STARTTX      REG32(NRF_TWIM0_BASE + 0x008)
#define TWIM0_TASKS_STOP         REG32(NRF_TWIM0_BASE + 0x014)
#define TWIM0_TASKS_RESUME       REG32(NRF_TWIM0_BASE + 0x01C)
#define TWIM0_EVENTS_STOPPED     REG32(NRF_TWIM0_BASE + 0x104)
#define TWIM0_EVENTS_ERROR       REG32(NRF_TWIM0_BASE + 0x124)
#define TWIM0_EVENTS_RXSTARTED   REG32(NRF_TWIM0_BASE + 0x14C)
#define TWIM0_EVENTS_TXSTARTED   REG32(NRF_TWIM0_BASE + 0x150)
#define TWIM0_EVENTS_LASTRX      REG32(NRF_TWIM0_BASE + 0x15C)
#define TWIM0_EVENTS_LASTTX      REG32(NRF_TWIM0_BASE + 0x160)
#define TWIM0_SHORTS             REG32(NRF_TWIM0_BASE + 0x200)
#define TWIM0_ERRORSRC           REG32(NRF_TWIM0_BASE + 0x4C4)
#define TWIM0_ENABLE             REG32(NRF_TWIM0_BASE + 0x500)
#define TWIM0_PSEL_SCL           REG32(NRF_TWIM0_BASE + 0x508)
#define TWIM0_PSEL_SDA           REG32(NRF_TWIM0_BASE + 0x50C)
#define TWIM0_FREQUENCY          REG32(NRF_TWIM0_BASE + 0x524)
#define TWIM0_RXD_PTR            REG32(NRF_TWIM0_BASE + 0x534)
#define TWIM0_RXD_MAXCNT         REG32(NRF_TWIM0_BASE + 0x538)
#define TWIM0_RXD_AMOUNT         REG32(NRF_TWIM0_BASE + 0x53C)
#define TWIM0_TXD_PTR            REG32(NRF_TWIM0_BASE + 0x544)
#define TWIM0_TXD_MAXCNT         REG32(NRF_TWIM0_BASE + 0x548)
#define TWIM0_TXD_AMOUNT         REG32(NRF_TWIM0_BASE + 0x54C)
#define TWIM0_ADDRESS            REG32(NRF_TWIM0_BASE + 0x588)

/* TWIM ENABLE values */
#define TWIM_ENABLE_DISABLED     0UL
#define TWIM_ENABLE_ENABLED      6UL

/* TWIM FREQUENCY values */
#define TWIM_FREQ_100K           0x01980000UL
#define TWIM_FREQ_250K           0x04000000UL
#define TWIM_FREQ_400K           0x06400000UL

/* TWIM SHORTS bits */
#define TWIM_SHORTS_LASTTX_STARTRX  (1UL << 7)
#define TWIM_SHORTS_LASTTX_STOP     (1UL << 9)
#define TWIM_SHORTS_LASTRX_STOP     (1UL << 12)

/* =========================================================================
 * TIMER2 — Used for 50 Hz IMU sampling interrupt
 * Base address: 0x4000A000
 * DESIGN DECISION: TIMER0 is reserved by SoftDevice, TIMER1 by μT-Kernel
 * system tick. TIMER2 is available for application use.
 * ========================================================================= */
#define NRF_TIMER2_BASE          0x4000A000UL

#define TIMER2_TASKS_START       REG32(NRF_TIMER2_BASE + 0x000)
#define TIMER2_TASKS_STOP        REG32(NRF_TIMER2_BASE + 0x004)
#define TIMER2_TASKS_CLEAR       REG32(NRF_TIMER2_BASE + 0x00C)
#define TIMER2_EVENTS_COMPARE0   REG32(NRF_TIMER2_BASE + 0x140)
#define TIMER2_SHORTS            REG32(NRF_TIMER2_BASE + 0x200)
#define TIMER2_INTENSET          REG32(NRF_TIMER2_BASE + 0x304)
#define TIMER2_INTENCLR          REG32(NRF_TIMER2_BASE + 0x308)
#define TIMER2_MODE              REG32(NRF_TIMER2_BASE + 0x504)
#define TIMER2_BITMODE           REG32(NRF_TIMER2_BASE + 0x508)
#define TIMER2_PRESCALER         REG32(NRF_TIMER2_BASE + 0x510)
#define TIMER2_CC0               REG32(NRF_TIMER2_BASE + 0x540)

/* TIMER SHORTS bits */
#define TIMER_SHORTS_COMPARE0_CLEAR  (1UL << 0)

/* TIMER INTENSET bits */
#define TIMER_INTENSET_COMPARE0      (1UL << 16)

/* TIMER MODE values */
#define TIMER_MODE_TIMER         0UL
#define TIMER_MODE_COUNTER       1UL

/* TIMER BITMODE values */
#define TIMER_BITMODE_16BIT      0UL
#define TIMER_BITMODE_32BIT      3UL

/* =========================================================================
 * GPIOTE — GPIO Tasks and Events (for button interrupts)
 * ========================================================================= */
#define NRF_GPIOTE_BASE          0x40006000UL

#define GPIOTE_EVENTS_IN(n)      REG32(NRF_GPIOTE_BASE + 0x100 + ((n) * 4))
#define GPIOTE_INTENSET          REG32(NRF_GPIOTE_BASE + 0x304)
#define GPIOTE_INTENCLR          REG32(NRF_GPIOTE_BASE + 0x308)
#define GPIOTE_CONFIG(n)         REG32(NRF_GPIOTE_BASE + 0x510 + ((n) * 4))

/* GPIOTE CONFIG field values */
#define GPIOTE_MODE_EVENT        1UL
#define GPIOTE_POLARITY_HITOLO   2UL
#define GPIOTE_POLARITY_TOGGLE   3UL

/* =========================================================================
 * NVIC — Nested Vectored Interrupt Controller
 * ========================================================================= */
#define NVIC_ISER_BASE           0xE000E100UL
#define NVIC_ICER_BASE           0xE000E180UL
#define NVIC_ISPR_BASE           0xE000E200UL
#define NVIC_IPR_BASE            0xE000E400UL

#define NVIC_ISER(n)             REG32(NVIC_ISER_BASE + ((n) * 4))
#define NVIC_ICER(n)             REG32(NVIC_ICER_BASE + ((n) * 4))

/* nRF52833 IRQ numbers */
#define GPIOTE_IRQn              6
#define TIMER2_IRQn              10
#define TWIM0_IRQn               3

static inline void nvic_enable_irq(uint32_t irqn) {
    NVIC_ISER(irqn >> 5) = (1UL << (irqn & 0x1F));
}

static inline void nvic_disable_irq(uint32_t irqn) {
    NVIC_ICER(irqn >> 5) = (1UL << (irqn & 0x1F));
}

static inline void nvic_set_priority(uint32_t irqn, uint8_t priority) {
    volatile uint8_t *ipr = (volatile uint8_t *)(NVIC_IPR_BASE + irqn);
    *ipr = (priority << 5); /* nRF52833 uses top 3 bits of 8-bit priority field */
}

/* =========================================================================
 * Temperature Sensor (on-chip, nRF52833)
 * ========================================================================= */
#define NRF_TEMP_BASE            0x4000C000UL
#define TEMP_TASKS_START         REG32(NRF_TEMP_BASE + 0x000)
#define TEMP_TASKS_STOP          REG32(NRF_TEMP_BASE + 0x004)
#define TEMP_EVENTS_DATARDY      REG32(NRF_TEMP_BASE + 0x100)
#define TEMP_VALUE               REG32(NRF_TEMP_BASE + 0x508)

/* =========================================================================
 * PWM0 — For speaker tone generation
 * ========================================================================= */
#define NRF_PWM0_BASE            0x4001C000UL
#define PWM0_TASKS_SEQSTART0     REG32(NRF_PWM0_BASE + 0x008)
#define PWM0_TASKS_STOP          REG32(NRF_PWM0_BASE + 0x004)
#define PWM0_EVENTS_STOPPED      REG32(NRF_PWM0_BASE + 0x104)
#define PWM0_ENABLE              REG32(NRF_PWM0_BASE + 0x500)
#define PWM0_MODE                REG32(NRF_PWM0_BASE + 0x504)
#define PWM0_COUNTERTOP          REG32(NRF_PWM0_BASE + 0x508)
#define PWM0_PRESCALER           REG32(NRF_PWM0_BASE + 0x50C)
#define PWM0_DECODER             REG32(NRF_PWM0_BASE + 0x510)
#define PWM0_LOOP                REG32(NRF_PWM0_BASE + 0x514)
#define PWM0_SEQ0_PTR            REG32(NRF_PWM0_BASE + 0x520)
#define PWM0_SEQ0_CNT            REG32(NRF_PWM0_BASE + 0x524)
#define PWM0_SEQ0_REFRESH        REG32(NRF_PWM0_BASE + 0x528)
#define PWM0_PSELOUT0            REG32(NRF_PWM0_BASE + 0x560)
#define PWM0_PSELOUT1            REG32(NRF_PWM0_BASE + 0x564)
#define PWM0_PSELOUT2            REG32(NRF_PWM0_BASE + 0x568)
#define PWM0_PSELOUT3            REG32(NRF_PWM0_BASE + 0x56C)

#endif /* NRF52833_HAL_H */
