/*
 *  GVRAM.C - Graphic VRAM
 */

#include	"common.h"
#include	"windraw.h"
#include	"winx68k.h"
#include	"crtc.h"
#include	"palette.h"
#include	"tvram.h"
#include	"gvram.h"
#include	"m68000.h"
#include	<string.h>

uint8_t	GVRAM[0x80000];
uint16_t	Grp_LineBuf[1024];
uint16_t	Grp_LineBufSP[1024];		/* Special priority/semi-transparent buffer */
uint16_t	Grp_LineBufSP2[1024];		/* Buffer for semi-transparent base plane (stores non-semi-transparent bits) */
static uint16_t	Grp_LineBufSP_Tr[1024];
static uint16_t	Pal16Adr[256];			/* Buffer for semi-transparent base plane (stores non-semi-transparent bits) */

int GVRAM_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFARRAYN(GVRAM, 524288, "MEM_GVRAM"),
		SFARRAY16(Grp_LineBuf, 1024),
		SFARRAY16(Grp_LineBufSP, 1024),
		SFARRAY16(Grp_LineBufSP2, 1024),
		SFARRAY16(Grp_LineBufSP_Tr, 1024),
		SFARRAY16(Pal16Adr, 256),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_GVRAM", false);

	return ret;
}

#ifdef MSB_FIRST
#define GET_WORD_W8(src) (*(uint16_t *)(src))
#else
#define GET_WORD_W8(src) (*(uint8_t *)(src) | *((uint8_t *)(src) + 1) << 8)
#endif

void GVRAM_Init(void)
{
	int i;

	memset(GVRAM, 0, 0x80000);
	for (i=0; i<128; i++) /* For 16bit color palette address calculation */
	{
		Pal16Adr[i*2] = i*4;
		Pal16Adr[i*2+1] = i*4+1;
	}
}

void FASTCALL GVRAM_FastClear(void)
{
	uint32_t v = ((CRTC_Regs[0x29]&4)?512:256);
	uint32_t h = ((CRTC_Regs[0x29]&3)?512:256);

	uint16_t *p;
	uint32_t x, y, offset;

	uint32_t w[2];

	w[0] = h;
	w[1] = 0;

	if (((GrphScrollX[0] & 0x1ff) + w[0]) > 512) {
		w[1] = (GrphScrollX[0] & 0x1ff) + w[0] - 512;
		w[0] = 512 - (GrphScrollX[0] & 0x1ff);
	}

	for (y = 0; y < v; y++) {
		offset = ((y + GrphScrollY[0]) & 0x1ff) << 10;
		p = (uint16_t *)(GVRAM + offset + ((GrphScrollX[0] & 0x1ff) * 2));

		for (x = 0; x < w[0]; x++) {
			*p++ &= CRTC_FastClrMask;
		}

		if (w[1] > 0) {
			p = (uint16_t *)(GVRAM + offset);
			for (x = 0; x < w[1]; x++) {
				*p++ &= CRTC_FastClrMask;
			}
		}
	}
}

