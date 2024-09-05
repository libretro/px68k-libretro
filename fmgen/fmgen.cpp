// ---------------------------------------------------------------------------
//	FM Sound Generator - Core Unit
//	Copyright (C) cisc 1998, 2003.
// ---------------------------------------------------------------------------
//	$fmgen-Id: fmgen.cpp,v 1.49 2003/09/02 14:51:04 cisc Exp $
// ---------------------------------------------------------------------------
//	参考:
//		FM sound generator for M.A.M.E., written by Tatsuyuki Satoh.
//
// 	謎:
//		OPNB の CSM モード(仕様がよくわからない)
//
//	制限:
//		・AR!=31 で SSGEC を使うと波形が実際と異なる可能性あり
//
//	謝辞:
//		Tatsuyuki Satoh さん(fm.c)
//		Hiromitsu Shioya さん(ADPCM-A)
//		DMP-SOFT. さん(OPNB)
//		KAJA さん(test program)
//		ほか掲示板等で様々なご助言，ご支援をお寄せいただいた皆様に
// ---------------------------------------------------------------------------

#include <math.h>
#include "misc.h"
#include "fmgen.h"
#include "fmgeninl.h"

// ---------------------------------------------------------------------------

#define FM_EG_BOTTOM 955

// ---------------------------------------------------------------------------
//	Table/etc
//
namespace FM
{
	const uint8_t Operator::notetable[128] =
	{
		 0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  3,  3,  3,  3,  3,  3, 
		 4,  4,  4,  4,  4,  4,  4,  5,  6,  7,  7,  7,  7,  7,  7,  7, 
		 8,  8,  8,  8,  8,  8,  8,  9, 10, 11, 11, 11, 11, 11, 11, 11, 
		12, 12, 12, 12, 12, 12, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 
		16, 16, 16, 16, 16, 16, 16, 17, 18, 19, 19, 19, 19, 19, 19, 19, 
		20, 20, 20, 20, 20, 20, 20, 21, 22, 23, 23, 23, 23, 23, 23, 23, 
		24, 24, 24, 24, 24, 24, 24, 25, 26, 27, 27, 27, 27, 27, 27, 27, 
		28, 28, 28, 28, 28, 28, 28, 29, 30, 31, 31, 31, 31, 31, 31, 31, 
	};
	
	const int8_t Operator::dttable[256] =
	{
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		  0,  0,  0,  0,  2,  2,  2,  2,  2,  2,  2,  2,  4,  4,  4,  4,
		  4,  6,  6,  6,  8,  8,  8, 10, 10, 12, 12, 14, 16, 16, 16, 16,
		  2,  2,  2,  2,  4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8, 10,
		 10, 12, 12, 14, 16, 16, 18, 20, 22, 24, 26, 28, 32, 32, 32, 32,
		  4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8, 10, 10, 12, 12, 14,
		 16, 16, 18, 20, 22, 24, 26, 28, 32, 34, 38, 40, 44, 44, 44, 44,
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		  0,  0,  0,  0, -2, -2, -2, -2, -2, -2, -2, -2, -4, -4, -4, -4,
		 -4, -6, -6, -6, -8, -8, -8,-10,-10,-12,-12,-14,-16,-16,-16,-16,
		 -2, -2, -2, -2, -4, -4, -4, -4, -4, -6, -6, -6, -8, -8, -8,-10,
		-10,-12,-12,-14,-16,-16,-18,-20,-22,-24,-26,-28,-32,-32,-32,-32,
		 -4, -4, -4, -4, -4, -6, -6, -6, -8, -8, -8,-10,-10,-12,-12,-14,
		-16,-16,-18,-20,-22,-24,-26,-28,-32,-34,-38,-40,-44,-44,-44,-44,
	};

