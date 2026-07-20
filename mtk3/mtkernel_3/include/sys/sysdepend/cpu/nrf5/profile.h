/*
 *	profile.h
 *
 *	Service Profile (nRF5 depended)
 */

#ifndef __SYS_DEPEND_PROFILE_CPU_H__
#define __SYS_DEPEND_PROFILE_CPU_H__

/*
 **** CPU core-depeneded profile (ARM Cortex-M4)
 */
#include "../core/armv7m/profile.h"

/*
 **** CPU-depeneded profile (nRF5)
 */

/*
 * Device Support
 */
#define TK_SUPPORT_IOPORT	TRUE	/* Support of I/O port access */

/*
 * Physical timer
 */
#if USE_PTMR
#define TK_SUPPORT_PTIMER	TRUE	/* Support of physical timer */
#define TK_MAX_PTIMER		5	/* Maximum number of physical timers. */
#else
#define TK_SUPPORT_PTIMER	FALSE	/* Support of physical timer */
#define TK_MAX_PTIMER		0	/* Maximum number of physical timers. */
#endif


#endif /* __SYS_DEPEND_PROFILE_CPU_H__ */
