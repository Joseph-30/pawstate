/*
 *	sysdef.h
 *
 *	System dependencies definition (nRF5 depended)
 *	Included also from assembler program.
 */

#ifndef __TK_SYSDEF_DEPEND_CPU_H__
#define __TK_SYSDEF_DEPEND_CPU_H__

#include "../../../machine.h"

/* CPU Core-dependent definition */
#include "../core/armv7m/sysdef.h"

/* ------------------------------------------------------------------------ */
/*
 * Internal Memorie (Main RAM)
 */
#define INTERNAL_RAM_SIZE       0x00020000	/* nRF52833 */
#define INTERNAL_RAM_START      0x20000000

#define INTERNAL_RAM_END        (INTERNAL_RAM_START+INTERNAL_RAM_SIZE)

/* ------------------------------------------------------------------------ */
/*
 * Initial Stack pointer (Used in initialization process)
 */
#define	INITIAL_SP		INTERNAL_RAM_END

/* ------------------------------------------------------------------------ */
/*
 * System Timer clock
 */

/* Settable interval range (millisecond) */
#define MIN_TIMER_PERIOD	1
#define MAX_TIMER_PERIOD	50

/* ------------------------------------------------------------------------ */
/*
 * Number of Interrupt vectors
 */
#define N_INTVEC		48	/* Number of Interrupt vectors */
#define	N_SYSVEC		16	/* Number of System Exceptions */

/*
 * The number of the implemented bit width for priority value fields.
 */
#define INTPRI_BITWIDTH		3

/*
 * Interrupt number = Peripheral ID
 */
#define	INTNO(ba)		(((ba) >> 12) & 0x3f)

/* ------------------------------------------------------------------------ */
/*
 * Interrupt Priority Levels
 */
#define INTPRI_MAX_EXTINT_PRI	1	/* Highest Ext. interrupt level */
#define INTPRI_SVC		0	/* SVCall */
#define INTPRI_SYSTICK		1	/* SysTick */
#define INTPRI_PENDSV		7	/* PendSV */

/*
 * Time-event handler interrupt level
 */
#define TIMER_INTLEVEL		0

/* ------------------------------------------------------------------------ */
/*
 * Coprocessor
 */
#define CPU_HAS_FPU		1
#define CPU_HAS_DSP		0

/*
 *  Number of coprocessors to use. Depends on user configuration
 */
#if USE_FPU
#define NUM_COPROCESSOR		1
#else
#define NUM_COPROCESSOR		0
#endif

/* ------------------------------------------------------------------------ */
/*
 * Clock control (CLOCK)
 */
#define CLOCK_BASE	0x40000000

#define	CLOCK_TASKS_HFCLKSTART		(CLOCK_BASE + 0x000)
#define	CLOCK_TASKS_HFCLKSTOP		(CLOCK_BASE + 0x004)
#define	CLOCK_TASKS_LFCLKSTART		(CLOCK_BASE + 0x008)
#define	CLOCK_TASKS_LFCLKSTOP		(CLOCK_BASE + 0x00C)
#define	CLOCK_TASKS_CAL			(CLOCK_BASE + 0x010)
#define	CLOCK_TASKS_CTSTART		(CLOCK_BASE + 0x014)
#define	CLOCK_TASKS_CTSTOP		(CLOCK_BASE + 0x018)
#define	CLOCK_EVENTS_HFCLKSTARTED	(CLOCK_BASE + 0x100)
#define	CLOCK_EVENTS_LFCLKSTARTED	(CLOCK_BASE + 0x104)
#define	CLOCK_EVENTS_DONE		(CLOCK_BASE + 0x10C)
#define	CLOCK_EVENTS_CTTO		(CLOCK_BASE + 0x110)
#define	CLOCK_EVENTS_CTSTARTED		(CLOCK_BASE + 0x128)
#define	CLOCK_EVENTS_CTSTOPPED		(CLOCK_BASE + 0x12C)
#define	CLOCK_INTENSET			(CLOCK_BASE + 0x304)
#define	CLOCK_INTENCLR			(CLOCK_BASE + 0x308)
#define	CLOCK_HFCLKRUN			(CLOCK_BASE + 0x408)
#define	CLOCK_HFCLKSTAT			(CLOCK_BASE + 0x40C)
#define	CLOCK_LFCLKRUN			(CLOCK_BASE + 0x414)
#define	CLOCK_LFCLKSTAT			(CLOCK_BASE + 0x418)
#define	CLOCK_LFCLKSRCCOPY		(CLOCK_BASE + 0x41C)
#define	CLOCK_LFCLKSRC			(CLOCK_BASE + 0x518)
#define	CLOCK_HFXODEBOUNCE		(CLOCK_BASE + 0x528)
#define	CLOCK_LFXODEBOUNCE		(CLOCK_BASE + 0x52C)
#define	CLOCK_CTIV			(CLOCK_BASE + 0x538)
#define	CLOCK_TRACECONFIG		(CLOCK_BASE + 0x55C)