	const int8_t Operator::decaytable1[64][8] = 
	{
		0, 0, 0, 0, 0, 0, 0, 0,		0, 0, 0, 0, 0, 0, 0, 0,
		1, 1, 1, 1, 1, 1, 1, 1,		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1,		1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 0, 1, 1, 1, 0,
		1, 0, 1, 0, 1, 0, 1, 0,		1, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 1, 1, 1, 1, 0,
		1, 0, 1, 0, 1, 0, 1, 0,		1, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 1, 1, 1, 1, 0,
		1, 0, 1, 0, 1, 0, 1, 0,		1, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 1, 1, 1, 1, 0,
		1, 0, 1, 0, 1, 0, 1, 0,		1, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 1, 1, 1, 1, 0,
		1, 0, 1, 0, 1, 0, 1, 0,		1, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 1, 1, 1, 1, 0,
		1, 0, 1, 0, 1, 0, 1, 0,		1, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 1, 1, 1, 1, 0,
		1, 0, 1, 0, 1, 0, 1, 0,		1, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 1, 1, 1, 1, 0,
		1, 0, 1, 0, 1, 0, 1, 0,		1, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 1, 1, 1, 1, 0,
		1, 0, 1, 0, 1, 0, 1, 0,		1, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 1, 1, 1, 1, 0,
		1, 0, 1, 0, 1, 0, 1, 0,		1, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 1, 0, 1, 1, 1, 0,		1, 1, 1, 1, 1, 1, 1, 0,
		1, 1, 1, 1, 1, 1, 1, 1,		2, 1, 1, 1, 2, 1, 1, 1,
		2, 1, 2, 1, 2, 1, 2, 1,		2, 2, 2, 1, 2, 2, 2, 1,
		2, 2, 2, 2, 2, 2, 2, 2,		4, 2, 2, 2, 4, 2, 2, 2,	
		4, 2, 4, 2, 4, 2, 4, 2,		4, 4, 4, 2, 4, 4, 4, 2,
		4, 4, 4, 4, 4, 4, 4, 4,		8, 4, 4, 4, 8, 4, 4, 4,	
		8, 4, 8, 4, 8, 4, 8, 4,		8, 8, 8, 4, 8, 8, 8, 4,
		16,16,16,16,16,16,16,16,	16,16,16,16,16,16,16,16,
		16,16,16,16,16,16,16,16,	16,16,16,16,16,16,16,16,
	};

	const int Operator::decaytable2[16] = 
	{
		1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2047, 2047, 2047, 2047, 2047
	};

	const int8_t Operator::attacktable[64][8] = 
	{
		-1,-1,-1,-1,-1,-1,-1,-1,	-1,-1,-1,-1,-1,-1,-1,-1,
		 4, 4, 4, 4, 4, 4, 4, 4,	 4, 4, 4, 4, 4, 4, 4, 4,
		 4, 4, 4, 4, 4, 4, 4, 4,	 4, 4, 4, 4, 4, 4, 4, 4,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4,-1, 4, 4, 4,-1,
		 4,-1, 4,-1, 4,-1, 4,-1,	 4, 4, 4,-1, 4,-1, 4,-1,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4, 4, 4, 4, 4,-1,
		 4,-1, 4,-1, 4,-1, 4,-1,	 4, 4, 4,-1, 4,-1, 4,-1,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4, 4, 4, 4, 4,-1,
		 4,-1, 4,-1, 4,-1, 4,-1,	 4, 4, 4,-1, 4,-1, 4,-1,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4, 4, 4, 4, 4,-1,
		 4,-1, 4,-1, 4,-1, 4,-1,	 4, 4, 4,-1, 4,-1, 4,-1,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4, 4, 4, 4, 4,-1,
		 4,-1, 4,-1, 4,-1, 4,-1,	 4, 4, 4,-1, 4,-1, 4,-1,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4, 4, 4, 4, 4,-1,
		 4,-1, 4,-1, 4,-1, 4,-1,	 4, 4, 4,-1, 4,-1, 4,-1,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4, 4, 4, 4, 4,-1,
		 4,-1, 4,-1, 4,-1, 4,-1,	 4, 4, 4,-1, 4,-1, 4,-1,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4, 4, 4, 4, 4,-1,
		 4,-1, 4,-1, 4,-1, 4,-1,	 4, 4, 4,-1, 4,-1, 4,-1,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4, 4, 4, 4, 4,-1,
		 4,-1, 4,-1, 4,-1, 4,-1,	 4, 4, 4,-1, 4,-1, 4,-1,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4, 4, 4, 4, 4,-1,
		 4,-1, 4,-1, 4,-1, 4,-1,	 4, 4, 4,-1, 4,-1, 4,-1,
		 4, 4, 4,-1, 4, 4, 4,-1,	 4, 4, 4, 4, 4, 4, 4,-1,
		 4, 4, 4, 4, 4, 4, 4, 4,	 3, 4, 4, 4, 3, 4, 4, 4,
		 3, 4, 3, 4, 3, 4, 3, 4,	 3, 3, 3, 4, 3, 3, 3, 4,
		 3, 3, 3, 3, 3, 3, 3, 3,	 2, 3, 3, 3, 2, 3, 3, 3,
		 2, 3, 2, 3, 2, 3, 2, 3,	 2, 2, 2, 3, 2, 2, 2, 3,
		 2, 2, 2, 2, 2, 2, 2, 2,	 1, 2, 2, 2, 1, 2, 2, 2,
		 1, 2, 1, 2, 1, 2, 1, 2,	 1, 1, 1, 2, 1, 1, 1, 2,
		 0, 0, 0, 0, 0, 0, 0, 0,	 0, 0 ,0, 0, 0, 0, 0, 0,
		 0, 0, 0, 0, 0, 0, 0, 0,	 0, 0 ,0, 0, 0, 0, 0, 0,
	};

