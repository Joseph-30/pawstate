/*
 *	machine.h
 *
 *	Machine type definition (micro:bit depended)
 */

#ifndef __SYS_SYSDEPEND_MACHINE_H__
#define __SYS_SYSDEPEND_MACHINE_H__

/*
 * [TYPE]_[CPU]		TARGET SYSTEM
 * CPU_xxxx		CPU type
 * CPU_CORE_xxx		CPU core type
 */

/* ----- micro:bit (CPU: nRF5) definition ----- */

#define MICROBIT		1	/* Target system   : micro:bit */
#define CPU_NRF5		1	/* Target CPU type : nRF5 series */
#define CPU_CORE_ARMV7M		1	/* Target CPU-Core type : ARMv7-M */
#define CPU_CORE_ACM4F		1	/* Target CPU-Core : ARM Cortex-M4F */

#define TARGET_DIR		microbit	/* Sysdepend-Directory name */

/*
 **** CPU-depeneded profile (nRF5)
 */
#include "../cpu/nrf5/machine.h"


#endif /* __SYS_SYSDEPEND_MACHINE_H__ */
