/*
 *	ser_mode_nrf5.h
 *	Serial communication device driver
 *	Communication mode definition for nRF5
 */

#ifndef	__DEV_SER_MODE_NRF5_H__
#define	__DEV_SER_MODE_NRF5_H__

/*	DEV_SER_MODE_7BIT	not support */	/* Word length 7bit */
#define	DEV_SER_MODE_8BIT	(0x00000000)	/* Word length 8bit */

#define	DEV_SER_MODE_1STOP	(0x00000000)	/* Stop-bit 1bit */
#define	DEV_SER_MODE_2STOP	(0x00000010)	/* Stop-bit 2bit */

#define DEV_SER_MODE_PODD	(0x0000010e)	/* parity Odd */
#define DEV_SER_MODE_PEVEN	(0x0000000e)	/* parity Even */
#define DEV_SER_MODE_PNON	(0x00000000)	/* parity None */

#define	DEV_SER_MODE_CTSEN	(0x00010000)	/* CTS Hard flow control enable */
#define	DEV_SER_MODE_RTSEN	(0x00000001)	/* RTS Hard flow control enable */

/* Communication Error */
#define	DEV_SER_ERR_OE		(1<<0)	/* Overrun Error */
#define	DEV_SER_ERR_PE		(1<<1)	/* Parity Error */
#define	DEV_SER_ERR_FE		(1<<2)	/* Framing Error */
#define	DEV_SER_ERR_BE		(1<<3)	/* Break Detection */

#endif /* __DEV_SER_MODE_NRF5_H__ */