uint8_t FASTCALL GVRAM_Read(uint32_t adr)
{
	int type;

	adr &= 0x1fffff;

	if (CRTC_Regs[0x28] & 8)
		type = 4;
	else if (CRTC_Regs[0x28] & 4)
		type = 0;
	else
		type = (CRTC_Regs[0x28] & 3) + 1;

	switch (type)
	{
	case 0: /* 1024 dot, 16 colors */
		if ((adr & 1) == 0)
			return 0;

		if (adr & 0x100000)
		{
			if (adr & 0x400)
			{
				/* page 3 */
				adr = ((adr >> 1) & 0x7fc00) | (adr & 0x3ff);
				return (GVRAM[adr] >> 4);
			}
			else
			{
				/* page 2 */
				adr = ((adr >> 1) & 0x7fc00) | (adr & 0x3ff);
				return (GVRAM[adr] & 0x0f);
			}
		}
		else
		{
			if (adr & 0x400)
			{
				/* page 1 */
				adr = ((adr >> 1) & 0x7fc00) | (adr & 0x3ff);
				return (GVRAM[adr ^ 1] >> 4);
			}
			else
			{
				/* page 0 */
				adr = ((adr >> 1) & 0x7fc00) | (adr & 0x3ff);
				return (GVRAM[adr ^ 1] & 0x0f);
			}
		}
		break;

	case 1: /* 512 dot, 16 colors */
		if ((adr & 1) == 0)
			return 0;

		if (adr < 0x80000)
		{
			/* page 0: Low byte of word b0-b3 */
			return (GVRAM[adr ^ 1] & 0x0f);
		}

		if (adr < 0x100000)
		{
			/* page 1: Low byte of word b4-b7 */
			adr &= 0x7ffff;
			return (GVRAM[adr ^ 1] >> 4);
		}

		if (adr < 0x180000)
		{
			/* page 2: High byte of word b0-b3 */
			adr &= 0x7ffff;
			return (GVRAM[adr] & 0x0f);
		}

		/* page 3: High byte of word b4-b7 */
		adr &= 0x7ffff;
		return (GVRAM[adr] >> 4);

	case 2: /* 512 dot, 256 colors */
	case 3: /* unknown */
	    /* page 0 */
		if (adr < 0x80000)
		{
			if (adr & 1)
			{
				/* Low byte of word */
				return GVRAM[adr ^ 1];
			}
			return 0;
		}

		/* page 1 */
		if (adr < 0x100000)
		{
			adr &= 0x7ffff;
			if (adr & 1)
			{
				/* High byte of word */
				return GVRAM[adr];
			}
			return 0;
		}
#if 0
		else
		{
			/* bus error */
			BusErrFlag = 1;
			return 0xff;
		}
#endif
		break;
		

	case 4: /* 65536 */
		if (adr < 0x80000) {
			return GVRAM[adr ^ 1];
		}
#if 0
		else
		{
			/* bus error */
			BusErrFlag = 1;
			return 0xff;
		}
#endif
		break;
		
	}

	return 0;
}

void FASTCALL GVRAM_Write(uint32_t adr, uint8_t data)
{
	int line = 1023, scr = 0;
	uint32_t temp;
	int type;

	adr &= 0x1fffff;

	if (CRTC_Regs[0x28] & 8)
		type = 4;
	else if (CRTC_Regs[0x28] & 4)
		type = 0;
	else
		type = (CRTC_Regs[0x28] & 3) + 1;

	switch (type)
	{
	case 0: /* 1024 dot, 16 colors */
		if ((adr & 1) == 0)
			break;

		line = ((adr / 2048) - GrphScrollY[0]) & 1023;

		if (adr & 0x100000)
		{
			if (adr & 0x400)
			{
				adr = ((adr & 0xff800) >> 1) + (adr & 0x3ff);
				temp = GVRAM[adr] & 0x0f;
				temp |= (data & 0x0f) << 4;
				GVRAM[adr] = (uint8_t)temp;
			}
			else
			{
				adr = ((adr & 0xff800) >> 1) + (adr & 0x3ff);
				temp = GVRAM[adr] & 0xf0;
				temp |= data & 0x0f;
				GVRAM[adr] = (uint8_t)temp;
			}
		}
		else
		{
			if (adr & 0x400)
			{
				adr = ((adr & 0xff800) >> 1) + (adr & 0x3ff);
				temp = GVRAM[adr ^ 1] & 0x0f;
				temp |= (data & 0x0f) << 4;
				GVRAM[adr ^ 1] = (uint8_t)temp;
			}
			else
			{
				adr = ((adr & 0xff800) >> 1) + (adr & 0x3ff);
				temp = GVRAM[adr ^ 1] & 0xf0;
				temp |= data & 0x0f;
				GVRAM[adr ^ 1] = (uint8_t)temp;
			}
		}
		break;

	case 1: /* 16 colors */
		if ((adr & 1) == 0)
			break;

		scr = GrphScrollY[(adr >> 19) & 3];
		line = (((adr & 0x7ffff) >> 10) - scr) & 511;

		if (adr < 0x80000)
		{
			/* page 0: low byte of word b0-b3 */
			temp = (GVRAM[adr ^ 1] & 0xf0);
			temp |= (data & 0x0f);
			GVRAM[adr ^ 1] = (uint8_t)temp;
		}
		else if (adr < 0x100000)
		{
			/* page 1: low byte of word b4-b7 */
			adr &= 0x7ffff;
			temp = (GVRAM[adr ^ 1] & 0x0f);
			temp |= (data << 4);
			GVRAM[adr ^ 1] = (uint8_t)temp;
		}
		else if (adr < 0x180000)
		{
			/* page 2: high byte of word b0-b3 */
			adr &= 0x7ffff;
			temp = (GVRAM[adr] & 0xf0);
			temp |= (data & 0x0f);
			GVRAM[adr] = (uint8_t)temp;
		}
		else
		{
			/* page 3: high byte of word b4-b7 */
			adr &= 0x7ffff;
			temp = (GVRAM[adr] & 0x0f);
			temp |= (data << 4);
			GVRAM[adr] = (uint8_t)temp;
		}
		break;

	case 2: /* 256 colors */
	case 3: /* unknown */
		if ((adr & 1) == 0)
			break;

		if (adr < 0x100000)
		{
			scr = GrphScrollY[(adr >> 18) & 2];
			line = (((adr & 0x7ffff) >> 10) - scr) & 511;

			TextDirtyLine[line] = 1; /* When used like 32 colors, 4 sides */

			scr = GrphScrollY[((adr >> 18) & 2) + 1];
			line = (((adr & 0x7ffff) >> 10) - scr) & 511;

			/* page 0 */
			if (adr < 0x80000)
			{
				/* low byte of word */
				GVRAM[adr ^ 1] = (uint8_t)data;
			}
			/* page 1 */
			else
			{
				/* high byte of word */
				adr &= 0x7ffff;
				GVRAM[adr] = (uint8_t)data;
			}
		}
#if 0
		/* TODO: */
		else
		{
			BusErrFlag = 1;
			return;
		}
#endif
		break;

	case 4: /* 65536 */
		if (adr < 0x80000)
		{
			line = (((adr & 0x7ffff) >> 10) - GrphScrollY[0]) & 511;
			GVRAM[adr ^ 1] = (uint8_t)data;
		}
#if 0
		/* TODO: */
		else
		{
			BusErrFlag = 1;
			return;
		}
#endif
		break;
	}

	TextDirtyLine[line] = 1;
}


