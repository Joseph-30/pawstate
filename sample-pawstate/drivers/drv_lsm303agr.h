/**
 * @file drv_lsm303agr.h
 * @brief LSM303AGR Accelerometer/Magnetometer Driver
 *
 * Complete register map and interface for the LSM303AGR on the
 * micro:bit v2 internal I2C bus.
 *
 * Reference: ST LSM303AGR Datasheet (DocID027765 Rev 8)
 */

#ifndef DRV_LSM303AGR_H
#define DRV_LSM303AGR_H

#include <stdint.h>
#include <stdbool.h>
#include "pawstate_types.h"

/* === I2C 7-bit Addresses === */
#define LSM303AGR_ACCEL_ADDR        0x19
#define LSM303AGR_MAG_ADDR          0x1E

/* === Accelerometer Register Map === */
#define ACCEL_WHO_AM_I              0x0F  /* Expected: 0x33 */
#define ACCEL_CTRL_REG1             0x20
#define ACCEL_CTRL_REG2             0x21
#define ACCEL_CTRL_REG3             0x22
#define ACCEL_CTRL_REG4             0x23
#define ACCEL_CTRL_REG5             0x24
#define ACCEL_CTRL_REG6             0x25
#define ACCEL_STATUS_REG            0x27
#define ACCEL_OUT_X_L               0x28
#define ACCEL_OUT_X_H               0x29
#define ACCEL_OUT_Y_L               0x2A
#define ACCEL_OUT_Y_H               0x2B
#define ACCEL_OUT_Z_L               0x2C
#define ACCEL_OUT_Z_H               0x2D
#define ACCEL_INT1_CFG              0x30
#define ACCEL_INT1_SRC              0x31
#define ACCEL_INT1_THS              0x32
#define ACCEL_INT1_DURATION         0x33

/* Auto-increment bit for burst reads */
#define ACCEL_AUTO_INC              0x80

/* CTRL_REG1 bit fields */
#define ACCEL_ODR_POWERDOWN         0x00
#define ACCEL_ODR_1HZ               0x10
#define ACCEL_ODR_10HZ              0x20
#define ACCEL_ODR_25HZ              0x30
#define ACCEL_ODR_50HZ              0x40
#define ACCEL_ODR_100HZ             0x50
#define ACCEL_ODR_200HZ             0x60
#define ACCEL_ODR_400HZ             0x70
#define ACCEL_LPEN                  0x08  /* Low-power enable */
#define ACCEL_ZEN                   0x04  /* Z-axis enable */
#define ACCEL_YEN                   0x02  /* Y-axis enable */
#define ACCEL_XEN                   0x01  /* X-axis enable */
#define ACCEL_AXES_ENABLE           (ACCEL_XEN | ACCEL_YEN | ACCEL_ZEN)

/* CTRL_REG4 bit fields */
#define ACCEL_BDU                   0x80  /* Block data update */
#define ACCEL_FS_2G                 0x00  /* ±2g */
#define ACCEL_FS_4G                 0x10  /* ±4g */
#define ACCEL_FS_8G                 0x20  /* ±8g */
#define ACCEL_FS_16G                0x30  /* ±16g */
#define ACCEL_HR                    0x08  /* High-resolution mode */

/* STATUS_REG bit fields */
#define ACCEL_STATUS_ZYXDA          0x08  /* New data available on all axes */

/* Expected WHO_AM_I response */
#define ACCEL_WHO_AM_I_VALUE        0x33

/* === Magnetometer Register Map === */
#define MAG_WHO_AM_I                0x4F  /* Expected: 0x40 */
#define MAG_CFG_REG_A               0x60
#define MAG_CFG_REG_B               0x61
#define MAG_CFG_REG_C               0x62
#define MAG_STATUS_REG              0x67
#define MAG_OUT_X_L                 0x68
#define MAG_OUT_X_H                 0x69
#define MAG_OUT_Y_L                 0x6A
#define MAG_OUT_Y_H                 0x6B
#define MAG_OUT_Z_L                 0x6C
#define MAG_OUT_Z_H                 0x6D

/* CFG_REG_A bit fields */
#define MAG_ODR_10HZ                0x00
#define MAG_ODR_20HZ                0x04
#define MAG_ODR_50HZ                0x08
#define MAG_ODR_100HZ               0x0C
#define MAG_MODE_CONTINUOUS         0x00
#define MAG_MODE_SINGLE             0x01
#define MAG_MODE_IDLE               0x03
#define MAG_COMP_TEMP_EN            0x80  /* Temperature compensation */

/* CFG_REG_C bit fields */
#define MAG_BDU                     0x10  /* Block data update */

/* STATUS_REG bit fields */
#define MAG_STATUS_ZYXDA            0x08  /* New data available */

/* Expected WHO_AM_I response */
#define MAG_WHO_AM_I_VALUE          0x40

/* === Public API === */

/** Initialise LSM303AGR: verify WHO_AM_I, configure ODR, range, BDU.
 *  Returns true if both accel and mag are detected and configured. */
bool lsm303agr_init(void);

/** Read 3-axis accelerometer data (raw int16_t values).
 *  Returns true on success. */
bool lsm303agr_read_accel(int16_t *ax, int16_t *ay, int16_t *az);

/** Read 3-axis magnetometer data (raw int16_t values).
 *  Returns true on success. */
bool lsm303agr_read_mag(int16_t *mx, int16_t *my, int16_t *mz);

/** Read all 6 axes (accel + mag) into an imu_sample_t struct.
 *  Returns true on success. */
bool lsm303agr_read_all(imu_sample_t *sample);

/** Check if new accelerometer data is available */
bool lsm303agr_accel_data_ready(void);

/** Check if new magnetometer data is available */
bool lsm303agr_mag_data_ready(void);

/** Put sensor into low-power mode */
void lsm303agr_sleep(void);

/** Wake sensor from low-power mode */
void lsm303agr_wake(void);

#endif /* DRV_LSM303AGR_H */
