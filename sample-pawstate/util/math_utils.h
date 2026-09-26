/**
 * @file math_utils.h
 * @brief Fixed-Point Math Utilities for Feature Extraction
 *
 * All computations use Q16.16 fixed-point (int32_t) to avoid FPU
 * dependency and ensure deterministic timing in the feature extractor task.
 * Cortex-M4F has an FPU, but fixed-point keeps the code portable and
 * consistent with the INT8 quantised classifier.
 */

#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <stdint.h>
#include "pawstate_types.h"

/* --- Q16.16 Fixed-Point Helpers --- */
#define Q16_ONE         (1 << 16)
#define Q16_HALF        (1 << 15)
#define INT_TO_Q16(x)   ((q16_16_t)((x) << 16))
#define Q16_TO_INT(x)   ((int32_t)((x) >> 16))
#define Q16_FRAC(x)     ((x) & 0xFFFF)

/** Multiply two Q16.16 values. Uses 64-bit intermediate. */
q16_16_t q16_mul(q16_16_t a, q16_16_t b);

/** Divide two Q16.16 values. Returns a/b in Q16.16. */
q16_16_t q16_div(q16_16_t a, q16_16_t b);

/** Integer square root (Babylonian method). Input/output are integers. */
uint32_t isqrt(uint32_t x);

/** Q16.16 square root. */
q16_16_t q16_sqrt(q16_16_t x);

/** Fast atan2 approximation. Returns angle in degrees as Q16.16.
 *  Used for tilt angle computation from accelerometer. */
q16_16_t q16_atan2_deg(q16_16_t y, q16_16_t x);

/* --- Statistical Functions Operating on IMU Sample Arrays --- */

/** Compute mean of int16_t array (returns Q16.16) */
q16_16_t compute_mean_i16(const int16_t *data, uint32_t count);

/** Compute variance of int16_t array given pre-computed mean (returns Q16.16) */
q16_16_t compute_variance_i16(const int16_t *data, uint32_t count,
                               q16_16_t mean);

/** Compute RMS of int16_t array (returns Q16.16) */
q16_16_t compute_rms_i16(const int16_t *data, uint32_t count);

/** Count zero-crossings in int16_t array with hysteresis.
 *  Returns count as Q16.16 (rate = count / window_duration). */
q16_16_t compute_zero_crossings(const int16_t *data, uint32_t count,
                                 int16_t hysteresis);

/** Compute magnitude of 3D vector (ax, ay, az) as Q16.16 */
q16_16_t compute_magnitude_3d(int16_t x, int16_t y, int16_t z);

#endif /* MATH_UTILS_H */