/* ------------------------------------------------------------------------ */
/*
 * General purpose input/output (GPIO)
 */
#define	GPIO(p, r)	(GPIO_##p##_BASE + GPIO_##r)

#define	GPIO_P0_BASE	0x50000000
#define	GPIO_P1_BASE	0x50000300

#define	GPIO_OUT			(0x504)
#define	GPIO_OUTSET			(0x508)
#define	GPIO_OUTCLR			(0x50C)
#define	GPIO_IN				(0x510)
#define	GPIO_DIR			(0x514)
#define	GPIO_DIRSET			(0x518)
#define	GPIO_DIRCLR			(0x51C)
#define	GPIO_LATCH			(0x520)
#define	GPIO_DETECTMODE			(0x524)
#define	GPIO_PIN_CNF(n)			(0x700 + (n)*4)

/* ------------------------------------------------------------------------ */
/*
 * Timer/counter (TIMER)
 */
#define	TIMER(p, r)	(TIMER##p##_BASE + TIMER_##r)

#define	TIMER0_BASE	0x40008000
#define	TIMER1_BASE	0x40009000
#define	TIMER2_BASE	0x4000A000
#define	TIMER3_BASE	0x4001A000
#define	TIMER4_BASE	0x4001B000

#define	TIMER_TASKS_START		(0x000)
#define	TIMER_TASKS_STOP		(0x004)
#define	TIMER_TASKS_COUNT		(0x008)
#define	TIMER_TASKS_CLEAR		(0x00C)
#define	TIMER_TASKS_SHUTDOWN		(0x010)
#define	TIMER_TASKS_CAPTURE(n)		(0x040 + (n)*4)
#define	TIMER_EVENTS_COMPARE(n)		(0x140 + (n)*4)
#define	TIMER_SHORTS			(0x200)
#define	TIMER_INTENSET			(0x304)
#define	TIMER_INTENCLR			(0x308)
#define	TIMER_MODE			(0x504)
#define	TIMER_BITMODE			(0x508)
#define	TIMER_PRESCALER			(0x510)
#define	TIMER_CC(n)			(0x540 + (n)*4)

#define	CPU_HAS_PTMR		1

/* ------------------------------------------------------------------------ */
/*
 * Universal asynchronous receiver/ transmitter (UART)
 */
#define	UART(p, r)	(UART##p##_BASE + UART_##r)

#define	UART0_BASE	0x40002000

#define	UART_TASKS_STARTRX		(0x000)
#define	UART_TASKS_STOPRX		(0x004)
#define	UART_TASKS_STARTTX		(0x008)
#define	UART_TASKS_STOPTX		(0x00C)
#define	UART_TASKS_SUSPEND		(0x01C)
#define	UART_EVENTS_CTS			(0x100)
#define	UART_EVENTS_NCTS		(0x104)
#define	UART_EVENTS_RXDRDY		(0x108)
#define	UART_EVENTS_TXDRDY		(0x11C)
#define	UART_EVENTS_ERROR		(0x124)
#define	UART_EVENTS_RXTO		(0x144)
#define	UART_SHORTS			(0x200)
#define	UART_INTENSET			(0x304)
#define	UART_INTENCLR			(0x308)
#define	UART_ERRORSRC			(0x480)
#define	UART_ENABLE			(0x500)
#define	UART_PSEL_RTS			(0x508)
#define	UART_PSEL_TXD			(0x50C)
#define	UART_PSEL_CTS			(0x510)
#define	UART_PSEL_RXD			(0x514)
#define	UART_RXD			(0x518)
#define	UART_TXD			(0x51C)
#define	UART_BAUDRATE			(0x524)
#define	UART_CONFIG			(0x56C)

/* ------------------------------------------------------------------------ */

#endif /* __TK_SYSDEF_DEPEND_CPU_H__ */
