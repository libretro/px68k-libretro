/*
 *  MFP.C - MFP (Multi-Function Peripheral)
 */

#include "mfp.h"
#include "irqh.h"
#include "crtc.h"
#include "m68000.h"
#include "winx68k.h"
#include "keyboard.h"

uint8_t LastKey = 0;

uint8_t MFP[24];
static uint8_t Timer_Reload[4]      = {0, 0, 0, 0};
static int32_t Timer_Tick[4]        = {0, 0, 0, 0};
static const int Timer_Prescaler[8] = {1, 10, 25, 40, 125, 160, 250, 500};

int MFP_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFARRAY(MFP, 24),
	   SFVAR(LastKey),
	   SFARRAY(Timer_Reload, 4),
	   SFARRAY32(Timer_Tick, 4),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_MFP", false);

	return ret;
}


uint32_t FASTCALL MFP_IntCallback(uint8_t irq)
{
   uint8_t flag;
   uint32_t vect;
   int offset = 0;
   IRQH_IRQCallBack(irq);
   if (irq!=6) return (uint32_t)-1;
   for (flag=0x80, vect=15; flag; flag>>=1, vect--)
   {
      if ((MFP[MFP_IPRA]&flag)&&(MFP[MFP_IMRA]&flag)&&(!(MFP[MFP_ISRA]&flag)))
         break;
   }
   if (!flag)
   {
      offset = 1;
      for (flag=0x80, vect=7; flag; flag>>=1, vect--)
      {
         if ((MFP[MFP_IPRB]&flag)&&(MFP[MFP_IMRB]&flag)&&(!(MFP[MFP_ISRB]&flag)))
            break;
      }
   }
   if (!flag)
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "[PX68K] Error: MFP Int w/o Request. Default Vector(-1) has been returned.\n");
      return (uint32_t)-1;
   }

   MFP[MFP_IPRA+offset] &= (~flag);
   if (MFP[MFP_VR]&8)
      MFP[MFP_ISRA+offset] |= flag;
   vect |= (MFP[MFP_VR]&0xf0);
   for (flag=0x80; flag; flag>>=1)
   {
      if ((MFP[MFP_IPRA]&flag)&&(MFP[MFP_IMRA]&flag)&&(!(MFP[MFP_ISRA]&flag)))
      {
         IRQH_Int(6, &MFP_IntCallback);
         break;
      }
      if ((MFP[MFP_IPRB]&flag)&&(MFP[MFP_IMRB]&flag)&&(!(MFP[MFP_ISRB]&flag)))
      {
         IRQH_Int(6, &MFP_IntCallback);
         break;
      }
   }
   return vect;
}

void MFP_RecheckInt(void)
{
   uint8_t flag;
   IRQH_IRQCallBack(6);
   for (flag=0x80; flag; flag>>=1)
   {
      if ((MFP[MFP_IPRA]&flag)&&(MFP[MFP_IMRA]&flag)&&(!(MFP[MFP_ISRA]&flag)))
      {
         IRQH_Int(6, &MFP_IntCallback);
         break;
      }
      if ((MFP[MFP_IPRB]&flag)&&(MFP[MFP_IMRB]&flag)&&(!(MFP[MFP_ISRB]&flag)))
      {
         IRQH_Int(6, &MFP_IntCallback);
         break;
      }
   }
}

void MFP_Int(int irq) 
{
	uint8_t flag = 0x80;
	if (irq<8)
	{
		flag >>= irq;
		if (MFP[MFP_IERA]&flag)
		{
			MFP[MFP_IPRA] |= flag;
			if ((MFP[MFP_IMRA]&flag)&&(!(MFP[MFP_ISRA]&flag)))
			{
				IRQH_Int(6, &MFP_IntCallback);
			}
		}
	}
	else
	{
		irq -= 8;
		flag >>= irq;
		if (MFP[MFP_IERB]&flag)
		{
			MFP[MFP_IPRB] |= flag;
			if ((MFP[MFP_IMRB]&flag)&&(!(MFP[MFP_ISRB]&flag)))
			{
				IRQH_Int(6, &MFP_IntCallback);
			}
		}
	}
}

void MFP_Init(void)
{
	int i;
	static const uint8_t initregs[24] = {
		0x7b, 0x06, 0x00, 0x18, 0x3e, 0x00, 0x00, 0x00,
		0x00, 0x18, 0x3e, 0x40, 0x08, 0x01, 0x77, 0x01,
		0x0d, 0xc8, 0x14, 0x00, 0x88, 0x01, 0x81, 0x00
	};
	memcpy(MFP, initregs, 24);
	for (i=0; i<4; i++)
      Timer_Tick[i] = 0;
}

