/**
 * @file imu_sampler.c
 * @brief Task 1 — IMU Sampler Implementation
 *
 * DESIGN DECISION: This task uses μT-Kernel's tk_slp_tsk/tk_wup_tsk
 * mechanism triggered by a cyclic handler rather than a hardware timer
 * interrupt. The cyclic handler runs at 50 Hz (every 20ms) and wakes
 * the sampling task, which then reads the IMU and stores the data.
 *
 * Why not a direct timer ISR?
 *   - ISRs in μT-Kernel should be kept minimal (just set flags/wake tasks)
 *   - I2C reads take ~500μs at 400kHz and should not run in ISR context
 *   - The cyclic handler → task wake pattern keeps ISR duration <10μs
 *   - μT-Kernel guarantees the woken task runs immediately if it's the
 *     highest priority ready task (which it is, at priority 1)
 *
 * The total sample-to-buffer latency is:
 *   Cyclic handler trigger → task wake → I2C read → buffer write
 *   ≈ 10μs + 500μs + 5μs = ~515μs (well within the 20ms period)
 */

#include "imu_sampler.h"
#include "drv_lsm303agr.h"
#include "drv_i2c.h"
#include "pawstate_config.h"

#include <tk/tkernel.h>
#include <tm/tmonitor.h>

/* --- Module State --- */
static circular_buffer_t imu_buffer;
static volatile uint32_t sample_count = 0;
static volatile uint32_t window_sample_count = 0;
static volatile bool imu_healthy = false;

/* Event flag ID (set by pawstate_main, used here to signal) */
extern ID flg_pipeline;

/* Semaphore for I2C bus mutual exclusion */
extern ID sem_i2c;

circular_buffer_t *imu_sampler_get_buffer(void)
{
    return &imu_buffer;
}

uint32_t imu_sampler_get_count(void)
{
    return sample_count;
}

bool imu_sampler_is_healthy(void)
{
    return imu_healthy;
}

/**
 * IMU Sampler Task — Priority 1 (highest)
 *
 * Runs in an infinite loop, sleeping between samples. Woken by the
 * cyclic handler (cyc_imu_sample) at 50 Hz.
 *
 * The task:
 *   1. Acquires the I2C semaphore (immediate, since no lower-priority
 *      task should be holding it when we wake)
 *   2. Reads 6-axis data from LSM303AGR
 *   3. Releases I2C semaphore
 *   4. Writes sample to circular buffer (lock-free)
 *   5. After 125 samples, sets EVT_NEW_SAMPLES event flag
 *   6. Goes back to sleep
 */
void imu_sampler_task(INT stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;

    imu_sample_t sample;

    /* Initialise the circular buffer */
    cbuf_init(&imu_buffer);

    /* Initialise I2C bus */
    i2c_init();

    /* Initialise and verify IMU sensor */
    imu_healthy = lsm303agr_init();

    if (!imu_healthy) {
        /* IMU init failed — enter error state.
         * DESIGN DECISION: Don't crash the system. Keep the task alive
         * but in a sleep loop, periodically retrying init. This allows
         * the rest of the system (BLE, LED) to still function and report
         * the error condition. */
        while (!imu_healthy) {
            tk_dly_tsk(1000); /* Retry every 1 second */
            imu_healthy = lsm303agr_init();
        }
    }

    /* Main sampling loop */
    for (;;) {
        /* Sleep until woken by the 50 Hz cyclic handler */
        tk_slp_tsk(TMO_FEVR);

        /* Acquire I2C bus (should be immediate at our priority) */
        tk_wai_sem(sem_i2c, 1, TMO_POL);

        /* Read all 6 axes from IMU */
        bool read_ok = lsm303agr_read_all(&sample);

        /* Release I2C bus */
        tk_sig_sem(sem_i2c, 1);

        if (read_ok) {
            /* Write to lock-free circular buffer (no blocking) */
            cbuf_write(&imu_buffer, &sample);

            sample_count++;
            window_sample_count++;

            /* Signal Feature Extractor after a full window */
            if (window_sample_count >= FEATURE_WINDOW_SIZE) {
                window_sample_count = 0;
                tm_printf((const UB*)"[IMU] Window full (125 samples). Last:ax=%d,ay=%d,az=%d\n",
                         sample.ax, sample.ay, sample.az);
                tk_set_flg(flg_pipeline, EVT_NEW_SAMPLES);
            }
        } else {
            /* I2C read failed — mark unhealthy but continue.
             * The feature extractor will notice the gap. */
            imu_healthy = false;
        }
    }
}
