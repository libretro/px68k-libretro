#ifndef _WINX68K_PAL_H
#define _WINX68K_PAL_H

#include <stdint.h>
#include "common.h"

extern uint8_t	Pal_Regs[1024];
extern uint16_t TextPal[256];
extern uint16_t GrphPal[256];
extern uint16_t Pal16[65536];

void Pal_SetColor(void);
void Pal_Init(void);

uint8_t FASTCALL Pal_Read(uint32_t adr);
void FASTCALL Pal_Write(uint32_t adr, uint8_t data);
void Pal_ChangeContrast(int num);
int Pal_StateAction(StateMem *sm, int load, int data_only);

extern uint16_t Ibit, Pal_HalfMask, Pal_Ix2;

#endif /* _WINX68K_PAL_H */
