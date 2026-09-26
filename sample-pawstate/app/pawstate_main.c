/**
 * @file pawstate_main.c
 * @brief PawState Application Entry Point — μT-Kernel 3.0 usermain()
 *
 * This is the application entry point called by μT-Kernel 3.0 after
 * the kernel has initialised. It creates all kernel objects (tasks,
 * semaphores, event flags, message buffers, cyclic handlers) and
 * starts the task scheduler.
 *
 * DESIGN DECISION: All kernel objects are created in usermain() before
 * any task is started. This ensures all IDs are valid before any task
 * attempts to use them. Tasks are started in reverse priority order
 * (lowest first) so higher-priority tasks don't preempt during the
 * startup sequence.
 *
 * Target: BBC micro:bit v2 (nRF52833) + μT-Kernel 3.0
 * Contest: TRON Programming Contest 2026
 */

#include "pawstate_main.h"
#include "pawstate_config.h"
#include "pawstate_types.h"
#include "imu_sampler.h"
#include "feature_extractor.h"
#include "tinyml_classifier.h"
#include "ble_logger.h"
#include "drv_led_matrix.h"
#include "drv_gpio.h"

#include <tk/tkernel.h>

/* =========================================================================
 * Global Kernel Object IDs
 * ========================================================================= */

ID tsk_imu_sampler     = 0;
ID tsk_feature_extractor = 0;
ID tsk_classifier      = 0;
ID tsk_ble_logger      = 0;

ID sem_i2c            = 0;
ID sem_feature_buf    = 0;

ID flg_pipeline       = 0;

ID mbf_ble_events     = 0;

ID cyc_imu_sample     = 0;
ID cyc_led_refresh    = 0;

/* =========================================================================
 * Cyclic Handler Callbacks
 *
 * These run in timer interrupt context. They must be short and must
 * not call blocking μT-Kernel APIs. They can call tk_wup_tsk and
 * tk_set_flg (both are safe from interrupt context).
 * ========================================================================= */

/**
 * 50 Hz IMU Sampling Cyclic Handler
 * Wakes the IMU Sampler task to read the sensor.
 */
static void cyc_imu_handler(void *exinf)
{
    (void)exinf;
    tk_wup_tsk(tsk_imu_sampler);
}

/**
 * LED Matrix Refresh Cyclic Handler
 * Performs one row-scan tick of the 5x5 LED matrix.
 * Runs at LED_REFRESH_RATE_HZ (120 Hz).
 */
static void cyc_led_handler(void *exinf)
{
    (void)exinf;
    led_matrix_scan_tick();
}

/* =========================================================================
 * Kernel Object Creation
 * ========================================================================= */

/**
 * Create all four application tasks.
 * Returns E_OK on success, error code on failure.
 */
static ER create_tasks(void)
{
    T_CTSK ctsk;

    /* --- Task 1: IMU Sampler (Priority 1 — highest) --- */
    ctsk.exinf   = (void *)0;
    ctsk.tskatr  = TA_HLNG | TA_RNG3;
    ctsk.task    = (FP)imu_sampler_task;
    ctsk.itskpri = TASK_PRI_IMU_SAMPLER;
    ctsk.stksz   = TASK_STACK_IMU_SAMPLER;

    tsk_imu_sampler = tk_cre_tsk(&ctsk);
    if (tsk_imu_sampler < E_OK) return tsk_imu_sampler;

    /* --- Task 2: Feature Extractor (Priority 5) --- */
    ctsk.exinf   = (void *)0;
    ctsk.tskatr  = TA_HLNG | TA_RNG3;
    ctsk.task    = (FP)feature_extractor_task;
    ctsk.itskpri = TASK_PRI_FEATURE_EXTRACT;
    ctsk.stksz   = TASK_STACK_FEATURE_EXTRACT;

    tsk_feature_extractor = tk_cre_tsk(&ctsk);
    if (tsk_feature_extractor < E_OK) return tsk_feature_extractor;

    /* --- Task 3: TinyML Classifier (Priority 10) --- */
    ctsk.exinf   = (void *)0;
    ctsk.tskatr  = TA_HLNG | TA_RNG3;
    ctsk.task    = (FP)tinyml_classifier_task;
    ctsk.itskpri = TASK_PRI_CLASSIFIER;
    ctsk.stksz   = TASK_STACK_CLASSIFIER;

    tsk_classifier = tk_cre_tsk(&ctsk);
    if (tsk_classifier < E_OK) return tsk_classifier;

    /* --- Task 4: BLE Event Logger (Priority 15 — lowest) --- */
    ctsk.exinf   = (void *)0;
    ctsk.tskatr  = TA_HLNG | TA_RNG3;
    ctsk.task    = (FP)ble_logger_task;
    ctsk.itskpri = TASK_PRI_BLE_LOGGER;
    ctsk.stksz   = TASK_STACK_BLE_LOGGER;

    tsk_ble_logger = tk_cre_tsk(&ctsk);
    if (tsk_ble_logger < E_OK) return tsk_ble_logger;

    return E_OK;
}

