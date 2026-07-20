#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include "../device/i2c/i2c.h"


ID heartbeat_tskid;

/* nRF52833 GPIO Registers */
#define NRF_P0_DIRSET 0x50000518  // Set pin as output
#define NRF_P0_OUTSET 0x50000508  // Set pin HIGH
#define NRF_P0_OUTCLR 0x5000050C  // Set pin LOW

void test_peripherals(void) {
    tm_printf((UB*)"[INFO] Testing Speaker and LED...\n");
    
    // 1. Light up top-left LED (Row 1 = P0.21 High, Col 1 = P0.28 Low)
    out_w(NRF_P0_DIRSET, (1 << 21) | (1 << 28)); 
    out_w(NRF_P0_OUTSET, (1 << 21));             
    out_w(NRF_P0_OUTCLR, (1 << 28));             

    // 2. Beep the speaker (P0.00)
    out_w(NRF_P0_DIRSET, (1 << 0));
    for (int i = 0; i < 200; i++) {
        out_w(NRF_P0_OUTSET, (1 << 0)); // Push cone out
        tk_dly_tsk(1);                  // Wait 1ms
        out_w(NRF_P0_OUTCLR, (1 << 0)); // Pull cone in
        tk_dly_tsk(1);                  // Wait 1ms
    }
}

void heartbeat_task(INT stacd, void *exinf) {
    test_peripherals();
    
    ID dev_id = tk_opn_dev((UB*)"iica", TD_READ | TD_WRITE);
    
    // 1. WAKE UP THE SENSOR (Bypassing the OS bug)
    UB init_cmd[2] = {0x20, 0x57}; 
    UB dummy_rx; // Dummy variable to trick the OS
    
    T_I2C_EXEC exec_init;
    exec_init.sadr = 0x19;
    exec_init.snd_data = init_cmd;
    exec_init.snd_size = 2; 
    exec_init.rcv_data = &dummy_rx; 
    exec_init.rcv_size = 1; // Trick i2c.c into allowing the command!
    tk_wri_dev(dev_id, TDN_I2C_EXEC, &exec_init, sizeof(T_I2C_EXEC), TMO_FEVR);

    tm_printf((UB*)"[INFO] Accelerometer powered up.\n");
    tk_dly_tsk(100); 

    // 2. CONTINUOUSLY READ THE DATA
    while(1) {
        UB start_reg = 0x28 | 0x80; // 0x80 triggers auto-increment
        UB xyz_data[6] = {0}; 
        T_I2C_EXEC exec_read;
        
        exec_read.sadr = 0x19;
        exec_read.snd_data = &start_reg;
        exec_read.snd_size = 1;
        exec_read.rcv_data = xyz_data;
        exec_read.rcv_size = 6; 

        ER err = tk_wri_dev(dev_id, TDN_I2C_EXEC, &exec_read, sizeof(T_I2C_EXEC), TMO_FEVR);

        if (err >= 0) {
            short x = (short)(xyz_data[0] | (xyz_data[1] << 8));
            short y = (short)(xyz_data[2] | (xyz_data[3] << 8));
            short z = (short)(xyz_data[4] | (xyz_data[5] << 8));
            tm_printf((UB*)"X: %6d | Y: %6d | Z: %6d\n", x, y, z);
        } else {
            tm_printf((UB*)"[ERROR] Bus glitched! Recovering... Err: %d\n", err);
        }
        tk_dly_tsk(200); 
    }
}

EXPORT INT usermain(void) {
    tm_printf((UB*)"\n=== BOOT SUCCESS ===\n");
    
    T_CTSK ctsk_heartbeat = {
        .tskatr  = TA_HLNG | TA_RNG3,
        .task    = heartbeat_task,
        .itskpri = 10, 
        .stksz   = 1024
    };
    heartbeat_tskid = tk_cre_tsk(&ctsk_heartbeat);
    tk_sta_tsk(heartbeat_tskid, 0);

    tk_slp_tsk(TMO_FEVR);
    return 0;
}