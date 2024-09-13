/*
 *  TVRAM.C - Text VRAM
 *  TODO: Transparent color processing and more
 */

#include	"common.h"
#include	"winx68k.h"
#include	"windraw.h"
#include	"bg.h"
#include	"crtc.h"
#include	"palette.h"
#include	"m68000.h"
#include	"tvram.h"

uint8_t	TVRAM[0x80000];
static uint8_t TextDrawWork[1024*1024];
uint8_t	TextDirtyLine[1024];
uint8_t	Text_TrFlag[1024];

/* pattern table */
static uint8_t TextDrawPattern[2048*4];

int TVRAM_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFARRAYN(TVRAM, 524288, "MEM_TVRAM"),
		SFARRAY(TextDrawWork, (1024 * 1024)),
		SFARRAY(TextDirtyLine, 1024),
		SFARRAY(Text_TrFlag, 1024),

		SFEND
	};
	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_TVRAM", false);

	if (load)
		TVRAM_SetAllDirty();

	return ret;
}

void TVRAM_SetAllDirty(void) { memset(TextDirtyLine, 1, 1024); }
void TVRAM_Cleanup(void)     { }

void TVRAM_Init(void)
{
	int i, j, bit;
	memset(TVRAM, 0, 0x80000);
	memset(TextDrawWork, 0, 1024*1024);
	TVRAM_SetAllDirty();

	memset(TextDrawPattern, 0, 2048*4);
	for (i=0; i<256; i++)
	{
		for (j=0, bit=0x80; j<8; j++, bit>>=1)
		{
			if (i&bit)
         {
				TextDrawPattern[i*8+j     ] = 1;
				TextDrawPattern[i*8+j+2048] = 2;
				TextDrawPattern[i*8+j+4096] = 4;
				TextDrawPattern[i*8+j+6144] = 8;
			}
		}
	}
}

uint8_t FASTCALL TVRAM_Read(uint32_t adr)
{
	adr &= 0x7ffff;
#ifndef MSB_FIRST
	adr ^= 1;
#endif
	return TVRAM[adr];
}


static INLINE void TVRAM_WriteByte(uint32_t adr, uint8_t data)
{
	if (TVRAM[adr]!=data)
	{
		TextDirtyLine[(((adr&0x1ffff)/128)-TextScrollY)&1023] = 1;
		TVRAM[adr] = data;
	}
}

static INLINE void TVRAM_WriteByteMask(uint32_t adr, uint8_t data)
{
	data = (TVRAM[adr] & CRTC_Regs[0x2e + ((adr^1) & 1)]) | (data & (~CRTC_Regs[0x2e + ((adr ^ 1) & 1)]));
	if (TVRAM[adr] != data)
	{
		TextDirtyLine[(((adr&0x1ffff)/128)-TextScrollY)&1023] = 1;
		TVRAM[adr] = data;
	}
}

