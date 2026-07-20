#include <sys/machine.h>
#ifdef CPU_NRF5

#include <tk/tkernel.h>
#include "../../ser.h"
#include "../../../include/dev_def.h"
#if DEV_SER_ENABLE
/*
 *	ser_nrf5.c
 *	Serial communication device driver
 *	System dependent processing for nRF5
 */

/*----------------------------------------------------------------------
 * Device low-level control data
 */
typedef struct {
	UW	ba;		// Register base address
	UW	mode;		// Serial mode
	UW	baud;		// Baudrate register value
	BOOL	txrdy;		// Tx ready flag
} T_DEV_SER_LLDEVCB;

LOCAL T_DEV_SER_LLDEVCB		ll_devcb[DEV_SER_UNITNM] = {
	{ UART0_BASE }
};

#undef	UART
#define	UART(cb, reg)		((cb)->ba + UART_##reg)

/*----------------------------------------------------------------------
 * Interrupt handler
 */
LOCAL void usart_inthdr( UINT intno )
{
	T_DEV_SER_LLDEVCB *cb;
	UW	unit, data;

	for ( unit = 0; unit < DEV_SER_UNITNM; ++unit ) {
		cb = &ll_devcb[unit];
		if ( INTNO(cb->ba) == intno ) break;
	}
	if ( unit >= DEV_SER_UNITNM ) return;

	/* Reception process */
	while ( in_w(UART(cb, EVENTS_RXDRDY)) != 0 ) {
		out_w(UART(cb, EVENTS_RXDRDY), 0);
		data = in_w(UART(cb, RXD));
		dev_ser_notify_rcv(unit, data);
	}

	/* Transmission process */
	if ( in_w(UART(cb, EVENTS_TXDRDY)) != 0 ) {
		out_w(UART(cb, EVENTS_TXDRDY), 0);
		if( dev_ser_get_snddat(unit, &data) ) {
			out_w(UART(cb, TXD), data);
			cb->txrdy = FALSE;
		} else {
			cb->txrdy = TRUE;
		}
	}

	/* Error handling */
	if ( in_w(UART(cb, EVENTS_ERROR)) != 0 ) {
		out_w(UART(cb, EVENTS_ERROR), 0);
		data = in_w(UART(cb, ERRORSRC));
		out_w(UART(cb, ERRORSRC), 0);
		dev_ser_notify_err(unit, data);
	}
}

/*----------------------------------------------------------------------
 * Set mode & Start communication
 */
LOCAL void start_com( T_DEV_SER_LLDEVCB *cb )
{
	out_w(UART(cb, EVENTS_CTS),    0);
	out_w(UART(cb, EVENTS_NCTS),   0);
	out_w(UART(cb, EVENTS_RXDRDY), 0);
	out_w(UART(cb, EVENTS_TXDRDY), 0);
	out_w(UART(cb, EVENTS_ERROR),  0);
	out_w(UART(cb, EVENTS_RXTO),   0);

	out_w(UART(cb, BAUDRATE), cb->baud);
	out_w(UART(cb, CONFIG), cb->mode & 0xffff);
	if ( (cb->mode & DEV_SER_MODE_CTSEN) != 0 ) {
		out_w(UART(cb, SHORTS), 0x18);
	} else {
		out_w(UART(cb, SHORTS), 0);
	}

	cb->txrdy = TRUE;

	out_w(UART(cb, ERRORSRC), 0);
	out_w(UART(cb, INTENSET), 0x284); /* RXDRDY TXDRDY ERROR int enable */

	out_w(UART(cb, TASKS_STARTTX), 1);
	out_w(UART(cb, TASKS_STARTRX), 1);
}

/*----------------------------------------------------------------------
 * Stop communication
 */
LOCAL void stop_com( T_DEV_SER_LLDEVCB *cb )
{
	cb->txrdy = FALSE;

	if( (cb - ll_devcb) != DEVCNF_SER_DBGUN) {
		out_w(UART(cb, TASKS_STOPRX), 1);
		out_w(UART(cb, TASKS_STOPTX), 1);
	}
	out_w(UART(cb, INTENCLR), 0xffffffff);
}

/*----------------------------------------------------------------------
 * Set baudrate
 */
LOCAL ER set_baudrate( T_DEV_SER_LLDEVCB *cb, UW speed )
{
	struct {
		UW	speed;
		UW	regval;
	} const tbl[] = {
		{    1200,	0x0004F000 },
		{    2400,	0x0009D000 },
		{    4800,	0x0013B000 },
		{    9600,	0x00275000 },
		{   14400,	0x003B0000 },
		{   19200,	0x004EA000 },
		{   28800,	0x0075F000 },
		{   31250,	0x00800000 },
		{   38400,	0x009D5000 },
		{   56000,	0x00E50000 },
		{   57600,	0x00EBF000 },
		{   76800,	0x013A9000 },
		{  115200,	0x01D7E000 },
		{  230400,	0x03AFB000 },
		{  250000,	0x04000000 },
		{  460800,	0x075F7000 },
		{  921600,	0x0EBED000 },
		{ 1000000,	0x10000000 }
	};
	INT	i;

	for ( i = 0; i < sizeof(tbl)/sizeof(tbl[0]); ++i ) {
		if ( tbl[i].speed == speed ) {
			cb->baud = tbl[i].regval;
			return E_OK;
		}
	}

	return E_PAR;
}

/*----------------------------------------------------------------------
 * Low level device control
 */
EXPORT ER dev_ser_llctl( UW unit, INT cmd, UW parm )
{
	T_DEV_SER_LLDEVCB *cb = &ll_devcb[unit];
	ER	err = E_OK;

	switch ( cmd ) {
	  case LLD_SER_MODE:	/* Set Communication mode */
		cb->mode = parm;
		break;

	  case LLD_SER_SPEED:	/* Set Communication Speed */
		err = set_baudrate(cb, parm);
		break;

	  case LLD_SER_START:	/* Start communication */
		start_com(cb);
		EnableInt(INTNO(cb->ba), DEVCNF_SER_INTPRI);
		break;

	  case LLD_SER_STOP:
		DisableInt(INTNO(cb->ba));
		stop_com(cb);
		break;

	  case LLD_SER_SEND:
		if ( cb->txrdy ) {
			cb->txrdy = FALSE;
			out_w(UART(cb, TXD), parm);
		} else {
			err = E_BUSY;
		}
		break;

	  case LLD_SER_BREAK:	/* Send Break */
		err = E_NOSPT;
		break;
	}

	return err;
}

/*----------------------------------------------------------------------
 * Device initialization
 */
EXPORT ER dev_ser_llinit( T_SER_DCB *p_dcb )
{
	T_DEV_SER_LLDEVCB *cb = &ll_devcb[p_dcb->unit];
	T_DINT const	dint = {
		.intatr	= TA_HLNG,
		.inthdr	= usart_inthdr,
	};
	ER	err;

	/* UART device initialize (Disable UART & Disable all interrupt) */
	out_w(UART(cb, ENABLE), 4);
	stop_com(cb);

	/* Device Control block Initizlize */
	p_dcb->intno_rcv = p_dcb->intno_snd = INTNO(cb->ba);

	/* Interrupt handler definition */
	err = tk_def_int(INTNO(cb->ba), &dint);

	return err;
}

#endif /* DEV_SER_ENABLE */
#endif /* CPU_NRF5 */
