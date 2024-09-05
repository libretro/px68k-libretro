// ---------------------------------------------------------------------------
//	OPM-like Sound Generator
//	Copyright (C) cisc 1998, 2003.
// ---------------------------------------------------------------------------

#ifndef FM_OPM_H
#define FM_OPM_H

#include <stdint.h>

#include "fmgen.h"
#include "fmtimer.h"
#include "psg.h"

namespace FM
{
	//	YM2151(OPM) ----------------------------------------------------
	class OPM : public Timer
	{
	public:
		OPM();
		virtual ~OPM() {}

		bool	Init(uint32_t c, uint32_t r);
		bool	SetRate(uint32_t c, uint32_t r);
		void	Reset();
		
		void 	SetReg(uint32_t addr, uint32_t data);
		uint32_t	GetReg(uint32_t addr);
		uint32_t	ReadStatus() { return status & 0x03; }
		
		void 	Mix(int16_t* buffer, int nsamples, uint8_t* pbsp, uint8_t* pbep);
		
		void	SetVolume(int db);
		void	SetChannelMask(uint32_t mask);

		int		StateAction(StateMem *sm, int load, int data_only);
		
	private:
		virtual void Intr(bool) {}
	
	private:
		enum
		{
			OPM_LFOENTS = 512,
		};
		
		void	SetStatus(uint32_t bit);
		void	ResetStatus(uint32_t bit);
		void	SetParameter(uint32_t addr, uint32_t data);
		void	TimerA();
		void	RebuildTimeTable();
		void	MixSub(int activech, ISample**);
		void	MixSubL(int activech, ISample**);
		void	LFO();
		uint32_t	Noise();
		
		int		fmvolume;

		uint32_t	clock;
		uint32_t	rate;

		uint32_t	pmd;
		uint32_t	amd;
		uint32_t	lfocount;
		uint32_t	lfodcount;

		uint32_t	lfo_count_;
		uint32_t	lfo_count_diff_;
		uint32_t	lfo_step_;
		uint32_t	lfo_count_prev_;

		uint32_t	lfowaveform;
		uint32_t	rateratio;
		uint32_t	noise;
		int32_t		noisecount;
		uint32_t	noisedelta;
		
		uint8_t	lfofreq;
		uint8_t	status;
		uint8_t	reg01;

		uint8_t	kc[8];
		uint8_t	kf[8];
		uint8_t	pan[8];

		Channel4 ch[8];
		Chip	chip;

		static void	BuildLFOTable();
		static int amtable[4][OPM_LFOENTS];
		static int pmtable[4][OPM_LFOENTS];
	};
}

#endif // FM_OPM_H
