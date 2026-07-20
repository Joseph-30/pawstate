#include <sys/machine.h>
#ifdef CPU_NRF5

#include <tk/tkernel.h>
#include "../../i2c.h"
#include "../../../include/dev_def.h"

#if DEV_IIC_ENABLE

#define PIN_SCL 8
#define PIN_SDA 16

/* TWIM0 Hardware Registers */
#define TWIM0_BASE          0x40003000
#define TWIM_TASKS_STARTRX  (TWIM0_BASE + 0x000)
#define TWIM_TASKS_STARTTX  (TWIM0_BASE + 0x008)
#define TWIM_TASKS_STOP     (TWIM0_BASE + 0x014)
#define TWIM_TASKS_RESUME   (TWIM0_BASE + 0x01C)
#define TWIM_EVENTS_STOPPED (TWIM0_BASE + 0x104)
#define TWIM_EVENTS_ERROR   (TWIM0_BASE + 0x124)
#define TWIM_SHORTS         (TWIM0_BASE + 0x200)
#define TWIM_ERRORSRC       (TWIM0_BASE + 0x4C4)
#define TWIM_ENABLE         (TWIM0_BASE + 0x500)
#define TWIM_PSEL_SCL       (TWIM0_BASE + 0x508)
#define TWIM_PSEL_SDA       (TWIM0_BASE + 0x50C)
#define TWIM_FREQUENCY      (TWIM0_BASE + 0x524)
#define TWIM_TXD_PTR        (TWIM0_BASE + 0x544)
#define TWIM_TXD_MAXCNT     (TWIM0_BASE + 0x548)
#define TWIM_RXD_PTR        (TWIM0_BASE + 0x534)
#define TWIM_RXD_MAXCNT     (TWIM0_BASE + 0x538)
#define TWIM_ADDRESS        (TWIM0_BASE + 0x588)

EXPORT W dev_i2c_llctl( UW unit, INT cmd, UW parm1, UW parm2, UW *parm3 )
{
    if (cmd == LLD_I2C_OPEN) {
        out_w(TWIM_ENABLE, 6);
        return E_OK;
    }
    if (cmd == LLD_I2C_CLOSE) {
        out_w(TWIM_ENABLE, 0);
        return E_OK;
    }
    if (cmd == LLD_I2C_EXEC) {
        T_I2C_EXEC *ex = (T_I2C_EXEC *)parm3;
        out_w(TWIM_ADDRESS, ex->sadr);
        
        out_w(TWIM_EVENTS_STOPPED, 0);
        out_w(TWIM_EVENTS_ERROR, 0);

        if (ex->snd_size > 0 && ex->rcv_size > 0) {
            out_w(TWIM_TXD_PTR, (UW)ex->snd_data);
            out_w(TWIM_TXD_MAXCNT, ex->snd_size);
            out_w(TWIM_RXD_PTR, (UW)ex->rcv_data);
            out_w(TWIM_RXD_MAXCNT, ex->rcv_size);
            out_w(TWIM_SHORTS, (1 << 7) | (1 << 12)); // LASTTX_STARTRX | LASTRX_STOP
            out_w(TWIM_TASKS_STARTTX, 1);
        } else if (ex->snd_size > 0) {
            out_w(TWIM_TXD_PTR, (UW)ex->snd_data);
            out_w(TWIM_TXD_MAXCNT, ex->snd_size);
            out_w(TWIM_SHORTS, (1 << 9)); // LASTTX_STOP
            out_w(TWIM_TASKS_STARTTX, 1);
        }

        while(in_w(TWIM_EVENTS_STOPPED) == 0) {
            if (in_w(TWIM_EVENTS_ERROR) != 0) {
                // HARDWARE ERROR RECOVERY
                out_w(TWIM_EVENTS_ERROR, 0);
                UW err_val = in_w(TWIM_ERRORSRC);
                out_w(TWIM_ERRORSRC, err_val); // Clear the error flags
                out_w(TWIM_TASKS_RESUME, 1);   // Resume the bus
                out_w(TWIM_TASKS_STOP, 1);     // Force a clean stop condition
                return -34; 
            }
        }
        return sizeof(T_I2C_EXEC);
    }
    return E_OK;
}

EXPORT ER dev_i2c_llinit( T_I2C_DCB *p_dcb )
{
    out_w(TWIM_PSEL_SCL, PIN_SCL);
    out_w(TWIM_PSEL_SDA, PIN_SDA);
    out_w(TWIM_FREQUENCY, 0x01980000); 
    return E_OK;
}

#endif /* DEV_IIC_ENABLE */
#endif /* CPU_NRF5 */