#ifndef _WINX68K_IOC_H
#define _WINX68K_IOC_H

#include <stdint.h>
#include "common.h"

extern	uint8_t	IOC_IntStat;
extern	uint8_t	IOC_IntVect;

void IOC_Init(void);
uint8_t FASTCALL IOC_Read(uint32_t adr);
void FASTCALL IOC_Write(uint32_t adr, uint8_t data);
int IOC_StateAction(StateMem *sm, int load, int data_only);

#endif /* WINX68K_IOC_H */
