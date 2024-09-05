#ifndef _WINX68K_BG_H
#define _WINX68K_BG_H

#include <stdint.h>
#include "common.h"

extern	uint8_t	BG_DrawWork0[1024*1024];
extern	uint8_t	BG_DrawWork1[1024*1024];
extern	uint8_t	BG_Regs[0x12];
extern	int32_t	BG_HAdjust;
extern	int32_t	BG_VLINE;
extern	uint32_t VLINEBG;

extern	uint8_t	Sprite_DrawWork[1024*1024];
extern	uint16_t BG_LineBuf[1600];

void BG_Init(void);

uint8_t FASTCALL BG_Read(uint32_t adr);
void FASTCALL BG_Write(uint32_t adr, uint8_t data);

void FASTCALL BG_DrawLine(int opaq, int gd);

int BG_StateAction(StateMem *sm, int load, int data_only);

#endif /* _WINX68K_BG_H */