	const int Operator::ssgenvtable[8][2][3][2] =
	{
		1, 1,  1, 1,  1, 1,		// 08 
		0, 1,  1, 1,  1, 1,		// 08 56~
		0, 1,  2, 0,  2, 0,		// 09
		0, 1,  2, 0,  2, 0,		// 09
		1,-1,  0, 1,  1,-1,		// 10
		0, 1,  1,-1,  0, 1,		// 10 60~
		1,-1,  0, 0,  0, 0,		// 11
		0, 1,  0, 0,  0, 0,		// 11 60~
		2,-1,  2,-1,  2,-1,		// 12
		1,-1,  2,-1,  2,-1,		// 12 56~
		1,-1,  0, 0,  0, 0,		// 13
		1,-1,  0, 0,  0, 0,		// 13
		0, 1,  1,-1,  0, 1,		// 14
		1,-1,  0, 1,  1,-1,		// 14 60~
		0, 1,  2, 0,  2, 0,		// 15
		1,-1,  2, 0,  2, 0,		// 15 60~
	};

	// fixed equasion-based tables
	int		pmtable[2][8][FM_LFOENTS];
	uint32_t	amtable[2][4][FM_LFOENTS];

	static bool tablemade = false;
}

namespace FM
{

// ---------------------------------------------------------------------------
//	テーブル作成
//
void MakeLFOTable()
{
	if (tablemade)
		return;

	tablemade = true;
	
	int i;

	static const double pms[2][8] = 
	{ 
		{ 0, 1/360., 2/360., 3/360.,  4/360.,  6/360., 12/360.,  24/360., },	// OPNA
//		{ 0, 1/240., 2/240., 4/240., 10/240., 20/240., 80/240., 140/240., },	// OPM
		{ 0, 1/480., 2/480., 4/480., 10/480., 20/480., 80/480., 140/480., },	// OPM
//		{ 0, 1/960., 2/960., 4/960., 10/960., 20/960., 80/960., 140/960., },	// OPM
	};
	//		 3		 6,      12      30       60       240      420		/ 720
	//	1.000963
	//	lfofref[level * max * wave];
	//	pre = lfofref[level][pms * wave >> 8];
	static const uint8_t amt[2][4] = 
	{
		{ 31, 6, 4, 3 }, // OPNA
		{ 31, 2, 1, 0 }, //	OPM
	};
	
	for (int type = 0; type < 2; type++)
	{
		for (i=0; i<8; i++)
		{
			double pmb = pms[type][i];
			for (int j=0; j<FM_LFOENTS; j++)
			{
				double w = 0.6 * pmb * sin(2 * j * 3.14159265358979323846 / FM_LFOENTS) + 1;
				pmtable[type][i][j] = int(0x10000 * (w - 1));
			}
		}
		for (i=0; i<4; i++)
		{
			for (int j=0; j<FM_LFOENTS; j++)
				amtable[type][i][j] = (((j * 4) >> amt[type][i]) * 2) << 2;
		}
	}
}

//
//
//
int Chip::StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT ChipStateRegs[] =
	{
		SFVAR(ratio_),
		SFVAR(aml_),
		SFVAR(pml_),
		SFVAR(pmv_),
		SFVAR(optype_),
		SFARRAY32(multable_, 4 * 16),

		SFEND
	};

	return PX68KSS_StateAction(sm, load, data_only, ChipStateRegs, "CHIP", false);
}


// ---------------------------------------------------------------------------
//	チップ内で共通な部分
//
Chip::Chip()
: ratio_(0), aml_(0), pml_(0), pmv_(0), optype_(TYPE_N)
{
}

//	クロック・サンプリングレート比に依存するテーブルを作成
void Chip::SetRatio(uint32_t ratio)
{
	if (ratio_ != ratio)
	{
		ratio_ = ratio;
		MakeTable();
	}
}

void Chip::MakeTable()
{
	int h, l;
	
	// PG Part
	static const float dt2lv[4] = { 1.f, 1.414f, 1.581f, 1.732f };
	for (h=0; h<4; h++)
	{
		double rr = dt2lv[h] * double(ratio_) / (1 << (2 + FM_RATIOBITS - FM_PGBITS));
		for (l=0; l<16; l++)
		{
			int mul = l ? l * 2 : 1;
			multable_[h][l] = uint32_t(mul * rr);
		}
	}
}


// ---------------------------------------------------------------------------
//	Operator
//
bool FM::Operator::tablehasmade = false;
uint32_t FM::Operator::sinetable[1024];
int32_t FM::Operator::cltable[FM_CLENTS];

