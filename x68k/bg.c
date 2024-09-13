/*
 *  BG.C - BG and sprites
 *  TODO: Check transparent color processing (especially with Text)
 */

#include <string.h>

#include "common.h"
#include "windraw.h"
#include "winx68k.h"
#include "palette.h"
#include "tvram.h"
#include "crtc.h"
#include "bg.h"

#include "m68000.h"

uint8_t	BG[0x8000];
uint8_t	Sprite_Regs[0x800];
uint8_t	BG_Regs[0x12];
static uint16_t BG_CHREND = 0;
static uint16_t	BG_BG0TOP = 0;
static uint16_t	BG_BG0END = 0;
static uint16_t	BG_BG1TOP = 0;
static uint16_t	BG_BG1END = 0;
static uint8_t	BG_CHRSIZE = 16;
static uint32_t BG_AdrMask = 511;
static uint32_t	BG0ScrollX = 0, BG0ScrollY = 0;
static uint32_t	BG1ScrollX = 0, BG1ScrollY = 0;

int32_t	BG_HAdjust = 0;
int32_t	BG_VLINE = 0;

static uint8_t	BGCHR8[8*8*256];
static uint8_t	BGCHR16[16*16*256];

uint16_t	BG_LineBuf[1600];
uint16_t	BG_PriBuf[1600];

uint32_t	VLINEBG = 0;

int BG_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFARRAY(BG, 0x8000),
		SFARRAY(Sprite_Regs, 0x800),
		SFARRAY(BG_Regs, 0x12),

		SFVAR(BG_CHREND),
		SFVAR(BG_BG0TOP),
		SFVAR(BG_BG0END),
		SFVAR(BG_BG1TOP),
		SFVAR(BG_BG1END),
		SFVAR(BG_CHRSIZE),
		SFVAR(BG_AdrMask),
		SFVAR(BG0ScrollX),
		SFVAR(BG0ScrollY),
		SFVAR(BG1ScrollX),
		SFVAR(BG1ScrollY),

		SFARRAY(BGCHR8, (8 * 8 * 256)),
		SFARRAY(BGCHR16, (16 * 16 * 256)),
		SFARRAY16(BG_PriBuf, 1600),
		SFARRAY16(BG_LineBuf, 1600),

		SFVAR(BG_HAdjust),
		SFVAR(BG_VLINE),
		SFVAR(VLINEBG),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_BG", false);

	return ret;
}


void BG_Init(void)
{
	uint32_t i;
	memset(Sprite_Regs, 0, 0x800);
	memset(BG, 0, 0x8000);
	memset(BGCHR8, 0, 8*8*256);
	memset(BGCHR16, 0, 16*16*256);
	memset(BG_LineBuf, 0, 1600*2);
	for (i=0; i<0x12; i++)
		BG_Write(0xeb0800+i, 0);
	BG_CHREND = 0x8000;
}

uint8_t FASTCALL BG_Read(uint32_t adr)
{
	if ((adr>=0xeb0000)&&(adr<0xeb0400))
	{
		adr -= 0xeb0000;
#ifndef MSB_FIRST
		adr ^= 1;
#endif
		return Sprite_Regs[adr];
	}
	else if ((adr>=0xeb0800)&&(adr<0xeb0812))
		return BG_Regs[adr-0xeb0800];
	else if ((adr>=0xeb8000)&&(adr<0xec0000))
		return BG[adr-0xeb8000];
	return 0xff;
}

