#include <sys/machine.h>
#ifdef CPU_NRF5

#include <tk/tkernel.h>
#include <tstdlib.h>

#include "../../i2c.h"
#include "../../../include/dev_def.h"

#if DEV_IIC_ENABLE

/*
 * i2c_nrf5.c
 * I2C device driver low-level processing for nRF5 (micro:bit)
 */

/*----------------------------------------------------------------------
 * Hardware definition
 */
#define TWI0_BASE               0x40003000

#define TWI_TASKS_STARTRX       0x000
#define TWI_TASKS_STARTTX       0x008
#define TWI_TASKS_STOP          0x014
#define TWI_EVENTS_STOPPED      0x104
#define TWI_EVENTS_RXDREADY     0x108
#define TWI_EVENTS_TXDSENT      0x11C
#define TWI_EVENTS_ERROR        0x124
#define TWI_SHORTS              0x200
#define TWI_INTENSET            0x304
#define TWI_INTENCLR            0x308
#define TWI_ERRORSRC            0x4C4
#define TWI_ENABLE              0x500
#define TWI_PSEL_SCL            0x508
#define TWI_PSEL_SDA            0x50C
#define TWI_FREQUENCY           0x524
#define TWI_RXD                 0x518
#define TWI_TXD                 0x51C
#define TWI_ADDRESS             0x588

#define TWI_ENABLE_DISABLED     0
#define TWI_ENABLE_ENABLED      5
#define TWI_FREQ_100KHZ         0x01980000

#define I2C_SCL_PIN             8
#define I2C_SDA_PIN             16

#define I2C_WAIT_LIMIT          1000000U

typedef struct {
    UW	base;
    BOOL	opened;
} T_I2C_LLDEVCB;

LOCAL T_I2C_LLDEVCB	ll_devcb[DEV_I2C_UNITNM] = {
    { TWI0_BASE, FALSE },
};

#undef	TWI
#define TWI(cb, reg)           ((cb)->base + TWI_##reg)

/*----------------------------------------------------------------------
 * Utility
 */
LOCAL void twi_clear_events( T_I2C_LLDEVCB *cb )
{
    out_w(TWI(cb, EVENTS_STOPPED), 0);
    out_w(TWI(cb, EVENTS_RXDREADY), 0);
    out_w(TWI(cb, EVENTS_TXDSENT), 0);
    out_w(TWI(cb, EVENTS_ERROR), 0);
    out_w(TWI(cb, ERRORSRC), 0);
}

LOCAL ER twi_wait_event( T_I2C_LLDEVCB *cb, UW event_off )
{
    UW	limit;

    for ( limit = 0; limit < I2C_WAIT_LIMIT; ++limit ) {
        if ( in_w(cb->base + event_off) != 0 ) return E_OK;
        if ( in_w(TWI(cb, EVENTS_ERROR)) != 0 ) {
            out_w(TWI(cb, EVENTS_ERROR), 0);
            out_w(TWI(cb, ERRORSRC), 0);
            return E_IO;
        }
    }

    return E_TMOUT;
}

LOCAL ER twi_wait_stopped( T_I2C_LLDEVCB *cb )
{
    ER	err;

    err = twi_wait_event(cb, TWI_EVENTS_STOPPED);
    if ( err < E_OK ) return err;
    out_w(TWI(cb, EVENTS_STOPPED), 0);

    return E_OK;
}

LOCAL W twi_write_bytes( T_I2C_LLDEVCB *cb, UW sadr, const UB *src, SZ size )
{
    SZ	i;
    ER	err;

    if ( size <= 0 || src == NULL ) return E_PAR;

    out_w(TWI(cb, ADDRESS), sadr & 0x7f);
    twi_clear_events(cb);
    out_w(TWI(cb, TASKS_STARTTX), 1);

    for ( i = 0; i < size; ++i ) {
        out_w(TWI(cb, TXD), src[i]);
        err = twi_wait_event(cb, TWI_EVENTS_TXDSENT);
        if ( err < E_OK ) goto error_stop;
        out_w(TWI(cb, EVENTS_TXDSENT), 0);
    }

    out_w(TWI(cb, TASKS_STOP), 1);
    err = twi_wait_stopped(cb);
    if ( err < E_OK ) return err;

    return (W)size;

error_stop:
    out_w(TWI(cb, TASKS_STOP), 1);
    (void)twi_wait_stopped(cb);
    return err;
}

