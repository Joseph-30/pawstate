#include <sys/machine.h>
#ifdef CPU_NRF5
/*
 *	ptimer_nrf5.c
 *
 *	Physical timer (for nRF5)
 */
#include <tk/tkernel.h>
#include <tk/syslib.h>

#if USE_PTMR

typedef struct {
	UW	baddr;		// Register Base Address
	FP	ptmrhdr;	// Timer Hnadler
	void	*exinf;		// Extended information
} T_PTMRCB;

LOCAL T_PTMRCB ptmrcb[TK_MAX_PTIMER] = {
	{ TIMER0_BASE },
	{ TIMER1_BASE },
	{ TIMER2_BASE },
	{ TIMER3_BASE },
	{ TIMER4_BASE },
};

#define	TMR(cb, r)	((cb)->baddr + TIMER_##r)	/* register address */
#define	INTLEVEL	3				/* interrupt priority */

/*
 * Check the physical timer number
 */
LOCAL T_PTMRCB* chk_ptmrno( UINT ptmrno )
{
	if ( (ptmrno < 1) || (ptmrno > TK_MAX_PTIMER) ) return NULL;
	return &ptmrcb[ptmrno - 1];
}

/*
 * Physical timer interrupt handler
 */
LOCAL void ptmr_inthdr( UINT intno )
{
	T_PTMRCB	*cb;
	INT		i;

	for ( i = 0; i < TK_MAX_PTIMER; ++i ) {
		cb = &ptmrcb[i];
		if ( INTNO(cb->baddr) == intno ) break;
	}
	if ( i >= TK_MAX_PTIMER ) return;

	/* clear interrupt */
	out_w(TMR(cb, EVENTS_COMPARE(0)), 0);

	if( cb->ptmrhdr != NULL ) {
		/* call user-defined interrupt handler */
		(*cb->ptmrhdr)(cb->exinf);
	}
}

/* ------------------------------------------------------------------------ */

/*
 * Start physical timer
 */
EXPORT ER StartPhysicalTimer( UINT ptmrno, UW limit, UINT mode )
{
	T_PTMRCB	*cb;
	T_DINT		dint;
	UINT		intno;
	ER		err;

	/* check parameter */
	if ( (mode != TA_ALM_PTMR) && (mode != TA_CYC_PTMR) ) return E_PAR;
	if ( limit == 0 ) return E_PAR;

	cb = chk_ptmrno(ptmrno);
	if ( cb == NULL ) return E_PAR;

	/* stop timer */
	out_w(TMR(cb, TASKS_STOP), 1);
	out_w(TMR(cb, INTENCLR), 0x003f0000);

	/* Set timer mode */
	out_w(TMR(cb, PRESCALER), 0);	/* 16MHz / 2^0 */
	out_w(TMR(cb, BITMODE), 3);	/* 32bit width */
	out_w(TMR(cb, MODE), 0);	/* timer mode */

	/* Set counter */
	out_w(TMR(cb, TASKS_CLEAR), 1);
	out_w(TMR(cb, CC(0)), limit);

	if ( mode == TA_ALM_PTMR ) {
		out_w(TMR(cb, SHORTS), 0x0101);	/* stop & clear */
	} else {
		out_w(TMR(cb, SHORTS), 0x0001);	/* clear */
	}
	out_w(TMR(cb, EVENTS_COMPARE(0)), 0);
	out_w(TMR(cb, EVENTS_COMPARE(1)), 0);
	out_w(TMR(cb, EVENTS_COMPARE(2)), 0);
	out_w(TMR(cb, EVENTS_COMPARE(3)), 0);
	out_w(TMR(cb, EVENTS_COMPARE(4)), 0);
	out_w(TMR(cb, EVENTS_COMPARE(5)), 0);

	/* Set up timer interrupt handler */
	intno = INTNO(cb->baddr);
	dint.intatr = TA_HLNG;
	dint.inthdr = (FP)ptmr_inthdr;
	err = tk_def_int(intno, &dint);
	if ( err < E_OK ) return err;

	/* Enable timer interrupt */
	EnableInt(intno, INTLEVEL);
	out_w(TMR(cb, INTENSET), 0x00010000);	/* CC[0] enable */

	/* Start counting */
	out_w(TMR(cb, TASKS_START), 1);

	return E_OK;
}

/*
 * Stop physical timer
 */
EXPORT ER StopPhysicalTimer( UINT ptmrno )
{
	T_PTMRCB	*cb;

	/* check parameter */
	cb = chk_ptmrno(ptmrno);
	if ( cb == NULL ) return E_PAR;

	/* Stop counting */
	out_w(TMR(cb, TASKS_STOP), 1);

	/* Stop timer interrupt */
	out_w(TMR(cb, INTENCLR), 0x003f0000);
	DisableInt(INTNO(cb->baddr));

	return E_OK;
}

/*
 * Obtain the count of the physical timer
 */
EXPORT ER GetPhysicalTimerCount( UINT ptmrno, UW *p_count )
{
	T_PTMRCB	*cb;

	/* check parameter */
	cb = chk_ptmrno(ptmrno);
	if ( cb == NULL ) return E_PAR;

	/* capture counter value */
	out_w(TMR(cb, TASKS_CAPTURE(1)), 1);
	*p_count = in_w(TMR(cb, CC(1)));

	return E_OK;
}

/*
 * Definition of physical timer hander
 */
EXPORT ER DefinePhysicalTimerHandler( UINT ptmrno, CONST T_DPTMR *pk_dptmr )
{
	UINT		imsk;
	T_PTMRCB	*cb;

	/* check parameter */
	cb = chk_ptmrno(ptmrno);
	if ( cb == NULL ) return E_PAR;

	if ( pk_dptmr != NULL ) {
		if ( (pk_dptmr->ptmratr & ~(TA_ASM|TA_HLNG)) != 0 )
					return E_PAR;
		DI(imsk);
		cb->exinf   = pk_dptmr->exinf;
		cb->ptmrhdr = pk_dptmr->ptmrhdr;
		EI(imsk);
	} else {
		cb->ptmrhdr = NULL;
	}

	return E_OK;
}

/*
 * Obtain the configuration information of physical timer
 */
EXPORT ER GetPhysicalTimerConfig( UINT ptmrno, T_RPTMR *pk_rptmr )
{
	T_PTMRCB	*cb;

	/* check parameter */
	cb = chk_ptmrno(ptmrno);
	if ( cb == NULL ) return E_PAR;

	/* set configuration */
	pk_rptmr->ptmrclk  = 16000000;	/* 16MHz */
	pk_rptmr->maxcount = 0xffffffff;
	pk_rptmr->defhdr   = TRUE;

	return E_OK;
}

#endif	/* USE_PTMR */
#endif	/* CPU_NRF5 */
