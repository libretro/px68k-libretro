/*
 *  ADPCM.C - ADPCM (OKI MSM6258V)
 */

#include <math.h>

#include "common.h"
#include "prop.h"
#include "pia.h"
#include "adpcm.h"
#include "dmac.h"

#define ADPCM_BufSize      96000
#define ADPCMMAX           2047
#define ADPCMMIN          -2048
#define FM_IPSCALE         256L

#define INTERPOLATE(y, x)	\
	(((((((-y[0]+3*y[1]-3*y[2]+y[3]) * x + FM_IPSCALE/2) / FM_IPSCALE \
	+ 3 * (y[0]-2*y[1]+y[2])) * x + FM_IPSCALE/2) / FM_IPSCALE \
	- 2*y[0]-3*y[1]+6*y[2]-y[3]) * x + 3*FM_IPSCALE) / (6*FM_IPSCALE) + y[1])

static int ADPCM_VolumeShift = 65536;
static const int32_t index_shift[8] = {
	-1, -1, -1, -1, 2, 4, 6, 8
};
static const int32_t index_table[58] = {
	0, 0,
	1, 2, 3, 4, 5, 6, 7, 8,
	9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24,
	25, 26,	27,	28, 29, 30,	31, 32,
	33, 34,	35, 36, 37, 38,	39, 40,
	41, 42,	43, 44, 45, 46,	47, 48,
	48, 48, 48, 48, 48, 48,	48, 48
};
static const int ADPCM_Clocks[8] = {
	93750, 125000, 187500, 125000, 46875, 62500, 93750, 62500 };
static int dif_table[49*16];
static int16_t ADPCM_BufR[ADPCM_BufSize];
static int16_t ADPCM_BufL[ADPCM_BufSize];

static int32_t  ADPCM_WrPtr = 0;
static int32_t  ADPCM_RdPtr = 0;
static uint32_t ADPCM_SampleRate = 44100*12;
static uint32_t ADPCM_ClockRate = 7800*12;
static uint32_t ADPCM_Count = 0;
static int ADPCM_Step = 0;
static int ADPCM_Out = 0;
static uint8_t ADPCM_Playing = 0;
static uint8_t ADPCM_Clock = 0;
static int ADPCM_PreCounter = 0;
static int ADPCM_DifBuf = 0;

static int ADPCM_Pan = 0x00;
static int OldR = 0, OldL = 0;
static int Outs[8];
static int OutsIp[4];
static int OutsIpR[4];
static int OutsIpL[4];

int ADPCM_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		/* TODO: Some of the vars might not be necessary */
		SFARRAY16(ADPCM_BufL, 96000),
		SFARRAY16(ADPCM_BufR, 96000),

		SFVAR(ADPCM_VolumeShift),
		SFVAR(ADPCM_WrPtr),
		SFVAR(ADPCM_RdPtr),

		SFVAR(ADPCM_ClockRate),
		SFVAR(ADPCM_Count),

		SFVAR(ADPCM_Step),
		SFVAR(ADPCM_Out),
		SFVAR(ADPCM_Playing),
		SFVAR(ADPCM_Clock),
		SFVAR(ADPCM_PreCounter),
		SFVAR(ADPCM_DifBuf),

		SFVAR(ADPCM_Pan),
		SFVAR(OldR),
		SFVAR(OldL),
		SFARRAY32(Outs, 8),
		SFARRAY32(OutsIp, 4),
		SFARRAY32(OutsIpR, 4),
		SFARRAY32(OutsIpL, 4),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_ADPC", false);

	return ret;
}


static void ADPCM_InitTable(void)
{
	int step, n;
	static int bit[16][4] =
	{
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};

	for (step=0; step<=48; step++)
   {
		double val = floor(16.0 * pow ((double)1.1, (double)step));
		for (n=0; n<16; n++)
      {
			dif_table[step*16+n] = bit[n][0] *
			   (int)(val   * bit[n][1] +
				 val/2 * bit[n][2] +
				 val/4 * bit[n][3] +
				 val/8);
		}
	}
}

void FASTCALL ADPCM_PreUpdate(uint32_t clock)
{
	ADPCM_PreCounter += ((ADPCM_ClockRate/24)*clock);
	while ( ADPCM_PreCounter>=10000000L )
   {
		ADPCM_DifBuf -= ( (ADPCM_SampleRate*400)/ADPCM_ClockRate );
		if ( ADPCM_DifBuf<=0 )
      {
			ADPCM_DifBuf = 0;
			DMA_Exec(3);
		}
		ADPCM_PreCounter -= 10000000L;
	}
}