int Operator::StateAction(StateMem *sm, int load, int data_only, const char *sname)
{
	SFORMAT StateRegs[] =
	{
		SFVAR(out_),
		SFVAR(out2_),
		SFVAR(dp_),
		SFVAR(detune_),
		SFVAR(detune2_),
		SFVAR(multiple_),
		SFVAR(pg_count_),
		SFVAR(pg_diff_),
		SFVAR(pg_diff_lfo_),
		SFVAR(type_),
		SFVAR(bn_),
		SFVAR(eg_level_),
		SFVAR(eg_level_on_next_phase_),
		SFVAR(eg_count_),
		SFVAR(eg_count_diff_),
		SFVAR(eg_out_),
		SFVAR(tl_out_),
		SFVAR(eg_rate_),
		SFVAR(eg_curve_count_),
		SFVAR(ssg_offset_),
		SFVAR(ssg_vector_),
		SFVAR(ssg_phase_),
		SFVAR(key_scale_rate_),
		SFVAR(eg_phase_),
		SFVAR(ms_),
		SFVAR(tl_),
		SFVAR(tl_latch_),
		SFVAR(ar_),
		SFVAR(dr_),
		SFVAR(sr_),
		SFVAR(sl_),
		SFVAR(rr_),
		SFVAR(ks_),
		SFVAR(ssg_type_),
		SFVAR(keyon_),
		SFVAR(amon_),
		SFVAR(param_changed_),
		SFVAR(mute_),

		SFEND
	};

	int ret =  PX68KSS_StateAction(sm, load, data_only, StateRegs, sname, false);
	
	if (load)
		ams_ = amtable[type_][amon_ ? (ms_ >> 4) & 3 : 0];
	
	return ret;
}

//	構築
FM::Operator::Operator()
: chip_(0)
{
	if (!tablehasmade)
		MakeTable();

	// EG Part
	ar_ = dr_ = sr_ = rr_ = key_scale_rate_ = 0;
	ams_ = amtable[0][0];
	mute_ = false;
	keyon_ = false;
	tl_out_ = false;
	ssg_type_ = 0;

	// PG Part
	multiple_ = 0;
	detune_ = 0;
	detune2_ = 0;

	// LFO
	ms_ = 0;
	
//	Reset();
}

//	初期化
void FM::Operator::Reset()
{
	// EG part
	tl_ = tl_latch_ = 127;
	ShiftPhase(OFF);
	eg_count_ = 0;
	eg_curve_count_ = 0;
	ssg_phase_ = 0;

	// PG part
	pg_count_ = 0;

	// OP part
	out_ = out2_ = 0;

	param_changed_ = true;
}

void Operator::MakeTable()
{
	// 対数テーブルの作成
	int* p = cltable;
	int i;
	for (i=0; i<256; i++)
	{
		int v = int(floor(pow(2., 13. - i / 256.)));
		v = (v + 2) & ~3;
		*p++ = v;
		*p++ = -v;
	}
	while (p < cltable + FM_CLENTS)
	{
		*p = p[-512] / 2;
		p++;
	}

	// サインテーブルの作成
	double log2 = log(2.);
	for (i=0; i<FM_OPSINENTS/2; i++)
	{
		double r = (i * 2 + 1) * FM_PI / FM_OPSINENTS;
		double q = -256 * log(sin(r)) / log2;
		uint32_t s = (int) (floor(q + 0.5)) + 1;
		sinetable[i]                  = s * 2 ;
		sinetable[FM_OPSINENTS / 2 + i] = s * 2 + 1;
	}

	::FM::MakeLFOTable();

	tablehasmade = true;
}



inline void FM::Operator::SetDPBN(uint32_t dp, uint32_t bn)
{
	dp_ = dp, bn_ = bn; param_changed_ = true; 
}