LOCAL W twi_read_bytes( T_I2C_LLDEVCB *cb, UW sadr, UB *dst, SZ size )
{
    SZ	i;
    ER	err;

    if ( size <= 0 || dst == NULL ) return E_PAR;

    out_w(TWI(cb, ADDRESS), sadr & 0x7f);
    twi_clear_events(cb);
    out_w(TWI(cb, TASKS_STARTRX), 1);

    for ( i = 0; i < size; ++i ) {
        err = twi_wait_event(cb, TWI_EVENTS_RXDREADY);
        if ( err < E_OK ) goto error_stop;
        dst[i] = (UB)in_w(TWI(cb, RXD));
        out_w(TWI(cb, EVENTS_RXDREADY), 0);

        if ( i == (size - 1) ) {
            out_w(TWI(cb, TASKS_STOP), 1);
            err = twi_wait_stopped(cb);
            if ( err < E_OK ) return err;
        }
    }

    return (W)size;

error_stop:
    out_w(TWI(cb, TASKS_STOP), 1);
    (void)twi_wait_stopped(cb);
    return err;
}

LOCAL W twi_exec( T_I2C_LLDEVCB *cb, T_I2C_EXEC *ex )
{
    SZ	i;
    ER	err;

    if ( ex == NULL || ex->snd_data == NULL || ex->rcv_data == NULL ) return E_PAR;

    out_w(TWI(cb, ADDRESS), ex->sadr & 0x7f);
    twi_clear_events(cb);
    out_w(TWI(cb, TASKS_STARTTX), 1);

    for ( i = 0; i < ex->snd_size; ++i ) {
        out_w(TWI(cb, TXD), ex->snd_data[i]);
        err = twi_wait_event(cb, TWI_EVENTS_TXDSENT);
        if ( err < E_OK ) goto error_stop;
        out_w(TWI(cb, EVENTS_TXDSENT), 0);
    }

    out_w(TWI(cb, TASKS_STARTRX), 1);

    for ( i = 0; i < ex->rcv_size; ++i ) {
        err = twi_wait_event(cb, TWI_EVENTS_RXDREADY);
        if ( err < E_OK ) goto error_stop;
        ex->rcv_data[i] = (UB)in_w(TWI(cb, RXD));
        out_w(TWI(cb, EVENTS_RXDREADY), 0);

        if ( i == (ex->rcv_size - 1) ) {
            out_w(TWI(cb, TASKS_STOP), 1);
            err = twi_wait_stopped(cb);
            if ( err < E_OK ) return err;
        }
    }

    return (W)sizeof(T_I2C_EXEC);

error_stop:
    out_w(TWI(cb, TASKS_STOP), 1);
    (void)twi_wait_stopped(cb);
    return err;
}

/*----------------------------------------------------------------------
 * Low level device control
 */
EXPORT W dev_i2c_llctl( UW unit, INT cmd, UW parm1, UW parm2, UW *parm3 )
{
    T_I2C_LLDEVCB	*cb;

    if ( unit >= DEV_I2C_UNITNM ) return E_PAR;
    cb = &ll_devcb[unit];

    switch ( cmd ) {
      case LLD_I2C_OPEN:
        cb->opened = TRUE;
        out_w(TWI(cb, ENABLE), TWI_ENABLE_ENABLED);
        return E_OK;

      case LLD_I2C_CLOSE:
        out_w(TWI(cb, TASKS_STOP), 1);
        (void)twi_wait_stopped(cb);
        out_w(TWI(cb, ENABLE), TWI_ENABLE_DISABLED);
        cb->opened = FALSE;
        return E_OK;

      case LLD_I2C_READ:
        return twi_read_bytes(cb, parm1, (UB*)parm3, (SZ)parm2);

      case LLD_I2C_WRITE:
        return twi_write_bytes(cb, parm1, (const UB*)parm3, (SZ)parm2);

      case LLD_I2C_EXEC:
        return twi_exec(cb, (T_I2C_EXEC*)parm3);

      default:
        return E_PAR;
    }
}

/*----------------------------------------------------------------------
 * Device initialization
 */
EXPORT ER dev_i2c_llinit( T_I2C_DCB *p_dcb )
{
    T_I2C_LLDEVCB	*cb;

    if ( p_dcb == NULL || p_dcb->unit >= DEV_I2C_UNITNM ) return E_PAR;

    cb = &ll_devcb[p_dcb->unit];

    out_w(TWI(cb, ENABLE), TWI_ENABLE_DISABLED);
    out_w(TWI(cb, PSEL_SCL), I2C_SCL_PIN);
    out_w(TWI(cb, PSEL_SDA), I2C_SDA_PIN);
    out_w(TWI(cb, FREQUENCY), TWI_FREQ_100KHZ);
    out_w(TWI(cb, SHORTS), 0);
    twi_clear_events(cb);

    cb->opened = FALSE;

    return E_OK;
}

#endif /* DEV_IIC_ENABLE */
#endif /* CPU_NRF5 */