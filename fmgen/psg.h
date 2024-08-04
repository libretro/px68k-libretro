// ---------------------------------------------------------------------------
//	PSG-like sound generator
//	Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------

#ifndef PSG_H
#define PSG_H

#define PSG_SAMPLETYPE		int16_t		// int32_t or int16_t

// ---------------------------------------------------------------------------
//	class PSG
//	PSG ���ɤ����������������벻����˥å�
//
//	interface:
//	bool SetClock(uint32_t clock, uint32_t rate)
//		����������Υ��饹����Ѥ������ˤ��ʤ餺�Ƥ�Ǥ������ȡ�
//		PSG �Υ���å��� PCM �졼�Ȥ����ꤹ��
//
//		clock:	PSG ��ư���å�
//		rate:	�������� PCM �Υ졼��
//		retval	���������������� true
//
//	void Mix(Sample* dest, int32_t nsamples)
//		PCM �� nsamples ʬ�������� dest �ǻϤޤ�����˲ä���(�û�����)
//		�����ޤǲû��ʤΤǡ��ǽ������򥼥��ꥢ����ɬ�פ�����
//
//	void Reset()
//		�ꥻ�åȤ���
//
//	void SetReg(uint32_t reg, uint8_t data)
//		�쥸���� reg �� data ��񤭹���
//
//	uint32_t GetReg(uint32_t reg)
//		�쥸���� reg �����Ƥ��ɤ߽Ф�
//
//	void SetVolume(int32_t db)
//		�Ʋ����β��̤�Ĵ�᤹��
//		ñ�̤��� 1/2 dB
//
class PSG
{
public:
	typedef PSG_SAMPLETYPE Sample;

	enum
	{
		noisetablesize = 1 << 11,	// ����������̤򸺤餷�����ʤ鸺�餷��
		toneshift = 24,
		envshift = 22,
		noiseshift = 14,
		oversampling = 2,		// �� �������®�٤�ͥ��ʤ鸺�餹�Ȥ�������
	};

public:
	PSG();
	~PSG();

	void Mix(Sample* dest, int32_t nsamples);
	void SetClock(int32_t clock, int32_t rate);

	void SetVolume(int32_t vol);
	void SetChannelMask(int32_t c);

	void Reset();
	void SetReg(uint32_t regnum, uint8_t data);
	uint32_t GetReg(uint32_t regnum) { return reg[regnum & 0x0f]; }

protected:
	void MakeNoiseTable();
	void MakeEnvelopTable();
	static void StoreSample(Sample& dest, int32_t data);

	uint8_t reg[16];

	const uint32_t* envelop;
	uint32_t olevel[3];
	uint32_t scount[3], speriod[3];
	uint32_t ecount, eperiod;
	uint32_t ncount, nperiod;
	uint32_t tperiodbase;
	uint32_t eperiodbase;
	uint32_t nperiodbase;
	int32_t volume;
	int32_t mask;

	static uint32_t enveloptable[16][64];
	static uint32_t noisetable[noisetablesize];
	static int32_t EmitTable[32];
};

#endif // PSG_H
