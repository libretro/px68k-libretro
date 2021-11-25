#ifndef _WINX68K_SCC
#define _WINX68K_SCC

#include "common.h"

void SCC_IntCheck(void);
void SCC_Init(void);
BYTE FASTCALL SCC_Read(DWORD adr);
void FASTCALL SCC_Write(DWORD adr, BYTE data);

extern signed char MouseX;
extern signed char MouseY;
extern BYTE MouseSt;

#endif
