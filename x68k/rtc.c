/*
 *  RTC.C - RTC (Real Time Clock / RICOH RP5C15)
 */

#include "common.h"
#include "mfp.h"

#include <time.h>

static uint8_t	RTC_Regs[2][16];
static uint8_t	RTC_Bank  = 0;
static int RTC_Timer1    = 0;
static int RTC_Timer16   = 0;

int RTC_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFARRAYN(RTC_Regs[0], 16, "RTCRegs0"),
      SFARRAYN(RTC_Regs[1], 16, "RTCRegs1"),
	   SFVAR(RTC_Bank), /* never changed since alarm is not implemented, but whatever */
	   SFVAR(RTC_Timer1),
	   SFVAR(RTC_Timer16),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_RTC", false);

	return ret;
}

void RTC_Init(void)
{
	memset(&RTC_Regs[1][0], 0, 16);
	RTC_Regs[0][13] = 0;
	RTC_Regs[0][14] = 0;
	RTC_Regs[0][15] = 0x0c;
}

uint8_t FASTCALL RTC_Read(uint32_t adr)
{
   time_t t      = time(NULL);
   struct tm *tm = localtime(&t);

   adr          &= 0x1f;
   if (!(adr & 1))
      return 0;

   if (RTC_Bank == 0)
   {
      switch(adr)
      {
         case 0x01: return (tm->tm_sec)  % 10;
         case 0x03: return (tm->tm_sec)  / 10;
         case 0x05: return (tm->tm_min)  % 10;
         case 0x07: return (tm->tm_min)  / 10;
         case 0x09: return (tm->tm_hour) % 10;
         case 0x0b: return (tm->tm_hour) / 10;
         case 0x0d: return (uint8_t)(tm->tm_wday);
         case 0x0f: return (tm->tm_mday)%10;
         case 0x11: return (tm->tm_mday)/10;
         case 0x13: return (tm->tm_mon+1)%10;
         case 0x15: return (tm->tm_mon+1)/10;
         case 0x17: return ((tm->tm_year)-80)%10;
         case 0x19: return (((tm->tm_year)-80)/10)&0xf;
         case 0x1b: return RTC_Regs[0][13];
         case 0x1d: return RTC_Regs[0][14];
         case 0x1f: return RTC_Regs[0][15];
      }
      return 0;
   }
   if (adr == 0x1b)
      return (RTC_Regs[1][13]|1);
   else if (adr == 0x17)
      return ((tm->tm_year)-80)%4;
   return RTC_Regs[1][adr>>1];
}

void FASTCALL RTC_Write(uint32_t adr, uint8_t data)
{
	if ( adr==0xe8a001 )
          return;
	if ( adr==0xe8a01b )       /* Alarm/Timer Enable control */
		RTC_Regs[0][13] = RTC_Regs[1][13] = data & 0x0c;
	else if ( adr==0xe8a01f ) /* Alarm terminal output control */
		RTC_Regs[0][15] = RTC_Regs[1][15] = data & 0x0c;
}

void RTC_Timer(int clock)
{
	RTC_Timer1  += clock;
	RTC_Timer16 += clock;
	if ( RTC_Timer1>=10000000 )
   {
		if ( !(RTC_Regs[0][15]&8) ) MFP_Int(15);
		RTC_Timer1 -= 10000000;
	}
	if ( RTC_Timer16>=625000 )
   {
		if ( !(RTC_Regs[0][15]&4) ) MFP_Int(15);
		RTC_Timer16 -= 625000;
	}
}