void ADPCM_Update(int16_t *buffer, size_t length, uint8_t *pbsp, uint8_t *pbep)
{
	int outs;
	int32_t outl, outr;

	if ( length<=0 ) return;

   if ( Config.Sound_LPF )
   {
      while ( length )
      {
         int tmpl, tmpr;
         if (buffer >= (int16_t *)pbep)
            buffer = (int16_t*)pbsp;

         if ( (ADPCM_WrPtr==ADPCM_RdPtr)&&(!(DMA[3].CCR&0x40)) )
            DMA_Exec(3);
         if ( ADPCM_WrPtr!=ADPCM_RdPtr )
         {
            OldR = outr = ADPCM_BufL[ADPCM_RdPtr];
            OldL = outl = ADPCM_BufR[ADPCM_RdPtr];
            ADPCM_RdPtr++;
            if ( ADPCM_RdPtr>=ADPCM_BufSize )
               ADPCM_RdPtr = 0;
         }
         else
         {
            outr = OldR;
            outl = OldL;
         }

         outr       = (int)(outr*40*ADPCM_VolumeShift);
         outs       = (outr + Outs[3]*2 + Outs[2] + Outs[1]*157 - Outs[0]*61) >> 8;
         Outs[2]    = Outs[3];
         Outs[3]    = outr;
         Outs[0]    = Outs[1];
         Outs[1]    = outs;

         OutsIpR[0] = OutsIpR[1];
         OutsIpR[1] = OutsIpR[2];
         OutsIpR[2] = OutsIpR[3];
         OutsIpR[3] = outs;

         outl       = (int)(outl*40*ADPCM_VolumeShift);
         outs       = (outl + Outs[7]*2 + Outs[6] + Outs[5]*157 - Outs[4]*61) >> 8;
         Outs[6]    = Outs[7];
         Outs[7]    = outl;
         Outs[4]    = Outs[5];
         Outs[5]    = outs;

         OutsIpL[0] = OutsIpL[1];
         OutsIpL[1] = OutsIpL[2];
         OutsIpL[2] = OutsIpL[3];
         OutsIpL[3] = outs;

         tmpr = INTERPOLATE(OutsIpR, 0);
         if ( tmpr>32767 )
            tmpr = 32767;
         else if ( tmpr<(-32768) )
            tmpr = -32768;
         *(buffer++) = (int16_t)tmpr;
         tmpl = INTERPOLATE(OutsIpL, 0);
         if ( tmpl>32767 )
            tmpl = 32767;
         else if ( tmpl<(-32768) )
            tmpl = -32768;
         *(buffer++) = (int16_t)tmpl;
         length--;
      }
   }
   else
   {
      while ( length )
      {
         int tmpl, tmpr;
         if (buffer >= (int16_t *)pbep)
            buffer = (int16_t*)pbsp;

         if ( (ADPCM_WrPtr==ADPCM_RdPtr)&&(!(DMA[3].CCR&0x40)) )
            DMA_Exec(3);
         if ( ADPCM_WrPtr!=ADPCM_RdPtr )
         {
            OldR = outr = ADPCM_BufL[ADPCM_RdPtr];
            OldL = outl = ADPCM_BufR[ADPCM_RdPtr];
            ADPCM_RdPtr++;
            if ( ADPCM_RdPtr>=ADPCM_BufSize )
               ADPCM_RdPtr = 0;
         }
         else
         {
            outr = OldR;
            outl = OldL;
         }

         outs       = (int)(outr*ADPCM_VolumeShift);

         OutsIpR[0] = OutsIpR[1];
         OutsIpR[1] = OutsIpR[2];
         OutsIpR[2] = OutsIpR[3];
         OutsIpR[3] = outs;

         outs       = (int)(outl*ADPCM_VolumeShift);

         OutsIpL[0] = OutsIpL[1];
         OutsIpL[1] = OutsIpL[2];
         OutsIpL[2] = OutsIpL[3];
         OutsIpL[3] = outs;

         tmpr = INTERPOLATE(OutsIpR, 0);
         if ( tmpr>32767 )
            tmpr = 32767;
         else if ( tmpr<(-32768) )
            tmpr = -32768;
         *(buffer++) = (int16_t)tmpr;
         tmpl = INTERPOLATE(OutsIpL, 0);
         if ( tmpl>32767 )
            tmpl = 32767;
         else if ( tmpl<(-32768) )
            tmpl = -32768;
         *(buffer++) = (int16_t)tmpl;
         length--;
      }
   }

	ADPCM_DifBuf = ADPCM_WrPtr-ADPCM_RdPtr;
	if ( ADPCM_DifBuf<0 )
      ADPCM_DifBuf += ADPCM_BufSize;
}