//	準備
void Operator::Prepare()
{
   int min_y;
   if (param_changed_)
   {
      param_changed_ = false;
      //	PG Part
      pg_diff_ = (dp_ + dttable[detune_ + bn_]) * chip_->GetMulValue(detune2_, multiple_);
      pg_diff_lfo_ = pg_diff_ >> 11;

      // EG Part
      key_scale_rate_ = bn_ >> (3-ks_);
      tl_out_ = mute_ ? 0x3ff : tl_ * 8;

      switch (eg_phase_)
      {
         case ATTACK:
            min_y = ar_ + key_scale_rate_;
            SetEGRate(ar_ ? FMGEN_MIN(63, min_y) : 0);
            break;
         case DECAY:
            min_y = dr_ + key_scale_rate_;
            SetEGRate(dr_ ? FMGEN_MIN(63, min_y) : 0);
            eg_level_on_next_phase_ = sl_ * 8;
            break;
         case SUSTAIN:
            min_y = sr_ + key_scale_rate_;
            SetEGRate(sr_ ? FMGEN_MIN(63, min_y) : 0);
            break;
         case RELEASE:
            min_y = rr_ + key_scale_rate_;
            SetEGRate(FMGEN_MIN(63, min_y));
            break;
         case NEXT:
         case OFF:
            break;
      }

      // SSG-EG
      if (ssg_type_ && (eg_phase_ != RELEASE))
      {
         int m = ar_ >= ((ssg_type_ == 8 || ssg_type_ == 12) ? 56 : 60);
         const int* table = ssgenvtable[ssg_type_ & 7][m][ssg_phase_];

         ssg_offset_ = table[0] * 0x200;
         ssg_vector_ = table[1];
      }
      // LFO
      ams_ = amtable[type_][amon_ ? (ms_ >> 4) & 3 : 0];
      EGUpdate();
   }
}
//	envelop の eg_phase_ 変更
void Operator::ShiftPhase(EGPhase nextphase)
{
	switch (nextphase)
   {
      case ATTACK:		// Attack Phase
         tl_ = tl_latch_;
         if (ssg_type_)
         {
            ssg_phase_ = ssg_phase_ + 1;
            if (ssg_phase_ > 2)
               ssg_phase_ = 1;

            int m = ar_ >= ((ssg_type_ == 8 || ssg_type_ == 12) ? 56 : 60);
            const int* table = ssgenvtable[ssg_type_ & 7][m][ssg_phase_];

            ssg_offset_ = table[0] * 0x200;
            ssg_vector_ = table[1];
         }
         if ((ar_ + key_scale_rate_) < 62)
         {
            int min_y = ar_ + key_scale_rate_;
            SetEGRate(ar_ ? FMGEN_MIN(63, min_y) : 0);
            eg_phase_ = ATTACK;
            break;
         }
      case DECAY:			// Decay Phase
         if (sl_)
         {
            int min_x = sl_ * 8;
            int min_y = dr_ + key_scale_rate_;
            eg_level_ = 0;
            eg_level_on_next_phase_ = ssg_type_ ? FMGEN_MIN(min_x, 0x200) : sl_ * 8;

            SetEGRate(dr_ ? FMGEN_MIN(63, min_y) : 0);
            eg_phase_ = DECAY;
            break;
         }
      case SUSTAIN:		// Sustain Phase
         {
            int min_y = sr_ + key_scale_rate_;
            eg_level_ = sl_ * 8;
            eg_level_on_next_phase_ = ssg_type_ ? 0x200 : 0x400;

            SetEGRate(sr_ ? FMGEN_MIN(63, min_y) : 0);
            eg_phase_ = SUSTAIN;
         }
         break;

      case RELEASE:		// Release Phase
         if (ssg_type_)
         {
            eg_level_ = eg_level_ * ssg_vector_ + ssg_offset_;
            ssg_vector_ = 1;
            ssg_offset_ = 0;
         }
         if (eg_phase_ == ATTACK || (eg_level_ < FM_EG_BOTTOM)) //0x400/* && eg_phase_ != off*/))
         {
            int min_y = rr_ + key_scale_rate_;
            eg_level_on_next_phase_ = 0x400;
            SetEGRate(FMGEN_MIN(63, min_y));
            eg_phase_ = RELEASE;
            break;
         }
      case OFF:			// off
      default:
         eg_level_ = FM_EG_BOTTOM;
         eg_level_on_next_phase_ = FM_EG_BOTTOM;
         EGUpdate();
         SetEGRate(0);
         eg_phase_ = OFF;
         break;
   }
}

//	Block/F-Num
void Operator::SetFNum(uint32_t f)
{
	dp_ = (f & 2047) << ((f >> 11) & 7);
	bn_ = notetable[(f >> 7) & 127];
	param_changed_ = true;
}

//	１サンプル合成

//	ISample を envelop count (2π) に変換するシフト量
#define IS2EC_SHIFT		((20 + FM_PGBITS) - 13)


// 入力: s = 20+FM_PGBITS = 29
#define Sine(s)	sinetable[((s) >> (20+FM_PGBITS-FM_OPSINBITS))&(FM_OPSINENTS-1)]
#define SINE(s) sinetable[(s) & (FM_OPSINENTS-1)]

inline FM::ISample Operator::LogToLin(uint32_t a)
{
	return (a < FM_CLENTS) ? cltable[a] : 0;
}

inline void Operator::EGUpdate()
{
	if (!ssg_type_)
   {
      int min_x = tl_out_ + eg_level_;
		eg_out_ = FMGEN_MIN(min_x, 0x3ff) << (1 + 2);
   }
	else
   {
      int min_x = tl_out_ + eg_level_ * ssg_vector_ + ssg_offset_;
		eg_out_   = FMGEN_MIN(min_x, 0x3ff) << (1 + 2);
   }
}

