#ifndef _WINX68K_MEMORY_H
#define _WINX68K_MEMORY_H

#include <stdint.h>
#include "../libretro/common.h"

extern	uint8_t*	IPL;
extern	uint8_t*  	MEM;
extern	uint8_t*	FONT;
extern  uint8_t    SCSIIPL[0x2000];
extern  uint8_t    SRAM[0x4000];
extern  uint8_t    GVRAM[0x80000];
extern  uint8_t   TVRAM[0x80000];

extern	uint32_t	BusErrFlag;

void Memory_Init(void);

uint32_t cpu_readmem24(uint32_t adr);
uint32_t cpu_readmem24_word(uint32_t adr);
uint32_t cpu_readmem24_dword(uint32_t adr);

void cpu_writemem24(uint32_t adr, uint32_t data);
void cpu_writemem24_word(uint32_t adr, uint32_t data);
void cpu_writemem24_dword(uint32_t adr, uint32_t data);

uint8_t dma_readmem24(uint32_t adr);
uint16_t dma_readmem24_word(uint32_t adr);
uint32_t dma_readmem24_dword(uint32_t adr);

void dma_writemem24(uint32_t adr, uint8_t data);
void dma_writemem24_word(uint32_t adr, uint16_t data);
void dma_writemem24_dword(uint32_t adr, uint32_t data);

void Memory_SetSCSIMode(void);

#endif