static INLINE void ADPCM_WriteOne(uint8_t val)
{
   ADPCM_Out += dif_table[(ADPCM_Step << 4) + val];
	if ( ADPCM_Out>ADPCMMAX )
      ADPCM_Out = ADPCMMAX;
   else if ( ADPCM_Out<ADPCMMIN )
      ADPCM_Out = ADPCMMIN;

   ADPCM_Step += index_shift[val & 0x07];
   ADPCM_Step  = index_table[ADPCM_Step + 1];

	if ( OutsIp[0]==-1 )
   {
      OutsIp[0] =
      OutsIp[1] =
      OutsIp[2] =
      OutsIp[3] = ADPCM_Out;
   }
   else
   {
      OutsIp[0] = OutsIp[1];
      OutsIp[1] = OutsIp[2];
      OutsIp[2] = OutsIp[3];
      OutsIp[3] = ADPCM_Out;
   }

	while ( ADPCM_SampleRate>ADPCM_Count ) {
		if ( ADPCM_Playing ) {
			int ratio = (((ADPCM_Count/100)*FM_IPSCALE)/(ADPCM_SampleRate/100));
			int tmp = INTERPOLATE(OutsIp, ratio);
			if ( tmp>ADPCMMAX ) tmp = ADPCMMAX; else if ( tmp<ADPCMMIN ) tmp = ADPCMMIN;
			if ( !(ADPCM_Pan&1) )
				ADPCM_BufR[ADPCM_WrPtr] = (int16_t)tmp;
			else
				ADPCM_BufR[ADPCM_WrPtr] = 0;
			if ( !(ADPCM_Pan&2) )
				ADPCM_BufL[ADPCM_WrPtr++] = (int16_t)tmp;
			else
				ADPCM_BufL[ADPCM_WrPtr++] = 0;
			if ( ADPCM_WrPtr>=ADPCM_BufSize ) ADPCM_WrPtr = 0;
		}
		ADPCM_Count += ADPCM_ClockRate;
	}
	ADPCM_Count -= ADPCM_SampleRate;
}

void FASTCALL ADPCM_Write(uint32_t adr, uint8_t data)
{
	if ( adr==0xe92001 )
   {
      if ( data&1 )
         ADPCM_Playing = 0;
      else if ( data&2 )
      {
         if ( !ADPCM_Playing )
         {
            ADPCM_Step    = 0;
            ADPCM_Out     = 0;
            OldL = OldR   = -2;
            ADPCM_Playing = 1;
         }
         OutsIp[0] = OutsIp[1] = OutsIp[2] = OutsIp[3] = -1;
      }
   }
   else if ( adr==0xe92003 )
   {
		if ( ADPCM_Playing )
      {
         ADPCM_WriteOne((uint8_t)(data & 15));
         ADPCM_WriteOne((uint8_t)((data >> 4) & 15));
      }
	}
}

uint8_t FASTCALL ADPCM_Read(uint32_t adr)
{
	if ( adr==0xe92001 )
		return ((ADPCM_Playing)?0xc0:0x40);
	return 0x00;
}

void ADPCM_SetVolume(uint8_t vol)
{
	if ( vol>16 ) vol=16;

	if ( vol )
		ADPCM_VolumeShift = (int)((double)16/pow(1.189207115, (16-vol)));
	else
		ADPCM_VolumeShift = 0;
}

void ADPCM_SetPan(int n)
{
	if ( (ADPCM_Pan&0x0c)!=(n&0x0c) )
   {
		ADPCM_Count     = 0;
		ADPCM_Clock     = (ADPCM_Clock&4)|((n>>2)&3);
		ADPCM_ClockRate = ADPCM_Clocks[ADPCM_Clock];
	}
	ADPCM_Pan = n;
}

void ADPCM_SetClock(int n)
{
	if ( (ADPCM_Clock&4)!=n )
   {
		ADPCM_Count     = 0;
		ADPCM_Clock     = n | ((ADPCM_Pan>>2)&3);
		ADPCM_ClockRate = ADPCM_Clocks[ADPCM_Clock];
	}
}

void ADPCM_Init(void)
{
	ADPCM_WrPtr      = 0;
	ADPCM_RdPtr      = 0;
	ADPCM_Out        = 0;
	ADPCM_Step       = 0;
	ADPCM_Playing    = 0;
	ADPCM_SampleRate = (44100 * 12);
	ADPCM_PreCounter = 0;
	memset(Outs, 0, sizeof(Outs));
	OutsIp[0]  = OutsIp[1]  = OutsIp[2]  = OutsIp[3]  = -1;
	OutsIpR[0] = OutsIpR[1] = OutsIpR[2] = OutsIpR[3] = 0;
	OutsIpL[0] = OutsIpL[1] = OutsIpL[2] = OutsIpL[3] = 0;
	OldL = OldR = 0;

	ADPCM_SetPan(0x0b);
	ADPCM_InitTable();
}