/*
 *   From here on, the screen will be expanded line by line.
 */
void Grp_DrawLine16(void)
{
	uint16_t *srcp, *destp;
	uint32_t x;
	uint32_t i;
	uint16_t v, v0;
	uint32_t y = GrphScrollY[0] + VLINE;
	if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
		y += VLINE;
	y = (y & 0x1ff) << 10;

	x = GrphScrollX[0] & 0x1ff;
	srcp = (uint16_t *)(GVRAM + y + x * 2);
	destp = (uint16_t *)Grp_LineBuf;

	x = (x ^ 0x1ff) + 1;

	v = v0 = 0;
	i = 0;
	if (x < TextDotX) {
		for (; i < x; ++i) {
			v = *srcp++;
			if (v != 0) {
				v0 = (v >> 8) & 0xff;
				v &= 0x00ff;

				v = Pal_Regs[Pal16Adr[v]];
				v |= Pal_Regs[Pal16Adr[v0] + 2] << 8;
				v = Pal16[v];
			}
			*destp++ = v;
		}
		srcp -= 0x200;
	}

	for (; i < TextDotX; ++i) {
		v = *srcp++;
		if (v != 0) {
			v0 = (v >> 8) & 0xff;
			v &= 0x00ff;

			v = Pal_Regs[Pal16Adr[v]];
			v |= Pal_Regs[Pal16Adr[v0] + 2] << 8;
			v = Pal16[v];
		}
		*destp++ = v;
	}
}