void FASTCALL TVRAM_Write(uint32_t adr, uint8_t data)
{
	adr &= 0x7ffff;
#ifndef MSB_FIRST
	adr ^= 1;
#endif
	if (CRTC_Regs[0x2a]&1)			/* Concurrent access */
	{
		adr &= 0x1ffff;
		if (CRTC_Regs[0x2a]&2)		/* Text Mask */
		{
			if (CRTC_Regs[0x2b]&0x10) TVRAM_WriteByteMask(adr        , data);
			if (CRTC_Regs[0x2b]&0x20) TVRAM_WriteByteMask(adr+0x20000, data);
			if (CRTC_Regs[0x2b]&0x40) TVRAM_WriteByteMask(adr+0x40000, data);
			if (CRTC_Regs[0x2b]&0x80) TVRAM_WriteByteMask(adr+0x60000, data);
		}
		else
		{
			if (CRTC_Regs[0x2b]&0x10) TVRAM_WriteByte(adr        , data);
			if (CRTC_Regs[0x2b]&0x20) TVRAM_WriteByte(adr+0x20000, data);
			if (CRTC_Regs[0x2b]&0x40) TVRAM_WriteByte(adr+0x40000, data);
			if (CRTC_Regs[0x2b]&0x80) TVRAM_WriteByte(adr+0x60000, data);
		}
	}
	else							 /* single access */
	{
		if (CRTC_Regs[0x2a]&2)		/* Text Mask */
			TVRAM_WriteByteMask(adr, data);
		else
			TVRAM_WriteByte(adr, data);
	}
	{
		uint32_t *ptr = (uint32_t *)TextDrawPattern;
		uint32_t tvram_addr = adr & 0x1ffff;
		uint32_t workadr = ((adr & 0x1ff80) + ((adr ^ 1) & 0x7f)) << 3;
		uint8_t pat = TVRAM[tvram_addr + 0x60000];
		uint32_t t0    = ptr[(pat * 2) + 1536];
		uint32_t t1    = ptr[(pat * 2 + 1) + 1536];

		pat         = TVRAM[tvram_addr + 0x40000];
		t0 |= ptr[(pat * 2) + 1024];
		t1 |= ptr[(pat * 2 + 1) + 1024];

		pat = TVRAM[tvram_addr + 0x20000];
		t0 |= ptr[(pat * 2) + 512];
		t1 |= ptr[(pat * 2 + 1) + 512];

		pat = TVRAM[tvram_addr];
		t0 |= ptr[(pat * 2)];
		t1 |= ptr[(pat * 2 + 1)];

		*((uint32_t *)&TextDrawWork[workadr]) = t0;
		*(((uint32_t *)(&TextDrawWork[workadr])) + 1) = t1;
	}
}

void FASTCALL TVRAM_RCUpdate(void)
{
	uint32_t adr = ((uint32_t)CRTC_Regs[0x2d]<<9);

	/* XXX: BUG */
	uint32_t *ptr = (uint32_t *)TextDrawPattern;
	uint32_t *wptr = (uint32_t *)(TextDrawWork + (adr << 3));
	uint32_t t0, t1;
	uint32_t tadr;
	uint8_t pat;
	int i;

	for (i = 0; i < 512; i++, adr++)
   {
		tadr = adr ^ 1;

		pat = TVRAM[tadr + 0x60000];
		t0 = ptr[(pat * 2) + 1536];
		t1 = ptr[(pat * 2 + 1) + 1536];

		pat = TVRAM[tadr + 0x40000];
		t0 |= ptr[(pat * 2) + 1024];
		t1 |= ptr[(pat * 2 + 1) + 1024];

		pat = TVRAM[tadr + 0x20000];
		t0 |= ptr[(pat * 2) + 512];
		t1 |= ptr[(pat * 2 + 1) + 512];

		pat = TVRAM[tadr];
		t0 |= ptr[(pat * 2)];
		t1 |= ptr[(pat * 2 + 1)];

		*wptr++ = t0;
		*wptr++ = t1;
	}
}

void FASTCALL Text_DrawLine(int opaq)
{
	uint32_t addr;
	uint32_t x;
	uint32_t off = 16;
	uint32_t i;
	uint8_t t;
	uint32_t y = TextScrollY + VLINE;
	if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
		y += VLINE;
	y = (y & 0x3ff) << 10;

	x = TextScrollX & 0x3ff;
	addr = x + y;
	x = (x ^ 0x3ff) + 1;

	if (opaq) {
		for (i = 0; (i < TextDotX) && (x > 0); i++, x--, off++) {
			t = TextDrawWork[addr++] & 0xf;
			Text_TrFlag[off] = t ? 1 : 0;
			BG_LineBuf[off] = TextPal[t];
		}
		if (i++ != TextDotX) {
			for (; i < TextDotX; i++, off++) {
				BG_LineBuf[off] = TextPal[0];
				Text_TrFlag[off] = 0;
			}
		}
	} else {
		for (i = 0; (i < TextDotX) && (x > 0); i++, x--, off++) {
			t = TextDrawWork[addr++] & 0xf;
			if (t) {
				Text_TrFlag[off] |= 1;
				BG_LineBuf[off] = TextPal[t];
			}
		}
	}
}
