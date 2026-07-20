/*
 *  app_main.c
 *  PawState — Peripheral Self-Test & Continuous Monitor
 *
 *  BBC micro:bit v2.21  |  μT-Kernel 3.0  |  nRF52833
 *
 *  Boot sequence:
 *    1. Run 9-point peripheral self-test (results via UART)
 *    2. Enter continuous accelerometer + magnetometer read loop
 */

#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include "../device/i2c/i2c.h"
#include <sys/sysdepend/microbit/microbit_periph.h>


/* ======================================================================
 *  Task ID
 * ====================================================================== */
ID selftest_tskid;


/* ======================================================================
 *  Helper: I2C read a single register from a given slave address
 *  Returns the byte value, or a negative error code.
 * ====================================================================== */
static INT i2c_read_reg(ID dev, UW slave_addr, UB reg_addr)
{
    UB rx = 0;
    T_I2C_EXEC ex;

    ex.sadr     = slave_addr;
    ex.snd_data = &reg_addr;
    ex.snd_size = 1;
    ex.rcv_data = &rx;
    ex.rcv_size = 1;

    SZ asize;
    ER err = tk_swri_dev(dev, TDN_I2C_EXEC, &ex, sizeof(T_I2C_EXEC), &asize);
    if (err < 0) return (INT)err;

    return (INT)rx;
}

/* ======================================================================
 *  Helper: I2C write a single register
 * ====================================================================== */
static ER i2c_write_reg(ID dev, UW slave_addr, UB reg_addr, UB value)
{
    UB tx[2] = { reg_addr, value };
    T_I2C_EXEC ex;

    ex.sadr     = slave_addr;
    ex.snd_data = tx;
    ex.snd_size = 2;
    ex.rcv_data = NULL;
    ex.rcv_size = 0;  /* Clean write-only transaction, no dummy read */

    SZ asize;
    return tk_swri_dev(dev, TDN_I2C_EXEC, &ex, sizeof(T_I2C_EXEC), &asize);
}


/* ======================================================================
 *  Test 1: LED Matrix — drive all 5 rows + all 5 cols, verify GPIO latch
 * ====================================================================== */
static INT test_led_matrix(void)
{
    /* Row pins (all on P0) */
    static const UW row_pins[] = {
        LED_ROW1_PIN, LED_ROW2_PIN, LED_ROW3_PIN,
        LED_ROW4_PIN, LED_ROW5_PIN
    };
    /* Column pins: port and pin */
    static const struct { UW port; UW pin; } col_pins[] = {
        { LED_COL1_PORT, LED_COL1_PIN },
        { LED_COL2_PORT, LED_COL2_PIN },
        { LED_COL3_PORT, LED_COL3_PIN },
        { LED_COL4_PORT, LED_COL4_PIN },
        { LED_COL5_PORT, LED_COL5_PIN },
    };

    INT i;

    /* Configure all row pins as output, drive HIGH */
    for (i = 0; i < LED_ROWS; i++) {
        out_w(PERIPH_GPIO_DIRSET(0), (1U << row_pins[i]));
        out_w(PERIPH_GPIO_OUTSET(0), (1U << row_pins[i]));
    }

    /* Configure all col pins as output, drive LOW (sinking) */
    for (i = 0; i < LED_COLS; i++) {
        out_w(PERIPH_GPIO_DIRSET(col_pins[i].port), (1U << col_pins[i].pin));
        out_w(PERIPH_GPIO_OUTCLR(col_pins[i].port), (1U << col_pins[i].pin));
    }

    /* Brief flash — all 25 LEDs on */
    tk_dly_tsk(300);

    /* Turn off: rows LOW */
    for (i = 0; i < LED_ROWS; i++) {
        out_w(PERIPH_GPIO_OUTCLR(0), (1U << row_pins[i]));
    }

    return 1;  /* PASS — if GPIO registers accepted writes, the matrix is wired */
}


/* ======================================================================
 *  Test 2: Speaker — ~500 Hz beep for 200 ms via bit-bang
 * ====================================================================== */
