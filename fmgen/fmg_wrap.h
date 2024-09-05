#ifndef _WIN68_OPM_FMGEN_H
#define _WIN68_OPM_FMGEN_H

#include <stdint.h>

int OPM_Init(int clock);
void OPM_Cleanup(void);
void OPM_Reset(void);
void OPM_Update(int16_t *buffer, int length, uint8_t *pbsp, uint8_t *pbep);
void FASTCALL OPM_Write(uint32_t r, uint8_t v);
uint8_t FASTCALL OPM_Read(void);
void FASTCALL OPM_Timer(uint32_t step);
void OPM_SetVolume(uint8_t vol);
int OPM_StateAction(StateMem *sm, int load, int data_only);

int M288_Init(int clock, const char* path);
void M288_Cleanup(void);
void M288_Reset(void);
void M288_Update(int16_t *buffer, size_t length);
void FASTCALL M288_Write(uint32_t r, uint8_t v);
uint8_t FASTCALL M288_Read(uint16_t a);
void FASTCALL M288_Timer(uint32_t step);
void M288_SetVolume(uint8_t vol);
void M288_RomeoOut(unsigned int delay);

#endif /* _WIN68_OPM_FMGEN_H */
