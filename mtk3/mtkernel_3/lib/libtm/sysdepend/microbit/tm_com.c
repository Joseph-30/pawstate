/*
 *    tm_com.c
 *    T-Monitor Communication low-level device driver (micro:bit)
 */

#include <tk/typedef.h>
#include <tk/syslib.h>
#include <sys/sysdef.h>

#if USE_TMONITOR
#include "../../libtm.h"

#ifdef MICROBIT
#if TM_COM_SERIAL_DEV

/* UART register */
#define	UART_(r)	UART(0, r)

/* Communication speed */
#define UART_BAUDRATE_VALUE	(0x01D7E000)		/* 115200 bps */

EXPORT	void	tm_snd_dat( const UB* buf, INT size )
{
	while ( size-- > 0 ) {
		out_w(UART_(EVENTS_TXDRDY), 0);
		out_w(UART_(TXD), *buf++);
		while ( (in_w(UART_(EVENTS_TXDRDY)) & 1) == 0 );
	}
}

EXPORT	void	tm_rcv_dat( UB* buf, INT size )
{
	while ( size-- > 0 ) {
		while ( (in_w(UART_(EVENTS_RXDRDY)) & 1) == 0 );
		out_w(UART_(EVENTS_RXDRDY), 0);
		*buf++ = in_w(UART_(RXD)) & 0xff;
	}
}

EXPORT	void	tm_com_init(void)
{
	out_w(UART_(ENABLE), 4);

	out_w(UART_(TASKS_STOPRX), 1);
	out_w(UART_(TASKS_STOPTX), 1);

	out_w(UART_(INTENCLR), 0xffffffff);
	out_w(UART_(BAUDRATE), UART_BAUDRATE_VALUE);
	out_w(UART_(CONFIG), 0);	/* non-parity 1-stop no-flow */
	out_w(UART_(SHORTS), 0);
	out_w(UART_(ERRORSRC), 0);

	out_w(UART_(EVENTS_CTS),    0);
	out_w(UART_(EVENTS_NCTS),   0);
	out_w(UART_(EVENTS_RXDRDY), 0);
	out_w(UART_(EVENTS_TXDRDY), 0);
	out_w(UART_(EVENTS_ERROR),  0);
	out_w(UART_(EVENTS_RXTO),   0);

	out_w(UART_(TASKS_STARTRX), 1);
	out_w(UART_(TASKS_STARTTX), 1);
}

#endif /* TM_COM_SERIAL_DEV */
#endif /* MICROBIT */
#endif /* USE_TMONITOR */
