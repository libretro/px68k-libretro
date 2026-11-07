#ifndef _WINX68K_D88_H
#define _WINX68K_D88_H

#include <stdint.h>

typedef struct
{			                     /* Header Part (size:2B0h) */
	uint8_t	fd_name[17];		/* Disk Name */
	uint8_t	reserved1[9]; 		/* Reserved */
	uint8_t	protect;		      /* Write Protect bit:4 */
	uint8_t	fd_type;		      /* Disk Format */
	uint32_t	fd_size;	         /* Disk Size */
	uint32_t	trackp[164];	   /* Track_pointer */
} D88_HEADER;

void D88_Init(void);
void D88_Cleanup(void);
int D88_SetFD(int drive, char* filename);
int D88_Eject(int drive);
int D88_Seek(int drv, int trk, FDCID* id);
int D88_ReadID(int drv, FDCID* id);
int D88_WriteID(int drv, int trk, uint8_t* buf, int num);
int D88_Read(int drv, FDCID* id, uint8_t* buf);
int D88_ReadDiag(int drv, FDCID* id, FDCID* retid, uint8_t* buf);
int D88_Write(int drv, FDCID* id, uint8_t* buf, int del);
int D88_GetCurrentID(int drv, FDCID* id);
void D88_GetCursor(int drv, int32_t *trk, int32_t *idx);
void D88_SetCursor(int drv, int32_t trk, int32_t idx);

#endif /* WINX68K_D88_H */