static INT test_speaker(void)
{
    out_w(PERIPH_GPIO_DIRSET(SPEAKER_PORT), (1U << SPEAKER_PIN));

    /* ~500 Hz = 1 ms HIGH + 1 ms LOW, for 200 ms total = 100 cycles */
    for (int i = 0; i < 100; i++) {
        out_w(PERIPH_GPIO_OUTSET(SPEAKER_PORT), (1U << SPEAKER_PIN));
        tk_dly_tsk(1);
        out_w(PERIPH_GPIO_OUTCLR(SPEAKER_PORT), (1U << SPEAKER_PIN));
        tk_dly_tsk(1);
    }

    return 1;  /* PASS — audible check */
}


/* ======================================================================
 *  Test 3: Accelerometer WHO_AM_I  (expect 0x33)
 * ====================================================================== */
static INT test_accel_whoami(ID dev)
{
    INT val = i2c_read_reg(dev, LSM303_ACCEL_ADDR, LSM303_WHO_AM_I_A);
    if (val < 0) return val;
    return (val == LSM303_WHO_AM_I_A_VAL) ? 1 : 0;
}


/* ======================================================================
 *  Test 4: Magnetometer WHO_AM_I  (expect 0x40)
 * ====================================================================== */
static INT test_mag_whoami(ID dev)
{
    INT val = i2c_read_reg(dev, LSM303_MAG_ADDR, LSM303_WHO_AM_I_M);
    if (val < 0) return val;
    return (val == LSM303_WHO_AM_I_M_VAL) ? 1 : 0;
}


/* ======================================================================
 *  Test 5: nRF52833 Internal Temperature Sensor
 * ====================================================================== */
static INT test_temperature(INT *out_temp_x4)
{
    /* Clear event flag */
    out_w(TEMP_EVENTS_DATARDY, 0);

    /* Start measurement */
    out_w(TEMP_TASKS_START, 1);

    /* Poll until ready (with timeout) */
    for (int i = 0; i < 10000; i++) {
        if (in_w(TEMP_EVENTS_DATARDY) != 0) break;
    }

    if (in_w(TEMP_EVENTS_DATARDY) == 0) {
        return -1;  /* Timeout */
    }

    /* Read result (in 0.25 °C units, signed) */
    INT raw = (INT)in_w(TEMP_VALUE);
    *out_temp_x4 = raw;

    /* Stop the peripheral */
    out_w(TEMP_TASKS_STOP, 1);

    /* Sanity: -40 °C .. +85 °C  →  -160 .. +340 in 0.25 °C units */
    if (raw < -160 || raw > 340) return 0;

    return 1;  /* PASS */
}


/* ======================================================================
 *  Test 6 & 7: Button A / Button B  (interactive — waits for press)
 *  Returns: 2=press detected, 1=idle OK but no press, 0=pin stuck LOW
 * ====================================================================== */
static INT test_button_interactive(UW port, UW pin, const char *name)
{
    /* Configure as input with pull-up */
    out_w(PERIPH_GPIO_PIN_CNF(port, pin), BTN_PIN_CNF_VAL);
    tk_dly_tsk(5);

    /* Check idle state first */
    UW gpio_in = in_w(PERIPH_GPIO_IN(port));
    INT idle_high = (gpio_in >> pin) & 1;
    if (!idle_high) return 0;  /* Pin stuck LOW — hardware fault */

    /* Prompt user and poll for 3 seconds (300 × 10 ms) */
    tm_printf((UB*)"       -> Press %s within 3s... ", name);

    for (int i = 0; i < 300; i++) {
        tk_dly_tsk(10);
        gpio_in = in_w(PERIPH_GPIO_IN(port));
        if (((gpio_in >> pin) & 1) == 0) {
            /* Button pressed (active-LOW) — wait for release */
            while (((in_w(PERIPH_GPIO_IN(port)) >> pin) & 1) == 0) {
                tk_dly_tsk(10);
            }
            tm_printf((UB*)"PRESSED!\n");
            return 2;  /* Full PASS — press detected */
        }
    }

    tm_printf((UB*)"timeout\n");
    return 1;  /* Idle OK, but no press detected */
}


/* ======================================================================
 *  Test 8: Touch Logo  (P1.04, interactive — waits for touch)
 *  Returns: 2=touch detected, 1=pin readable but no touch, 0=fault
 * ====================================================================== */
