#include <sys/machine.h>
#ifdef CPU_NRF5

#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include "../../i2c.h"
#include "../../../include/dev_def.h"

#if DEV_IIC_ENABLE

#define PIN_SCL 8
#define PIN_SDA 16

/* PIN_CNF values */
#define PIN_CNF_OUTPUT      0x00000001  /* DIR=out, INPUT=disconnect, PULL=none, DRIVE=S0S1 */
#define PIN_CNF_INPUT       0x00000000  /* DIR=in,  INPUT=connect,    PULL=none, DRIVE=S0S1 */
#define PIN_CNF_I2C         0x0000060C  /* DIR=in,  INPUT=connect,    PULL=up,   DRIVE=S0D1 (Open Drain) */

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
#define TWIM_EVENTS_TXSTARTED (TWIM0_BASE + 0x14C)
#define TWIM_EVENTS_RXSTARTED (TWIM0_BASE + 0x150)
#define TWIM_EVENTS_LASTTX  (TWIM0_BASE + 0x15C)
#define TWIM_EVENTS_LASTRX  (TWIM0_BASE + 0x160)
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

    /* drive SCL and SDA high */
    out_w(GPIO_P0_PIN_CNF(PIN_SCL), PIN_CNF_OUTPUT);
    out_w(GPIO_P0_PIN_CNF(PIN_SDA), PIN_CNF_OUTPUT);
    out_w(GPIO_P0_OUTSET, (1U << PIN_SCL));
    out_w(GPIO_P0_OUTSET, (1U << PIN_SDA));
    tiny_delay();

    /* input to read */
    out_w(GPIO_P0_PIN_CNF(PIN_SDA), PIN_CNF_INPUT);
    
    /* Clock 18 times */
    for (int c = 0; c < 18; c++) {       
        out_w(GPIO_P0_OUTCLR, (1U << PIN_SCL));
        tiny_delay();
        out_w(GPIO_P0_OUTSET, (1U << PIN_SCL));
        tiny_delay();
    }

    /* manual STOP */
    out_w(GPIO_P0_PIN_CNF(PIN_SDA), PIN_CNF_OUTPUT);
    out_w(GPIO_P0_OUTCLR, (1U << PIN_SDA));
    tiny_delay();
    out_w(GPIO_P0_OUTSET, (1U << PIN_SCL));
    tiny_delay();
    out_w(GPIO_P0_OUTSET, (1U << PIN_SDA));
    tiny_delay();

    /* release pins to correct Open-Drain I2C configuration */
    out_w(GPIO_P0_PIN_CNF(PIN_SCL), PIN_CNF_I2C);
    out_w(GPIO_P0_PIN_CNF(PIN_SDA), PIN_CNF_I2C);

    /* full TWIM re-init */
    out_w(TWIM_PSEL_SCL, PIN_SCL);
    out_w(TWIM_PSEL_SDA, PIN_SDA);
    out_w(TWIM_FREQUENCY, 0x01980000); // 100KHz
    out_w(TWIM_SHORTS, 0);
    out_w(TWIM_ENABLE, 6); // TWIM_ENABLE_ENABLED
}

LOCAL void twim_clear_events(void)
{
    out_w(TWIM_EVENTS_STOPPED, 0);
    out_w(TWIM_EVENTS_ERROR, 0);
    out_w(TWIM_EVENTS_TXSTARTED, 0);
    out_w(TWIM_EVENTS_RXSTARTED, 0);
    out_w(TWIM_EVENTS_LASTTX, 0);
    out_w(TWIM_EVENTS_LASTRX, 0);
    out_w(TWIM_ERRORSRC, in_w(TWIM_ERRORSRC));
}

LOCAL W twim_handle_error(void)
{
    out_w(TWIM_EVENTS_ERROR, 0);
    UW src = in_w(TWIM_ERRORSRC);
    out_w(TWIM_ERRORSRC, src);  /* W1C */
    tm_printf((UB*)"[TWIM HW ERR] ERRORSRC = 0x%08X\n", src);

    /* try graceful stop first */
    out_w(TWIM_EVENTS_STOPPED, 0);
    out_w(TWIM_TASKS_STOP, 1);
    
    for (volatile int i = 0; i < 50000; i++) {
        if (in_w(TWIM_EVENTS_STOPPED) != 0) break;
    }

    if (in_w(TWIM_EVENTS_STOPPED) == 0) {
        tm_printf((UB*)"[TWIM] Stop timeout, forcing bus clear...\n");
        i2c_bus_clear();
    } else {
        /* Still clear it anyway to be safe */
        i2c_bus_clear();
    }

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
        twim_clear_events();

        if (ex->snd_size > 0 && ex->rcv_size > 0) {
            /* Write then Read */
            out_w(TWIM_TXD_PTR, (UW)ex->snd_data);
            out_w(TWIM_TXD_MAXCNT, ex->snd_size);
            out_w(TWIM_RXD_PTR, (UW)ex->rcv_data);
            out_w(TWIM_RXD_MAXCNT, ex->rcv_size);
            out_w(TWIM_SHORTS, (1 << 7) | (1 << 12)); // LASTTX_STARTRX | LASTRX_STOP
            out_w(TWIM_TASKS_STARTTX, 1);
        } else if (ex->snd_size > 0) {
            /* Write only */
            out_w(TWIM_TXD_PTR, (UW)ex->snd_data);
            out_w(TWIM_TXD_MAXCNT, ex->snd_size);
            out_w(TWIM_RXD_MAXCNT, 0);
            out_w(TWIM_SHORTS, (1 << 9)); // LASTTX_STOP
            out_w(TWIM_TASKS_STARTTX, 1);
        } else if (ex->rcv_size > 0) {
            /* Read only */
            out_w(TWIM_RXD_PTR, (UW)ex->rcv_data);
            out_w(TWIM_RXD_MAXCNT, ex->rcv_size);
            out_w(TWIM_TXD_MAXCNT, 0);
            out_w(TWIM_SHORTS, (1 << 12)); // LASTRX_STOP
            out_w(TWIM_TASKS_STARTRX, 1);
        } else {
            return E_PAR;
        }

        /* Wait for DMA to complete or error */
        while(in_w(TWIM_EVENTS_STOPPED) == 0) {
            if (in_w(TWIM_EVENTS_ERROR) != 0) {
                return twim_handle_error();
            }
        }
        
        twim_clear_events();
        return sizeof(T_I2C_EXEC);
    }
    return E_OK;
}

EXPORT ER dev_i2c_llinit( T_I2C_DCB *p_dcb )
{
    /* Configure GPIOs for Open-Drain with Pull-up before assigning to TWIM */
    out_w(GPIO_P0_PIN_CNF(PIN_SCL), PIN_CNF_I2C);
    out_w(GPIO_P0_PIN_CNF(PIN_SDA), PIN_CNF_I2C);

    out_w(TWIM_PSEL_SCL, PIN_SCL);
    out_w(TWIM_PSEL_SDA, PIN_SDA);
    out_w(TWIM_FREQUENCY, 0x01980000); 
    return E_OK;
}

#endif /* DEV_IIC_ENABLE */
#endif /* CPU_NRF5 */