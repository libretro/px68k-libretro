/*
 *  SYSPORT.C - X68k System Port
 */

#include "common.h"
#include "prop.h"
#include "sram.h"
#include "sysport.h"
#include "palette.h"

typedef struct
{
	uint8_t contrast;
	uint8_t monitor;
	uint8_t keyctrl;
	uint8_t cputype;
} SYSPORT;

static SYSPORT sysport;

int SysPort_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFVAR(sysport.contrast),
		SFVAR(sysport.monitor),
		SFVAR(sysport.keyctrl),
		SFVAR(sysport.cputype),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_SysPort", false);

	return ret;
}

void SysPort_Init(void)
{
	static uint8_t cputype[] = {
		0xff, /* 68000, 10Mhz */
		0xfe,
		0xfe, /* 68020, 16Mgz */
		0xdc  /* 68030, 25Mhz */
	};

	sysport.contrast = 0;
	sysport.monitor = 0;
	sysport.cputype = cputype[Config.XVIMode & 3];
}

uint8_t FASTCALL SysPort_Read(uint32_t adr)
{
	if (adr & 1)
	{
		adr &= 0x0f;
		adr >>= 1;

		switch (adr)
		{
		case 0: /* contrast */
			return (0xf0 | sysport.contrast);
		case 1: /* b3: monitor control, b0-b1 3d scope */
			return (0xf0 | sysport.monitor);
		case 3: /* b3: keyboard connec*ed */
			return (0xf0 | sysport.keyctrl);
		case 5:
			return (0xf0 | sysport.cputype);
		default:
			/* unimplemented registers returns 0xff */
			return 0xff;
		}
	}

	return 0xff;
}

void FASTCALL SysPort_Write(uint32_t adr, uint8_t data)
{
	if (adr & 1)
	{
		adr &= 0x0f;
		adr >>= 1;

		switch (adr)
		{
		case 0:
			data &= 0x0f;
			if (sysport.contrast != data)
			{
				sysport.contrast = data;
				Pal_ChangeContrast(data);
			}
			break;
		case 1:
			data &= 0x0b;
			sysport.monitor = data;
			break;
		case 3:
			data &= 0xe0;
			sysport.keyctrl = data;
			break;
		case 6:
			if (data == 0x31)
				SRAM_WriteEnable(1);
			else
				SRAM_WriteEnable(0);
			break;
		default:
			break;
		}
	}
}