/**
 * Create semaphores for shared resource protection.
 */
static ER create_semaphores(void)
{
    T_CSEM csem;

    /* --- I2C Bus Mutex ---
     * Binary semaphore (max=1, initial=1) for I2C bus access.
     * The IMU Sampler acquires this before reading the sensor. */
    csem.exinf   = (void *)0;
    csem.sematr  = TA_TPRI;  /* Task priority queueing order */
    csem.isemcnt = 1;        /* Initially available */
    csem.maxsem  = 1;        /* Binary semaphore */

    sem_i2c = tk_cre_sem(&csem);
    if (sem_i2c < E_OK) return sem_i2c;

    /* --- Feature Buffer Mutex ---
     * Protects the shared feature vector between Feature Extractor
     * (writer) and Classifier (reader). */
    csem.exinf   = (void *)0;
    csem.sematr  = TA_TPRI;
    csem.isemcnt = 1;
    csem.maxsem  = 1;

    sem_feature_buf = tk_cre_sem(&csem);
    if (sem_feature_buf < E_OK) return sem_feature_buf;

    return E_OK;
}

/**
 * Create the pipeline event flag group.
 */
static ER create_event_flags(void)
{
    T_CFLG cflg;

    /* Single event flag with multiple bit positions for inter-task signaling.
     * TA_WMUL allows multiple tasks to wait on the same flag. */
    cflg.exinf   = (void *)0;
    cflg.flgatr  = TA_WMUL;  /* Multiple tasks can wait */
    cflg.iflgptn = 0;        /* All flags cleared initially */

    flg_pipeline = tk_cre_flg(&cflg);
    if (flg_pipeline < E_OK) return flg_pipeline;

    return E_OK;
}

/**
 * Create the BLE event message buffer.
 */
static ER create_message_buffers(void)
{
    T_CMBF cmbf;

    /* Message buffer for BLE events from Classifier → BLE Logger.
     * DESIGN DECISION: Buffer size accommodates BLE_EVENT_QUEUE_DEPTH
     * messages. Each message is sizeof(ble_event_t) + 4 bytes overhead
     * for the μT-Kernel message header. */
    cmbf.exinf   = (void *)0;
    cmbf.mbfatr  = TA_TFIFO;  /* FIFO ordering */
    cmbf.bufsz   = (BLE_EVENT_MSG_SIZE + 4) * BLE_EVENT_QUEUE_DEPTH;
    cmbf.maxmsz  = BLE_EVENT_MSG_SIZE;

    mbf_ble_events = tk_cre_mbf(&cmbf);
    if (mbf_ble_events < E_OK) return mbf_ble_events;

    return E_OK;
}

/**
 * Create cyclic handlers for periodic operations.
 */
