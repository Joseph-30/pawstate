#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#define LSM303_ACCEL_ADDR  0x19  // Micro:bit V2 Sensor
#define MMA8653_ACCEL_ADDR 0x1D  // Micro:bit V1 Sensor

ID accel_tskid;
ID i2c_dev_id = -1;

void accel_task(INT stacd, void *exinf) {
    tk_dly_tsk(1000); 
    tm_printf((UB*)"\n=== TRON DEVICE SCANNER ===\n");

    // Array of standard uT-Kernel I2C device names
    const char *device_names[] = {
        "iica", "iicb", "iicc", "iicd", 
        "iic0", "iic1", "iic2", "iic ", 
        "i2ca", "i2cb", "i2c0", "i2c1"
    };

    int num_names = sizeof(device_names) / sizeof(device_names[0]);
    int found = 0;

    for (int i = 0; i < num_names; i++) {
        ID dev_id = tk_opn_dev((UB*)device_names[i], TD_READ | TD_WRITE);
        
        if (dev_id >= 0) {
            tm_printf((UB*)"SUCCESS: Found I2C driver named -> '%s' (ID: %d)\n", device_names[i], dev_id);
            tk_cls_dev(dev_id, 0); // Close it gracefully
            found = 1;
        } else {
            tm_printf((UB*)"Failed: '%s' (Error %d)\n", device_names[i], dev_id);
        }
    }

    if (!found) {
        tm_printf((UB*)"\nCRITICAL: No standard I2C drivers are registered in the OS!\n");
    }

    while(1) { tk_dly_tsk(1000); }
}

EXPORT INT usermain(void) {
    T_CTSK ctsk_accel = {
        .tskatr  = TA_HLNG | TA_RNG3,
        .task    = accel_task,
        .itskpri = 10,
        .stksz   = 1024
    };
    accel_tskid = tk_cre_tsk(&ctsk_accel);
    tk_sta_tsk(accel_tskid, 0);
    tk_slp_tsk(TMO_FEVR);
    return 0;
}