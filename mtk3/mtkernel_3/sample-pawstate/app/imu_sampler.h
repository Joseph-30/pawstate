/**
 * @file imu_sampler.h
 * @brief Task 1 — IMU Sampler (Priority 1, Highest)
 *
 * Hard real-time 50 Hz sampling of the LSM303AGR accelerometer and
 * magnetometer. Writes raw samples to a lock-free circular buffer.
 * Signals the Feature Extractor after every full window (125 samples).
 */

#ifndef IMU_SAMPLER_H
#define IMU_SAMPLER_H

#include <tk/tkernel.h>
#include "pawstate_types.h"
#include "circular_buffer.h"

/** IMU Sampler task entry point (registered with tk_cre_tsk) */
void imu_sampler_task(INT stacd, void *exinf);

/** Get pointer to the shared circular buffer (for Feature Extractor) */
circular_buffer_t *imu_sampler_get_buffer(void);

/** Get the total number of samples collected since boot */
uint32_t imu_sampler_get_count(void);

/** Check if the IMU is healthy (WHO_AM_I verified) */
bool imu_sampler_is_healthy(void);

#endif /* IMU_SAMPLER_H */
