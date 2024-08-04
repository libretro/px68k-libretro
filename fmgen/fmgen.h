// ---------------------------------------------------------------------------
//	FM Sound Generator
//	Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------

#ifndef FM_GEN_H
#define FM_GEN_H

// ---------------------------------------------------------------------------
//	���ϥ���ץ�η�
//
#define FM_SAMPLETYPE	int16_t				// int16_t or int32_t

// ---------------------------------------------------------------------------
//	������Σ�
//	��Ū�ơ��֥�Υ�����

#define FM_LFOBITS		8					// �ѹ��Բ�
#define FM_TLBITS		7

// ---------------------------------------------------------------------------

#define FM_TLENTS		(1 << FM_TLBITS)
#define FM_LFOENTS		(1 << FM_LFOBITS)
#define FM_TLPOS		(FM_TLENTS/4)

//	�������Ȥ����٤� 2^(1/256)
#define FM_CLENTS		(0x1000 * 2)	// sin + TL + LFO

// ---------------------------------------------------------------------------

namespace FM
{
	//	Types ----------------------------------------------------------------
	typedef FM_SAMPLETYPE	Sample;
	typedef int32_t 			ISample;

	enum OpType		{ typeN=0, typeM=1 };
	enum EGPhase	{ next, attack, decay, sustain, release, off };

	void StoreSample(ISample& dest, int data);

	class Chip;

	//	Operator -------------------------------------------------------------
	typedef struct
	{
		ISample		out_;
		ISample		out2_;
		ISample 	in2_;
		uint32_t	dp_;
		uint32_t	detune_;
		uint32_t	detune2_;
		uint32_t	multiple_;
		uint32_t	pg_count_;
		uint32_t	pg_diff_;
		int32_t		pg_diff_lfo_;
		OpType		type_;
		uint32_t	bn_;
		int			eg_level_;
		int			eg_level_on_next_phase_;
		int			eg_count_;
		int			eg_count_diff_;
		int			eg_out_;
		int			tl_out_;
		int			eg_rate_;
		int			eg_curve_count_;
		int			ssg_offset_;
		int			ssg_vector_;
		int			ssg_phase_;
		uint32_t	key_scale_rate_;
		EGPhase		eg_phase_;
		uint32_t	ms_;
		
		uint32_t	tl_;
		uint32_t	tl_latch_;
		uint32_t	ar_;
		uint32_t	dr_;
		uint32_t	sr_;
		uint32_t	sl_;
		uint32_t	rr_;
		uint32_t	ks_;
		uint32_t	ssg_type_;

		bool		keyon_;
		bool		amon_;
		bool		param_changed_;
		bool		mute_;
	} operator_savestate_t;

	class Operator
	{
	public:
		Operator();
		void		SetChip(Chip* chip) { chip_ = chip; }

		static void	MakeTimeTable(uint32_t ratio);

		ISample		Calc(ISample in);
		ISample		CalcL(ISample in);
		ISample 	CalcFB(uint32_t fb);
		ISample 	CalcFBL(uint32_t fb);
		ISample 	CalcN(uint32_t noise);
		void		Prepare();
		void		KeyOn();
		void		KeyOff();
		void		Reset();
		void		ResetFB();
		int			IsOn();

		void		SetDT(uint32_t dt);
		void		SetDT2(uint32_t dt2);
		void		SetMULTI(uint32_t multi);
		void		SetTL(uint32_t tl, bool csm);
		void		SetKS(uint32_t ks);
		void		SetAR(uint32_t ar);
		void		SetDR(uint32_t dr);
		void		SetSR(uint32_t sr);
		void		SetRR(uint32_t rr);
		void		SetSL(uint32_t sl);
		void		SetSSGEC(uint32_t ssgec);
		void		SetFNum(uint32_t fnum);
		void		SetDPBN(uint32_t dp, uint32_t bn);
		void		SetMode(bool modulator);
		void		SetAMON(bool on);
		void		SetMS(uint32_t ms);
		void		Mute(bool);

//		static void SetAML(uint32_t l);
//		static void SetPML(uint32_t l);

		int			Out() { return out_; }

		int			dbgGetIn2() { return in2_; }
		void		dbgStopPG() { pg_diff_ = 0; pg_diff_lfo_ = 0; }

		void		Save(operator_savestate_t *state);
		void		Load(operator_savestate_t *state);

	private:
		typedef uint32_t Counter;

		Chip*		chip_;
		ISample		out_, out2_;
		ISample 	in2_;

	//	Phase Generator ------------------------------------------------------
		uint32_t	PGCalc();
		uint32_t	PGCalcL();

		uint32_t	dp_;		// ��P
		uint32_t	detune_;		// Detune
		uint32_t	detune2_;	// DT2
		uint32_t	multiple_;	// Multiple
		uint32_t	pg_count_;	// Phase ������
		uint32_t	pg_diff_;	// Phase ��ʬ��
		int32_t		pg_diff_lfo_;	// Phase ��ʬ�� >> x

	//	Envelop Generator ---------------------------------------------------
		void		EGCalc();
		void		EGStep();
		void		ShiftPhase(EGPhase nextphase);
		void		SSGShiftPhase(int32_t mode);
		void		SetEGRate(uint32_t);
		void		EGUpdate();
		int			FBCalc(int32_t fb);
		ISample		LogToLin(uint32_t a);


