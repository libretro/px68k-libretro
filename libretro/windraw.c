/* 
 * Copyright (c) 2003 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include "winx68k.h"
#include "winui.h"

#include "bg.h"
#include "crtc.h"
#include "gvram.h"
#include "mouse.h"
#include "palette.h"
#include "prop.h"
#include "status.h"
#include "tvram.h"
#include "joystick.h"
#include "keyboard.h"

#define		SCREEN_WIDTH		768
#define		FULLSCREEN_WIDTH	800

extern uint16_t *videoBuffer;
uint16_t menu_buffer[800*600];

extern uint8_t Debug_Text, Debug_Grp, Debug_Sp;

static uint16_t *ScrBuf = 0;

uint16_t WinDraw_Pal16B, WinDraw_Pal16R, WinDraw_Pal16G;

void WinDraw_Init(void)
{
	WinDraw_Pal16R = 0xf800;
	WinDraw_Pal16G = 0x07e0;
	WinDraw_Pal16B = 0x001f;

	ScrBuf         = (uint16_t *)calloc(800 * 600, sizeof(uint16_t));
}

void WinDraw_Cleanup(void)
{
        if (ScrBuf)
           free(ScrBuf);
        ScrBuf = NULL;
}

/* Forward declarations */
extern uint32_t retrow, retroh;
extern int CHANGEAV;

void FASTCALL WinDraw_Draw(void)
{
	static int oldtextx = -1, oldtexty = -1;

	if (oldtextx != TextDotX)
	{
		oldtextx = TextDotX;
		CHANGEAV=1;
	}
	if (oldtexty != TextDotY)
	{
		oldtexty = TextDotY;
		CHANGEAV=1;
	}

	if (CHANGEAV==1)
	{
		/* Clamp only frontend presentation dimensions; do not alter CRTC state. */
        retrow = TextDotX > 800 ? 800 : TextDotX;
        retroh = TextDotY > 600 ? 600 : TextDotY;
        if (!retrow) retrow = 1;
        if (!retroh) retroh = 1;
	}

	videoBuffer = (uint16_t*)ScrBuf;
}

#define WD_MEMCPY(src) memcpy(&ScrBuf[adr], (src), TextDotX * 2)

#define WD_LOOP(start, end, sub)                 \
	{                                            \
		for (i = (start); i < (end); i++, adr++) \
		{                                        \
			sub();                               \
		}                                        \
	}

#define WD_SUB(SUFFIX, src)          \
	{                                \
		w = (src);                   \
		if (w != 0)                  \
			ScrBuf##SUFFIX[adr] = w; \
	}

static INLINE void WinDraw_DrawGrpLine(int opaq)
{
#define _DGL_SUB(SUFFIX) WD_SUB(SUFFIX, Grp_LineBuf[i])

	uint32_t adr = VLINE * FULLSCREEN_WIDTH;
	uint16_t w;
	int i;

	if (opaq) {
		WD_MEMCPY(Grp_LineBuf);
	} else {
		WD_LOOP(0,  TextDotX, _DGL_SUB);
	}
}

static INLINE void WinDraw_DrawGrpLineNonSP(int opaq)
{
#define _DGL_NSP_SUB(SUFFIX) WD_SUB(SUFFIX, Grp_LineBufSP2[i])

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint16_t w;
	int i;

	if (opaq) {
		WD_MEMCPY(Grp_LineBufSP2);
	} else {
		WD_LOOP(0,  TextDotX, _DGL_NSP_SUB);
	}
}

static INLINE void WinDraw_DrawTextLine(int opaq, int td)
{
#define _DTL_SUB2(SUFFIX) WD_SUB(SUFFIX, BG_LineBuf[i])

#define _DTL_SUB(SUFFIX)        \
	{                           \
		if (Text_TrFlag[i] & 1) \
		{                       \
			_DTL_SUB2(SUFFIX);  \
		}                       \
	}

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint16_t w;
	int i;

	if (opaq) {
		WD_MEMCPY(&BG_LineBuf[16]);
	} else {
		if (td) {
			WD_LOOP(16, TextDotX + 16, _DTL_SUB);
		} else {
			WD_LOOP(16, TextDotX + 16, _DTL_SUB2);
		}
	}
}

static INLINE void WinDraw_DrawTextLineTR(int opaq)
{
#define _DTL_TR_SUB(SUFFIX)                \
	{                                      \
		w = Grp_LineBufSP[i - 16];         \
		if (w != 0)                        \
		{                                  \
			w &= Pal_HalfMask;             \
			v = BG_LineBuf[i];             \
			if (v & Ibit)                  \
				w += Pal_Ix2;              \
			v &= Pal_HalfMask;             \
			v += w;                        \
			v >>= 1;                       \
		}                                  \
		else                               \
		{                                  \
			if (Text_TrFlag[i] & 1)        \
				v = BG_LineBuf[i];         \
			else                           \
				v = 0;                     \
		}                                  \
		ScrBuf##SUFFIX[adr] = (uint16_t)v; \
	}

#define _DTL_TR_SUB2(SUFFIX)                       \
	{                                              \
		if (Text_TrFlag[i] & 1)                    \
		{                                          \
			w = Grp_LineBufSP[i - 16];             \
			v = BG_LineBuf[i];                     \
                                                   \
			if (v != 0)                            \
			{                                      \
				if (w != 0)                        \
				{                                  \
					w &= Pal_HalfMask;             \
					if (v & Ibit)                  \
						w += Pal_Ix2;              \
					v &= Pal_HalfMask;             \
					v += w;                        \
					v >>= 1;                       \
				}                                  \
				ScrBuf##SUFFIX[adr] = (uint16_t)v; \
			}                                      \
		}                                          \
	}

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint32_t v;
	uint16_t w;
	int i;

	if (opaq) {
		WD_LOOP(16, TextDotX + 16, _DTL_TR_SUB);
	} else {
		WD_LOOP(16, TextDotX + 16, _DTL_TR_SUB2);
	}
}

