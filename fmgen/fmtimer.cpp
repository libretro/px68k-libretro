// ---------------------------------------------------------------------------
//	FM sound generator common timer module
//	Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//	$fmgen-Id: fmtimer.cpp,v 1.1 2000/09/08 13:45:56 cisc Exp $

#include "common.h"
#include "fmgen_types.h"
#include "headers.h"
#include "fmtimer.h"

using namespace FM;

// ---------------------------------------------------------------------------
//	•ø•§•ﬁ°º¿©∏Ê
//
void Timer::SetTimerControl(uint data)
{
	uint tmp = regtc ^ data;
	regtc = uint8(data);
	
	if (data & 0x10) 
		ResetStatus(1);
	if (data & 0x20) 
		ResetStatus(2);

	if (tmp & 0x01)
		timera_count = (data & 1) ? timera : 0;
	if (tmp & 0x02)
		timerb_count = (data & 2) ? timerb : 0;
}

#if 1

// ---------------------------------------------------------------------------
//	•ø•§•ﬁ°ºA º˛¥¸¿ﬂƒÍ
//
void Timer::SetTimerA(uint addr, uint data)
{
	uint tmp;
	regta[addr & 1] = uint8(data);
	tmp = (regta[0] << 2) + (regta[1] & 3);
	timera = (1024-tmp) * timer_step;
//	LOG2("Timer A = %d   %d us\n", tmp, timera >> 16);
}

// ---------------------------------------------------------------------------
//	•ø•§•ﬁ°ºB º˛¥¸¿ﬂƒÍ
//
void Timer::SetTimerB(uint data)
{
	timerb = (256-data) * timer_step;
//	LOG2("Timer B = %d   %d us\n", data, timerb >> 12);
}

// ---------------------------------------------------------------------------
//	•ø•§•ﬁ°ºª˛¥÷ΩËÕ˝
//
bool Timer::Count(int32 us)
{
	bool event = false;

	if (timera_count)
	{
		timera_count -= us << 16;
		if (timera_count <= 0)
		{
			event = true;
			TimerA();

			while (timera_count <= 0)
				timera_count += timera;
			
			if (regtc & 4)
				SetStatus(1);
		}
	}
	if (timerb_count)
	{
		timerb_count -= us << 12;
		if (timerb_count <= 0)
		{
			event = true;
			while (timerb_count <= 0)
				timerb_count += timerb;
			
			if (regtc & 8)
				SetStatus(2);
		}
	}
	return event;
}

// ---------------------------------------------------------------------------
//	º°§À•ø•§•ﬁ°º§¨»Ø¿∏§π§Î§ﬁ§«§Œª˛¥÷§Úµ·§·§Î
//
int32 Timer::GetNextEvent()
{
	uint32 ta = ((timera_count + 0xffff) >> 16) - 1;
	uint32 tb = ((timerb_count + 0xfff) >> 12) - 1;
	return (ta < tb ? ta : tb) + 1;
}

// ---------------------------------------------------------------------------
//	•ø•§•ﬁ°º¥Ω‡√Õ¿ﬂƒÍ
//
void Timer::SetTimerBase(uint clock)
{
	timer_step = int32(1000000. * 65536 / clock);
}

#else

// ---------------------------------------------------------------------------
//	•ø•§•ﬁ°ºA º˛¥¸¿ﬂƒÍ
//
void Timer::SetTimerA(uint addr, uint data)
{
	regta[addr & 1] = uint8(data);
	timera = (1024 - ((regta[0] << 2) + (regta[1] & 3))) << 16;
}

// ---------------------------------------------------------------------------
//	•ø•§•ﬁ°ºB º˛¥¸¿ﬂƒÍ
//
void Timer::SetTimerB(uint data)
{
	timerb = (256-data) << (16 + 4);
}

// ---------------------------------------------------------------------------
//	•ø•§•ﬁ°ºª˛¥÷ΩËÕ˝
//
bool Timer::Count(int32 us)
{
	bool event = false;

	int tick = us * timer_step;

	if (timera_count)
	{
		timera_count -= tick;
		if (timera_count <= 0)
		{
			event = true;
			TimerA();

			while (timera_count <= 0)
				timera_count += timera;
			
			if (regtc & 4)
				SetStatus(1);
		}
	}
	if (timerb_count)
	{
		timerb_count -= tick;
		if (timerb_count <= 0)
		{
			event = true;
			while (timerb_count <= 0)
				timerb_count += timerb;
			
			if (regtc & 8)
				SetStatus(2);
		}
	}
	return event;
}

// ---------------------------------------------------------------------------
//	º°§À•ø•§•ﬁ°º§¨»Ø¿∏§π§Î§ﬁ§«§Œª˛¥÷§Úµ·§·§Î
//
int32 Timer::GetNextEvent()
{
	uint32 ta = timera_count - 1;
	uint32 tb = timerb_count - 1;
	uint32 t = (ta < tb ? ta : tb) + 1;

	return (t+timer_step-1) / timer_step;
}

// ---------------------------------------------------------------------------
//	•ø•§•ﬁ°º¥Ω‡√Õ¿ﬂƒÍ
//
void Timer::SetTimerBase(uint clock)
{
	timer_step = clock * 1024 / 15625;
}

#endif
