/**
 * @file drv_i2c.c
 * @brief nRF52833 TWIM0 I2C Master Driver Implementation
 *
 * Uses EasyDMA for efficient data transfer. Blocking with timeout
 * to prevent deadlocks if sensor is unresponsive.
 *
 * DESIGN DECISION: Using TWIM0 (not TWIM1) because TWIM0 is the
 * standard instance for the internal I2C bus. TWIM1 is available
 * for external peripherals if needed in future.
 */

#include "drv_i2c.h"
#include "nrf52833_hal.h"
#include "pawstate_config.h"

/* DMA buffers must be in RAM (not flash). Static allocation. */
static uint8_t i2c_tx_buf[16];
static uint8_t i2c_rx_buf[16];

void i2c_init(void)
{
    /* Disable TWIM0 before configuration */
    TWIM0_ENABLE = TWIM_ENABLE_DISABLED;

    /* Configure SCL pin (P0.08): standard drive, no pull */
    P0_PIN_CNF(I2C_INT_SCL_PIN) = PIN_CNF_DIR_INPUT |
                                   PIN_CNF_INPUT_CONNECT |
                                   PIN_CNF_PULL_NONE |
                                   PIN_CNF_DRIVE_S0S1;

    /* Configure SDA pin (P0.16): standard drive, no pull */
    P0_PIN_CNF(I2C_INT_SDA_PIN) = PIN_CNF_DIR_INPUT |
                                   PIN_CNF_INPUT_CONNECT |
                                   PIN_CNF_PULL_NONE |
                                   PIN_CNF_DRIVE_S0S1;

    /* Select pins — PSEL format: bit[4:0] = pin, bit[5] = port (0=P0) */
    TWIM0_PSEL_SCL = (I2C_INT_SCL_PORT << 5) | I2C_INT_SCL_PIN;
    TWIM0_PSEL_SDA = (I2C_INT_SDA_PORT << 5) | I2C_INT_SDA_PIN;

    /* Set frequency to 400 kHz fast mode */
    TWIM0_FREQUENCY = TWIM_FREQ_400K;

    /* Enable TWIM0 */
    TWIM0_ENABLE = TWIM_ENABLE_ENABLED;
}

/**
 * Wait for an event register to become non-zero with timeout.
 * Returns true if event occurred, false on timeout.
 */
static bool i2c_wait_event(volatile uint32_t *event_reg)
{
    uint32_t timeout = I2C_TIMEOUT_MS * 10000; /* Rough loop count at 64 MHz */

    while (*event_reg == 0) {
        if (--timeout == 0) {
            /* Timeout — force stop */
            TWIM0_TASKS_STOP = 1;
            return false;
        }
    }

    *event_reg = 0; /* Clear the event */
    return true;
}

bool i2c_write(uint8_t addr, const uint8_t *data, uint32_t len)
{
    if (len == 0 || len > sizeof(i2c_tx_buf)) return false;

    /* Copy data to DMA-accessible buffer */
    for (uint32_t i = 0; i < len; i++) {
        i2c_tx_buf[i] = data[i];
    }

    /* Clear events */
    TWIM0_EVENTS_STOPPED = 0;
    TWIM0_EVENTS_ERROR = 0;

    /* Configure DMA pointers */
    TWIM0_ADDRESS = addr;
    TWIM0_TXD_PTR = (uint32_t)i2c_tx_buf;
    TWIM0_TXD_MAXCNT = len;

    /* Set shortcut: auto-stop after last TX byte */
    TWIM0_SHORTS = TWIM_SHORTS_LASTTX_STOP;

    /* Start TX */
    TWIM0_TASKS_STARTTX = 1;

    /* Wait for completion */
    if (!i2c_wait_event(&TWIM0_EVENTS_STOPPED)) {
        return false;
    }

    /* Check for errors */
    if (TWIM0_ERRORSRC != 0) {
        TWIM0_ERRORSRC = TWIM0_ERRORSRC; /* Write to clear */
        return false;
    }

    return true;
}

bool i2c_read(uint8_t addr, uint8_t *data, uint32_t len)
{
    if (len == 0 || len > sizeof(i2c_rx_buf)) return false;

    /* Clear events */
    TWIM0_EVENTS_STOPPED = 0;
    TWIM0_EVENTS_ERROR = 0;

    /* Configure DMA */
    TWIM0_ADDRESS = addr;
    TWIM0_RXD_PTR = (uint32_t)i2c_rx_buf;
    TWIM0_RXD_MAXCNT = len;

    /* Auto-stop after last RX byte */
    TWIM0_SHORTS = TWIM_SHORTS_LASTRX_STOP;

    /* Start RX */
    TWIM0_TASKS_STARTRX = 1;

    /* Wait for completion */
    if (!i2c_wait_event(&TWIM0_EVENTS_STOPPED)) {
        return false;
    }

    if (TWIM0_ERRORSRC != 0) {
        TWIM0_ERRORSRC = TWIM0_ERRORSRC;
        return false;
    }

    /* Copy from DMA buffer to output */
    for (uint32_t i = 0; i < len; i++) {
        data[i] = i2c_rx_buf[i];
    }

    return true;
}

bool i2c_write_read(uint8_t addr, uint8_t reg, uint8_t *data, uint32_t len)
{
    if (len == 0 || len > sizeof(i2c_rx_buf)) return false;

    /* Prepare TX buffer with register address */
    i2c_tx_buf[0] = reg;

    /* Clear events */
    TWIM0_EVENTS_STOPPED = 0;
    TWIM0_EVENTS_ERROR = 0;

    /* Configure address */
    TWIM0_ADDRESS = addr;

    /* Configure TX (register address byte) */
    TWIM0_TXD_PTR = (uint32_t)i2c_tx_buf;
    TWIM0_TXD_MAXCNT = 1;

    /* Configure RX (data read) */
    TWIM0_RXD_PTR = (uint32_t)i2c_rx_buf;
    TWIM0_RXD_MAXCNT = len;

    /* Shortcut: after last TX byte, start RX; after last RX byte, stop */
    TWIM0_SHORTS = TWIM_SHORTS_LASTTX_STARTRX | TWIM_SHORTS_LASTRX_STOP;

    /* Start the combined transaction */
    TWIM0_TASKS_STARTTX = 1;

    /* Wait for full transaction to complete */
    if (!i2c_wait_event(&TWIM0_EVENTS_STOPPED)) {
        return false;
    }

    if (TWIM0_ERRORSRC != 0) {
        TWIM0_ERRORSRC = TWIM0_ERRORSRC;
        return false;
    }

    /* Copy from DMA buffer */
    for (uint32_t i = 0; i < len; i++) {
        data[i] = i2c_rx_buf[i];
    }

    return true;
}

bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value)
{
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = value;
    return i2c_write(addr, buf, 2);
}

void i2c_deinit(void)
{
    TWIM0_ENABLE = TWIM_ENABLE_DISABLED;

    /* Disconnect pins */
    TWIM0_PSEL_SCL = 0xFFFFFFFF;
    TWIM0_PSEL_SDA = 0xFFFFFFFF;
}