static INLINE void WinDraw_DrawBGLine(int opaq, int td)
{
#define _DBL_SUB2(SUFFIX) WD_SUB(SUFFIX, BG_LineBuf[i])

#define _DBL_SUB(SUFFIX)        \
	{                           \
		if (Text_TrFlag[i] & 2) \
		{                       \
			_DBL_SUB2(SUFFIX);  \
		}                       \
	}

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint16_t w;
	int i;

	if (opaq) {
		WD_MEMCPY(&BG_LineBuf[16]);
	} else {
		if (td) {
			WD_LOOP(16, TextDotX + 16, _DBL_SUB);
		} else {
			WD_LOOP(16, TextDotX + 16, _DBL_SUB2);
		}
	}
}

static INLINE void WinDraw_DrawBGLineTR(int opaq)
{
#define _DBL_TR_SUB3()         \
	{                          \
		if (w != 0)            \
		{                      \
			w &= Pal_HalfMask; \
			if (v & Ibit)      \
				w += Pal_Ix2;  \
			v &= Pal_HalfMask; \
			v += w;            \
			v >>= 1;           \
		}                      \
	}

#define _DBL_TR_SUB(SUFFIX)                \
	{                                      \
		w = Grp_LineBufSP[i - 16];         \
		v = BG_LineBuf[i];                 \
                                           \
		_DBL_TR_SUB3()                     \
		ScrBuf##SUFFIX[adr] = (uint16_t)v; \
	}

#define _DBL_TR_SUB2(SUFFIX)                       \
	{                                              \
		if (Text_TrFlag[i] & 2)                    \
		{                                          \
			w = Grp_LineBufSP[i - 16];             \
			v = BG_LineBuf[i];                     \
                                                   \
			if (v != 0)                            \
			{                                      \
				_DBL_TR_SUB3()                     \
				ScrBuf##SUFFIX[adr] = (uint16_t)v; \
			}                                      \
		}                                          \
	}

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint32_t v;
	uint16_t w;
	int i;

	if (opaq) {
		WD_LOOP(16, TextDotX + 16, _DBL_TR_SUB);
	} else {
		WD_LOOP(16, TextDotX + 16, _DBL_TR_SUB2);
	}

}

static INLINE void WinDraw_DrawPriLine(void)
{
#define _DPL_SUB(SUFFIX) WD_SUB(SUFFIX, Grp_LineBufSP[i])

	uint32_t adr = VLINE*FULLSCREEN_WIDTH;
	uint16_t w;
	int i;

	WD_LOOP(0, TextDotX, _DPL_SUB);
}

