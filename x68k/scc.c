/*
 *  SCC.C - Z8530 SCC (mouse only)
 */

#include "common.h"
#include "scc.h"
#include "m68000.h"
#include "irqh.h"
#include "mouse.h"

int8_t MouseX = 0;
int8_t MouseY = 0;
uint8_t MouseSt = 0;

static uint8_t SCC_RegNumA   = 0;
static uint8_t SCC_RegSetA   = 0;
static uint8_t SCC_RegsB[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint8_t SCC_RegNumB   = 0;
static uint8_t SCC_RegSetB   = 0;
static uint8_t SCC_Vector    = 0;
static uint8_t SCC_Dat[3]    = {0, 0, 0};
static uint8_t SCC_DatNum    = 0;

int SCC_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFVAR(MouseX),
		SFVAR(MouseY),
		SFVAR(MouseSt),

		SFVAR(SCC_RegNumA),
		SFVAR(SCC_RegSetA),
		SFARRAY(SCC_RegsB, 16),
		SFVAR(SCC_RegNumB),
		SFVAR(SCC_RegSetB),
		SFVAR(SCC_Vector),
		SFARRAY(SCC_Dat, 3),
		SFVAR(SCC_DatNum),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_SCC", false);

	return ret;
}

static uint32_t FASTCALL SCC_Int(uint8_t irq)
{
   IRQH_IRQCallBack(irq);
   if ( (irq==5)&&(!(SCC_RegsB[9]&2)) )
   {
      if (SCC_RegsB[9]&1)
      {
         if (SCC_RegsB[9]&0x10)
            return ((uint32_t)(SCC_Vector&0x8f)+0x20);
         return ((uint32_t)(SCC_Vector&0xf1)+4);
      }
      return ((uint32_t)SCC_Vector);
   }

   return (uint32_t)(-1);
}

void SCC_IntCheck(void)
{
	if ( (SCC_DatNum) && ((SCC_RegsB[1]&0x18)==0x10) && (SCC_RegsB[9]&0x08) )
	{
		IRQH_Int(5, &SCC_Int);
	}
	else if ( (SCC_DatNum==3) && ((SCC_RegsB[1]&0x18)==0x08) && (SCC_RegsB[9]&0x08) )
	{
		IRQH_Int(5, &SCC_Int);
	}
}


void SCC_Init(void)
{
	MouseX = 0;
	MouseY = 0;
	MouseSt = 0;
	SCC_RegNumA = 0;
	SCC_RegSetA = 0;
	SCC_RegNumB = 0;
	SCC_RegSetB = 0;
	SCC_Vector = 0;
	SCC_DatNum = 0;
}

void FASTCALL SCC_Write(uint32_t adr, uint8_t data)
{
	if (adr>=0xe98008)
      return;

	if ((adr&7) == 1)
	{
		if (SCC_RegSetB)
		{
			if (SCC_RegNumB == 5)
			{
				if ( (!(SCC_RegsB[5]&2))&&(data&2)&&(SCC_RegsB[3]&1)
                  &&(!SCC_DatNum) )	
				{
					Mouse_SetData();
					SCC_DatNum = 3;
					SCC_Dat[2] = MouseSt;
					SCC_Dat[1] = MouseX;
					SCC_Dat[0] = MouseY;
				}
			}
			else if (SCC_RegNumB == 2)
            SCC_Vector = data;
			SCC_RegSetB = 0;
			SCC_RegsB[SCC_RegNumB] = data;
			SCC_RegNumB = 0;
		}
		else
		{
			if (!(data&0xf0))
			{
				data &= 15;
				SCC_RegSetB = 1;
				SCC_RegNumB = data;
			}
			else
			{
				SCC_RegSetB = 0;
				SCC_RegNumB = 0;
			}
		}
	}
	else if ((adr&7) == 5)
	{
		if (SCC_RegSetA)
      {
         SCC_RegSetA = 0;
         switch (SCC_RegNumA)
         {
            case 2:
               SCC_RegsB[2] = data;
               SCC_Vector   = data;
               break;
            case 9:
               SCC_RegsB[9] = data;
               break;
         }
      }
		else
      {
         data &= 15;
         if (data)
         {
            SCC_RegSetA = 1;
            SCC_RegNumA = data;
         }
         else
         {
            SCC_RegSetA = 0;
            SCC_RegNumA = 0;
         }
      }
	}
}

uint8_t FASTCALL SCC_Read(uint32_t adr)
{
	uint8_t ret = 0;

	if (adr >= 0xe98008)
      return 0;

	if ((adr&7) == 1)
	{
		if (!SCC_RegNumB)
			ret = ((SCC_DatNum)?1:0);
		SCC_RegNumB = 0;
		SCC_RegSetB = 0;
	}
	else if ((adr&7) == 3)
	{
		if (SCC_DatNum)
		{
			SCC_DatNum--;
			ret = SCC_Dat[SCC_DatNum];
		}
	}
	else if ((adr&7) == 5)
	{
		switch(SCC_RegNumA)
		{
		case 0:
			ret = 4;
			break;
		case 3:
			ret = ((SCC_DatNum)?4:0);
			break;
		}
		SCC_RegNumA = 0;
		SCC_RegSetA = 0;
	}
	return ret;
}