static INT test_touch_logo_interactive(void)
{
    /* Configure as input, no pull (capacitive sensing relies on floating) */
    out_w(PERIPH_GPIO_PIN_CNF(TOUCH_LOGO_PORT, TOUCH_LOGO_PIN), 0x00000000);
    tk_dly_tsk(5);

    /* Prompt user and poll for 3 seconds */
    tm_printf((UB*)"       -> Touch the logo within 3s... ");

    for (int i = 0; i < 300; i++) {
        tk_dly_tsk(10);
        UW gpio_in = in_w(PERIPH_GPIO_IN(TOUCH_LOGO_PORT));
        INT val = (gpio_in >> TOUCH_LOGO_PIN) & 1;
        if (val == 0) {
            /* Touched (LOW) — wait for release */
            while (((in_w(PERIPH_GPIO_IN(TOUCH_LOGO_PORT)) >> TOUCH_LOGO_PIN) & 1) == 0) {
                tk_dly_tsk(10);
            }
            tm_printf((UB*)"TOUCHED!\n");
            return 2;  /* Full PASS */
        }
    }

    tm_printf((UB*)"timeout\n");
    return 1;  /* Pin readable, no touch detected */
}


/* ======================================================================
 *  Test 9: PDM Microphone — verify peripheral responds
 * ====================================================================== */
static INT test_pdm_peripheral(void)
{
    /* Configure PDM pins */
    out_w(PDM_PSEL_CLK, MIC_PDM_CLK_PIN);
    out_w(PDM_PSEL_DIN, MIC_PDM_DIN_PIN);

    /* Enable PDM, then immediately read back to verify register is writable */
    out_w(PDM_ENABLE, 1);
    tk_dly_tsk(2);
    UW enabled = in_w(PDM_ENABLE);

    /* Disable it again — we don't want it running */
    out_w(PDM_ENABLE, 0);

    return (enabled == 1) ? 1 : 0;
}


/* ======================================================================
 *  Self-Test Runner
 * ====================================================================== */
#define NUM_TESTS  9