static ER create_cyclic_handlers(void)
{
    T_CCYC ccyc;

    /* --- 50 Hz IMU Sampling Trigger ---
     * DESIGN DECISION: TA_STA flag starts the cyclic handler immediately
     * upon creation. The IMU Sampler task is already created (in DORMANT
     * state) at this point, so tk_wup_tsk in the handler is safe. */
    ccyc.exinf   = (void *)0;
    ccyc.cycatr  = TA_HLNG | TA_STA;  /* Start immediately */
    ccyc.cychdr  = (FP)cyc_imu_handler;
    ccyc.cyctim  = IMU_SAMPLE_PERIOD_MS;  /* 20ms = 50 Hz */
    ccyc.cycphs  = 0;  /* No initial phase delay */

    cyc_imu_sample = tk_cre_cyc(&ccyc);
    if (cyc_imu_sample < E_OK) return cyc_imu_sample;

    /* --- LED Matrix Refresh ---
     * Runs at 120 Hz to maintain flicker-free display via row scanning. */
    ccyc.exinf   = (void *)0;
    ccyc.cycatr  = TA_HLNG | TA_STA;
    ccyc.cychdr  = (FP)cyc_led_handler;
    ccyc.cyctim  = LED_REFRESH_PERIOD_MS;  /* ~8ms = 120 Hz */
    ccyc.cycphs  = 0;

    cyc_led_refresh = tk_cre_cyc(&ccyc);
    if (cyc_led_refresh < E_OK) return cyc_led_refresh;

    return E_OK;
}

/* =========================================================================
 * Application Entry Point
 *
 * usermain() is the μT-Kernel 3.0 application entry function.
 * It runs after the kernel initialises and before the scheduler starts
 * dispatching tasks. All setup must complete here.
 * ========================================================================= */

EXPORT INT usermain(void)
{
    ER err;

    /* --- Phase 1: Hardware Initialisation (pre-scheduler) --- */

    /* Initialise on-board peripherals */
    gpio_periph_init();
    led_matrix_init();

    /* Show boot animation on LED matrix */
    led_matrix_show_boot_animation();

    /* --- Phase 2: Create Kernel Objects --- */

    err = create_semaphores();
    if (err < E_OK) {
        /* Fatal: cannot create semaphores. Show error pattern on LEDs. */
        led_pattern_t err_pat = { .rows = {0x15, 0x0A, 0x15, 0x0A, 0x15} };
        led_matrix_set_pattern(&err_pat);
        return 1;
    }

    err = create_event_flags();
    if (err < E_OK) {
        return 1;
    }

    err = create_message_buffers();
    if (err < E_OK) {
        return 1;
    }

    err = create_tasks();
    if (err < E_OK) {
        return 1;
    }

    err = create_cyclic_handlers();
    if (err < E_OK) {
        return 1;
    }

    /* --- Phase 3: Start Tasks ---
     * DESIGN DECISION: Start in reverse priority order (lowest first)
     * so that higher-priority tasks are started last. This prevents
     * the IMU Sampler from running before the Feature Extractor is
     * ready. Each task initialises its own resources in its entry
     * function before entering its main loop. */

    tk_sta_tsk(tsk_ble_logger, 0);
    tk_sta_tsk(tsk_classifier, 0);
    tk_sta_tsk(tsk_feature_extractor, 0);
    tk_sta_tsk(tsk_imu_sampler, 0);

    /* --- Phase 4: usermain becomes the idle monitor ---
     * DESIGN DECISION: Rather than returning (which would terminate
     * the initial task), we keep usermain alive as a low-priority
     * system health monitor. It periodically updates the system
     * status and can detect fatal conditions.
     *
     * Note: In μT-Kernel 3.0, usermain runs as the "initial task"
     * at a configurable priority. We effectively make it the lowest
     * priority by having all our tasks at higher priorities. */

    for (;;) {
        /* Sleep for 10 seconds between health checks */
        tk_dly_tsk(10000);

        /* Read temperature periodically */
        int8_t temp = gpio_read_temperature();
        (void)temp; /* Available for system status reporting */

        /* Check IMU health */
        if (!imu_sampler_is_healthy()) {
            /* Show error pattern briefly */
            led_pattern_t err_pat = { .rows = {0x0A, 0x04, 0x0A, 0x04, 0x0A} };
            led_matrix_set_pattern(&err_pat);
            tk_dly_tsk(500);
            /* Restore normal state display */
            led_matrix_show_state(classifier_get_state());
        }
    }

    /* Never reached */
    return 0;
}
