#ifndef _WINX68K_FDC
#define _WINX68K_FDC

#include <stdint.h>

#include "common.h"

void FDC_Init(void);
BYTE FASTCALL FDC_Read(DWORD adr);
void FASTCALL FDC_Write(DWORD adr, BYTE data);
int16_t FDC_Flush(void);
void FDC_EPhaseEnd(void);
void FDC_SetForceReady(int n);
int FDC_IsDataReady(void);

#endif /* _WINX68K_FDC */

