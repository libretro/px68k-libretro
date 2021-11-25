#ifndef _WINX68K_SYSPORT
#define _WINX68K_SYSPORT

#include "common.h"

extern	BYTE	SysPort[7];

void SysPort_Init(void);
BYTE FASTCALL SysPort_Read(DWORD adr);
void FASTCALL SysPort_Write(DWORD adr, BYTE data);

#endif
