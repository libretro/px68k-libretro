/*	OPM-like Sound Generator
 *	Copyright (C) cisc 1998, 2003.
 */

#ifndef FM_OPM_H
#define FM_OPM_H

#include "fmgen.h"
#include "fmtimer.h"
#include "psg.h"

namespace FM
{
   /* YM2151(OPM)  */
   class OPM : public Timer
   {
      public:
         OPM();
         virtual ~OPM() {}

         bool	Init(uint c, uint r, bool=false);
         bool	SetRate(uint c, uint r, bool);
         void	SetLPFCutoff(uint freq);
         void	Reset();

         void 	SetReg(uint addr, uint data);
         uint	GetReg(uint addr);
         uint	ReadStatus() { return status & 0x03; }

         void 	Mix(Sample* buffer, int nsamples, int rate, BYTE* pbsp, BYTE* pbep);

         void	SetVolume(int db);
         void	SetChannelMask(uint mask);

      private:
         virtual void Intr(bool) {}

      private:
         enum
         {
            OPM_LFOENTS = 512,
         };

         void	SetStatus(uint bit);
         void	ResetStatus(uint bit);
         void	SetParameter(uint addr, uint data);
         void	TimerA();
         void	RebuildTimeTable();
         void	MixSub(int activech, ISample**);
         void	MixSubL(int activech, ISample**);
         void	LFO();
         uint	Noise();

         int		fmvolume;

         uint	clock;
         uint	rate;
         uint	pcmrate;

         uint	pmd;
         uint	amd;
         uint	lfocount;
         uint	lfodcount;

         uint	lfo_count_;
         uint	lfo_count_diff_;
         uint	lfo_step_;
         uint	lfo_count_prev_;

         uint	lfowaveform;
         uint	rateratio;
         uint	noise;
         int32	noisecount;
         uint32	noisedelta;

         bool	interpolation;
         uint8	lfofreq;
         uint8	status;
         uint8	reg01;

         uint8	kc[8];
         uint8	kf[8];
         uint8	pan[8];

         Channel4 ch[8];
         Chip	chip;

         static void	BuildLFOTable();
         static int amtable[4][OPM_LFOENTS];
         static int pmtable[4][OPM_LFOENTS];

      public:
         int		dbgGetOpOut(int c, int s) { return ch[c].op[s].dbgopout_; }
         Channel4* dbgGetCh(int c) { return &ch[c]; }

   };
}

#endif /* FM_OPM_H */
