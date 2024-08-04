// ---------------------------------------------------------------------------
//	OPN/A/B interface with ADPCM support
//	Copyright (C) cisc 1998, 2003.
// ---------------------------------------------------------------------------
//	$fmgen-Id: opna.h,v 1.33 2003/06/12 13:14:37 cisc Exp $

#ifndef FM_OPNA_H
#define FM_OPNA_H

#include "fmgen.h"
#include "fmtimer.h"
#include "psg.h"

// ---------------------------------------------------------------------------
//	class OPN/OPNA
//	OPN/OPNA ���ɤ����������������벻����˥å�
//
//	interface:
//	bool Init(uint32_t clock, uint32_t rate, bool, const char* path);
//		����������Υ��饹����Ѥ������ˤ��ʤ餺�Ƥ�Ǥ������ȡ�
//		OPNA �ξ��Ϥ��δؿ��ǥꥺ�ॵ��ץ���ɤ߹���
//
//		clock:	OPN/OPNA/OPNB �Υ���å����ȿ�(Hz)
//
//		rate:	�������� PCM ��ɸ�ܼ��ȿ�(Hz)
//
//		path:	�ꥺ�ॵ��ץ�Υѥ�(OPNA �Τ�ͭ��)
//				��ά���ϥ����ȥǥ��쥯�ȥ꤫���ɤ߹���
//				ʸ����������ˤ� '\' �� '/' �ʤɤ�Ĥ��뤳��
//
//		�֤���	���������������� true
//
//	bool LoadRhythmSample(const char* path)
//		(OPNA ONLY)
//		Rhythm ����ץ���ɤ�ľ����
//		path �� Init �� path ��Ʊ����
//
//	bool SetRate(uint32_t clock, uint32_t rate, bool)
//		����å��� PCM �졼�Ȥ��ѹ�����
//		�������� Init �򻲾ȤΤ��ȡ�
//
//	void Mix(FM_SAMPLETYPE* dest, int32_t nsamples)
//		Stereo PCM �ǡ����� nsamples ʬ�������� dest �ǻϤޤ������
//		�ä���(�û�����)
//		��dest �ˤ� sample*2 ��ʬ���ΰ褬ɬ��
//		����Ǽ������ L, R, L, R... �Ȥʤ롥
//		�������ޤǲû��ʤΤǡ����餫��������򥼥��ꥢ����ɬ�פ�����
//		��FM_SAMPLETYPE �� short ���ξ�祯��åԥ󥰤��Ԥ���.
//		�����δؿ��ϲ��������Υ����ޡ��Ȥ���Ω���Ƥ��롥
//		  Timer �� Count �� GetNextEvent ������ɬ�פ����롥
//
//	void Reset()
//		������ꥻ�å�(�����)����
//
//	void SetReg(uint32_t reg, uint32_t data)
//		�����Υ쥸���� reg �� data ��񤭹���
//
//	uint32_t GetReg(uint32_t reg)
//		�����Υ쥸���� reg �����Ƥ��ɤ߽Ф�
//		�ɤ߹��ळ�Ȥ������쥸������ PSG, ADPCM �ΰ�����ID(0xff) �Ȥ�
//
//	uint32_t ReadStatus()/ReadStatusEx()
//		�����Υ��ơ������쥸�������ɤ߽Ф�
//		ReadStatusEx �ϳ�ĥ���ơ������쥸�������ɤ߽Ф�(OPNA)
//		busy �ե饰�Ͼ�� 0
//
//	bool Count(uint32_t t)
//		�����Υ����ޡ��� t [����] �ʤ�롥
//		�������������֤��Ѳ������ä���(timer �����С��ե�)
//		true ���֤�
//
//	uint32_t GetNextEvent()
//		�����Υ����ޡ��Τɤ��餫�������С��ե�����ޤǤ�ɬ�פ�
//		����[����]���֤�
//		�����ޡ�����ߤ��Ƥ������ ULONG_MAX ���֤��� �Ȼפ�
//
//	void SetVolumeFM(int32_t db)/SetVolumePSG(int32_t db) ...
//		�Ʋ����β��̤�ܡ�������Ĵ�᤹�롥ɸ���ͤ� 0.
//		ñ�̤��� 1/2 dB��ͭ���ϰϤξ�¤� 20 (10dB)
//
namespace FM
{
	//	OPN Base -------------------------------------------------------
	class OPNBase : public Timer
	{
	public:
		OPNBase();
		virtual ~OPNBase() {}

