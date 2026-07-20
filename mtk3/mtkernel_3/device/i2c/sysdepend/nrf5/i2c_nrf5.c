#include <sys/machine.h>
#ifdef CPU_NRF5

#include <tk/tkernel.h>
#include "../../i2c.h"
#include "../../../include/dev_def.h"

#if DEV_IIC_ENABLE

#define PIN_SCL 8
#define PIN_SDA 16

/* GPIO P0 */
#define GPIO_P0_OUTSET      0x50000508
#define GPIO_P0_OUTCLR      0x5000050C
#define GPIO_P0_IN          0x50000510
#define GPIO_P0_DIRSET      0x50000518
#define GPIO_P0_PIN_CNF(n)  (0x50000700 + (n)*4)

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

LOCAL void tiny_delay(void) { for (volatile int i = 0; i < 80; i++); }

/* ---- GPIO-level I2C bus clear (I2C spec 3.1.16) ---- */
LOCAL void i2c_bus_clear(void)
{
    out_w(TWIM_ENABLE, 0);

    out_w(GPIO_P0_PIN_CNF(PIN_SCL), 1);  /* output */
    out_w(GPIO_P0_PIN_CNF(PIN_SDA), 1);
    out_w(GPIO_P0_OUTSET, (1U << PIN_SCL) | (1U << PIN_SDA));
    tiny_delay();

    out_w(GPIO_P0_PIN_CNF(PIN_SDA), 0);  /* input to read */
    for (int c = 0; c < 18; c++) {       
        out_w(GPIO_P0_OUTCLR, (1U << PIN_SCL));
        tiny_delay();
        out_w(GPIO_P0_OUTSET, (1U << PIN_SCL));
        tiny_delay();
    }

    /* manual STOP */
    out_w(GPIO_P0_PIN_CNF(PIN_SDA), 1);
    out_w(GPIO_P0_OUTCLR, (1U << PIN_SDA));
    tiny_delay();
    out_w(GPIO_P0_OUTSET, (1U << PIN_SCL));
    tiny_delay();
    out_w(GPIO_P0_OUTSET, (1U << PIN_SDA));
    tiny_delay();

    /* release pins */
    out_w(GPIO_P0_PIN_CNF(PIN_SCL), 0);
    out_w(GPIO_P0_PIN_CNF(PIN_SDA), 0);

    /* full TWIM re-init */
    out_w(TWIM_PSEL_SCL, PIN_SCL);
    out_w(TWIM_PSEL_SDA, PIN_SDA);
    out_w(TWIM_FREQUENCY, 0x01980000);
    out_w(TWIM_SHORTS, 0);
    out_w(TWIM_ENABLE, 6);
}

/* ---- Error handler: clear + bus clear + return error code ---- */
LOCAL W twim_handle_error(void)
{
    out_w(TWIM_EVENTS_ERROR, 0);
    UW src = in_w(TWIM_ERRORSRC);
    out_w(TWIM_ERRORSRC, src);  /* W1C */

    /* try graceful stop first */
    out_w(TWIM_EVENTS_STOPPED, 0);
    out_w(TWIM_SHORTS, 0);
    out_w(TWIM_TASKS_STOP, 1);
    for (volatile int t = 0; t < 50000; t++) {
        if (in_w(TWIM_EVENTS_STOPPED) != 0) break;
    }

    /* nuclear option: GPIO bus clear + full re-init */
    i2c_bus_clear();

    /* encode error source in return: -100 - src */
    return (W)(-(100 + (INT)src));
}

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
            out_w(TWIM_RXD_MAXCNT, 0);
            out_w(TWIM_SHORTS, (1 << 9)); // LASTTX_STOP
            out_w(TWIM_TASKS_STARTTX, 1);
        }

        while(in_w(TWIM_EVENTS_STOPPED) == 0) {
            if (in_w(TWIM_EVENTS_ERROR) != 0) {
                return twim_handle_error();
            }
        }
        
        out_w(TWIM_SHORTS, 0);
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