void FASTCALL BG_Write(uint32_t adr, uint8_t data)
{
	uint32_t bg16chr;
	int v  = 0;
	int s1 = (((BG_Regs[0x11]  &4)?2:1)-((BG_Regs[0x11]  &16)?1:0));
	int s2 = (((CRTC_Regs[0x29]&4)?2:1)-((CRTC_Regs[0x29]&16)?1:0));
	if ( !(BG_Regs[0x11]&16) ) v = ((BG_Regs[0x0f]>>s1)-(CRTC_Regs[0x0d]>>s2));
	if ((adr>=0xeb0000)&&(adr<0xeb0400))
	{
		adr &= 0x3ff;
#ifndef MSB_FIRST
		adr ^= 1;
#endif
		if (Sprite_Regs[adr] != data)
		{

			uint16_t t0, t, *pw;

			v = BG_VLINE - 16 - v;
			/* get YPOS pointer (Sprite_Regs[] is little endian) */
			pw = (uint16_t *)(Sprite_Regs + (adr & 0x3f8) + 2);

#define UPDATE_TDL(t)				\
{						\
	int i;					\
	for (i = 0; i < 16; i++) {		\
		TextDirtyLine[(t)] = 1;		\
		(t) = ((t) + 1) & 0x3ff;	\
	}					\
}

			t = t0 = (*pw + v) & 0x3ff;
			UPDATE_TDL(t);

			Sprite_Regs[adr] = data;

			t = (*pw + v) & 0x3ff;
			if (t != t0) {
				UPDATE_TDL(t);
			}

		}
	}
	else if ((adr>=0xeb0800)&&(adr<0xeb0812))
	{
		adr -= 0xeb0800;
		if (BG_Regs[adr]==data) return;	/* return if no data is changed */
		BG_Regs[adr] = data;
		switch(adr)
		{
		case 0x00:
		case 0x01:
			BG0ScrollX = (((uint32_t)BG_Regs[0x00]<<8)+BG_Regs[0x01])&BG_AdrMask;
			TVRAM_SetAllDirty();
			break;
		case 0x02:
		case 0x03:
			BG0ScrollY = (((uint32_t)BG_Regs[0x02]<<8)+BG_Regs[0x03])&BG_AdrMask;
			TVRAM_SetAllDirty();
			break;
		case 0x04:
		case 0x05:
			BG1ScrollX = (((uint32_t)BG_Regs[0x04]<<8)+BG_Regs[0x05])&BG_AdrMask;
			TVRAM_SetAllDirty();
			break;
		case 0x06:
		case 0x07:
			BG1ScrollY = (((uint32_t)BG_Regs[0x06]<<8)+BG_Regs[0x07])&BG_AdrMask;
			TVRAM_SetAllDirty();
			break;

		case 0x08:		/* BG On/Off Changed */
			TVRAM_SetAllDirty();
			break;

		case 0x0d:
			BG_HAdjust = ((int32_t)BG_Regs[0x0d] - (CRTC_HSTART + 4)) * 8; /* Isn't it necessary to divide the horizontal resolution by 1/2? (Tetris) */
			TVRAM_SetAllDirty();
			break;
		case 0x0f:
			BG_VLINE = ((int32_t)BG_Regs[0x0f] - CRTC_VSTART) / ((BG_Regs[0x11] & 4) ? 1 : 2); /* Difference when BG and other elements are misaligned */
			TVRAM_SetAllDirty();
			break;

		case 0x11:		/* BG ScreenRes Changed */
			if (data&3)
			{
				if ((BG_BG0TOP==0x4000)||(BG_BG1TOP==0x4000))
					BG_CHREND = 0x4000;
				else if ((BG_BG0TOP==0x6000)||(BG_BG1TOP==0x6000))
					BG_CHREND = 0x6000;
				else
					BG_CHREND = 0x8000;
			}
			else
				BG_CHREND = 0x2000;
			BG_CHRSIZE = ((data&3)?16:8);
			BG_AdrMask = ((data&3)?1023:511);
			BG_HAdjust = ((int32_t)BG_Regs[0x0d] - (CRTC_HSTART + 4)) * 8; /*Isn't it necessary to divide the horizontal resolution by 1/2? (Tetris) */
			BG_VLINE   = ((int32_t)BG_Regs[0x0f] - CRTC_VSTART) / ((BG_Regs[0x11] & 4) ? 1 : 2); /* Difference when BG and other elements are misaligned */
			break;
		case 0x09:		/* BG Plane Cfg Changed */
			TVRAM_SetAllDirty();
			if (data&0x08)
			{
				if (data&0x30)
				{
					BG_BG1TOP = 0x6000;
					BG_BG1END = 0x8000;
				}
				else
				{
					BG_BG1TOP = 0x4000;
					BG_BG1END = 0x6000;
				}
			}
			else
				BG_BG1TOP = BG_BG1END = 0;
			if (data&0x01)
			{
				if (data&0x06)
				{
					BG_BG0TOP = 0x6000;
					BG_BG0END = 0x8000;
				}
				else
				{
					BG_BG0TOP = 0x4000;
					BG_BG0END = 0x6000;
				}
			}
			else
				BG_BG0TOP = BG_BG0END = 0;
			if (BG_Regs[0x11]&3)
			{
				if ((BG_BG0TOP==0x4000)||(BG_BG1TOP==0x4000))
					BG_CHREND = 0x4000;
				else if ((BG_BG0TOP==0x6000)||(BG_BG1TOP==0x6000))
					BG_CHREND = 0x6000;
				else
					BG_CHREND = 0x8000;
			}
			break;
		case 0x0b:
			break;
		}
	}
	else if ((adr>=0xeb8000)&&(adr<0xec0000))
	{
		adr -= 0xeb8000;
		if (BG[adr]==data) return;			/* return if no data is changed */
		BG[adr] = data;
		if (adr<0x2000)
		{
			BGCHR8[adr*2]   = data>>4;
			BGCHR8[adr*2+1] = data&15;
		}
		bg16chr = ((adr&3)*2)+((adr&0x3c)*4)+((adr&0x40)>>3)+((adr&0x7f80)*2);
		BGCHR16[bg16chr]   = data>>4;
		BGCHR16[bg16chr+1] = data&15;

		if (adr<BG_CHREND)						/* pattern area */
			TVRAM_SetAllDirty();
		if ((adr>=BG_BG1TOP)&&(adr<BG_BG1END))	/* BG1 MAP Area */
			TVRAM_SetAllDirty();
		if ((adr>=BG_BG0TOP)&&(adr<BG_BG0END))	/* BG0 MAP Area */
			TVRAM_SetAllDirty();
	}
}