static uint8_t GetGPIP(void)
{
	uint8_t ret = 0x20; /* bit 5 is always 1 */
	int hpos    = (int)(ICount % HSYNC_CLK);

	if ((vline >= CRTC_VSTART) && (vline < CRTC_VEND))
		ret     |= 0x13;
	else
		ret     |= 0x03;

	if ((hpos >= ((int)CRTC_Regs[0x05] * HSYNC_CLK / CRTC_Regs[0x01])) &&
	    (hpos < ((int)CRTC_Regs[0x07] * HSYNC_CLK / CRTC_Regs[0x01])))
		ret     &= 0x7f;
	else
		ret     |= 0x80;

	if (vline != CRTC_IntLine)
		ret     |= 0x40;
	return ret;
}

uint8_t FASTCALL MFP_Read(uint32_t adr)
{
   if (adr > 0xe8802f)
      return 0;

   if (adr&1)
   {
      uint8_t reg = (uint8_t)((adr&0x3f)>>1);

      switch(reg)
      {
         case MFP_GPIP:
            return GetGPIP();
         case MFP_UDR:
               KeyIntFlag = 0;
               return LastKey;
         case MFP_RSR:
               if (KeyBufRP != KeyBufWP)
                  return MFP[reg] & 0x7f;
               return MFP[reg] | 0x80;
         default:
               break;
      }
      return MFP[reg];
   }
   return 0xff;
}

void FASTCALL MFP_Write(uint32_t adr, uint8_t data)
{
   if (adr>0xe8802f)
      return;

   if ((adr & 1) != 0)
   {
      uint8_t reg = (uint8_t)((adr&0x3f)>>1);

      switch(reg)
      {
         case MFP_IERA:
         case MFP_IERB:
            MFP[reg]    = data;
            MFP[reg+2] &= data;  /* Prohibited items drop IPRA/B */
            MFP_RecheckInt();
            break;
         case MFP_IPRA:
         case MFP_IPRB:
         case MFP_ISRA:
         case MFP_ISRB:
            MFP[reg] &= data;
            MFP_RecheckInt();
            break;
         case MFP_IMRA:
         case MFP_IMRB:
            MFP[reg] = data;
            MFP_RecheckInt();
            break;
         case MFP_TADR:
            Timer_Reload[0] = MFP[reg] = data;
            break;
         case MFP_TBDR:
            Timer_Reload[1] = MFP[reg] = data;
            break;
         case MFP_TCDR:
            Timer_Reload[2] = MFP[reg] = data;
            break;
         case MFP_TDDR:
            Timer_Reload[3] = MFP[reg] = data;
            break;
         case MFP_TSR:
            MFP[reg] = data | 0x80; /* Tx is always enabled */
            break;
         case MFP_UDR:
            break;
         case MFP_TACR:
         case MFP_TBCR:
         case MFP_TCDCR:
         default:
            MFP[reg] = data;
            break;
      }
   }
}

void FASTCALL MFP_Timer(int32_t clock)
{
   static const int TimerInt[4] = { 2, 7, 10, 11 };
   int chan;

   for (chan = 0; chan < 4; chan++)
   {
      int mode;

      switch (chan)
      {
         case 2:
            mode = MFP[MFP_TCDCR] >> 4;
            break;

         case 3:
            mode = MFP[MFP_TCDCR];
            break;

         default:
            mode = MFP[MFP_TACR + chan];
            break;
      }

      if (!((chan == 0) && (mode & 8)) && (mode & 7))
      {
         int             t = Timer_Prescaler[mode & 7];
         Timer_Tick[chan] += clock;

         while (Timer_Tick[chan] >= t)
         {
            Timer_Tick[chan] -= t;
            MFP[MFP_TADR + chan]--;
            if (MFP[MFP_TADR + chan] == 0)
            {
               MFP[MFP_TADR + chan] = Timer_Reload[chan];
               MFP_Int(TimerInt[chan]);
            }
         }
      }
   }
}

void FASTCALL MFP_TimerA(void)
{
	if ((MFP[MFP_TACR] & 15) == 8)
   {
      if (MFP[MFP_AER] & 0x10)
      {
         if (vline == CRTC_VSTART)
            MFP[MFP_TADR]--;
      }
      else
      {
         if (CRTC_VEND >= VLINE_TOTAL)
         {
            if ((long)vline == (VLINE_TOTAL - 1))
               MFP[MFP_TADR]--;
         }
         else
         {
            if (vline == CRTC_VEND)
               MFP[MFP_TADR]--;
         }
      }
      if (!MFP[MFP_TADR])
      {
         MFP[MFP_TADR] = Timer_Reload[0];
         MFP_Int(2);
      }
   }
}
