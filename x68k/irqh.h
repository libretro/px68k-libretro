#ifndef _WINX68K_IRQH
#define _WINX68K_IRQH

#include "common.h"

void IRQH_Init(void);
DWORD FASTCALL IRQH_DefaultVector(BYTE irq);
void IRQH_IRQCallBack(BYTE irq);
void IRQH_Int(BYTE irq, void* handler);

#endif