void FASTCALL Grp_DrawLine8(int page, int opaq)
{
	uint16_t *srcp, *destp;
	uint32_t x, x0;
	uint32_t y, y0;
	uint32_t off;
	uint32_t i;
	uint16_t v;

	page &= 1;

	y = GrphScrollY[page * 2] + VLINE;
	y0 = GrphScrollY[page * 2 + 1] + VLINE;
	if ((CRTC_Regs[0x29] & 0x1c) == 0x1c) {
		y += VLINE;
		y0 += VLINE;
	}
	y = ((y & 0x1ff) << 10) + page;
	y0 = ((y0 & 0x1ff) << 10) + page;

	x = GrphScrollX[page * 2] & 0x1ff;
	x0 = GrphScrollX[page * 2 + 1] & 0x1ff;

	off = y0 + x0 * 2;
	srcp = (uint16_t *)(GVRAM + y + x * 2);
	destp = (uint16_t *)Grp_LineBuf;

	x = (x ^ 0x1ff) + 1;

	v = 0;
	i = 0;

	if (opaq) {
		if (x < TextDotX) {
			for (; i < x; ++i) {
				v = GET_WORD_W8(srcp);
				srcp++;
				v = GrphPal[(GVRAM[off] & 0xf0) | (v & 0x0f)];
				*destp++ = v;

				off += 2;
				if ((off & 0x3fe) == 0x000)
					off -= 0x400;
			}
			srcp -= 0x200;
		}

		for (; i < TextDotX; ++i) {
			v = GET_WORD_W8(srcp);
			srcp++;
			v = GrphPal[(GVRAM[off] & 0xf0) | (v & 0x0f)];
			*destp++ = v;

			off += 2;
			if ((off & 0x3fe) == 0x000)
				off -= 0x400;
		}
	} else {
		if (x < TextDotX) {
			for (; i < x; ++i) {
				v = GET_WORD_W8(srcp);
				srcp++;
				v = (GVRAM[off] & 0xf0) | (v & 0x0f);
				if (v != 0x00)
					*destp = GrphPal[v];
				destp++;

				off += 2;
				if ((off & 0x3fe) == 0x000)
					off -= 0x400;
			}
			srcp -= 0x200;
		}

		for (; i < TextDotX; ++i) {
			v = GET_WORD_W8(srcp);
			srcp++;
			v = (GVRAM[off] & 0xf0) | (v & 0x0f);
			if (v != 0x00)
				*destp = GrphPal[v];
			destp++;

			off += 2;
			if ((off & 0x3fe) == 0x000)
				off -= 0x400;
		}
	}
}

/* Manhattan Requiem Opening 7.0ｿｿ7.5MHz */
void FASTCALL Grp_DrawLine4(uint32_t page, int opaq)
{
	uint16_t *srcp, *destp;	/* XXX: ALIGN */
	uint32_t x, y;
	uint32_t off;
	uint32_t i;
	uint16_t v;

	page &= 3;

	y = GrphScrollY[page] + VLINE;
	if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
		y += VLINE;
	y = (y & 0x1ff) << 10;

	x = GrphScrollX[page] & 0x1ff;
	off = y + x * 2;

	x ^= 0x1ff;

	srcp = (uint16_t *)(GVRAM + off + (page >> 1));
	destp = (uint16_t *)Grp_LineBuf;

	v = 0;
	i = 0;

	if (page & 1) {
		if (opaq) {
			if (x < TextDotX) {
				for (; i < x; ++i) {
					v = GET_WORD_W8(srcp);
					srcp++;
					v = GrphPal[(v >> 4) & 0xf];
					*destp++ = v;
				}
				srcp -= 0x200;
			}
			for (; i < TextDotX; ++i) {
				v = GET_WORD_W8(srcp);
				srcp++;
				v = GrphPal[(v >> 4) & 0xf];
				*destp++ = v;
			}
		} else {
			if (x < TextDotX) {
				for (; i < x; ++i) {
					v = GET_WORD_W8(srcp);
					srcp++;
					v = (v >> 4) & 0x0f;
					if (v != 0x00)
						*destp = GrphPal[v];
					destp++;
				}
				srcp -= 0x200;
			}
			for (; i < TextDotX; ++i) {
				v = GET_WORD_W8(srcp);
				srcp++;
				v = (v >> 4) & 0x0f;
				if (v != 0x00)
					*destp = GrphPal[v];
				destp++;
			}
		}
	} else {
		if (opaq) {
			if (x < TextDotX) {
				for (; i < x; ++i) {
					v = GET_WORD_W8(srcp);
					srcp++;
					v = GrphPal[v & 0x0f];
					*destp++ = v;
				}
				srcp -= 0x200;
			}
			for (; i < TextDotX; ++i) {
				v = GET_WORD_W8(srcp);
				srcp++;
				v = GrphPal[v & 0x0f];
				*destp++ = v;
			}
		} else {
			if (x < TextDotX) {
				for (; i < x; ++i) {
					v = GET_WORD_W8(srcp);
					srcp++;
					v &= 0x0f;
					if (v != 0x00)
						*destp = GrphPal[v];
					destp++;
				}
				srcp -= 0x200;
			}
			for (; i < TextDotX; ++i) {
				v = GET_WORD_W8(srcp);
				srcp++;
				v &= 0x0f;
				if (v != 0x00)
					*destp = GrphPal[v];
				destp++;
			}
		}
	}
}

