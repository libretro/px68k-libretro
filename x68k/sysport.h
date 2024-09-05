#ifndef _WINX68K_SYSPORT_H
#define _WINX68K_SYSPORT_H

#include <stdint.h>
#include "common.h"

extern	uint8_t	SysPort[7];

void SysPort_Init(void);
uint8_t FASTCALL SysPort_Read(uint32_t adr);
void FASTCALL SysPort_Write(uint32_t adr, uint8_t data);
int SysPort_StateAction(StateMem *sm, int load, int data_only);

#endif /* _WINX68K_SYSPORT_H */
