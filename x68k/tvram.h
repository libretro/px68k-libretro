#ifndef _WINX68K_TVRAM
#define _WINX68K_TVRAM

#include "common.h"

extern	BYTE	TVRAM[0x80000];
extern	BYTE	TextDrawWork[1024*1024];
extern	BYTE	TextDirtyLine[1024];
extern	BYTE	Text_TrFlag[1024];

void TVRAM_SetAllDirty(void);

void TVRAM_Init(void);
void TVRAM_Cleanup(void);

BYTE FASTCALL TVRAM_Read(DWORD adr);
void FASTCALL TVRAM_Write(DWORD adr, BYTE data);
void FASTCALL TVRAM_RCUpdate(void);
void FASTCALL Text_DrawLine(int opaq);

#endif
