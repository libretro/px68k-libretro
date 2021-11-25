#ifndef _WINX68K_IOC
#define _WINX68K_IOC

#include "common.h"

extern	BYTE	IOC_IntStat;
extern	BYTE	IOC_IntVect;

void IOC_Init(void);
BYTE FASTCALL IOC_Read(DWORD adr);
void FASTCALL IOC_Write(DWORD adr, BYTE data);

#endif
