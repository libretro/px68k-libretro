#ifndef _WINX68K_PIA
#define _WINX68K_PIA

#include "common.h"

extern	BYTE	PIA_PortA;
extern	BYTE	PIA_PortB;
extern	BYTE	PIA_PortC;

void PIA_Init(void);
BYTE FASTCALL PIA_Read(DWORD adr);
void FASTCALL PIA_Write(DWORD adr, BYTE data);

#endif
