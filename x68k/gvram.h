#ifndef _WINX68K_GVRAM_H
#define _WINX68K_GVRAM_H

#include <stdint.h>
#include "common.h"

extern	uint8_t	GVRAM[0x80000];
extern	uint16_t	Grp_LineBuf[1024];
extern	uint16_t	Grp_LineBufSP[1024];
extern	uint16_t	Grp_LineBufSP2[1024];

void GVRAM_Init(void);

void FASTCALL GVRAM_FastClear(void);

uint8_t FASTCALL GVRAM_Read(uint32_t adr);
void FASTCALL GVRAM_Write(uint32_t adr, uint8_t data);

void Grp_DrawLine16(void);
void FASTCALL Grp_DrawLine8(int page, int opaq);
void FASTCALL Grp_DrawLine4(uint32_t page, int opaq);
void FASTCALL Grp_DrawLine4h(void);
void FASTCALL Grp_DrawLine16SP(void);
void FASTCALL Grp_DrawLine8SP(int page/*, int opaq*/);
void FASTCALL Grp_DrawLine4SP(uint32_t page/*, int opaq*/);
void FASTCALL Grp_DrawLine4hSP(void);
void FASTCALL Grp_DrawLine8TR(int page, int opaq);
void FASTCALL Grp_DrawLine8TR_GT(int page, int opaq);
void FASTCALL Grp_DrawLine4TR(uint32_t page, int opaq);
int GVRAM_StateAction(StateMem *sm, int load, int data_only);

#endif /* _WINX68K_GVRAM_H */