inline void Operator::SetEGRate(uint32_t rate)
{
	eg_rate_ = rate;
	eg_count_diff_ = decaytable2[rate / 4] * chip_->GetRatio();
}

//	EG 計算
void FM::Operator::EGCalc()
{
   eg_count_ = (2047 * 3) << FM_RATIOBITS;				// ##この手抜きは再現性を低下させる

   if (eg_phase_ == ATTACK)
   {
      int c = attacktable[eg_rate_][eg_curve_count_ & 7];
      if (c >= 0)
      {
         eg_level_ -= 1 + (eg_level_ >> c);
         if (eg_level_ <= 0)
            ShiftPhase(DECAY);
      }
      EGUpdate();
   }
   else
   {
      if (!ssg_type_)
      {
         eg_level_ += decaytable1[eg_rate_][eg_curve_count_ & 7];
         if (eg_level_ >= eg_level_on_next_phase_)
            ShiftPhase(EGPhase(eg_phase_+1));
         EGUpdate();
      }
      else
      {
         eg_level_ += 4 * decaytable1[eg_rate_][eg_curve_count_ & 7];
         if (eg_level_ >= eg_level_on_next_phase_)
         {
            EGUpdate();
            switch (eg_phase_)
            {
               case DECAY:
                  ShiftPhase(SUSTAIN);
                  break;
               case SUSTAIN:
                  ShiftPhase(ATTACK);
                  break;
               case RELEASE:
                  ShiftPhase(OFF);
                  break;
               case NEXT:
               case ATTACK:
               case OFF:
                  break;
            }
         }
      }
   }
   eg_curve_count_++;
}

inline void FM::Operator::EGStep()
{
	eg_count_ -= eg_count_diff_;

	// EG の変化は全スロットで同期しているという噂もある
	if (eg_count_ <= 0)
		EGCalc();
}

//	PG 計算
//	ret:2^(20+PGBITS) / cycle
inline uint32_t FM::Operator::PGCalc()
{
	uint32_t ret = pg_count_;
	pg_count_ += pg_diff_;
	return ret;
}

inline uint32_t FM::Operator::PGCalcL()
{
	uint32_t ret = pg_count_;
	pg_count_ += pg_diff_ + ((pg_diff_lfo_ * chip_->GetPMV()) >> 5);// & -(1 << (2+IS2EC_SHIFT)));
	return ret /* + pmv * pg_diff_;*/;
}

//	OP 計算
//	in: ISample (最大 8π)
inline FM::ISample FM::Operator::Calc(ISample in)
{
	EGStep();
	out2_ = out_;

	int pgin = PGCalc() >> (20+FM_PGBITS-FM_OPSINBITS);
	pgin += in >> (20+FM_PGBITS-FM_OPSINBITS-(2+IS2EC_SHIFT));
	out_ = LogToLin(eg_out_ + SINE(pgin));
	return out_;
}

inline FM::ISample FM::Operator::CalcL(ISample in)
{
	EGStep();

	int pgin = PGCalcL() >> (20+FM_PGBITS-FM_OPSINBITS);
	pgin += in >> (20+FM_PGBITS-FM_OPSINBITS-(2+IS2EC_SHIFT));
	out_ = LogToLin(eg_out_ + SINE(pgin) + ams_[chip_->GetAML()]);
	return out_;
}

inline FM::ISample FM::Operator::CalcN(uint32_t noise)
{
	EGStep();
	int y  = 0x3ff - (tl_out_ + eg_level_);
	int lv = FMGEN_MAX(0, y) << 1;
	// noise & 1 ? lv : -lv と等価 
	noise  = (noise & 1) - 1;
	out_   = (lv + noise) ^ noise;
	return out_;
}

//	OP (FB) 計算
//	Self Feedback の変調最大 = 4π
inline FM::ISample FM::Operator::CalcFB(uint32_t fb)
{
	EGStep();

	ISample in = out_ + out2_;
	out2_ = out_;

	int pgin = PGCalc() >> (20+FM_PGBITS-FM_OPSINBITS);
	if (fb < 31)
	{
		pgin += ((in << (1 + IS2EC_SHIFT)) >> fb) >> (20+FM_PGBITS-FM_OPSINBITS);
	}
	out_ = LogToLin(eg_out_ + SINE(pgin));
	return out2_;
}

