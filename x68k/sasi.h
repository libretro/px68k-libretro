#ifndef _WINX68K_SASI_H
#define _WINX68K_SASI_H

#include <stdint.h>
#include "common.h"

void SASI_Init(void);
uint8_t FASTCALL SASI_Read(uint32_t adr);
void FASTCALL SASI_Write(uint32_t adr, uint8_t data);
int SASI_IsReady(void);
int SASI_StateAction(StateMem *sm, int load, int data_only);

#endif /* _WINX68K_SASI_H */
