extern "C" {

#include "common.h"
#include "winx68k.h"
#include "dswin.h"
#include "prop.h"
#include "mfp.h"
#include "adpcm.h"
#include "mercury.h"
#include "fdc.h"
#include "fmg_wrap.h"

#include "opm.h"
#include "opna.h"
};

class MyOPM : public FM::OPM
{
public:
	MyOPM();
	virtual ~MyOPM() {}
	void WriteIO(uint32_t adr, uint8_t data);
	void Count2(uint32_t clock);

private:
	virtual void Intr(bool);

public:
	int CurReg;
	uint32_t CurCount;

	int StateAction(StateMem *sm, int load, int data_only);
};


MyOPM::MyOPM()
{
	CurReg = 0;
}

int MyOPM::StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] =
	{
		SFVARN(CurReg, "CurReg"),
		SFVARN(CurCount, "CurCount"),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "MYOPM", false);
	ret &= FM::OPM::StateAction(sm, load, data_only);

	return(1);
}

void MyOPM::WriteIO(uint32_t adr, uint8_t data)
{
	if( adr&1 )
	{
		if ( CurReg==0x1b ) {
			::ADPCM_SetClock((data>>5)&4);
			::FDC_SetForceReady((data>>6)&1);
		}
		SetReg((int)CurReg, (int)data);
	}
	else
		CurReg = (int)data;
}

void MyOPM::Intr(bool f)
{
	if ( f ) ::MFP_Int(12);
}

void MyOPM::Count2(uint32_t clock)
{
	CurCount += clock;
	Count(CurCount/10);
	CurCount %= 10;
}

static MyOPM* opm = NULL;

int OPM_StateAction(StateMem *sm, int load, int data_only)
{
	if (opm)
		return opm->StateAction(sm, load, data_only);

	return 1;
}

int OPM_Init(int clock)
{
	opm = new MyOPM();
	if ( !opm ) return 0;
	if ( !opm->Init(clock, 44100) ) {
		delete opm;
		opm = NULL;
		return 0;
	}
	return 1;
}


void OPM_Cleanup(void)
{
	delete opm;
	opm = NULL;
}

void OPM_Reset(void)
{
	if ( opm ) opm->Reset();
}


uint8_t FASTCALL OPM_Read(void)
{
	if ( opm ) return opm->ReadStatus();
	return 0;
}


void FASTCALL OPM_Write(uint32_t adr, uint8_t data)
{
	if ( opm ) opm->WriteIO(adr, data);
}


void OPM_Update(int16_t *buffer, int length, uint8_t *pbsp, uint8_t *pbep)
{
	if ( opm ) opm->Mix((int16_t*)buffer, length, pbsp, pbep);
}


void FASTCALL OPM_Timer(uint32_t step)
{
	if ( opm ) opm->Count2(step);
}


void OPM_SetVolume(uint8_t vol)
{
	int v = (vol)?((16-vol)*4):192;		// このくらいかなぁ
	if ( opm ) opm->SetVolume(-v);
}


// ----------------------------------------------------------
// ---------------------------- YMF288 (満開版ま~きゅり~)
// ----------------------------------------------------------
// TODO : ROMEOの288を叩くの

class YMF288 : public FM::Y288
{
public:
	YMF288();
	virtual ~YMF288() {}
	void WriteIO(uint32_t adr, uint8_t data);
	uint8_t ReadIO(uint32_t adr);
	void Count2(uint32_t clock);
	void SetInt(int f) { IntrFlag = f; };
private:
	virtual void Intr(bool);
	int CurReg[2];
	uint32_t CurCount;
	int IntrFlag;
};

YMF288::YMF288()
{
	CurReg[0] = 0;
	CurReg[1] = 0;
	IntrFlag = 0;
}

void YMF288::WriteIO(uint32_t adr, uint8_t data)
{
	if( adr&1 )
		SetReg(((adr&2)?(CurReg[1]+0x100):CurReg[0]), (int)data);
   else
		CurReg[(adr>>1)&1] = (int)data;
}


uint8_t YMF288::ReadIO(uint32_t adr)
{
	if ( adr&1 )
		return GetReg(((adr&2)?(CurReg[1]+0x100):CurReg[0]));
   return ((adr)?(ReadStatusEx()):(ReadStatus()));
}

void YMF288::Intr(bool f)
{
   if ( (f)&&(IntrFlag) )
      ::Mcry_Int();
}


void YMF288::Count2(uint32_t clock)
{
	CurCount += clock;
	Count(CurCount/10);
	CurCount %= 10;
}

static YMF288* ymf288a = NULL;
static YMF288* ymf288b = NULL;

int M288_Init(int clock, const char* path)
{
	ymf288a = new YMF288();
	ymf288b = new YMF288();
	if ( (!ymf288a)||(!ymf288b) )
      goto error;
   if ( (!ymf288a->Init(clock, 44100, path))||(!ymf288b->Init(clock, 44100, path)) )
      goto error;
	ymf288a->SetInt(1);
	ymf288b->SetInt(0);
	return 1;

error:
   M288_Cleanup();
   return 0;
}


void M288_Cleanup(void)
{
	delete ymf288a;
	delete ymf288b;
	ymf288a = ymf288b = NULL;
}

void M288_Reset(void)
{
	if ( ymf288a ) ymf288a->Reset();
	if ( ymf288b ) ymf288b->Reset();
}


uint8_t FASTCALL M288_Read(uint16_t adr)
{
	if ( adr<=3 )
   {
		if ( ymf288a )
			return ymf288a->ReadIO(adr);
	}
   else
   {
		if ( ymf288b )
			return ymf288b->ReadIO(adr&3);
	}
   return 0;
}


void FASTCALL M288_Write(uint32_t adr, uint8_t data)
{
	if ( adr<=3 )
   {
      if ( ymf288a )
         ymf288a->WriteIO(adr, data);
	}
   else
   {
		if ( ymf288b )
         ymf288b->WriteIO(adr&3, data);
	}
}


void M288_Update(int16_t *buffer, size_t length)
{
	if ( ymf288a ) ymf288a->Mix((int16_t*)buffer, length);
	if ( ymf288b ) ymf288b->Mix((int16_t*)buffer, length);
}


void FASTCALL M288_Timer(uint32_t step)
{
	if ( ymf288a ) ymf288a->Count2(step);
	if ( ymf288b ) ymf288b->Count2(step);
}


void M288_SetVolume(uint8_t vol)
{
	int v1 = (vol)?((16-vol)*4-24):192;		// このくらいかなぁ
	int v2 = (vol)?((16-vol)*4):192;		// 少し小さめに
	if ( ymf288a ) {
		ymf288a->SetVolumeFM(-v1);
		ymf288a->SetVolumePSG(-v2);
	}
	if ( ymf288b ) {
		ymf288b->SetVolumeFM(-v1);
		ymf288b->SetVolumePSG(-v2);
	}
}
