#ifndef _X68K_RTC_H
#define _X68K_RTC_H

#include <stdint.h>

void RTC_Init(void);
uint8_t FASTCALL RTC_Read(uint32_t adr);
void FASTCALL RTC_Write(uint32_t adr, uint8_t data);
void RTC_Timer(int clock);
int RTC_StateAction(StateMem *sm, int load, int data_only);

#endif /* _X68K_RTC_H */
