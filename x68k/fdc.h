#ifndef _WINX68K_FDC_H
#define _WINX68K_FDC_H

#include <stdint.h>
#include "common.h"

void FDC_Init(void);
uint8_t FASTCALL FDC_Read(uint32_t adr);
void FASTCALL FDC_Write(uint32_t adr, uint8_t data);
int16_t FDC_Flush(void);
void FDC_SetForceReady(int n);
int FDC_IsDataReady(void);
int FDC_StateAction(StateMem *sm, int load, int data_only);

#endif /* _WINX68K_FDC_H */
