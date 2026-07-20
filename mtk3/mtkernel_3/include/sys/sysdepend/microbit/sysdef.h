/*
 *	sysdef.h
 *
 *	System dependencies definition (micro:bit depended)
 *	Included also from assembler program.
 */

#ifndef __SYS_SYSDEF_DEPEND_H__
#define __SYS_SYSDEF_DEPEND_H__


/* CPU-dependent definition */
#include "../cpu/nrf5/sysdef.h"

/* ------------------------------------------------------------------------ */
/*
 * Clock control definition
 */
#define TMCLK_KHz	(64 * 1000)	/* System timer clock input (kHz) */

/* ------------------------------------------------------------------------ */
/*
 * Maximum value of Power-saving mode switching prohibition request.
 * Used in tk_set_pow API.
 */
#define LOWPOW_LIMIT	0x7fff		/* Maximum number for disabling */


#endif /* __TK_SYSDEF_DEPEND_H__ */