		OpType		type_;		// OP �μ��� (M, N...)
		uint32_t	bn_;		// Block/Note
		int32_t		eg_level_;	// EG �ν�����
		int32_t		eg_level_on_next_phase_;	// ���� eg_phase_ �˰ܤ���
		int32_t		eg_count_;		// EG �μ����ѰܤޤǤλ���
		int32_t		eg_count_diff_;	// eg_count_ �κ�ʬ
		int32_t		eg_out_;		// EG+TL ���碌��������
		int32_t		tl_out_;		// TL ʬ�ν�����
//		int32_t		pm_depth_;		// PM depth
//		int32_t		am_depth_;		// AM depth
		int32_t		eg_rate_;
		int32_t		eg_curve_count_;
		int32_t		ssg_offset_;
		int32_t		ssg_vector_;
		int32_t		ssg_phase_;


		uint32_t	key_scale_rate_;		// key scale rate
		EGPhase		eg_phase_;
		uint32_t*	ams_;
		uint32_t	ms_;

		uint32_t	tl_;			// Total Level	 (0-127)
		uint32_t	tl_latch_;		// Total Level Latch (for CSM mode)
		uint32_t	ar_;			// Attack Rate   (0-63)
		uint32_t	dr_;			// Decay Rate    (0-63)
		uint32_t	sr_;			// Sustain Rate  (0-63)
		uint32_t	sl_;			// Sustain Level (0-127)
		uint32_t	rr_;			// Release Rate  (0-63)
		uint32_t	ks_;			// Keyscale      (0-3)
		uint32_t	ssg_type_;	// SSG-Type Envelop Control

		bool		keyon_;
		bool		amon_;		// enable Amplitude Modulation
		bool		param_changed_;	// �ѥ�᡼�����������줿
		bool		mute_;

	//	Tables ---------------------------------------------------------------
		static Counter			rate_table[16];
		static uint32_t			multable[4][16];

		static const uint8_t	notetable[128];
		static const int8_t		dttable[256];
		static const int8_t		decaytable1[64][8];
		static const int32_t	decaytable2[16];
		static const int8_t		attacktable[64][8];
		static const int32_t	ssgenvtable[8][2][3][2];

		static uint32_t			sinetable[1024];
		static int32_t			cltable[FM_CLENTS];

		static bool				tablehasmade;
		static void				MakeTable();



	//	friends --------------------------------------------------------------
		friend class Channel4;

	public:
		int32_t		dbgopout_;
		int32_t		dbgpgout_;
		static const int32_t* dbgGetClTable() { return cltable; }
		static const uint32_t* dbgGetSineTable() { return sinetable; }
	};

	//	4-op Channel ---------------------------------------------------------
	typedef struct
	{
		uint32_t	fb;
		int32_t		buf[4];
		int32_t		algo_;
		operator_savestate_t	op[4];
	} channel4_savestate_t;

	class Channel4
	{
	public:
		Channel4();
		void		SetChip(Chip* chip);
		void		SetType(OpType type);

		ISample		Calc();
		ISample		CalcL();
		ISample		CalcN(uint32_t noise);
		ISample		CalcLN(uint32_t noise);
		void		SetFNum(uint32_t fnum);
		void		SetFB(uint32_t fb);
		void		SetKCKF(uint32_t kc, uint32_t kf);
		void		SetAlgorithm(uint32_t algo);
		int			Prepare();
		void		KeyControl(uint32_t key);
		void		Reset();
		void		SetMS(uint32_t ms);
		void		Mute(bool);
		void		Refresh();

		void		dbgStopPG() { for (int i=0; i<4; i++) op[i].dbgStopPG(); }

		void		Save(channel4_savestate_t *state);
		void		Load(channel4_savestate_t *state);

	private:
		static const uint8_t fbtable[8];
		uint32_t	fb;
		int32_t		buf[4];
		int32_t*	in[3];			// �� OP �����ϥݥ���
		int32_t*	out[3];			// �� OP �ν��ϥݥ���
		int32_t*	pms;
		int32_t		algo_;
		Chip*		chip_;

		static void MakeTable();

		static bool tablehasmade;
		static int32_t 	kftable[64];


	public:
		Operator op[4];
	};

	//	Chip resource
	typedef struct
	{
		uint32_t	ratio_;
		uint32_t	aml_;
		uint32_t	pml_;
		int32_t		pmv_;
		OpType		optype_;
		uint32_t	multable_[4][16];
	} chip_savestate_t;

	class Chip
	{
	public:
		Chip();
		void		SetRatio(uint32_t ratio);
		void		SetAML(uint32_t l);
		void		SetPML(uint32_t l);
		void		SetPMV(int32_t pmv) { pmv_ = pmv; }

		uint32_t	GetMulValue(uint32_t dt2, uint32_t mul) { return multable_[dt2][mul]; }
		uint32_t	GetAML() { return aml_; }
		uint32_t	GetPML() { return pml_; }
		int			GetPMV() { return pmv_; }
		uint32_t	GetRatio() { return ratio_; }

		void		Save(chip_savestate_t *state);
		void		Load(chip_savestate_t *state);

	private:
		void		MakeTable();

		uint32_t	ratio_;
		uint32_t	aml_;
		uint32_t	pml_;
		int32_t		pmv_;
		OpType		optype_;
		uint32_t	multable_[4][16];
	};
}

#endif // FM_GEN_H