		bool	Init(uint32_t c, uint32_t r);
		virtual void Reset();

		void	SetVolumeFM(int32_t db);
		void	SetVolumePSG(int32_t db);

	protected:
		void	SetParameter(Channel4* ch, uint32_t addr, uint32_t data);
		void	SetPrescaler(uint32_t p);
		void	RebuildTimeTable();

		int32_t		fmvolume;

		uint32_t	clock;				// OPN ����å�
		uint32_t	rate;				// FM ���������졼��
		uint32_t	psgrate;			// FMGen  ���ϥ졼��
		uint32_t	status;
		Channel4* csmch;


		static  uint32_t lfotable[8];

	private:
		void	TimerA();
		uint8_t	prescale;

	protected:
		Chip	chip;
		PSG		psg;
	};

	//	OPN2 Base ------------------------------------------------------
	class OPNABase : public OPNBase
	{
	public:
		OPNABase();
		virtual ~OPNABase();

		uint32_t	ReadStatus() { return status & 0x03; }
		uint32_t	ReadStatusEx();
		void	SetChannelMask(uint32_t mask);

	private:
		virtual void Intr(bool) {}

		void	MakeTable2();

	protected:
		bool	Init(uint32_t c, uint32_t r, bool);
		bool	SetRate(uint32_t c, uint32_t r, bool);

		void	Reset();
		void 	SetReg(uint32_t addr, uint32_t data);
		void	SetADPCMBReg(uint32_t reg, uint32_t data);
		uint32_t	GetReg(uint32_t addr);

	protected:
		void	FMMix(Sample* buffer, int32_t nsamples);
		void 	Mix6(Sample* buffer, int32_t nsamples, int32_t activech);

		void	MixSubS(int32_t activech, ISample**);
		void	MixSubSL(int32_t activech, ISample**);

		void	SetStatus(uint32_t bit);
		void	ResetStatus(uint32_t bit);
		void	UpdateStatus();
		void	LFO();

		void	DecodeADPCMB();
		void	ADPCMBMix(Sample* dest, uint32_t count);

		void	WriteRAM(uint32_t data);
		uint32_t	ReadRAM();
		int		ReadRAMN();
		int		DecodeADPCMBSample(uint32_t);

	// FM �����ط�
		uint8_t	pan[6];
		uint8_t	fnum2[9];

		uint8_t	reg22;
		uint32_t	reg29;		// OPNA only?

		uint32_t	stmask;
		uint32_t	statusnext;

		uint32_t	lfocount;
		uint32_t	lfodcount;

		uint32_t	fnum[6];
		uint32_t	fnum3[3];

	// ADPCM �ط�
		uint8_t*	adpcmbuf;		// ADPCM RAM
		uint32_t	adpcmmask;		// ���ꥢ�ɥ쥹���Ф���ӥåȥޥ���
		uint32_t	adpcmnotice;	// ADPCM ������λ���ˤ��ĥӥå�
		uint32_t	startaddr;		// Start address
		uint32_t	stopaddr;		// Stop address
		uint32_t	memaddr;		// �����楢�ɥ쥹
		uint32_t	limitaddr;		// Limit address/mask
		int32_t		adpcmlevel;		// ADPCM ����
		int32_t		adpcmvolume;
		int32_t		adpcmvol;
		uint32_t	deltan;			// delta (n)
		int32_t		adplc;			// ���ȿ��Ѵ����ѿ�
		int32_t		adpld;			// ���ȿ��Ѵ����ѿ���ʬ��
		uint32_t	adplbase;		// adpld �θ�
		int32_t		adpcmx;			// ADPCM ������ (x)
		int32_t		adpcmd;			// ADPCM ������ (delta)
		int32_t		adpcmout;		// ADPCM ������ν���
		int32_t		apout0;			// out(t-2)+out(t-1)
		int32_t		apout1;			// out(t-1)+out(t)

		uint32_t	adpcmreadbuf;	// ADPCM �꡼���ѥХåե�
		bool	adpcmplay;		// ADPCM ������
		int8_t	granuality;
		bool	adpcmmask_;

		uint8_t	control1;		// ADPCM ����ȥ���쥸������
		uint8_t	control2;		// ADPCM ����ȥ���쥸������
		uint8_t	adpcmreg[8];	// ADPCM �쥸�����ΰ���ʬ

