/**
 * @file drv_i2c.h
 * @brief nRF52833 TWIM0 I2C Master Driver (Internal Bus)
 *
 * Drives the internal I2C bus (P0.08 SCL, P0.16 SDA) that connects
 * the nRF52833 to the on-board LSM303AGR sensor on the micro:bit v2.
 */

#ifndef DRV_I2C_H
#define DRV_I2C_H

#include <stdint.h>
#include <stdbool.h>

/** Initialise TWIM0 for internal I2C bus at 400 kHz */
void i2c_init(void);

/** Write `len` bytes from `data` to device at 7-bit `addr`.
 *  Returns true on success. */
bool i2c_write(uint8_t addr, const uint8_t *data, uint32_t len);

/** Read `len` bytes into `data` from device at 7-bit `addr`.
 *  Returns true on success. */
bool i2c_read(uint8_t addr, uint8_t *data, uint32_t len);

/** Write register address then read response (combined write-read).
 *  Writes `reg` byte, then reads `len` bytes into `data`.
 *  Returns true on success. */
bool i2c_write_read(uint8_t addr, uint8_t reg, uint8_t *data, uint32_t len);

/** Write a single byte to a register.
 *  Returns true on success. */
bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value);

/** De-initialise TWIM0 and release pins */
void i2c_deinit(void);

#endif /* DRV_I2C_H */
