/*
 *   55.6fpsキープ用たいまー
 */
#include "common.h"
#include "crtc.h"
#include "mfp.h"

uint32_t tick = 0;
uint32_t timercnt = 0;

/* Get elapsed time from libretro frontend by way of its frame time callback,
 * if available. Only provides per frame granularity, enough for this case
 * since it's only called once per frame, so can't replace
 * timeGetTime/FAKE_GetTickCount entirely
 */
static unsigned int timeGetUsec(void)
{
	extern unsigned int total_usec;		/* from libretro.c */
	if (total_usec == (unsigned int) -1)
		return timeGetTime() * 1000;
	return total_usec;
}

void Timer_Init(void)
{
	tick = timeGetUsec();
}

uint16_t Timer_GetCount(void)
{
	uint32_t ticknow   = timeGetUsec();
	uint32_t dif       = ticknow-tick;
	uint32_t TIMEBASE  = ((CRTC_Regs[0x29]&0x10)?VSYNC_HIGH:VSYNC_NORM);
	timercnt          += dif*10;  /* switch from msec to usec */
	tick               = ticknow;
	if (timercnt >= TIMEBASE)
	{
		timercnt -= TIMEBASE;
		if (timercnt>=(TIMEBASE*2)) timercnt = 0;
		return 1;
	}
	return 0;
}
