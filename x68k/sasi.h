#ifndef _WINX68K_SASI
#define _WINX68K_SASI

#include <stdint.h>
#include "common.h"

void SASI_Init(void);
BYTE FASTCALL SASI_Read(DWORD adr);
void FASTCALL SASI_Write(DWORD adr, BYTE data);
int SASI_IsReady(void);

#endif /*_WINX68K_SASI */