struct SPRITECTRLTBL {
	uint16_t	sprite_posx;
	uint16_t	sprite_posy;
	uint16_t	sprite_ctrl;
	uint8_t	sprite_ply;
	uint8_t	dummy;
} __attribute__ ((packed));
typedef struct SPRITECTRLTBL SPRITECTRLTBL_T;

static INLINE void Sprite_DrawLineMcr(int pri)
{
	SPRITECTRLTBL_T *sct = (SPRITECTRLTBL_T *)Sprite_Regs;
	uint32_t y;
	uint32_t t;
	int n;

	for (n = 127; n >= 0; --n) {
		if ((sct[n].sprite_ply & 3) == pri) {
			SPRITECTRLTBL_T *sctp = &sct[n];

			t = (sctp->sprite_posx + BG_HAdjust) & 0x3ff;
			if (t >= TextDotX + 16)
				continue;

			y = sctp->sprite_posy & 0x3ff;
			y -= VLINEBG;
			y += BG_VLINE;
			y = -y;
			y += 16;

			/* add y, 16; jnc .spline_lpcnt */
			if (y <= 15) {
				uint8_t *p;
				uint32_t pal;
				int i, d;
				
				if (sctp->sprite_ctrl < 0x4000) {
					p = &BGCHR16[((sctp->sprite_ctrl * 256) & 0xffff)  + (y * 16)];
					d = 1;
				} else  if ((sctp->sprite_ctrl - 0x4000) & 0x8000) {
					p = &BGCHR16[((sctp->sprite_ctrl * 256) & 0xffff) + (((y * 16) & 0xff) ^ 0xf0) + 15];
					d = -1;
				} else if ((int16_t)(sctp->sprite_ctrl) >= 0x4000) {
					p = &BGCHR16[((sctp->sprite_ctrl * 256) & 0xffff) + (y * 16) + 15];
					d = -1;
				}  else {
					p = &BGCHR16[((sctp->sprite_ctrl << 8) & 0xffff)  + (((y * 16) & 0xff) ^ 0xf0)];
					d = 1;
				}

				for (i = 0; i < 16; i++, t++, p += d) {
					pal = *p & 0xf;
					if (pal) {
						pal |= (sctp->sprite_ctrl >> 4) & 0xf0;
						if (BG_PriBuf[t] >= n * 8) {
							BG_LineBuf[t] = TextPal[pal];
							Text_TrFlag[t] |= 2;
							BG_PriBuf[t] = n * 8;
						}
					}
				}
			}
		}
	}
}

#define BG_DRAWLINE_LOOPY(cnt) \
{ \
	bl = bl << 4;							\
	for (j = 0; j < cnt; j++, esi += d, edi++) {			\
		dat = *esi | bl;					\
		if (dat == 0)						\
			continue;					\
		if ((dat & 0xf) || !(Text_TrFlag[edi + 1] & 2)) {	\
			BG_LineBuf[1 + edi] = TextPal[dat];		\
			Text_TrFlag[edi + 1] |= 2;			\
		}							\
	}								\
}

#define BG_DRAWLINE_LOOPY_NG(cnt) \
{  \
	bl = bl << 4;					    \
        for (j = 0; j < cnt; j++, esi += d, edi++) {	    \
                dat = *esi & 0xf;			    \
		if (dat) {				    \
			dat |= bl;			    \
                        BG_LineBuf[1 + edi] = TextPal[dat]; \
			Text_TrFlag[edi + 1] |= 2;	    \
                }					    \
        }						    \
}