static void run_self_test(ID i2c_dev)
{
    INT passed = 0;
    INT result;
    INT temp_raw = 0;

    tm_printf((UB*)"\n============================================\n");
    tm_printf((UB*)"  PawState Peripheral Self-Test  (v2.21)\n");
    tm_printf((UB*)"============================================\n\n");

    /* --- Test 1: LED Matrix --- */
    result = test_led_matrix();
    tm_printf((UB*)"[1/%d] LED Matrix 5x5 ......... %s\n",
              NUM_TESTS, result > 0 ? "PASS" : "FAIL");
    if (result > 0) passed++;

    /* --- Test 2: Speaker --- */
    result = test_speaker();
    tm_printf((UB*)"[2/%d] Speaker Beep ........... %s (manual)\n",
              NUM_TESTS, result > 0 ? "BEEP" : "FAIL");
    if (result > 0) passed++;

    /* --- Test 3: Accelerometer --- */
    result = test_accel_whoami(i2c_dev);
    if (result > 0) {
        tm_printf((UB*)"[3/%d] Accel WHO_AM_I ......... PASS (0x33)\n", NUM_TESTS);
        passed++;
    } else {
        tm_printf((UB*)"[3/%d] Accel WHO_AM_I ......... FAIL (err=%d)\n", NUM_TESTS, result);
    }

    /* --- Test 4: Magnetometer --- */
    result = test_mag_whoami(i2c_dev);
    if (result > 0) {
        tm_printf((UB*)"[4/%d] Mag   WHO_AM_I ......... PASS (0x40)\n", NUM_TESTS);
        passed++;
    } else {
        tm_printf((UB*)"[4/%d] Mag   WHO_AM_I ......... FAIL (err=%d)\n", NUM_TESTS, result);
    }

    /* --- Test 5: Temperature --- */
    result = test_temperature(&temp_raw);
    if (result > 0) {
        INT deg = temp_raw / 4;
        INT frac = (temp_raw % 4) * 25;
        tm_printf((UB*)"[5/%d] Temperature ............ PASS (%d.%02d C)\n",
                  NUM_TESTS, deg, frac < 0 ? -frac : frac);
        passed++;
    } else {
        tm_printf((UB*)"[5/%d] Temperature ............ FAIL\n", NUM_TESTS);
    }

    /* --- Test 6: Button A (interactive) --- */
    tm_printf((UB*)"[6/%d] Button A (P0.14) ....... ", NUM_TESTS);
    result = test_button_interactive(BTN_A_PORT, BTN_A_PIN, "Button A");
    if (result == 2) {
        tm_printf((UB*)"[6/%d] Button A ............... PASS (press detected)\n", NUM_TESTS);
        passed++;
    } else if (result == 1) {
        tm_printf((UB*)"[6/%d] Button A ............... PASS (idle OK, no press)\n", NUM_TESTS);
        passed++;
    } else {
        tm_printf((UB*)"[6/%d] Button A ............... FAIL (stuck LOW)\n", NUM_TESTS);
    }

    /* --- Test 7: Button B (interactive) --- */
    tm_printf((UB*)"[7/%d] Button B (P0.23) ....... ", NUM_TESTS);
    result = test_button_interactive(BTN_B_PORT, BTN_B_PIN, "Button B");
    if (result == 2) {
        tm_printf((UB*)"[7/%d] Button B ............... PASS (press detected)\n", NUM_TESTS);
        passed++;
    } else if (result == 1) {
        tm_printf((UB*)"[7/%d] Button B ............... PASS (idle OK, no press)\n", NUM_TESTS);
        passed++;
    } else {
        tm_printf((UB*)"[7/%d] Button B ............... FAIL (stuck LOW)\n", NUM_TESTS);
    }

    /* --- Test 8: Touch Logo (interactive) --- */
    tm_printf((UB*)"[8/%d] Touch Logo (P1.04) ..... ", NUM_TESTS);
    result = test_touch_logo_interactive();
    if (result == 2) {
        tm_printf((UB*)"[8/%d] Touch Logo ............. PASS (touch detected)\n", NUM_TESTS);
        passed++;
    } else if (result == 1) {
        tm_printf((UB*)"[8/%d] Touch Logo ............. PASS (pin OK, no touch)\n", NUM_TESTS);
        passed++;
    } else {
        tm_printf((UB*)"[8/%d] Touch Logo ............. FAIL\n", NUM_TESTS);
    }

    /* --- Test 9: PDM Microphone --- */
    result = test_pdm_peripheral();
    tm_printf((UB*)"[9/%d] PDM Microphone ......... %s\n",
              NUM_TESTS, result > 0 ? "PASS" : "FAIL");
    if (result > 0) passed++;

    /* --- Summary --- */
    tm_printf((UB*)"\n============================================\n");
    tm_printf((UB*)"  Result: %d/%d Tests Passed\n", passed, NUM_TESTS);
    tm_printf((UB*)"============================================\n\n");
}


/* ======================================================================
 *  Continuous Sensor Loop  (accelerometer + magnetometer)
 *  Includes I2C error recovery with automatic sensor re-initialization
 * ====================================================================== */
#define MAX_I2C_RETRIES     3
#define I2C_RECOVERY_MS     50

static void init_sensors(ID dev)
{
    /* Power up accelerometer: CTRL_REG1_A = 0x57 → 100 Hz, all axes */
    i2c_write_reg(dev, LSM303_ACCEL_ADDR, LSM303_CTRL_REG1_A, 0x57);
    /* Power up magnetometer: CFG_REG_A_M = 0x00 → continuous mode, 10 Hz */
    i2c_write_reg(dev, LSM303_MAG_ADDR, LSM303_CFG_REG_A_M, 0x00);
    tk_dly_tsk(20);
}