void WinDraw_DrawLine(void)
{
	int opaq, ton=0, gon=0, bgon=0, tron=0, pron=0, tdrawed=0;

	/* CRTC registers may contain transitional values while software changes
     * video modes. Never let these values index beyond the host framebuffer
     * or the 1024-entry dirty-line table. Preserve the emulated registers. */
    if (!ScrBuf || VLINE >= 600 || VLINE >= 1024 ||
        TextDotX == 0 || TextDotX > FULLSCREEN_WIDTH)
        return;

	/* Redraw clean scanlines only when a hardware sprite actually covers them.
	 * This preserves the dirty-line fast path on the rest of the screen while
	 * avoiding stale sprite pixels in games that update sprites without
	 * reliably invalidating every affected scanline. */
	if (!TextDirtyLine[VLINE])
	{
		if (!((VCReg2[1] & 0x40) && (BG_Regs[8] & 2) &&
		      !(BG_Regs[0x11] & 2) && Debug_Sp))
			return;

		{
			int s1 = (((BG_Regs[0x11] & 4) ? 2 : 1) -
			          ((BG_Regs[0x11] & 16) ? 1 : 0));
			int s2 = (((CRTC_Regs[0x29] & 4) ? 2 : 1) -
			          ((CRTC_Regs[0x29] & 16) ? 1 : 0));
			uint32_t sprite_line = VLINE;
			sprite_line <<= s1;
			sprite_line >>= s2;
			if (!(BG_Regs[0x11] & 16))
				sprite_line -= ((BG_Regs[0x0f] >> s1) -
				                (CRTC_Regs[0x0d] >> s2));
			if (!BG_SpriteOnLine(sprite_line))
				return;
		}
	}

	TextDirtyLine[VLINE] = 0;

	if (Debug_Grp)
	{
	switch(VCReg0[1]&3)
	{
	case 0:					/* 16 colors */
		if (VCReg0[1]&4)		/* 1024dot */
		{
			if (VCReg2[1]&0x10)
			{
				if ( (VCReg2[0]&0x14)==0x14 )
				{
					Grp_DrawLine4hSP();
					pron = tron = 1;
				}
				else
				{
					Grp_DrawLine4h();
					gon=1;
				}
			}
		}
		else				/* 512dot */
		{
			if ( (VCReg2[0]&0x10)&&(VCReg2[1]&1) )
			{
				Grp_DrawLine4SP((VCReg1[1]   )&3/*, 1*/);			/* 半透明の下準備 */
				pron = tron = 1;
			}
			opaq = 1;
			if (VCReg2[1]&8)
			{
				Grp_DrawLine4((VCReg1[1]>>6)&3, 1);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&4)
			{
				Grp_DrawLine4((VCReg1[1]>>4)&3, opaq);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&2)
			{
				if ( ((VCReg2[0]&0x1e)==0x1e)&&(tron) )
					Grp_DrawLine4TR((VCReg1[1]>>2)&3, opaq);
				else
					Grp_DrawLine4((VCReg1[1]>>2)&3, opaq);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&1)
			{
				if ( (VCReg2[0]&0x14)!=0x14 )
				{
					Grp_DrawLine4((VCReg1[1]   )&3, opaq);
					gon=1;
				}
			}
		}
		break;
	case 1:	
	case 2:	
		/* 256-colour GRP enable pairing (XEiJ-style, test 23):
		 * VC R2 bits 0/1 enable the first 8-bit GRP layer (OR), and
		 * bits 2/3 enable the second (OR). Each layer combines two
		 * 4-bit GVRAM planes. Previously PX68K checked only bit 0
		 * (0x01) for the first layer and bit 2 (0x04) for the second.
		 * This fixes missing graphics in Etoile Princesse and passed
		 * our Dracula, Gradius II and SSF2 regression scenes.
		 * The alternative is to restore the original 0x01/0x04
		 * checks if hardware evidence contradicts paired-bit OR.
		 * Keep priority ordering, transparency and blending unchanged.
		 */
		opaq = 1; /* 256 colors */
		if ( (VCReg1[1]&3) <= ((VCReg1[1]>>4)&3) ) /* 同じ値の時は、GRP0が優先（ドラスピ） */
		{
			if ( (VCReg2[0]&0x10)&&(VCReg2[1]&0x03) )
			{
				Grp_DrawLine8SP(0); /* 半透明の下準備 */
				tron = pron = 1;
			}
			if (VCReg2[1]&0x0c)
			{
				if ( ((VCReg2[0]&0x1e)==0x1e)&&(tron) )
					Grp_DrawLine8TR(1, 1);
				else if ( ((VCReg2[0]&0x1d)==0x1d)&&(tron) )
					Grp_DrawLine8TR_GT(1, 1);
				else
					Grp_DrawLine8(1, 1);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&0x03)
			{
				if ( (VCReg2[0]&0x14)!=0x14 )
				{
					Grp_DrawLine8(0, opaq);
					gon=1;
				}
			}
		}
		else
		{
			if ( (VCReg2[0]&0x10)&&(VCReg2[1]&0x03) )
			{
				Grp_DrawLine8SP(1); /* 半透明の下準備 */
				tron = pron = 1;
			}
			if (VCReg2[1]&0x0c)
			{
				if ( ((VCReg2[0]&0x1e)==0x1e)&&(tron) )
					Grp_DrawLine8TR(0, 1);
				else if ( ((VCReg2[0]&0x1d)==0x1d)&&(tron) )
					Grp_DrawLine8TR_GT(0, 1);
				else
					Grp_DrawLine8(0, 1);
				opaq = 0;
				gon=1;
			}
			if (VCReg2[1]&0x03)
			{
				if ( (VCReg2[0]&0x14)!=0x14 )
				{
					Grp_DrawLine8(1, opaq);
					gon=1;
				}
			}
		}
		break;
	case 3:					/* 65536 colors */
		if (VCReg2[1]&15)
		{
			if ( (VCReg2[0]&0x14)==0x14 )
			{
				Grp_DrawLine16SP();
				tron = pron = 1;
			}
			else
			{
				Grp_DrawLine16();
				gon=1;
			}
		}
		break;
	}
	}


	if ( ((VCReg1[0]&0x30)>>2) < (VCReg1[0]&0x0c) )
	{						/* BGの方が上 */
		if ((VCReg2[1]&0x20)&&(Debug_Text))
		{
			Text_DrawLine(1);
			ton = 1;
		}
		else
			memset(Text_TrFlag, 0, TextDotX+16);

		if ((VCReg2[1]&0x40)&&(BG_Regs[8]&2)&&(!(BG_Regs[0x11]&2))&&(Debug_Sp))
		{
			int s1, s2;
			s1 = (((BG_Regs[0x11]  &4)?2:1)-((BG_Regs[0x11]  &16)?1:0));
			s2 = (((CRTC_Regs[0x29]&4)?2:1)-((CRTC_Regs[0x29]&16)?1:0));
			VLINEBG = VLINE;
			VLINEBG <<= s1;
			VLINEBG >>= s2;
			if ( !(BG_Regs[0x11]&16) ) VLINEBG -= ((BG_Regs[0x0f]>>s1)-(CRTC_Regs[0x0d]>>s2));
			BG_DrawLine(!ton, 0);
			bgon = 1;
		}
	}
	else
	{						/* Textの方が上 */
		if ((VCReg2[1]&0x40)&&(BG_Regs[8]&2)&&(!(BG_Regs[0x11]&2))&&(Debug_Sp))
		{
			int s1, s2;
			s1 = (((BG_Regs[0x11]  &4)?2:1)-((BG_Regs[0x11]  &16)?1:0));
			s2 = (((CRTC_Regs[0x29]&4)?2:1)-((CRTC_Regs[0x29]&16)?1:0));
			VLINEBG = VLINE;
			VLINEBG <<= s1;
			VLINEBG >>= s2;
			if ( !(BG_Regs[0x11]&16) ) VLINEBG -= ((BG_Regs[0x0f]>>s1)-(CRTC_Regs[0x0d]>>s2));
			memset(Text_TrFlag, 0, TextDotX+16);
			BG_DrawLine(1, 1);
			bgon = 1;
		}
		else
		{
			if ((VCReg2[1]&0x20)&&(Debug_Text))
			{
				int i;
				for (i = 16; i < TextDotX + 16; ++i)
					BG_LineBuf[i] = TextPal[0];
			} else {		/* 20010120 （琥珀色） */
				memset(&BG_LineBuf[16], 0, TextDotX * 2);
			}
			memset(Text_TrFlag, 0, TextDotX+16);
			bgon = 1;
		}

		if ((VCReg2[1]&0x20)&&(Debug_Text))
		{
			Text_DrawLine(!bgon);
			ton = 1;
		}
	}

	opaq = 1;

	/* Pri = 2 or 3（最下位）に設定されている画面を表示
	 * プライオリティが同じ場合は、GRP<SP<TEXT？（ドラスピ、桃伝、YsIII等）

	 * GrpよりTextが上にある場合にTextとの半透明を行うと、SPのプライオリティも
	 * Textに引きずられる？（つまり、Grpより下にあってもSPが表示される？）
	 * KnightArmsとかを見ると、半透明のベースプレーンは一番上になるみたい…。
	 */

	if ( (VCReg1[0]&0x02) )
	{
		if (gon)
		{
			WinDraw_DrawGrpLine(opaq);
			opaq = 0;
		}
		if (tron)
		{
			WinDraw_DrawGrpLineNonSP(opaq);
			opaq = 0;
		}
	}
	if ( (VCReg1[0]&0x20)&&(bgon) )
	{
		if ( ((VCReg2[0]&0x5d)==0x1d)&&((VCReg1[0]&0x03)!=0x02)&&(tron) )
		{
			if ( (VCReg1[0]&3)<((VCReg1[0]>>2)&3) )
			{
				WinDraw_DrawBGLineTR(opaq);
				tdrawed = 1;
				opaq = 0;
			}
		}
		else
		{
			WinDraw_DrawBGLine(opaq, /*0*/tdrawed);
			tdrawed = 1;
			opaq = 0;
		}
	}
	if ( (VCReg1[0]&0x08)&&(ton) )
	{
		if ( ((VCReg2[0]&0x5d)==0x1d)&&((VCReg1[0]&0x03)!=0x02)&&(tron) )
			WinDraw_DrawTextLineTR(opaq);
		else
			WinDraw_DrawTextLine(opaq, tdrawed/*((VCReg1[0]&0x30)>=0x20)*/);
		opaq = 0;
		tdrawed = 1;
	}

	/* Pri = 1（2番目）に設定されている画面を表示 */
	if ( ((VCReg1[0]&0x03)==0x01)&&(gon) )
	{
		WinDraw_DrawGrpLine(opaq);
		opaq = 0;
	}
	if ( ((VCReg1[0]&0x30)==0x10)&&(bgon) )
	{
		if ( ((VCReg2[0]&0x5d)==0x1d)&&(!(VCReg1[0]&0x03))&&(tron) )
		{
			if ( (VCReg1[0]&3)<((VCReg1[0]>>2)&3) )
			{
				WinDraw_DrawBGLineTR(opaq);
				tdrawed = 1;
				opaq = 0;
			}
		}
		else
		{
			WinDraw_DrawBGLine(opaq, ((VCReg1[0]&0xc)==0x8));
			tdrawed = 1;
			opaq = 0;
		}
	}
	if ( ((VCReg1[0]&0x0c)==0x04) && ((VCReg2[0]&0x5d)==0x1d) && (VCReg1[0]&0x03) && (((VCReg1[0]>>4)&3)>(VCReg1[0]&3)) && (bgon) && (tron) )
	{
		WinDraw_DrawBGLineTR(opaq);
		tdrawed = 1;
		opaq = 0;
		if (tron)
		{
			WinDraw_DrawGrpLineNonSP(opaq);
		}
	}
	else if ( ((VCReg1[0]&0x03)==0x01)&&(tron)&&(gon)&&(VCReg2[0]&0x10) )
	{
		WinDraw_DrawGrpLineNonSP(opaq);
		opaq = 0;
	}
	if ( ((VCReg1[0]&0x0c)==0x04)&&(ton) )
	{
		if ( ((VCReg2[0]&0x5d)==0x1d)&&(!(VCReg1[0]&0x03))&&(tron) )
			WinDraw_DrawTextLineTR(opaq);
		else
			/* Keep text transparency testing enabled at this priority.
			 * Overdriver stage 2 needs td=1 to composite the water
			 * with the GRP mountains and rain. The previous condition,
			 * ((VCReg1[0] & 0x30) >= 0x10), selected td=0 there and
			 * obscured the GRP scene. Revisit this choice if a game
			 * demonstrates different hardware behaviour. */
			WinDraw_DrawTextLine(opaq, 1);
		opaq = 0;
		tdrawed = 1;
	}

	/* Pri = 0（最優先）に設定されている画面を表示 */
	if ( (!(VCReg1[0]&0x03))&&(gon) )
	{
		WinDraw_DrawGrpLine(opaq);
		opaq = 0;
	}
	if ( (!(VCReg1[0]&0x30))&&(bgon) )
	{
		WinDraw_DrawBGLine(opaq, /*tdrawed*/((VCReg1[0]&0xc)>=0x4));
		tdrawed = 1;
		opaq = 0;
	}
	if ( (!(VCReg1[0]&0x0c)) && ((VCReg2[0]&0x5d)==0x1d) && (((VCReg1[0]>>4)&3)>(VCReg1[0]&3)) && (bgon) && (tron) )
	{
		WinDraw_DrawBGLineTR(opaq);
		tdrawed = 1;
		opaq = 0;
		if (tron)
		{
			WinDraw_DrawGrpLineNonSP(opaq);
		}
	}
	else if ( (!(VCReg1[0]&0x03))&&(tron)&&(VCReg2[0]&0x10) )
	{
		WinDraw_DrawGrpLineNonSP(opaq);
		opaq = 0;
	}
	if ( (!(VCReg1[0]&0x0c))&&(ton) )
	{
		WinDraw_DrawTextLine(opaq, 1);
		tdrawed = 1;
		opaq = 0;
	}

	/* 特殊プライオリティ時のグラフィック */
	if ( ((VCReg2[0]&0x5c)==0x14)&&(pron) )	/* 特殊Pri時は、対象プレーンビットは意味が無いらしい（ついんびー） */
	{
		WinDraw_DrawPriLine();
	}
	else if ( ((VCReg2[0]&0x5d)==0x1c)&&(tron) )	/* 半透明時に全てが透明なドットをハーフカラーで埋める */
	{						/* （AQUALES） */
#define _DL_SUB(SUFFIX)                                    \
	{                                                      \
		w = Grp_LineBufSP[i];                              \
		if (w != 0 && (ScrBuf##SUFFIX[adr] & 0xffff) == 0) \
			ScrBuf##SUFFIX[adr] = (w & Pal_HalfMask) >> 1; \
	}

		uint32_t adr = VLINE*FULLSCREEN_WIDTH;
		uint16_t w;
		int i;

		WD_LOOP(0, TextDotX, _DL_SUB);
	}

	if (opaq)
	{
		uint32_t adr = VLINE*FULLSCREEN_WIDTH;
		memset(&ScrBuf[adr], 0, TextDotX * 2);
	}
}

/********** menu 関連ルーチン **********/

struct _px68k_menu
{
	uint16_t *sbp;    /* surface buffer ptr */
	uint16_t *mlp;    /* menu locate ptr */
	uint16_t mcolor;  /* color of chars to write */
	uint16_t mbcolor; /* back ground color of chars to write */
	int ml_x;
	int ml_y;
	int mfs;      /* menu font size; */
} p6m;

/* sjis→jisコード変換 */
static uint16_t sjis2jis(uint16_t w)
{
	uint8_t wh = w / 256;
	uint8_t wl = w % 256;

	wh <<= 1;
	if (wl < 0x9f)
	{
		wh += (wh < 0x3f)? 0x1f : -0x61;
		wl -= (wl > 0x7e)? 0x20 : 0x1f;
	}
	else
	{
		wh += (wh < 0x3f)? 0x20 : -0x60;
		wl -= 0x7e;
	}

	return (wh * 256 + wl);
}

/* JISコードから0 originのindexに変換する */
/* ただし0x2921-0x2f7eはX68KのROM上にないので飛ばす */
static uint16_t jis2idx(uint16_t jc)
{
	if (jc >= 0x3000)
		jc -= 0x3021;
	else
		jc -= 0x2121;
	jc = jc % 256 + (jc / 256) * 0x5e;

	return jc;
}

#define isHankaku(s) ((s) >= 0x20 && (s) <= 0x7e || (s) >= 0xa0 && (s) <= 0xdf)
#define MENU_WIDTH 800
#ifndef PX68K_MENU_VERSION
#define PX68K_MENU_VERSION "0.15+"
#endif

/* fs : font size : 16 or 24
 * 半角文字の場合は16bitの上位8bitにデータを入れておくこと
 * (半角or全角の判断ができるように)
 */
static uint32_t get_font_addr(uint16_t sjis, int fs)
{
	uint16_t jis, j_idx;
	uint8_t jhi;
	int fsb; /* file size in bytes */

	/* 半角文字 */
	if (isHankaku(sjis >> 8)) {
		switch (fs) {
		case 8:
			return (0x3a000 + (sjis >> 8) * (1 * 8));
		case 16:
			return (0x3a800 + (sjis >> 8) * (1 * 16));
		case 24:
			return (0x3d000 + (sjis >> 8) * (2 * 24));
		default:
			return -1;
		}
	}

	/* 全角文字 */
	if (fs == 16)
		fsb = 2 * 16;
	else if (fs == 24)
		fsb = 3 * 24;
	else
		return -1;

	jis   = sjis2jis(sjis);
	j_idx = (uint32_t)jis2idx(jis);
	jhi   = (uint8_t)(jis >> 8);

	/* 非漢字 */
	if (jhi >= 0x21 && jhi <= 0x28)
		return  ((fs == 16)? 0x0 : 0x40000) + j_idx * fsb;
	/* 第一水準/第二水準 */
	else if (jhi >= 0x30 && jhi <= 0x74)
		return  ((fs == 16)? 0x5e00 : 0x4d380) + j_idx * fsb;
	/* ここにくることはないはず */
	return -1;
}

/* RGB565 */

/* キャラクタ文字の座標 (横軸は1座標が半角文字幅になる) */
static void set_mlocateC(int x, int y)
{
	p6m.ml_x = x * p6m.mfs / 2, p6m.ml_y = y * p6m.mfs;
}

static uint16_t *get_ml_ptr(void)
{
	p6m.mlp = p6m.sbp + MENU_WIDTH * p6m.ml_y + p6m.ml_x;
	return p6m.mlp;
}

/* ・半角文字の場合は16bitの上位8bitにデータを入れておくこと
 *   (半角or全角の判断ができるように)
 * ・表示した分cursorは先に移動する
 */
static void draw_char(uint16_t sjis)
{
	int i, j, k, wc, w;
	uint8_t c;
	uint16_t bc;
	int h    = p6m.mfs;
	uint16_t *p  = get_ml_ptr();
	uint32_t f  = get_font_addr(sjis, h);

	if (f < 0)
		return;

	/* h=8は半角のみ */
	w = (h == 8)? 8 : (isHankaku(sjis >> 8)? h / 2 : h);

	for (i = 0; i < h; i++) {
		wc = w;
		for (j = 0; j < ((w % 8 == 0)? w / 8 : w / 8 + 1); j++) {
			c = FONT[f++];
			for (k = 0; k < 8 ; k++) {
				bc = p6m.mbcolor? p6m.mbcolor : *p;
				*p = (c & 0x80)? p6m.mcolor : bc;
				p++;
				c = c << 1;
				wc--;
				if (wc == 0)
					break;
			}
		}
		p = p + MENU_WIDTH - w;
	}

	p6m.ml_x += w;
}

static void draw_str(char *cp)
{
	int i;
	uint16_t wc;
	int len    = strlen(cp);
	uint8_t *s = (uint8_t *)cp;

	for (i = 0; i < len; i++) {
		if (isHankaku(*s)) {
			/* 最初の8bitで半全角を判断するので半角の場合は
			 * あらかじめ8bit左シフトしておく */
			draw_char((uint16_t)*s << 8);
			s++;
		} else {
			wc = (uint16_t)(*s << 8) + *(s + 1);
			draw_char(wc);
			s += 2;
			i++;
		}
		/* 8x8描画(ソフトキーボードのFUNCキーは文字幅を縮める) */
		if (p6m.mfs == 8) {
			p6m.ml_x -= 3;
		}
	}
}

int WinDraw_MenuInit(void)
{
	p6m.sbp     = menu_buffer;
	p6m.mfs     = 16;
	p6m.mcolor  = 0xffff;
	p6m.mbcolor = 0;
	return 1;
}

#include "menu_str_sjis.txt"
char menu_item_desc[][60] = {
	"Reset / NMI reset / Quit",
	"Change / Eject floppy 0",
	"Change / Eject floppy 1",
	"Change / Eject HDD 0",
	"Change / Eject HDD 1"
};

/* Draw the menu frames in pixels so their width does not depend on font size. */
static void menu_line(int x0, int y0, int x1, int y1, uint16_t color)
{
   int x, y;
   if (x0 == x1)
      for (y = y0; y <= y1; ++y)
         menu_buffer[y * MENU_WIDTH + x0] = color;
   else
      for (x = x0; x <= x1; ++x)
         menu_buffer[y0 * MENU_WIDTH + x] = color;
}

static void menu_box(int x0, int y0, int x1, int y1, uint16_t color)
{
   menu_line(x0, y0, x1, y0, color);
   menu_line(x0, y1, x1, y1, color);
   menu_line(x0, y0, x0, y1, color);
   menu_line(x1, y0, x1, y1, color);
}

/* Limit paths by displayed glyph width, keeping Shift-JIS pairs intact. */
static void menu_path_fit(char *dst, size_t capacity, const char *src, int pixels)
{
   size_t n = 0;
   int used = 0;
   const unsigned char *p = (const unsigned char *)src;
   while (*p && n + 2 < capacity)
   {
      int half = isHankaku(*p);
      int bytes = half ? 1 : 2;
      int width = half ? p6m.mfs / 2 : p6m.mfs;
      if (used + width > pixels || (!half && !p[1]))
         break;
      dst[n++] = (char)*p++;
      if (bytes == 2)
         dst[n++] = (char)*p++;
      used += width;
   }
   dst[n] = '\0';
}

/* Preserve the URI scheme and filename, spending only the required width
 * on shortening directory components. Work in complete Shift-JIS glyphs. */
static int menu_text_pixels(const char *s)
{
   int pixels = 0;
   const unsigned char *p = (const unsigned char *)s;
   while (*p)
   {
      if (isHankaku(*p)) { pixels += p6m.mfs / 2; ++p; }
      else if (p[1]) { pixels += p6m.mfs; p += 2; }
      else break;
   }
   return pixels;
}

/* Fit a path exactly in pixels. Keep the URI prefix and final filename;
 * remove the minimum amount of parent-directory text, one glyph at a time. */
static void menu_path_compact(char *dst, size_t cap, const char *src, int pixels)
{
   char work[PATH_MAX];
   size_t prefix = 0, len;
   if (!cap) return;
   dst[0] = 0;
   if (!strncmp(src, "zip://", 6)) prefix = 6;
   else if (!strncmp(src, "file://", 7)) prefix = 7;
   len = strlen(src);
   if (len >= sizeof(work)) { menu_path_fit(dst, cap, src, pixels); return; }
   memcpy(work, src, len + 1);
   while (menu_text_pixels(work) > pixels)
   {
      char *last = strrchr(work + prefix, '/');
      size_t i = prefix, best = 0, bestlen = 0;
      if (!last) break;
      /* Choose the longest remaining parent; preserve its first glyph and
       * shorten its end. The three dots count against the width budget. */
      while (work + i < last)
      {
         size_t j = i;
         while (work[j] && work[j] != '/') ++j;
         if (work + j > last) break;
         if (j - i > bestlen && j - i > 4)
         { best = i; bestlen = j - i; }
         i = j + 1;
      }
      if (!bestlen) break;
      {
         size_t end = best + bestlen;
         size_t cut = end - 1;
         /* Avoid splitting a Shift-JIS character. */
         size_t k = best;
         while (k < cut)
         {
            size_t step = isHankaku((unsigned char)work[k]) ? 1 : 2;
            if (k + step >= cut) { cut = k; break; }
            k += step;
         }
         /* If already abbreviated, extend the existing ellipsis leftward. */
         if (end >= best + 3 && !memcmp(work + end - 3, "...", 3))
         {
            cut = end - 3;
            k = best;
            while (k < cut)
            {
               size_t step = isHankaku((unsigned char)work[k]) ? 1 : 2;
               if (k + step >= cut) { cut = k; break; }
               k += step;
            }
         }
         if (cut <= best) break;
         memmove(work + cut + 3, work + end, strlen(work + end) + 1);
         memcpy(work + cut, "...", 3);
      }
   }
   if (menu_text_pixels(work) <= pixels)
      menu_path_fit(dst, cap, work, pixels);
   else
   {
      /* Extremely long final filenames: preserve the extension and tail. */
      const char *tail = strrchr(work + prefix, '/');
      const char *name = tail ? tail + 1 : work + prefix;
      int budget = pixels - (int)(prefix * (p6m.mfs / 2)) - 3 * (p6m.mfs / 2);
      size_t n = strlen(name);
      if (budget > 0)
      {
         size_t start = n;
         int used = 0;
         while (start && used + p6m.mfs / 2 <= budget)
         { --start; used += p6m.mfs / 2; }
         snprintf(work, sizeof(work), "%.*s...%s", (int)prefix, src, name + start);
      }
      menu_path_fit(dst, cap, work, pixels);
   }
}

/* Prefer recognizable filename prefixes in filesystem directories; preserve
 * the old suffix-oriented display for ZIP archive members. */
static void menu_explorer_fit(char *dst, size_t cap, const char *src,
                              int pixels, int in_zip)
{
   size_t n = strlen(src);
   int chars = pixels / (p6m.mfs / 2);
   if (menu_text_pixels(src) <= pixels)
   { menu_path_fit(dst, cap, src, pixels); return; }
   if (chars > 6 && n > (size_t)chars)
   {
      size_t keep = (size_t)chars - 3;
      if (in_zip)
         snprintf(dst, cap, "...%s", src + n - keep);
      else
         snprintf(dst, cap, "%.*s...", (int)keep, src);
   }
   else menu_path_fit(dst, cap, src, pixels);
}

/* Split a displayed path into two pixel-bounded lines at a glyph boundary.
 * Prefer a nearby directory separator or space, without losing characters. */
static const char *menu_wrap_line(char *dst, size_t cap, const char *src,
                                  int pixels)
{
   const unsigned char *p = (const unsigned char *)src;
   const unsigned char *break_at = NULL;
   const unsigned char *end = p;
   int width = 0;
   size_t count;
   while (*end)
   {
      int half = isHankaku(*end);
      int step = half ? 1 : 2;
      int glyph_width = half ? p6m.mfs / 2 : p6m.mfs;
      if (!half && !end[1]) break;
      if (width + glyph_width > pixels) break;
      if (half && (*end == '/' || *end == ' ')) break_at = end + 1;
      width += glyph_width;
      end += step;
   }
   if (*end && break_at && break_at > p + (end - p) / 2 &&
       menu_text_pixels((const char *)break_at) <= pixels)
      end = break_at;
   count = (size_t)(end - p);
   if (count >= cap) count = cap - 1;
   memcpy(dst, src, count);
   dst[count] = 0;
   return src + count;
}

void WinDraw_DrawMenu(int menu_state, int mkey_pos, int mkey_y, int *mval_y)
{
   int i, drv, fs, margin, left, right, separator, text_x, path_x, available;
   int title_top, main_top, main_bottom, help_top, help_bottom;
   int row_top, row_y, row_height, row_selected;
   char tmp[PATH_MAX], line[PATH_MAX];

   p6m.sbp = menu_buffer;
   p6m.mfs = Config.MenuFontSize ? 24 : 16;
   fs = p6m.mfs;
   margin = fs / 2;
   left = margin;
   right = MENU_WIDTH - margin - 1;
   separator = left + fs * 5;
   text_x = left + fs;
   path_x = separator + fs / 2;
   available = right - margin - path_x;
   title_top = margin;
   main_top = 4 * fs + margin;
   /* SYSTEM uses one line; each of the four drives uses two lines. */
   main_bottom = main_top + 14 * fs;
   help_top = main_bottom + fs;
   help_bottom = help_top + 3 * fs;

   /* Redraw the complete frame to remove text from the previous selection. */
   memset(menu_buffer, 0, sizeof(menu_buffer));
   p6m.mbcolor = 0;
   menu_box(left, title_top, right, title_top + 3 * fs, 0x07ff);
   menu_box(left, main_top, right, main_bottom, 0xffe0);
   menu_line(separator, main_top, separator, main_bottom, 0xffe0);
   menu_box(left, help_top, right, help_bottom, 0x07ff);

   p6m.mcolor = 0xffff;
   set_mlocateC(0, 1);
   p6m.ml_x = text_x;
   snprintf(tmp, sizeof(tmp), "SWITCH for PX68K Version %s", PX68K_MENU_VERSION);
   if ((int)strlen(tmp) * (fs / 2) > right - text_x - margin)
      tmp[(right - text_x - margin) / (fs / 2)] = '\0';
   draw_str(tmp);

   row_top = main_top;
   for (i = 0; i < 5; ++i)
   {
      row_height = (i == 0) ? 2 * fs : 3 * fs;
      row_y = row_top + fs / 2;
      row_selected = i == mkey_y - mkey_pos;
      p6m.mcolor = 0xffff;
      p6m.mbcolor = 0;
      if (menu_state == MS_KEY && row_selected)
      {
         p6m.mcolor = 0;
         p6m.mbcolor = 0xffe0;
      }
      p6m.ml_y = row_y;
      p6m.ml_x = text_x;
      draw_str(menu_item_key[i + mkey_pos]);
      p6m.mcolor = 0xffff;
      p6m.mbcolor = 0;
      if ((menu_state == MS_VALUE || menu_state == MS_HWJOY_SET) && row_selected)
      {
         p6m.mcolor = 0;
         p6m.mbcolor = 0xffe0;
      }
      p6m.ml_y = row_y;
      p6m.ml_x = path_x;
      drv = WinUI_get_drv_num(i + mkey_pos);
      if (drv >= 0 && mval_y[i + mkey_pos] == 0)
      {
         const char *path = drv < 2 ? Config.FDDImage[drv] : Config.HDImage[drv - 2];
         if (!path[0])
            draw_str(" -- no disk --");
         else
         {
            const char *remaining;
            if (!strncmp(cur_dir_str, path, cur_dir_slen))
               path += cur_dir_slen;
            /* Keep the complete path when it fits on two lines. */
            if (menu_text_pixels(path) <= available * 2 &&
                strlen(path) < sizeof(tmp))
               strcpy(tmp, path);
            else
               menu_path_compact(tmp, sizeof(tmp), path, available * 2);
            remaining = menu_wrap_line(line, sizeof(line), tmp, available);
            draw_str(line);
            if (*remaining)
            {
               p6m.ml_y = row_y + fs;
               p6m.ml_x = path_x;
               menu_path_fit(line, sizeof(line), remaining, available);
               draw_str(line);
            }
         }
      }
      else
         draw_str(menu_items[i + mkey_pos][mval_y[i + mkey_pos]]);
      row_top += row_height;
      if (i < 4)
         menu_line(left, row_top, right, row_top, 0xffe0);
   }
   p6m.mcolor = 0xffff;
   p6m.mbcolor = 0;
   p6m.ml_y = help_top + fs / 2;
   p6m.ml_x = text_x;
   draw_str(menu_item_desc[mkey_y]);
   videoBuffer = (uint16_t *)menu_buffer;
}

/* Number of explorer rows available without an expanded filename. */
int WinDraw_ExplorerRows(void)
{
   int fs = Config.MenuFontSize ? 24 : 16;
   return (600 - 2 * fs - fs) / fs;
}

void WinDraw_DrawMenufile(struct menu_flist *mfl)
{
   int i, fs = p6m.mfs;
   int margin = fs / 2;
   int left = margin, right = MENU_WIDTH - margin - 1;
   int top = fs, bottom = 600 - fs;
   int text_x = left + fs / 2;
   int available = right - margin - text_x - fs / 2;
   int rows = WinDraw_ExplorerRows();
   int preview = 0, preview_rows = 0;
   int selected;
   char ptr[PATH_MAX];

   selected = mfl->ptr + mfl->y;
   if (selected >= 0 && selected < mfl->num)
   {
      const char *name = mfl->name[selected];
      int directory = mfl->type[selected];
      menu_explorer_fit(ptr, sizeof(ptr), name, available - (directory ? fs : 0),
                        (!strncmp(mfl->dir[0], "zipbrowse://", 12) ||
                         !strncmp(mfl->dir[1], "zipbrowse://", 12)));
      preview = strcmp(ptr, name) != 0;
   }
   /* Only shortened selections consume preview space. */
   if (preview)
   {
      preview_rows = 3;
      rows -= preview_rows + 1;
   }
   if (rows < 1) rows = 1;
   /* Keep the selected absolute index while adapting the scroll window. */
   if (selected >= mfl->num) selected = mfl->num - 1;
   if (selected < 0) selected = 0;
   if (selected < mfl->ptr) mfl->ptr = selected;
   if (selected >= mfl->ptr + rows) mfl->ptr = selected - rows + 1;
   if (mfl->ptr < 0) mfl->ptr = 0;
   mfl->y = selected - mfl->ptr;

   memset(menu_buffer, 0, sizeof(menu_buffer));
   menu_box(left, top, right, bottom, 0xffff);
   for (i = 0; i < rows && i + mfl->ptr < mfl->num; ++i)
   {
      const char *name = mfl->name[i + mfl->ptr];
      int directory = mfl->type[i + mfl->ptr];
      p6m.mcolor = (i == mfl->y) ? 0 : 0xffff;
      p6m.mbcolor = (i == mfl->y) ? 0xffff : 0x1;
      set_mlocateC(0, i + 2);
      p6m.ml_x = text_x;
      if (directory) draw_str("[");
      menu_explorer_fit(ptr, sizeof(ptr), name, available - (directory ? fs : 0),
                        (!strncmp(mfl->dir[0], "zipbrowse://", 12) ||
                         !strncmp(mfl->dir[1], "zipbrowse://", 12)));
      draw_str(ptr);
      if (directory) draw_str("]");
   }
   if (preview && selected < mfl->num)
   {
      const unsigned char *scan = (const unsigned char *)mfl->name[selected];
      int row = (bottom / fs) - preview_rows;
      int maxrow = row + preview_rows - 1;
      int x = text_x;
      int limit = right - margin;
      char piece[3];
      p6m.mcolor = 0xffff;
      p6m.mbcolor = 0;
      menu_line(left + 1, row * fs - fs / 2, right - 1, row * fs - fs / 2, 0xffff);
      while (*scan && row <= maxrow)
      {
         int bytes = isHankaku(*scan) ? 1 : (scan[1] ? 2 : 1);
         int width = bytes == 1 ? fs / 2 : fs;
         if (x + width > limit)
         {
            row++;
            x = text_x;
            if (row > maxrow) break;
         }
         piece[0] = (char)*scan++;
         if (bytes == 2) piece[1] = (char)*scan++;
         piece[bytes] = '\0';
         set_mlocateC(0, row);
         p6m.ml_x = x;
         draw_str(piece);
         x = p6m.ml_x;
      }
   }
   p6m.mbcolor = 0;
   videoBuffer = (uint16_t *)menu_buffer;
}

void WinDraw_ClearMenuBuffer(void)
{
	memset(menu_buffer, 0, 800*600*2);
}
