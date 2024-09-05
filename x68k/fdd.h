#ifndef _WINX68K_FDD_H
#define _WINX68K_FDD_H

#include <stdint.h>
#include "common.h"

typedef struct
{
	uint8_t c;
	uint8_t h;
	uint8_t r;
	uint8_t n;
} FDCID;

enum
{
	FD_Non = 0,
	FD_XDF,
	FD_D88,
	FD_DIM
};

void FDD_SetFD(int drive, char* filename, int readonly);
void FDD_EjectFD(int drive);
void FDD_Init(void);
void FDD_Cleanup(void);
void FDD_Reset(void);
void FDD_SetFDInt(void);
int FDD_Seek(int drv, int trk, FDCID* id);
int FDD_ReadID(int drv, FDCID* id);
int FDD_WriteID(int drv, int trk, uint8_t* buf, int num);
int FDD_Read(int drv, FDCID* id, uint8_t* buf);
int FDD_ReadDiag(int drv, FDCID* id, FDCID* retid, uint8_t* buf);
int FDD_Write(int drv, FDCID* id, uint8_t* buf, int del);
int FDD_IsReady(int drv);
int FDD_IsReadOnly(int drv);
int FDD_GetCurrentID(int drv, FDCID* id);
void FDD_SetReadOnly(int drv);
void FDD_SetEMask(int drive, int emask);
void FDD_SetAccess(int drive);
void FDD_SetBlink(int drive, int blink);
int FDD_StateAction(StateMem *sm, int load, int data_only);

/* Misc: Used to trigger rumble when FDD is reading data.
 * Reset at every frame */
extern int FDD_IsReading;

#endif /* _WINX68K_FDD_H */