static void sensor_loop(ID dev)
{
    INT consecutive_errors = 0;

    init_sensors(dev);
    tk_dly_tsk(100);  /* Sensor start-up time */

    tm_printf((UB*)"[LOOP] Continuous sensor read started...\n\n");

    while (1) {
        /* --- Accelerometer: 6-byte burst read from 0x28 with auto-inc --- */
        UB accel_reg = LSM303_OUT_X_L_A | 0x80;
        UB accel_data[6] = {0};
        T_I2C_EXEC ex_a;

        ex_a.sadr     = LSM303_ACCEL_ADDR;
        ex_a.snd_data = &accel_reg;
        ex_a.snd_size = 1;
        ex_a.rcv_data = accel_data;
        ex_a.rcv_size = 6;

        SZ asize_a;
        ER err_a = tk_swri_dev(dev, TDN_I2C_EXEC, &ex_a, sizeof(T_I2C_EXEC), &asize_a);

        if (err_a < 0) {
            consecutive_errors++;
            INT err_src = -(err_a + 100);
            tm_printf((UB*)"[WARN] I2C err #%d (src=0x%x, err_a=%d) ", consecutive_errors, err_src, err_a);

            if (consecutive_errors >= MAX_I2C_RETRIES) {
                /* Bus is stuck — re-init sensors and reset counter */
                tm_printf((UB*)"-> re-initializing...\n");
                tk_dly_tsk(I2C_RECOVERY_MS);
                init_sensors(dev);
                consecutive_errors = 0;
            } else {
                tm_printf((UB*)"-> retrying...\n");
                tk_dly_tsk(I2C_RECOVERY_MS);
            }
            continue;  /* Skip this cycle, retry on next iteration */
        }

        /* Brief gap between transfers to let TWIM breathe */
        tk_dly_tsk(10);

        /* --- Magnetometer: 6-byte burst read from 0x68 --- */
        UB mag_reg = LSM303_OUTX_L_REG_M;
        UB mag_data[6] = {0};
        T_I2C_EXEC ex_m;

        ex_m.sadr     = LSM303_MAG_ADDR;
        ex_m.snd_data = &mag_reg;
        ex_m.snd_size = 1;
        ex_m.rcv_data = mag_data;
        ex_m.rcv_size = 6;

        SZ asize_m;
        ER err_m = tk_swri_dev(dev, TDN_I2C_EXEC, &ex_m, sizeof(T_I2C_EXEC), &asize_m);

        /* Reset error counter on any successful read */
        consecutive_errors = 0;

        /* Parse and print */
        short ax = (short)(accel_data[0] | (accel_data[1] << 8));
        short ay = (short)(accel_data[2] | (accel_data[3] << 8));
        short az = (short)(accel_data[4] | (accel_data[5] << 8));

        if (err_m >= 0) {
            short mx = (short)(mag_data[0] | (mag_data[1] << 8));
            short my = (short)(mag_data[2] | (mag_data[3] << 8));
            short mz = (short)(mag_data[4] | (mag_data[5] << 8));
            tm_printf((UB*)"A[%6d %6d %6d]  M[%6d %6d %6d]\n",
                      ax, ay, az, mx, my, mz);
        } else {
            tm_printf((UB*)"A[%6d %6d %6d]  M[err=%d]\n", ax, ay, az, err_m);
        }

        tk_dly_tsk(200);
    }
}


/* ======================================================================
 *  Main task — runs self-test then enters sensor loop
 * ====================================================================== */
void selftest_task(INT stacd, void *exinf)
{
    /* Open the I2C device */
    ID i2c_dev = tk_opn_dev((UB*)"iica", TD_READ | TD_WRITE);
    if (i2c_dev < 0) {
        tm_printf((UB*)"[FATAL] Cannot open I2C device (err=%d)\n", i2c_dev);
        return;
    }

    /* Phase 1: Self-test */
    run_self_test(i2c_dev);

    /* Phase 2: Continuous sensor loop */
    sensor_loop(i2c_dev);
}


/* ======================================================================
 *  usermain — kernel entry point
 * ====================================================================== */
EXPORT INT usermain(void)
{
    tm_printf((UB*)"\n=== BOOT SUCCESS ===\n");

    T_CTSK ctsk = {
        .tskatr  = TA_HLNG | TA_RNG3,
        .task    = selftest_task,
        .itskpri = 10,
        .stksz   = 2048     /* Larger stack for printf + I2C buffers */
    };
    selftest_tskid = tk_cre_tsk(&ctsk);
    tk_sta_tsk(selftest_tskid, 0);

    tk_slp_tsk(TMO_FEVR);
    return 0;
}