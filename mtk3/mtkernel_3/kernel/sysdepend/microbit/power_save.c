#include <sys/machine.h>
#ifdef MICROBIT

#include <tk/tkernel.h>
#include <kernel.h>

/*
 *	power_save.c (micro:bit)
 *	Power-Saving Function
 */

#include "sysdepend.h"

/*
 * Switch to power-saving mode
 */
EXPORT void low_pow( void )
{
}

/*
 * Move to suspend mode
 */
EXPORT void off_pow( void )
{
}


#endif /* MICROBIT */