void FASTCALL Grp_DrawLine4h(void)
{
	uint16_t *srcp, *destp;
	uint32_t x, y;
	uint32_t i;
	uint16_t v;
	int bits;

	y = GrphScrollY[0] + VLINE;
	if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
		y += VLINE;
	y &= 0x3ff;

	if ((y & 0x200) == 0x000) {
		y <<= 10;
		bits = (GrphScrollX[0] & 0x200) ? 4 : 0;
	} else {
		y = (y & 0x1ff) << 10;
		bits = (GrphScrollX[0] & 0x200) ? 12 : 8;
	}

	x = GrphScrollX[0] & 0x1ff;
	srcp = (uint16_t *)(GVRAM + y + x * 2);
	destp = (uint16_t *)Grp_LineBuf;

	x = ((x & 0x1ff) ^ 0x1ff) + 1;

	for (i = 0; i < TextDotX; ++i) {
		v = *srcp++;
		*destp++ = GrphPal[(v >> bits) & 0x0f];

		if (--x == 0) {
			srcp -= 0x200;
			bits ^= 4;
			x = 512;
		}
	}
}


/*
 * --- 半透明／特殊Priのベースとなるページの描画 ---
 */
void FASTCALL Grp_DrawLine16SP(void)
{
	uint32_t x, y;
	uint32_t off;
	uint32_t i;
	uint16_t v;

	y = GrphScrollY[0] + VLINE;
	if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
		y += VLINE;
	y = (y & 0x1ff) << 10;

	x = GrphScrollX[0] & 0x1ff;
	off = y + x * 2;
	x = (x ^ 0x1ff) + 1;

	for (i = 0; i < TextDotX; ++i) {
		v = (Pal_Regs[GVRAM[off+1]*2] << 8) | Pal_Regs[GVRAM[off]*2+1];
		if ((GVRAM[off] & 1) == 0) {
			Grp_LineBufSP[i] = 0;
			Grp_LineBufSP2[i] = Pal16[v & 0xfffe];
		} else {
			Grp_LineBufSP[i] = Pal16[v & 0xfffe];
			Grp_LineBufSP2[i] = 0;
		}

		off += 2;
		if (--x == 0)
			off -= 0x400;
	}
}


void FASTCALL Grp_DrawLine8SP(int page)
{
	uint32_t x, x0;
	uint32_t y, y0;
	uint32_t off, off0;
	uint32_t i;
	uint16_t v;

	page &= 1;

	y = GrphScrollY[page * 2] + VLINE;
	y0 = GrphScrollY[page * 2 + 1] + VLINE;
	if ((CRTC_Regs[0x29] & 0x1c) == 0x1c) {
		y += VLINE;
		y0 += VLINE;
	}
	y = (y & 0x1ff) << 10;
	y0 = (y0 & 0x1ff) << 10;

	x = GrphScrollX[page * 2] & 0x1ff;
	x0 = GrphScrollX[page * 2 + 1] & 0x1ff;

	off = y + x * 2 + page;
	off0 = y0 + x0 * 2 + page;

	x = (x ^ 0x1ff) + 1;

	for (i = 0; i < TextDotX; ++i) {
		v = (GVRAM[off] & 0x0f) | (GVRAM[off0] & 0xf0);
		Grp_LineBufSP_Tr[i] = 0;

		if ((v & 1) == 0) {
			v &= 0xfe;
			if (v != 0x00) {
 				v = GrphPal[v];
				if (!v)
					Grp_LineBufSP_Tr[i] = 0x1234;
			}

			Grp_LineBufSP[i] = 0;
			Grp_LineBufSP2[i] = v;
		} else {
			v &= 0xfe;
			if (v != 0x00)
				v = GrphPal[v] | Ibit;
			Grp_LineBufSP[i] = v;
			Grp_LineBufSP2[i] = 0;
		}

		off += 2;
		off0 += 2;
		if ((off0 & 0x3fe) == 0)
			off0 -= 0x400;
		if (--x == 0)
			off -= 0x400;
	}
}

