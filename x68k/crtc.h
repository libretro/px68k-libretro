#ifndef _WINX68K_CRTC_H
#define _WINX68K_CRTC_H

#include <stdint.h>
#include "common.h"

#define	VSYNC_HIGH	180310L
#define	VSYNC_NORM	162707L

extern	uint8_t	CRTC_Regs[48];
extern	uint8_t	CRTC_Mode;
extern	uint16_t CRTC_VSTART, CRTC_VEND;
extern	uint16_t CRTC_HSTART, CRTC_HEND;
extern	uint32_t TextDotX, TextDotY;
extern	uint32_t TextScrollX, TextScrollY;
extern	uint8_t	VCReg0[2];
extern	uint8_t	VCReg1[2];
extern	uint8_t	VCReg2[2];
extern	uint16_t CRTC_IntLine;
extern	uint8_t	CRTC_FastClr;
extern	uint8_t	CRTC_DispScan;
extern	uint32_t CRTC_FastClrLine;
extern	uint16_t CRTC_FastClrMask;
extern	uint8_t	CRTC_VStep;
extern  int	HSYNC_CLK;

extern	uint32_t GrphScrollX[4];
extern	uint32_t GrphScrollY[4];

void CRTC_Init(void);

void CRTC_RasterCopy(void);

uint8_t FASTCALL CRTC_Read(uint32_t adr);
void FASTCALL CRTC_Write(uint32_t adr, uint8_t data);

uint8_t FASTCALL VCtrl_Read(uint32_t adr);
void FASTCALL VCtrl_Write(uint32_t adr, uint8_t data);
int CRTC_StateAction(StateMem *sm, int load, int data_only);

#endif /* _WINX68K_CRTC_H */