		int32_t		rhythmmask_;

		Channel4 ch[6];

		static void	BuildLFOTable();
		static int32_t amtable[FM_LFOENTS];
		static int32_t pmtable[FM_LFOENTS];
		static int32_t tltable[FM_TLENTS+FM_TLPOS];
		static bool	tablehasmade;
	};

	//	YM2203(OPN) ----------------------------------------------------
	class OPN : public OPNBase
	{
	public:
		OPN();
		virtual ~OPN() {}

		bool	Init(uint32_t c, uint32_t r, bool=false, const char* =0);
		bool	SetRate(uint32_t c, uint32_t r, bool=false);

		void	Reset();
		void 	Mix(Sample* buffer, int32_t nsamples);
		void 	SetReg(uint32_t addr, uint32_t data);
		uint32_t	GetReg(uint32_t addr);
		uint32_t	ReadStatus() { return status & 0x03; }
		uint32_t	ReadStatusEx() { return 0xff; }

		void	SetChannelMask(uint32_t mask);

		int		dbgGetOpOut(int c, int s) { return ch[c].op[s].dbgopout_; }
		int		dbgGetPGOut(int c, int s) { return ch[c].op[s].dbgpgout_; }
		Channel4* dbgGetCh(int c) { return &ch[c]; }

	private:
		virtual void Intr(bool) {}

		void	SetStatus(uint32_t bit);
		void	ResetStatus(uint32_t bit);

		uint32_t	fnum[3];
		uint32_t	fnum3[3];
		uint8_t	fnum2[6];

		Channel4 ch[3];
	};

	//	YM2608(OPNA) ---------------------------------------------------
	class OPNA : public OPNABase
	{
	public:
		OPNA();
		virtual ~OPNA();

		bool	Init(uint32_t c, uint32_t r, bool  = false, const char* rhythmpath=0);
		bool	LoadRhythmSample(const char*);

		bool	SetRate(uint32_t c, uint32_t r, bool = false);
		void 	Mix(Sample* buffer, int32_t nsamples);

		void	Reset();
		void 	SetReg(uint32_t addr, uint32_t data);
		uint32_t	GetReg(uint32_t addr);

		void	SetVolumeADPCM(int32_t db);
		void	SetVolumeRhythmTotal(int32_t db);
		void	SetVolumeRhythm(int32_t index, int32_t db);

		uint8_t*	GetADPCMBuffer() { return adpcmbuf; }

		int		dbgGetOpOut(int c, int s) { return ch[c].op[s].dbgopout_; }
		int		dbgGetPGOut(int c, int s) { return ch[c].op[s].dbgpgout_; }
		Channel4* dbgGetCh(int c) { return &ch[c]; }


	private:
		struct Rhythm
		{
			uint8_t	pan;		// �Ѥ�
			int8_t	level;		// �����礦
			int32_t		volume;		// �����礦���äƤ�
			int16_t*	sample;		// ����פ�
			uint32_t	size;		// ������
			uint32_t	pos;		// ����
			uint32_t	step;		// ���Ƥäפ�
			uint32_t	rate;		// ����פ�Τ졼��
		};

		void	RhythmMix(Sample* buffer, uint32_t count);

	// �ꥺ�಻���ط�
		Rhythm	rhythm[6];
		int8_t	rhythmtl;		// �ꥺ�����Τβ���
		int32_t		rhythmtvol;
		uint8_t	rhythmkey;		// �ꥺ��Υ���
	};

	//	YM2610/B(OPNB) ---------------------------------------------------
	class OPNB : public OPNABase
	{
	public:
		OPNB();
		virtual ~OPNB();

		bool	Init(uint32_t c, uint32_t r, bool = false,
					 uint8_t *_adpcma = 0, int32_t _adpcma_size = 0,
					 uint8_t *_adpcmb = 0, int32_t _adpcmb_size = 0);

		bool	SetRate(uint32_t c, uint32_t r, bool = false);
		void 	Mix(Sample* buffer, int32_t nsamples);

		void	Reset();
		void 	SetReg(uint32_t addr, uint32_t data);
		uint32_t	GetReg(uint32_t addr);
		uint32_t	ReadStatusEx();