void FASTCALL Grp_DrawLine4SP(uint32_t page/*, int opaq*/)
{
	uint32_t x, y;
	uint32_t off;
	uint32_t i;
	uint16_t v;
	uint32_t scrx, scry;
	page &= 3;
	switch(page)
   {
      case 0:
         scrx = GrphScrollX[0];
         scry = GrphScrollY[0];
         break;
      case 1:
         scrx = GrphScrollX[1];
         scry = GrphScrollY[1];
         break;
      case 2:
         scrx = GrphScrollX[2];
         scry = GrphScrollY[2];
         break;
      case 3:
         scrx = GrphScrollX[3];
         scry = GrphScrollY[3];
         break;
   }

	if (page & 1)
   {
      y = scry + VLINE;
      if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
         y += VLINE;
      y = (y & 0x1ff) << 10;

      x = scrx & 0x1ff;
      off = y + x * 2;
      if (page & 2)
         off++;
      x = (x ^ 0x1ff) + 1;

      for (i = 0; i < TextDotX; ++i) {
         v = GVRAM[off] >> 4;
         if ((v & 1) == 0) {
            v &= 0x0e;
            Grp_LineBufSP[i] = 0;
            Grp_LineBufSP2[i] = GrphPal[v];
         } else {
            v &= 0x0e;
            Grp_LineBufSP[i] = GrphPal[v];
            Grp_LineBufSP2[i] = 0;
         }

         off += 2;
         if (--x == 0)
            off -= 0x400;
      }
   }
   else
   {
      y = scry + VLINE;
      if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
         y += VLINE;
      y = (y & 0x1ff) << 10;

      x = scrx & 0x1ff;
      off = y + x * 2;
      if (page & 2)
         off++;
      x = (x ^ 0x1ff) + 1;

      for (i = 0; i < TextDotX; ++i) {
         v = GVRAM[off];
         if ((v & 1) == 0) {
            v &= 0x0e;
            Grp_LineBufSP[i] = 0;
            Grp_LineBufSP2[i] = GrphPal[v];
         } else {
            v &= 0x0e;
            Grp_LineBufSP[i] = GrphPal[v];
            Grp_LineBufSP2[i] = 0;
         }

         off += 2;
         if (--x == 0)
            off -= 0x400;
      }
   }
}


void FASTCALL Grp_DrawLine4hSP(void)
{
	uint16_t *srcp;
	uint32_t x;
	uint32_t i;
	int bits;
	uint16_t v;
	uint32_t y = GrphScrollY[0] + VLINE;
	if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
		y += VLINE;
	y &= 0x3ff;

	if ((y & 0x200) == 0x000)
   {
		y <<= 10;
		bits = (GrphScrollX[0] & 0x200) ? 4 : 0;
	}
   else
   {
		y    = (y & 0x1ff) << 10;
		bits = (GrphScrollX[0] & 0x200) ? 12 : 8;
	}

	x    = GrphScrollX[0] & 0x1ff;
	srcp = (uint16_t *)(GVRAM + y + x * 2);
	x    = ((x & 0x1ff) ^ 0x1ff) + 1;

	for (i = 0; i < TextDotX; ++i)
   {
      v = *srcp++ >> bits;
      if ((v & 1) == 0)
      {
         Grp_LineBufSP[i]  = 0;
         Grp_LineBufSP2[i] = GrphPal[v & 0x0e];
      }
      else
      {
         Grp_LineBufSP[i]  = GrphPal[v & 0x0e];
         Grp_LineBufSP2[i] = 0;
      }

      if (--x == 0)
         srcp -= 0x400;
   }
}

void FASTCALL Grp_DrawLine8TR(int page, int opaq)
{
	if (opaq)
   {
      uint32_t x, y;
      uint32_t v, v0;
      uint32_t i;

      page &= 1;

      y = GrphScrollY[page * 2] + VLINE;
      if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
         y += VLINE;
      y = ((y & 0x1ff) << 10) + page;
      x = GrphScrollX[page * 2] & 0x1ff;

      for (i = 0; i < TextDotX; ++i, x = (x + 1) & 0x1ff) {
         v0 = Grp_LineBufSP[i];
         v = GVRAM[y + x * 2];

         if (v0 != 0) {
            if (v != 0) {
               v = GrphPal[v];
               if (v != 0) {
                  v0 &= Pal_HalfMask;
                  if (v & Ibit)
                     v0 |= Pal_Ix2;
                  v &= Pal_HalfMask;
                  v += v0;
                  v >>= 1;
               }
            }
         } else
            v = GrphPal[v];
         Grp_LineBuf[i] = (uint16_t)v;
      }
   }
}

