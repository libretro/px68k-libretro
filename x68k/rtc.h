#ifndef _X68K_RTC
#define _X68K_RTC

void RTC_Init(void);
BYTE FASTCALL RTC_Read(DWORD adr);
void FASTCALL RTC_Write(DWORD adr, BYTE data);
void RTC_Timer(int clock);

#endif
