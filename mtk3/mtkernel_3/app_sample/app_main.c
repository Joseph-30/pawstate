#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include "../device/i2c/i2c.h"
ID heartbeat_tskid;
ID i2c_scan_tskid;

/* Task 1: Heartbeat - Proves the RTOS scheduler and timer are running */
void heartbeat_task(INT stacd, void *exinf) {
    int counter = 1;
    while(1) {
        tm_printf((UB*)"[Heartbeat] PawState OS running... Uptime: %d sec\n", counter);
        counter++;
        ID dev_id = tk_opn_dev((UB*)"iica", TD_READ | TD_WRITE);
        if (dev_id >= 0) {
            T_I2C_EXEC exec;
            UB reg_addr = 0x0F; // WHO_AM_I register (renamed to avoid the trademark bug!)
            UB data = 0;        // Buffer to hold the response

            // Configure the I2C transaction
            exec.sadr = 0x19;          // LSM303 slave address
            exec.snd_data = &reg_addr; // What to send (the register we want)
            exec.snd_size = 1;         // Sending 1 byte
            exec.rcv_data = &data;     // Where to put the answer
            exec.rcv_size = 1;         // Reading 1 byte

            // Execute the transaction with a timeout of TMO_FEVR (Wait Forever)
            W err = tk_wri_dev(dev_id, TDN_I2C_EXEC, &exec, sizeof(T_I2C_EXEC), TMO_FEVR);
            
            if (err >= 0 && data == 0x33) {
                tm_printf((UB*)"[VERIFIED] Hardware is alive! WHO_AM_I = 0x%02X\n", data);
            } else {
                tm_printf((UB*)"[ERROR] Failed to read sensor. Data: 0x%02X, Err: %d\n", data, err);
            }
            tk_cls_dev(dev_id, 0); // Close the driver gracefully
        }
        tk_dly_tsk(1000); // Tell the OS to pause this task for 1000 milliseconds
    }
}

/* Task 2: Peripheral Scanner - Proves the I2C driver is mounted */
void i2c_scan_task(INT stacd, void *exinf) {
    tk_dly_tsk(500); // Let the system settle before scanning
    tm_printf((UB*)"\n=== HARDWARE PERIPHERAL SCANNER ===\n");

    const char *device_names[] = {
        "iic",              // Standard I2C driver name
        "iica", "iicb", "iic0", "iic1", 
        "i2ca", "i2cb", "i2c0", "i2c1",
        "sera", "adca"      // Also check Serial and ADC drivers
    };

    int num_names = sizeof(device_names) / sizeof(device_names[0]);
    int found_i2c = 0;

    for (int i = 0; i < num_names; i++) {
        // Attempt to open the device driver
        ID dev_id = tk_opn_dev((UB*)device_names[i], TD_READ | TD_WRITE);
        
        if (dev_id >= 0) {
            tm_printf((UB*)"[SUCCESS] Found driver: '%s' (ID: %d)\n", device_names[i], dev_id);
            if (device_names[i][0] == 'i') found_i2c = 1;
            tk_cls_dev(dev_id, 0); // Close it gracefully
        }
    }

    if (!found_i2c) {
        tm_printf((UB*)"[WARNING] No standard I2C drivers found for the activity monitor!\n");
    }

    // Exit the task (it only needs to run once)
    tk_ext_tsk();
}

/* Main OS Entry Point */
EXPORT INT usermain(void) {
    tm_printf((UB*)"\n\n=== BOOT SUCCESS ===\n");
    tm_printf((UB*)"Starting micro:bit diagnostic sequence...\n");

    // 1. Create and start the I2C scanner task
    T_CTSK ctsk_scan = {
        .tskatr  = TA_HLNG | TA_RNG3,
        .task    = i2c_scan_task,
        .itskpri = 10,
        .stksz   = 1024
    };
    i2c_scan_tskid = tk_cre_tsk(&ctsk_scan);
    tk_sta_tsk(i2c_scan_tskid, 0);

    // 2. Create and start the heartbeat task
    T_CTSK ctsk_heartbeat = {
        .tskatr  = TA_HLNG | TA_RNG3,
        .task    = heartbeat_task,
        .itskpri = 11, // Slightly lower priority than the scanner
        .stksz   = 1024
    };
    heartbeat_tskid = tk_cre_tsk(&ctsk_heartbeat);
    tk_sta_tsk(heartbeat_tskid, 0);

    // 3. Put the main thread to sleep forever so the background tasks can run
    tk_slp_tsk(TMO_FEVR);
    
    return 0;
}