#include <sys/machine.h>
#ifdef CPU_NRF5

/*
 *	int_nrf5.c
 *
 *	Interrupt controller (nRF5)
 */
#include <tk/tkernel.h>
#include <tk/syslib.h>

#include "../core/armv7m/int_armv7m.h"

/*----------------------------------------------------------------------*/
/*
 * Interrupt control API
 */

/*
 * Enable interrupt
 */
EXPORT void EnableInt(UINT intno, INT level)
{
	EnableInt_nvic(intno, level);
}

/*
 * Disable interrupt
 */
EXPORT void DisableInt(UINT intno)
{
	DisableInt_nvic(intno);
}

/*
 * Clear interrupt
 */
EXPORT void ClearInt(UINT intno)
{
	ClearInt_nvic(intno);
}

/*
 * Issue EOI to interrupt controller
 */
EXPORT void EndOfInt(UINT intno)
{
	/* No opetarion. */
}

/*
 * Check active state
 */
EXPORT BOOL CheckInt(UINT intno)
{
	return CheckInt_nvic(intno);
}

/*
 * Set interrupt mode
 */
EXPORT void SetIntMode(UINT intno, UINT mode)
{
	/* No opetarion. */
}

#endif /* CPU_NRF5 */
