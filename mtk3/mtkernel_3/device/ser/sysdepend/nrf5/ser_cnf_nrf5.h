/*
 *	ser_cnf_nrf5.h
 *	Serial Device configuration file
 *		for nRF5
 */
#ifndef	__DEV_SER_CNF_NRF5_H__
#define	__DEV_SER_CNF_NRF5_H__

/* Device control data */
#define	DEVCNF_SER_INTPRI	5		// Interrupt priority

/* Debug option
 *	Specify the device used by T-Monitor.
 *	  0: "sera" - UART0
 *	  other : T-Monitor does not use serial devices
 */
#if USE_TMONITOR
#define	DEVCNF_SER_DBGUN	0		// Used by T-Monitor
#else
#define	DEVCNF_SER_DBGUN	(-1)		// T-Monitor not executed
#endif

#endif /* __DEV_SER_CNF_NRF5_H__ */