		void	SetVolumeADPCMATotal(int32_t db);
		void	SetVolumeADPCMA(int32_t index, int32_t db);
		void	SetVolumeADPCMB(int32_t db);

//		void	SetChannelMask(uint32_t mask);

	private:
		struct ADPCMA
		{
			uint8_t	pan;		// �Ѥ�
			int8_t	level;		// �����礦
			int32_t		volume;		// �����礦���äƤ�
			uint32_t	pos;		// ����
			uint32_t	step;		// ���Ƥäפ�

			uint32_t	start;		// ����
			uint32_t	stop;		// ��λ
			uint32_t	nibble;		// ���� 4 bit
			int32_t		adpcmx;		// �Ѵ���
			int32_t		adpcmd;		// �Ѵ���
		};

		int		DecodeADPCMASample(uint32_t);
		void	ADPCMAMix(Sample* buffer, uint32_t count);
		static void InitADPCMATable();

	// ADPCMA �ط�
		uint8_t*	adpcmabuf;		// ADPCMA ROM
		int32_t		adpcmasize;
		ADPCMA	adpcma[6];
		int8_t	adpcmatl;		// ADPCMA ���Τβ���
		int32_t		adpcmatvol;
		uint8_t	adpcmakey;		// ADPCMA �Υ���
		int32_t		adpcmastep;
		uint8_t	adpcmareg[32];

		static int32_t jedi_table[(48+1)*16];

		Channel4 ch[6];
	};

	//	YM2612/3438(OPN2) ----------------------------------------------------
	class OPN2 : public OPNBase
	{
	public:
		OPN2();
		virtual ~OPN2() {}

		bool	Init(uint32_t c, uint32_t r, bool=false, const char* =0);
		bool	SetRate(uint32_t c, uint32_t r, bool);

		void	Reset();
		void 	Mix(Sample* buffer, int32_t nsamples);
		void 	SetReg(uint32_t addr, uint32_t data);
		uint32_t	GetReg(uint32_t addr);
		uint32_t	ReadStatus() { return status & 0x03; }
		uint32_t	ReadStatusEx() { return 0xff; }

		void	SetChannelMask(uint32_t mask);

	private:
		virtual void Intr(bool) {}

		void	SetStatus(uint32_t bit);
		void	ResetStatus(uint32_t bit);

		uint32_t	fnum[3];
		uint32_t	fnum3[3];
		uint8_t	fnum2[6];

	// ��������ѥ��
		int32_t	mixc, mixc1;

		Channel4 ch[3];
	};

	//	YMF288 ---------------------------------------------------
	class Y288 : public OPNABase
	{
	public:
		Y288();
		virtual ~Y288();

		bool	Init(uint32_t c, uint32_t r, bool ipflag = false, const char* =0);
		bool	LoadRhythmSample(const char*);

		bool	SetRate(uint32_t c, uint32_t r, bool ipflag = false);
		void 	Mix(Sample* buffer, int32_t nsamples);

		void	SetVolumeRhythmTotal(int32_t db);
		void	SetVolumeRhythm(int32_t index, int32_t db);

		void	Reset();
		void 	SetReg(uint32_t addr, uint32_t data);
		uint32_t	GetReg(uint32_t addr);

	private:
		struct Rhythm
		{
			uint8_t	pan;		// �Ѥ�
			int8_t	level;		// �����礦
			int32_t		volume;		// �����礦���äƤ�
			int16_t*	sample;		// ����פ�
			uint32_t	size;		// ������
			uint32_t	pos;		// ����
			uint32_t	step;		// ���Ƥäפ�
			uint32_t	rate;		// ����פ�Τ졼��
		};

		void	RhythmMix(Sample* buffer, uint32_t count);

	// �ꥺ�಻���ط�
		Rhythm	rhythm[6];
		int8_t	rhythmtl;		// �ꥺ�����Τβ���
		int32_t		rhythmtvol;
		uint8_t	rhythmkey;		// �ꥺ��Υ���

		int32_t	mode288;		// 288/2608�⡼�ɥե饰
	};
}

// ---------------------------------------------------------------------------

inline void FM::OPNBase::RebuildTimeTable()
{
	int32_t p = prescale;
	prescale = (uint8_t)-1;
	SetPrescaler(p);
}

inline void FM::OPNBase::SetVolumePSG(int32_t db)
{
	psg.SetVolume(db);
}

#endif // FM_OPNA_H
