/*
 *  SRAM.C - SRAM (16kb)
 */

#include	"common.h"
#include	"../libretro/dosio.h"
#include	"prop.h"
#include	"winx68k.h"
#include	"sysport.h"
#include	"x68kmemory.h"
#include	"sram.h"

static int write_enabled = 0;
uint8_t	SRAM[0x4000];

int SRAM_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFARRAYN(SRAM, 0x4000, "MEM_SRAM"),
		SFVAR(write_enabled),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_SRAM", false);

	return ret;
}

void SRAM_VirusCheck(void)
{
	if ( (cpu_readmem24_dword(0xed3f60)==0x60000002)
	   &&(cpu_readmem24_dword(0xed0010)==0x00ed3f60) )
	{
		SRAM_Cleanup();
		SRAM_Init();
	}
}

void SRAM_WriteEnable(int enable)
{
	if (write_enabled != enable)
	{
		log_cb(RETRO_LOG_DEBUG, "sram write enable = %d\n", enable);
		write_enabled = enable;
	}
}

void SRAM_Init(void)
{
	int i;
	void *fp;

	write_enabled = 0;

	for (i=0; i<0x4000; i++)
		SRAM[i] = 0xFF;

	if ((fp = file_open_c("sram.dat")))
	{
		file_lread(fp, SRAM, 0x4000);
		file_close(fp);

#ifndef MSB_FIRST
		for (i=0; i<0x4000; i+=2)
		{
			uint8_t tmp = SRAM[i];
			SRAM[i]     = SRAM[i+1];
			SRAM[i+1]   = tmp;
		}
#endif
	}
}

void SRAM_Cleanup(void)
{
   int i;
   void *fp;

   for (i=0; i<0x4000; i+=2)
   {
      uint8_t tmp = SRAM[i];
      SRAM[i]     = SRAM[i+1];
      SRAM[i+1]   = tmp;
   }

   if (!(fp = file_open_c("sram.dat")))
      if (!(fp = file_create_c("sram.dat")))
         return;

   file_lwrite(fp, SRAM, 0x4000);
   file_close(fp);
}

uint8_t FASTCALL SRAM_Read(uint32_t adr)
{
	adr &= 0xffff;
	adr ^= 1;
	if (adr<0x4000)
		return SRAM[adr];
	return 0xff;
}


void FASTCALL SRAM_Write(uint32_t adr, uint8_t data)
{
	if ( write_enabled && (adr < 0xed4000) )
	{
		adr       &= 0xffff;
#ifndef MSB_FIRST
		adr       ^= 1;
#endif
		SRAM[adr]  = data;
	}
}

void FASTCALL SRAM_UpdateBoot(void)
{
	cpu_writemem24(0xe8e00d, 0x31); /* SRAM write permission */
	cpu_writemem24_dword(0xed0040, cpu_readmem24_dword(0xed0040) + 1); /* Estimated operation time(min.) */
	cpu_writemem24_dword(0xed0044, cpu_readmem24_dword(0xed0044) + 1); /* Estimated booting times */
	cpu_writemem24(0xe8e00d, 0x00);
}