static void bg_drawline_loopx8(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY, int32_t adjust, int ng)
{
       uint8_t dat, bl;
       int i, j, d;
       uint16_t si;
       uint8_t *esi;
       uint32_t ebp = ((BGScrollY + VLINEBG - BG_VLINE) & 7) << 3;
       uint32_t edx = BGTOP + (((BGScrollY + VLINEBG - BG_VLINE) & 0x1f8) << 4);
       uint32_t edi = ((BGScrollX - adjust) & 7) ^ 15;
       uint32_t ecx = ((BGScrollX - adjust) & 0x1f8) >> 2;

       for (i = TextDotX >> 3; i >= 0; i--) {
               bl = BG[ecx + edx];
               si = (uint16_t)BG[ecx + edx + 1] << 6;

               if (bl < 0x40) {
                       esi = &BGCHR8[si + ebp];
                       d = +1;
               } else if ((bl - 0x40) & 0x80) {
                       esi = &BGCHR8[si + 0x3f - ebp];
                       d = -1;
               } else if ((int8_t)bl >= 0x40) {
                       esi = &BGCHR8[si + ebp + 7];
                       d = -1;
               } else {
                       esi = &BGCHR8[si + 0x38 - ebp];
                       d = +1;
               }
	       if (ng) {
		       BG_DRAWLINE_LOOPY_NG(8);
	       } else {
		       BG_DRAWLINE_LOOPY(8);
	       }
               ecx += 2;
               ecx &= 0x7f;
       }
}

static void bg_drawline_loopx16(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY, int32_t adjust, int ng)
{
       uint16_t si;
       uint8_t dat, bl;
       int i, j, d;
       uint8_t *esi;
       uint32_t ebp = ((BGScrollY + VLINEBG - BG_VLINE) & 15) << 4;
       uint32_t edx = BGTOP + (((BGScrollY + VLINEBG - BG_VLINE) & 0x3f0) << 3);
       uint32_t edi = ((BGScrollX - adjust) & 15) ^ 15;
       uint32_t ecx = ((BGScrollX - adjust) & 0x3f0) >> 3;

       for (i = TextDotX >> 4; i >= 0; i--) {
		bl = BG[ecx + edx];
		si = BG[ecx + edx + 1] << 8;

		if (bl < 0x40) {
			esi = &BGCHR16[si + ebp];
			d = +1;
		} else if ((bl - 0x40) & 0x80) {
			esi = &BGCHR16[si + 0xff - ebp];
			d = -1;
		} else if ((int8_t)bl >= 0x40) {
			esi = &BGCHR16[si + ebp + 15];
			d = -1;
		} else {
			esi = &BGCHR16[si + 0xf0 - ebp];
			d = +1;
		}
		if (ng) {
			BG_DRAWLINE_LOOPY_NG(16);
		} else {
			BG_DRAWLINE_LOOPY(16);
		}
		ecx += 2;
		ecx &= 0x7f;
	}
}

static INLINE void BG_DrawLineMcr8(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY)
{
   bg_drawline_loopx8(BGTOP, BGScrollX, BGScrollY, BG_HAdjust, 0);
}

static INLINE void BG_DrawLineMcr16(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY)
{
	bg_drawline_loopx16(BGTOP, BGScrollX, BGScrollY, BG_HAdjust, 0);
}

static INLINE void
BG_DrawLineMcr8_ng(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY)
{
   bg_drawline_loopx8(BGTOP, BGScrollX, BGScrollY, BG_HAdjust, 1);
}

static INLINE void BG_DrawLineMcr16_ng(uint16_t BGTOP,
      uint32_t BGScrollX, uint32_t BGScrollY)
{
   bg_drawline_loopx16(BGTOP, BGScrollX, BGScrollY, 0, 1);
}

void FASTCALL BG_DrawLine(int opaq, int gd)
{
	int i;
	void (*func8)(uint16_t, uint32_t, uint32_t), (*func16)(uint16_t, uint32_t, uint32_t);

	if (opaq)
   {
		for (i = 16; i < TextDotX + 16; ++i)
      {
			BG_LineBuf[i] = TextPal[0];
			BG_PriBuf[i] = 0xffff;
		}
	}
   else
   {
		for (i = 16; i < TextDotX + 16; ++i)
			BG_PriBuf[i] = 0xffff;
	}

	func8 = (gd)? BG_DrawLineMcr8 : BG_DrawLineMcr8_ng;
	func16 = (gd)? BG_DrawLineMcr16 : BG_DrawLineMcr16_ng;

	Sprite_DrawLineMcr(1);
	if ((BG_Regs[9] & 8) && (BG_CHRSIZE == 8)) /* BG1 on */
		(*func8)(BG_BG1TOP, BG1ScrollX, BG1ScrollY);
	Sprite_DrawLineMcr(2);
	if (BG_Regs[9] & 1)
   { /* BG0 on */
		if (BG_CHRSIZE == 8)
			(*func8)(BG_BG0TOP, BG0ScrollX, BG0ScrollY);
      else
			(*func16)(BG_BG0TOP, BG0ScrollX, BG0ScrollY);
	}
	Sprite_DrawLineMcr(3);
}
