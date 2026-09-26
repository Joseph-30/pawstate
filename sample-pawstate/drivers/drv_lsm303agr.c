/**
 * @file drv_lsm303agr.c
 * @brief LSM303AGR Driver Implementation
 *
 * Configures accelerometer for 50 Hz, ±2g, high-resolution mode
 * and magnetometer for 50 Hz continuous mode with temperature
 * compensation. Uses burst reads with auto-increment for efficient
 * 6-byte transfers.
 */

#include "drv_lsm303agr.h"
#include "drv_i2c.h"

bool lsm303agr_init(void)
{
    uint8_t who_am_i;

    /* ---- Verify Accelerometer ---- */
    if (!i2c_write_read(LSM303AGR_ACCEL_ADDR, ACCEL_WHO_AM_I, &who_am_i, 1)) {
        return false;
    }
    if (who_am_i != ACCEL_WHO_AM_I_VALUE) {
        return false;
    }

    /* CTRL_REG1: 50 Hz ODR, all axes enabled, normal mode (not low-power).
     * DESIGN DECISION: 50 Hz matches our sampling requirement exactly.
     * Normal mode (not LP) gives 10-bit resolution. HR mode in CTRL_REG4
     * upgrades to 12-bit. */
    if (!i2c_write_reg(LSM303AGR_ACCEL_ADDR, ACCEL_CTRL_REG1,
                       ACCEL_ODR_50HZ | ACCEL_AXES_ENABLE)) {
        return false;
    }

    /* CTRL_REG4: ±2g full-scale, high-resolution, block data update.
     * DESIGN DECISION: ±2g gives maximum sensitivity for collar-mounted
     * use (dog motion typically <2g except during hard play). BDU prevents
     * reading mixed old/new data across register pairs. */
    if (!i2c_write_reg(LSM303AGR_ACCEL_ADDR, ACCEL_CTRL_REG4,
                       ACCEL_BDU | ACCEL_FS_2G | ACCEL_HR)) {
        return false;
    }

    /* ---- Verify Magnetometer ---- */
    if (!i2c_write_read(LSM303AGR_MAG_ADDR, MAG_WHO_AM_I, &who_am_i, 1)) {
        return false;
    }
    if (who_am_i != MAG_WHO_AM_I_VALUE) {
        return false;
    }

    /* CFG_REG_A: 50 Hz ODR, continuous mode, temperature compensation.
     * DESIGN DECISION: Matching accel ODR ensures synchronised sampling.
     * Temperature compensation improves magnetometer accuracy when the
     * collar heats up against the dog's body. */
    if (!i2c_write_reg(LSM303AGR_MAG_ADDR, MAG_CFG_REG_A,
                       MAG_ODR_50HZ | MAG_MODE_CONTINUOUS | MAG_COMP_TEMP_EN)) {
        return false;
    }

    /* CFG_REG_C: Block data update enabled */
    if (!i2c_write_reg(LSM303AGR_MAG_ADDR, MAG_CFG_REG_C, MAG_BDU)) {
        return false;
    }

    return true;
}

bool lsm303agr_read_accel(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t raw[6];

    /* Burst read 6 bytes starting at OUT_X_L with auto-increment (MSB set) */
    if (!i2c_write_read(LSM303AGR_ACCEL_ADDR,
                        ACCEL_OUT_X_L | ACCEL_AUTO_INC, raw, 6)) {
        return false;
    }

    /* Combine low/high bytes. LSM303AGR accel data is left-justified:
     * In high-resolution mode: 12-bit data in bits [15:4].
     * We keep the full 16-bit value for maximum precision in feature extraction. */
    *ax = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
    *ay = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
    *az = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);

    return true;
}

bool lsm303agr_read_mag(int16_t *mx, int16_t *my, int16_t *mz)
{
    uint8_t raw[6];

    /* Burst read 6 bytes starting at OUT_X_L_REG_M.
     * Magnetometer auto-increments by default on the LSM303AGR. */
    if (!i2c_write_read(LSM303AGR_MAG_ADDR, MAG_OUT_X_L, raw, 6)) {
        return false;
    }

    /* Magnetometer data is 16-bit two's complement, right-justified */
    *mx = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
    *my = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
    *mz = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);

    return true;
}

bool lsm303agr_read_all(imu_sample_t *sample)
{
    if (!lsm303agr_read_accel(&sample->ax, &sample->ay, &sample->az)) {
        return false;
    }
    if (!lsm303agr_read_mag(&sample->mx, &sample->my, &sample->mz)) {
        return false;
    }
    return true;
}

bool lsm303agr_accel_data_ready(void)
{
    uint8_t status;
    if (!i2c_write_read(LSM303AGR_ACCEL_ADDR, ACCEL_STATUS_REG, &status, 1)) {
        return false;
    }
    return (status & ACCEL_STATUS_ZYXDA) != 0;
}

bool lsm303agr_mag_data_ready(void)
{
    uint8_t status;
    if (!i2c_write_read(LSM303AGR_MAG_ADDR, MAG_STATUS_REG, &status, 1)) {
        return false;
    }
    return (status & MAG_STATUS_ZYXDA) != 0;
}

void lsm303agr_sleep(void)
{
    /* Put accelerometer in power-down mode */
    i2c_write_reg(LSM303AGR_ACCEL_ADDR, ACCEL_CTRL_REG1, ACCEL_ODR_POWERDOWN);

    /* Put magnetometer in idle mode */
    i2c_write_reg(LSM303AGR_MAG_ADDR, MAG_CFG_REG_A, MAG_MODE_IDLE);
}

void lsm303agr_wake(void)
{
    /* Restore accelerometer to 50 Hz */
    i2c_write_reg(LSM303AGR_ACCEL_ADDR, ACCEL_CTRL_REG1,
                  ACCEL_ODR_50HZ | ACCEL_AXES_ENABLE);

    /* Restore magnetometer to 50 Hz continuous */
    i2c_write_reg(LSM303AGR_MAG_ADDR, MAG_CFG_REG_A,
                  MAG_ODR_50HZ | MAG_MODE_CONTINUOUS | MAG_COMP_TEMP_EN);
}