void FASTCALL Grp_DrawLine8TR_GT(int page, int opaq)
{
	if (opaq)
   {
      uint32_t x, y;
      uint32_t i;

      page &= 1;

      y = GrphScrollY[page * 2] + VLINE;
      if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
         y += VLINE;
      y = ((y & 0x1ff) << 10) + page;
      x = GrphScrollX[page * 2] & 0x1ff;

      for (i = 0; i < TextDotX; ++i, x = (x + 1) & 0x1ff)
      {
         Grp_LineBuf[i]      = (Grp_LineBufSP[i] || Grp_LineBufSP_Tr[i]) ? 0 : GrphPal[GVRAM[y + x * 2]];
         Grp_LineBufSP_Tr[i] = 0;
      }
   }
}

void FASTCALL Grp_DrawLine4TR(uint32_t page, int opaq)
{
   uint32_t x, y;
   uint32_t v, v0;
   uint32_t i;

   page &= 3;

   y = GrphScrollY[page] + VLINE;
   if ((CRTC_Regs[0x29] & 0x1c) == 0x1c)
      y += VLINE;
   y = (y & 0x1ff) << 10;
   x = GrphScrollX[page] & 0x1ff;

   if (page & 1) {
      page >>= 1;
      y += page;

      if (opaq) {
         for (i = 0; i < TextDotX; ++i, x = (x + 1) & 0x1ff) {
            v0 = Grp_LineBufSP[i];
            v = GVRAM[y + x * 2] >> 4;

            if (v0 != 0) {
               if (v != 0) {
                  v = GrphPal[v];
                  if (v != 0) {
                     v0 &= Pal_HalfMask;
                     if (v & Ibit)
                        v0 |= Pal_Ix2;
                     v &= Pal_HalfMask;
                     v += v0;
                     v >>= 1;
                  }
               }
            } else
               v = GrphPal[v];
            Grp_LineBuf[i] = (uint16_t)v;
         }
      } else {
         for (i = 0; i < TextDotX; ++i, x = (x + 1) & 0x1ff) {
            v0 = Grp_LineBufSP[i];

            if (v0 == 0)
            {
               v = GVRAM[y + x * 2] >> 4;
               if (v != 0)
                  Grp_LineBuf[i] = GrphPal[v];
            }
            else
            {
               v = GVRAM[y + x * 2] >> 4;
               if (v != 0)
               {
                  v = GrphPal[v];
                  if (v != 0)
                  {
                     v0 &= Pal_HalfMask;
                     if (v & Ibit)
                        v0 |= Pal_Ix2;
                     v &= Pal_HalfMask;
                     v += v0;
                     v = GrphPal[v >> 1];
                     Grp_LineBuf[i]=(uint16_t)v;
                  }
               } else
                  Grp_LineBuf[i] = (uint16_t)v;
            }
         }
      }
   } else {
      page >>= 1;
      y += page;

      if (opaq)
      {
         for (i = 0; i < TextDotX; ++i, x = (x + 1) & 0x1ff)
         {
            v  = GVRAM[y + x * 2] & 0x0f;
            v0 = Grp_LineBufSP[i];

            if (v0 != 0)
            {
               if (v != 0)
               {
                  v = GrphPal[v];
                  if (v != 0)
                  {
                     v0 &= Pal_HalfMask;
                     if (v & Ibit)
                        v0 |= Pal_Ix2;
                     v &= Pal_HalfMask;
                     v += v0;
                     v >>= 1;
                  }
               }
            } else
               v = GrphPal[v];
            Grp_LineBuf[i] = (uint16_t)v;
         }
      }
      else
      {
         for (i = 0; i < TextDotX; ++i, x = (x + 1) & 0x1ff)
         {
            v  = GVRAM[y + x * 2] & 0x0f;
            v0 = Grp_LineBufSP[i];

            if (v0 != 0)
            {
               if (v != 0)
               {
                  v = GrphPal[v];
                  if (v != 0)
                  {
                     v0 &= Pal_HalfMask;
                     if (v & Ibit)
                        v0 |= Pal_Ix2;
                     v &= Pal_HalfMask;
                     v += v0;
                     v >>= 1;
                     Grp_LineBuf[i]=(uint16_t)v;
                  }
               } else
                  Grp_LineBuf[i] = (uint16_t)v;
            } else if (v != 0)
               Grp_LineBuf[i] = GrphPal[v];
         }
      }
   }
}
