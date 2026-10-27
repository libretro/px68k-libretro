/*
 *  IRQH.C - IRQ Handler
 */

#include "common.h"
#include "../m68000/m68000.h"
#include "irqh.h"

#if defined (HAVE_CYCLONE)
extern struct Cyclone m68k;
#endif
typedef int32_t  FASTCALL C68K_INT_CALLBACK(int32_t level);

static uint8_t	IRQH_IRQ[8];
static void	*IRQH_CallBack[8];

int IRQH_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFARRAY(IRQH_IRQ, 8),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_IRQH", false);

	return ret;
}

void IRQH_Init(void)
{
	memset(IRQH_IRQ, 0, 8);
}

static uint32_t FASTCALL IRQH_DefaultVector(uint8_t irq)
{
	IRQH_IRQCallBack(irq);
	return -1;
}


void IRQH_IRQCallBack(uint8_t irq)
{
	int i;
	IRQH_IRQ[irq&7] = 0;

m68000_set_irq_line(0, CLEAR_LINE);

	for (i=7; i>0; i--)
	{
		if (IRQH_IRQ[i])
		{
m68000_set_irq_line(i, HOLD_LINE);
			return;
		}
	}
}

void IRQH_Int(uint8_t irq, void* handler)
{
	int i;
	IRQH_IRQ[irq&7] = 1;
	if (handler==NULL)
		IRQH_CallBack[irq&7] = &IRQH_DefaultVector;
	else
		IRQH_CallBack[irq&7] = handler;
	for (i=7; i>0; i--)
	{
		if (IRQH_IRQ[i])
		{
m68000_set_irq_line(i, HOLD_LINE);
			return;
		}
	}
}

int32_t my_irqh_callback(int32_t level)
{
   int i;
   C68K_INT_CALLBACK *func = IRQH_CallBack[level&7];
   int vect = (func)(level&7);

   for (i=7; i>0; i--)
   {
      if (IRQH_IRQ[i])
      {
m68000_set_irq_line(i, HOLD_LINE);
         break;
      }
   }

   return (int32_t)vect;
}
