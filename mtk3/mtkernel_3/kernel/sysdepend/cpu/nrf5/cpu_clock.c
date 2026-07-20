#include <sys/machine.h>
#ifdef CPU_NRF5

/*
 *	cpu_clock.c (nRF5)
 *	Clock Setting
 */

#include <tk/tkernel.h>

#include "sysdepend.h"

/*
 *  Startup System Clock
 */
EXPORT void startup_clock(void)
{
	/* HFCLK: external oscillator (HFXO) */
	out_w(CLOCK_EVENTS_HFCLKSTARTED, 0);
	out_w(CLOCK_HFXODEBOUNCE, 0x40);
	out_w(CLOCK_TASKS_HFCLKSTART, 1);
	while ( (in_w(CLOCK_EVENTS_HFCLKSTARTED) & 1) == 0 );

	/* LFCLK: synthesized from HFCLK (LFSYNT) */
	out_w(CLOCK_EVENTS_LFCLKSTARTED, 0);
	out_w(CLOCK_LFCLKSRC, 0x00000002);
	out_w(CLOCK_TASKS_LFCLKSTART, 1);
	while ( (in_w(CLOCK_EVENTS_LFCLKSTARTED) & 1) == 0 );
}

EXPORT void shutdown_clock(void)
{

}

#endif /* CPU_NRF5 */
