#ifndef _WINX68K_SRAM_H
#define _WINX68K_SRAM_H

#include "common.h"

extern	uint8_t	SRAM[0x4000];

void SRAM_Init(void);
void SRAM_Cleanup(void);
void SRAM_VirusCheck(void);

void SRAM_UpdateBoot(void);
void SRAM_WriteEnable(int enable);

uint8_t FASTCALL SRAM_Read(uint32_t adr);
void FASTCALL SRAM_Write(uint32_t adr, uint8_t data);
int SRAM_StateAction(StateMem *sm, int load, int data_only);

#endif /* _WINX68K_SRAM_H */
