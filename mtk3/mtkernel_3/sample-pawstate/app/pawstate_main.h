/**
 * @file pawstate_main.h
 * @brief PawState Application Entry Point Header
 *
 * Declares the μT-Kernel 3.0 usermain() entry and global kernel
 * object IDs shared across all tasks.
 */

#ifndef PAWSTATE_MAIN_H
#define PAWSTATE_MAIN_H

#include <tk/tkernel.h>

/* === Global Kernel Object IDs ===
 * These are created in usermain() and referenced by extern in task files.
 * DESIGN DECISION: Global IDs are the standard μT-Kernel pattern for
 * sharing kernel objects. Each ID is set once at creation and never
 * modified, so no synchronisation is needed for the IDs themselves. */

/** Task IDs */
extern ID tsk_imu_sampler;
extern ID tsk_feature_extractor;
extern ID tsk_classifier;
extern ID tsk_ble_logger;

/** Semaphore IDs */
extern ID sem_i2c;          /* I2C bus mutual exclusion */
extern ID sem_feature_buf;  /* Feature vector buffer protection */

/** Event Flag ID */
extern ID flg_pipeline;     /* Inter-task signaling */

/** Message Buffer ID */
extern ID mbf_ble_events;   /* BLE event queue */

/** Cyclic Handler IDs */
extern ID cyc_imu_sample;   /* 50 Hz IMU sampling trigger */
extern ID cyc_led_refresh;  /* LED matrix refresh */

#endif /* PAWSTATE_MAIN_H */