inline FM::ISample FM::Operator::CalcFBL(uint32_t fb)
{
	EGStep();
	
	ISample in = out_ + out2_;
	out2_ = out_;

	int pgin = PGCalcL() >> (20+FM_PGBITS-FM_OPSINBITS);
	if (fb < 31)
	{
		pgin += ((in << (1 + IS2EC_SHIFT)) >> fb) >> (20+FM_PGBITS-FM_OPSINBITS);
	}

	out_ = LogToLin(eg_out_ + SINE(pgin) + ams_[chip_->GetAML()]);
	return out_;
}

#undef Sine

// ---------------------------------------------------------------------------
//	4-op Channel
//
const uint8_t Channel4::fbtable[8] = { 31, 7, 6, 5, 4, 3, 2, 1 };
int Channel4::kftable[64];

bool Channel4::tablehasmade = false;

Channel4::Channel4()
{
	if (!tablehasmade)
		MakeTable();

	SetAlgorithm(0);
	pms = pmtable[0][0];
}

int Channel4::StateAction(StateMem *sm, int load, int data_only, const char *sname)
{
	SFORMAT StateRegs[] =
	{
		SFVAR(fb),
		SFVAR(algo_),
		SFARRAY32(buf, 4),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, sname, false);

	for (int i = 0; i < 4; i++)
	{
		char tmpstr[10] = "x";
		char tmpstr2[5] = "OPx";
		tmpstr2[2] = '0' + i;
		strcpy(tmpstr, sname);
		strcat(tmpstr, tmpstr2);

		ret &= op[i].StateAction(sm, load, data_only, tmpstr);
	}

	if (load)
	{
		SetAlgorithm(algo_);
		pms = pmtable[op[0].type_][op[0].ms_ & 7];
	}

	return ret;
}

void Channel4::MakeTable()
{
	// 100/64 cent =  2^(i*100/64*1200)
	for (int i=0; i<64; i++)
	{
		kftable[i] = int(0x10000 * pow(2., i / 768.) );
	}
}

// リセット
void Channel4::Reset()
{
	op[0].Reset();
	op[1].Reset();
	op[2].Reset();
	op[3].Reset();
}

//	Calc の用意
int Channel4::Prepare()
{
	op[0].Prepare();
	op[1].Prepare();
	op[2].Prepare();
	op[3].Prepare();
	
	pms = pmtable[op[0].type_][op[0].ms_ & 7];
	int key = (op[0].IsOn() | op[1].IsOn() | op[2].IsOn() | op[3].IsOn()) ? 1 : 0;
	int lfo = op[0].ms_ & (op[0].amon_ | op[1].amon_ | op[2].amon_ | op[3].amon_ ? 0x37 : 7) ? 2 : 0;
	return key | lfo;
}

//	F-Number/BLOCK を設定
void Channel4::SetFNum(uint32_t f)
{
	for (int i=0; i<4; i++)
		op[i].SetFNum(f);
}

//	KC/KF を設定
void Channel4::SetKCKF(uint32_t kc, uint32_t kf)
{
	static const uint32_t kctable[16] = 
	{ 
		5197, 5506, 5833, 6180, 6180, 6547, 6937, 7349, 
		7349, 7786, 8249, 8740, 8740, 9259, 9810, 10394, 
	};

	int oct = 19 - ((kc >> 4) & 7);

	uint32_t kcv = kctable[kc & 0x0f];
	kcv = (kcv + 2) / 4 * 4;
	uint32_t dp = kcv * kftable[kf & 0x3f];
	dp >>= 16 + 3;
	dp <<= 16 + 3;
	dp >>= oct;	
	uint32_t bn = (kc >> 2) & 31;
	op[0].SetDPBN(dp, bn);
	op[1].SetDPBN(dp, bn);
	op[2].SetDPBN(dp, bn);
	op[3].SetDPBN(dp, bn);
}

//	キー制御
void Channel4::KeyControl(uint32_t key)
{
	if (key & 0x1) op[0].KeyOn(); else op[0].KeyOff();
	if (key & 0x2) op[1].KeyOn(); else op[1].KeyOff();
	if (key & 0x4) op[2].KeyOn(); else op[2].KeyOff();
	if (key & 0x8) op[3].KeyOn(); else op[3].KeyOff();
}

//	アルゴリズムを設定
void Channel4::SetAlgorithm(uint32_t algo)
{
	static const uint8_t table1[8][6] = 
	{
		{ 0, 1, 1, 2, 2, 3 },	{ 1, 0, 0, 1, 1, 2 },
		{ 1, 1, 1, 0, 0, 2 },	{ 0, 1, 2, 1, 1, 2 },
		{ 0, 1, 2, 2, 2, 1 },	{ 0, 1, 0, 1, 0, 1 },
		{ 0, 1, 2, 1, 2, 1 },	{ 1, 0, 1, 0, 1, 0 },
	};

	in [0] = &buf[table1[algo][0]];
	out[0] = &buf[table1[algo][1]];
	in [1] = &buf[table1[algo][2]];
	out[1] = &buf[table1[algo][3]];
	in [2] = &buf[table1[algo][4]];
	out[2] = &buf[table1[algo][5]];

	op[0].out_  = 0;
	op[0].out2_ = 0;
	algo_       = algo;
}

