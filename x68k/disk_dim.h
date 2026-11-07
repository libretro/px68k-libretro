#ifndef _WINX68K_DIM_H
#define _WINX68K_DIM_H

#include <stdint.h>

void DIM_Init(void);
void DIM_Cleanup(void);
int DIM_SetFD(int drive, char* filename);
int DIM_Eject(int drive);
int DIM_Seek(int drv, int trk, FDCID* id);
int DIM_ReadID(int drv, FDCID* id);
int DIM_WriteID(int drv, int trk, uint8_t* buf, int num);
int DIM_Read(int drv, FDCID* id, uint8_t* buf);
int DIM_ReadDiag(int drv, FDCID* id, FDCID* retid, uint8_t* buf);
int DIM_Write(int drv, FDCID* id, uint8_t* buf, int del);
int DIM_GetCurrentID(int drv, FDCID* id);
void DIM_GetCursor(int drv, int32_t *trk, int32_t *cur);
void DIM_SetCursor(int drv, int32_t trk, int32_t cur);

#endif /* _WINX68K_DIM_H */
