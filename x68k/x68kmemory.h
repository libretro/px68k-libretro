#ifndef _WINX68K_MEMORY_H
#define _WINX68K_MEMORY_H

#include "../libretro/common.h"

#define	Memory_ReadB		cpu_readmem24
#define Memory_ReadW		cpu_readmem24_word
#define Memory_ReadD		cpu_readmem24_dword

#define	Memory_WriteB		cpu_writemem24
#define Memory_WriteW		cpu_writemem24_word
#define Memory_WriteD		cpu_writemem24_dword

extern	BYTE*	IPL;
extern	BYTE*	MEM;
extern	BYTE*	OP_ROM;
extern	BYTE*	FONT;
extern  BYTE    SCSIIPL[0x2000];
extern  BYTE    SRAM[0x4000];
extern  BYTE    GVRAM[0x80000];
extern  BYTE   TVRAM[0x80000];

extern	DWORD	BusErrFlag;
extern	DWORD	BusErrAdr;
extern	DWORD	MemByteAccess;

void Memory_ErrTrace(void);
void Memory_IntErr(int i);

void Memory_Init(void);
DWORD Memory_ReadB(DWORD adr);
DWORD Memory_ReadW(DWORD adr);
DWORD Memory_ReadD(DWORD adr);

BYTE dma_readmem24(DWORD adr);
WORD dma_readmem24_word(DWORD adr);
DWORD dma_readmem24_dword(DWORD adr);

void Memory_WriteB(DWORD adr, DWORD data);
void Memory_WriteW(DWORD adr, DWORD data);
void Memory_WriteD(DWORD adr, DWORD data);

void dma_writemem24(DWORD adr, BYTE data);
void dma_writemem24_word(DWORD adr, WORD data);
void dma_writemem24_dword(DWORD adr, DWORD data);

void cpu_setOPbase24(DWORD adr);

void Memory_SetSCSIMode(void);

#endif
