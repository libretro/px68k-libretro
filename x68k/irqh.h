#ifndef _WINX68K_IRQ_H
#define _WINX68K_IRQ_H

#include <stdint.h>
#include "common.h"

void IRQH_Init(void);
void IRQH_IRQCallBack(uint8_t irq);
void IRQH_Int(uint8_t irq, void* handler);
int IRQH_StateAction(StateMem *sm, int load, int data_only);

#endif /* WINX68K_IRQ_H */
