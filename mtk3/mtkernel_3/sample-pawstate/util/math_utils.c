/**
 * @file math_utils.c
 * @brief Fixed-Point Math Utilities Implementation
 *
 * Pure integer arithmetic — no floating-point. Uses 64-bit intermediates
 * only where overflow is possible (Q16.16 multiply).
 */

#include "math_utils.h"

q16_16_t q16_mul(q16_16_t a, q16_16_t b)
{
    int64_t result = (int64_t)a * (int64_t)b;
    return (q16_16_t)(result >> 16);
}

q16_16_t q16_div(q16_16_t a, q16_16_t b)
{
    if (b == 0) {
        return (a >= 0) ? 0x7FFFFFFF : (q16_16_t)0x80000001;
    }
    int64_t temp = (int64_t)a << 16;
    return (q16_16_t)(temp / b);
}

uint32_t isqrt(uint32_t x)
{
    if (x == 0) return 0;

    uint32_t guess = x;
    uint32_t prev;

    /* Babylonian method — converges in ~15 iterations for 32-bit */
    do {
        prev = guess;
        guess = (guess + x / guess) / 2;
    } while (guess < prev);

    return prev;
}

q16_16_t q16_sqrt(q16_16_t x)
{
    if (x <= 0) return 0;

    /* Scale up by 2^16 before integer sqrt, then result is in Q16.16.
     * x is Q16.16, so x represents x/65536 in real value.
     * sqrt(x/65536) = sqrt(x)/256, but we want Q16.16 result.
     * So: result_q16 = isqrt(x) * 256 = isqrt(x) << 8 */
    uint32_t root = isqrt((uint32_t)x);
    return (q16_16_t)(root << 8);
}

q16_16_t q16_atan2_deg(q16_16_t y, q16_16_t x)
{
    /* Fast polynomial approximation of atan2 in degrees.
     * Based on: atan(z) ≈ z * (45 - (z-1) * (14 + 3.83*z)) for |z| <= 1
     * Simplified to integer-friendly form. */

    if (x == 0 && y == 0) return 0;

    int32_t abs_y = (y < 0) ? -y : y;
    int32_t abs_x = (x < 0) ? -x : x;
    q16_16_t angle;

    if (abs_x >= abs_y) {
        /* |angle| <= 45 degrees */
        q16_16_t z = q16_div(abs_y, abs_x);
        /* atan(z) ≈ z * 45 degrees (linear approximation, good for small z) */
        angle = q16_mul(z, INT_TO_Q16(45));
    } else {
        /* |angle| > 45 degrees, use atan(z) = 90 - atan(1/z) */
        q16_16_t z = q16_div(abs_x, abs_y);
        angle = INT_TO_Q16(90) - q16_mul(z, INT_TO_Q16(45));
    }

    /* Quadrant correction */
    if (x < 0) angle = INT_TO_Q16(180) - angle;
    if (y < 0) angle = -angle;

    return angle;
}

q16_16_t compute_mean_i16(const int16_t *data, uint32_t count)
{
    if (count == 0) return 0;

    int32_t sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        sum += data[i];
    }
    /* Convert sum to Q16.16 then divide by count */
    return q16_div(INT_TO_Q16(sum), INT_TO_Q16((int32_t)count));
}

q16_16_t compute_variance_i16(const int16_t *data, uint32_t count,
                               q16_16_t mean)
{
    if (count < 2) return 0;

    int64_t sum_sq = 0;
    int32_t mean_int = Q16_TO_INT(mean);

    for (uint32_t i = 0; i < count; i++) {
        int32_t diff = (int32_t)data[i] - mean_int;
        sum_sq += (int64_t)diff * diff;
    }

    uint32_t var = (uint32_t)(sum_sq / (int64_t)(count - 1));
    /* Return std deviation instead of variance to avoid overflow */
    return INT_TO_Q16((int32_t)isqrt(var));
}

q16_16_t compute_rms_i16(const int16_t *data, uint32_t count)
{
    if (count == 0) return 0;

    int64_t sum_sq = 0;
    for (uint32_t i = 0; i < count; i++) {
        sum_sq += (int64_t)data[i] * data[i];
    }

    uint32_t mean_sq = (uint32_t)(sum_sq / count);
    /* Convert to Q16.16 then sqrt */
    return q16_sqrt(INT_TO_Q16((int32_t)isqrt(mean_sq)));
}

q16_16_t compute_zero_crossings(const int16_t *data, uint32_t count,
                                 int16_t hysteresis)
{
    if (count < 2) return 0;

    uint32_t crossings = 0;
    int state = (data[0] >= hysteresis) ? 1 : ((data[0] <= -hysteresis) ? -1 : 0);

    for (uint32_t i = 1; i < count; i++) {
        if (state >= 0 && data[i] <= -hysteresis) {
            crossings++;
            state = -1;
        } else if (state <= 0 && data[i] >= hysteresis) {
            crossings++;
            state = 1;
        }
    }

    return INT_TO_Q16((int32_t)crossings);
}

q16_16_t compute_magnitude_3d(int16_t x, int16_t y, int16_t z)
{
    int32_t sum_sq = (int32_t)x * x + (int32_t)y * y + (int32_t)z * z;
    uint32_t mag = isqrt((uint32_t)sum_sq);
    return INT_TO_Q16((int32_t)mag);
}