//  合成
ISample Channel4::Calc()
{
	int r = 0;
	switch (algo_)
	{
	case 0:
		op[2].Calc(op[1].Out());
		op[1].Calc(op[0].Out());
		r = op[3].Calc(op[2].Out());
		op[0].CalcFB(fb);
		break;
	case 1:
		op[2].Calc(op[0].Out() + op[1].Out());
		op[1].Calc(0);
		r = op[3].Calc(op[2].Out());
		op[0].CalcFB(fb);
		break;
	case 2:
		op[2].Calc(op[1].Out());
		op[1].Calc(0);
		r = op[3].Calc(op[0].Out() + op[2].Out());
		op[0].CalcFB(fb);
		break;
	case 3:
		op[2].Calc(0);
		op[1].Calc(op[0].Out());
		r = op[3].Calc(op[1].Out() + op[2].Out());
		op[0].CalcFB(fb);
		break;
	case 4:
		op[2].Calc(0);
		r = op[1].Calc(op[0].Out());
		r += op[3].Calc(op[2].Out());
		op[0].CalcFB(fb);
		break;
	case 5:
		r =  op[2].Calc(op[0].Out());
		r += op[1].Calc(op[0].Out());
		r += op[3].Calc(op[0].Out());
		op[0].CalcFB(fb);
		break;
	case 6:
		r  = op[2].Calc(0);
		r += op[1].Calc(op[0].Out());
		r += op[3].Calc(0);
		op[0].CalcFB(fb);
		break;
	case 7:
		r  = op[2].Calc(0);
		r += op[1].Calc(0);
		r += op[3].Calc(0);
		r += op[0].CalcFB(fb);
		break;
	}
	return r;
}

//  合成
ISample Channel4::CalcL()
{
	chip_->SetPMV(pms[chip_->GetPML()]);

	int r = 0;
	switch (algo_)
	{
	case 0:
		op[2].CalcL(op[1].Out());
		op[1].CalcL(op[0].Out());
		r = op[3].CalcL(op[2].Out());
		op[0].CalcFBL(fb);
		break;
	case 1:
		op[2].CalcL(op[0].Out() + op[1].Out());
		op[1].CalcL(0);
		r = op[3].CalcL(op[2].Out());
		op[0].CalcFBL(fb);
		break;
	case 2:
		op[2].CalcL(op[1].Out());
		op[1].CalcL(0);
		r = op[3].CalcL(op[0].Out() + op[2].Out());
		op[0].CalcFBL(fb);
		break;
	case 3:
		op[2].CalcL(0);
		op[1].CalcL(op[0].Out());
		r = op[3].CalcL(op[1].Out() + op[2].Out());
		op[0].CalcFBL(fb);
		break;
	case 4:
		op[2].CalcL(0);
		r = op[1].CalcL(op[0].Out());
		r += op[3].CalcL(op[2].Out());
		op[0].CalcFBL(fb);
		break;
	case 5:
		r =  op[2].CalcL(op[0].Out());
		r += op[1].CalcL(op[0].Out());
		r += op[3].CalcL(op[0].Out());
		op[0].CalcFBL(fb);
		break;
	case 6:
		r  = op[2].CalcL(0);
		r += op[1].CalcL(op[0].Out());
		r += op[3].CalcL(0);
		op[0].CalcFBL(fb);
		break;
	case 7:
		r  = op[2].CalcL(0);
		r += op[1].CalcL(0);
		r += op[3].CalcL(0);
		r += op[0].CalcFBL(fb);
		break;
	}
	return r;
}

//  合成
ISample Channel4::CalcN(uint32_t noise)
{
	buf[1] = buf[2] = buf[3] = 0;

	buf[0] = op[0].out_; op[0].CalcFB(fb);
	*out[0] += op[1].Calc(*in[0]);
	*out[1] += op[2].Calc(*in[1]);
	int o = op[3].out_;
	op[3].CalcN(noise);
	return *out[2] + o;
}

//  合成
ISample Channel4::CalcLN(uint32_t noise)
{
	chip_->SetPMV(pms[chip_->GetPML()]);
	buf[1] = buf[2] = buf[3] = 0;

	buf[0] = op[0].out_; op[0].CalcFBL(fb); 
	*out[0] += op[1].CalcL(*in[0]);
	*out[1] += op[2].CalcL(*in[1]);
	int o = op[3].out_;
	op[3].CalcN(noise);
	return *out[2] + o;
}

}	// namespace FM
