#ifndef _WINX68K_SCC_H
#define _WINX68K_SCC_H

#include <stdint.h>
#include "common.h"

void SCC_IntCheck(void);
void SCC_Init(void);
uint8_t FASTCALL SCC_Read(uint32_t adr);
void FASTCALL SCC_Write(uint32_t adr, uint8_t data);
int SCC_StateAction(StateMem *sm, int load, int data_only);

extern int8_t MouseX;
extern int8_t MouseY;
extern uint8_t MouseSt;

#endif /* _WINX68K_SCC_H */
