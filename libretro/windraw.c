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
extern float retro_aspect;
extern int CHANGEAV;

/* Set to 0 to get the old behaviour (every window stretched to 4:3). */
#ifndef PX68K_TIMING_ASPECT
#define PX68K_TIMING_ASPECT 1
#endif

/*
 * Physical size of the active picture on a 4:3 monitor, as a fraction of the
 * full screen, derived from the CRTC and dot clock timing (see CRTC_Model in
 * crtc.c).  This is what the "X68000" aspect ratio option uses:
 *
 *   width  = window time / visible scan time of the monitor
 *   height = lines / 256 (or / 512 for real 512-line frames)
 *
 * so e.g. Cotton "512*256 Hi" (a 384 dot window on a 512 dot clock) fills 75%
 * of the width, and a 224 line picture fills 87.5% of the height.  Checked
 * against XM6 type G for 22 modes of 8 games.
 */
extern int px68k_aspect_mode;

static float WinDraw_TimingAspect(uint32_t height)
{
	float wf, hf;

	if (px68k_aspect_mode == 1)
		return 4.0f / 3.0f;
	if (px68k_aspect_mode == 2)
		return (TextDotX && height) ? (float)TextDotX / (float)height : 4.0f / 3.0f;
#if PX68K_TIMING_ASPECT
	/* Width: share of the monitor's visible scan time that the picture
	 * occupies, from the dot clock (see CRTC_Model). */
	wf = CRTC_WidthFraction();
	if (wf <= 0.0f)
	{
		/* timing not modelled: fall back to window / total line length */
		uint32_t htotal = ((((uint32_t)CRTC_Regs[0]) << 8) | CRTC_Regs[1]) + 1;
		if (htotal < 8 || TextDotX < 8)
			return 4.0f / 3.0f;
		wf = ((float)(TextDotX / 8) / (float)htotal) /
		     ((VLINE_TOTAL < 400) ? (32.0f / 38.0f) : (32.0f / 46.0f));
	}
	if (wf > 1.0f)  wf = 1.0f;   /* overscan: fill the screen */
	if (wf < 0.25f) wf = 0.25f;  /* transitional register values */

	/* Height: a picture of N lines fills N/256 of the screen height
	 * (N/512 for real 512-line frames); e.g. 224 lines = 87.5%. */
	hf = (float)height / CRTC_FullHeightLines();
	if (hf > 1.0f)  hf = 1.0f;
	if (hf < 0.25f) hf = 0.25f;

	return (4.0f / 3.0f) * wf / hf;
#else
	return 4.0f / 3.0f;
#endif
}

void FASTCALL WinDraw_Draw(void)
{
	static int oldtextx = -1, oldtexty = -1;
	static int oldaspect = -1;
	uint32_t ey;
	int asp;

	/* Lines the frame loop can really produce: it stops at VLINE_TOTAL, so a
	 * window whose VEND lies beyond VTOTAL (Cotton "448*240Low": VEND=275,
	 * VTOTAL=259) is shorter than VEND-VSTART. */
	ey = TextDotY;
	if (CRTC_VEND > VLINE_TOTAL && VLINE_TOTAL > CRTC_VSTART)
		ey = (((uint32_t)VLINE_TOTAL - CRTC_VSTART) * CRTC_VStep) / 2;

	if (oldtextx != (int)TextDotX)
	{
		oldtextx = TextDotX;
		CHANGEAV=1;
	}
	if (oldtexty != (int)ey)
	{
		oldtexty = ey;
		CHANGEAV=1;
	}
	retro_aspect = WinDraw_TimingAspect(ey);
	asp = (int)(retro_aspect * 1000.0f);
	if (oldaspect != asp)
	{
		oldaspect = asp;
		CHANGEAV=1;
	}

	if (CHANGEAV==1)
	{
		/* Clamp only frontend presentation dimensions; do not alter CRTC state. */
        retrow = TextDotX > 800 ? 800 : TextDotX;
        retroh = ey > 600 ? 600 : ey;
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
	else if ( ((VCReg1[0]&0x03)==0x01)&&(tron)&&(VCReg2[0]&0x10) )
	{
		/* MAME keeps ordinary pixels from the special-priority GRP
		 * layer in the graphics bitmap at that layer's priority.
		 * They must be drawn even when no second GRP layer is enabled
		 * (gon is false); only the bit-0 pixels remain deferred to
		 * WinDraw_DrawPriLine(). */
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


/* Original Sharp X68000 software-keyboard geometry (800x218). */
struct osd_keyboard_key
{
   int x, y, w, h;
   char *label;
   uint8_t code;
};

static struct osd_keyboard_key osd_keyboard_keys[] = {
#include "keytbl.inc"
};

#define OSD_KEYBOARD_X_ORIGIN 17
#define OSD_KEYBOARD_Y_ORIGIN 191
#define OSD_KEYBOARD_Y_OFFSET 98
#define OSD_KB_REGION_Y (188 + OSD_KEYBOARD_Y_OFFSET)
#define OSD_KB_REGION_H 226

static void osd_fill_rect(uint16_t *dst, int pitch, int x, int y,
      int w, int h, uint16_t color)
{
   int row, col;
   for (row = 0; row < h; row++)
      for (col = 0; col < w; col++)
         dst[(y + row) * pitch + x + col] = color;
}

static int osd_scale_x(int value)
{
   /* Fit the original 800-wide keyboard drawing into the 768-wide output. */
   return (value * 24 + 12) / 25;
}

/* Number of entries in the key table (the table is terminated by x < 0). */
int WinDraw_OSDKeyCount(void)
{
   static int count = -1;
   if (count < 0)
   {
      count = 0;
      while (osd_keyboard_keys[count].x >= 0)
         count++;
   }
   return count;
}

/* Rectangle and X68000 scancode of a key in 768x512 output coordinates.
 * This is the single source of truth shared by the renderer, the gamepad
 * navigation and the pointer hit test. Any output pointer may be NULL.
 * A scancode of 0 marks a layout dummy (not a real key). */
int WinDraw_OSDKeyInfo(int index, int *x, int *y, int *w, int *h,
      uint8_t *code)
{
   const struct osd_keyboard_key *k;
   if (index < 0 || index >= WinDraw_OSDKeyCount())
      return 0;
   k = &osd_keyboard_keys[index];
   if (x)    *x    = osd_scale_x(OSD_KEYBOARD_X_ORIGIN + k->x);
   if (y)    *y    = OSD_KEYBOARD_Y_ORIGIN + OSD_KEYBOARD_Y_OFFSET + k->y;
   if (w)    *w    = osd_scale_x(k->w);
   if (h)    *h    = k->h;
   if (code) *code = k->code;
   return 1;
}

/* Alternate legends. Columns: Shift, hiragana, hiragana+Shift, katakana,
 * katakana+Shift, romaji, romaji+Shift. A NULL falls back to the plain
 * legend of that mode (or the base label); an empty string means the key
 * types nothing visible in that layer (Shift+kana on the "_" key types a
 * space, Shift+0 types nothing). The kana layout is the JIS one; the romaji
 * entries are what the guest's romaji input mode really types. Kana strings
 * are Shift-JIS, which is what the ROM-font text renderer expects. */
struct osd_legend
{
   uint8_t code;
   const char *shift, *hira, *hira_shift, *kata, *kata_shift, *rom, *rom_shift;
};

static const struct osd_legend osd_legends[] = {
   { 0x02, "!", "\x82\xca", NULL, "\x83\x6b", NULL, NULL, NULL },
   { 0x03, "\"", "\x82\xd3", NULL, "\x83\x74", NULL, NULL, NULL },
   { 0x04, "#", "\x82\xa0", "\x82\x9f", "\x83\x41", "\x83\x40", NULL, NULL },
   { 0x05, "$", "\x82\xa4", "\x82\xa3", "\x83\x45", "\x83\x44", NULL, NULL },
   { 0x06, "%", "\x82\xa6", "\x82\xa5", "\x83\x47", "\x83\x46", NULL, NULL },
   { 0x07, "&", "\x82\xa8", "\x82\xa7", "\x83\x49", "\x83\x48", NULL, NULL },
   { 0x08, "'", "\x82\xe2", "\x82\xe1", "\x83\x84", "\x83\x83", NULL, NULL },
   { 0x09, "(", "\x82\xe4", "\x82\xe3", "\x83\x86", "\x83\x85", NULL, NULL },
   { 0x0a, ")", "\x82\xe6", "\x82\xe5", "\x83\x88", "\x83\x87", NULL, NULL },
   { 0x0b, "", "\x82\xed", "\x82\xf0", "\x83\x8f", "\x83\x92", NULL, NULL },
   { 0x0c, "=", "\x82\xd9", NULL, "\x83\x7a", NULL, NULL, NULL },
   { 0x0d, "~", "\x82\xd6", NULL, "\x83\x77", NULL, NULL, NULL },
   { 0x0e, "|", "\x81\x5b", NULL, "\x81\x5b", NULL, NULL, NULL },
   { 0x11, NULL, "\x82\xbd", NULL, "\x83\x5e", NULL, NULL, NULL },
   { 0x12, NULL, "\x82\xc4", NULL, "\x83\x65", NULL, NULL, NULL },
   { 0x13, NULL, "\x82\xa2", "\x82\xa1", "\x83\x43", "\x83\x42", NULL, NULL },
   { 0x14, NULL, "\x82\xb7", NULL, "\x83\x58", NULL, NULL, NULL },
   { 0x15, NULL, "\x82\xa9", NULL, "\x83\x4a", NULL, NULL, NULL },
   { 0x16, NULL, "\x82\xf1", NULL, "\x83\x93", NULL, NULL, NULL },
   { 0x17, NULL, "\x82\xc8", NULL, "\x83\x69", NULL, NULL, NULL },
   { 0x18, NULL, "\x82\xc9", NULL, "\x83\x6a", NULL, NULL, NULL },
   { 0x19, NULL, "\x82\xe7", NULL, "\x83\x89", NULL, NULL, NULL },
   { 0x1a, NULL, "\x82\xb9", NULL, "\x83\x5a", NULL, NULL, NULL },
   { 0x1b, "`", "\x81\x4a", NULL, "\x81\x4a", NULL, "\x81\x4a", "\x81\x4b" },
   { 0x1c, "{", "\x81\x4b", "\x81\x75", "\x81\x4b", "\x81\x75", "\x81\x75", NULL },
   { 0x1e, NULL, "\x82\xbf", NULL, "\x83\x60", NULL, NULL, NULL },
   { 0x1f, NULL, "\x82\xc6", NULL, "\x83\x67", NULL, NULL, NULL },
   { 0x20, NULL, "\x82\xb5", NULL, "\x83\x56", NULL, NULL, NULL },
   { 0x21, NULL, "\x82\xcd", NULL, "\x83\x6e", NULL, NULL, NULL },
   { 0x22, NULL, "\x82\xab", NULL, "\x83\x4c", NULL, NULL, NULL },
   { 0x23, NULL, "\x82\xad", NULL, "\x83\x4e", NULL, NULL, NULL },
   { 0x24, NULL, "\x82\xdc", NULL, "\x83\x7d", NULL, NULL, NULL },
   { 0x25, NULL, "\x82\xcc", NULL, "\x83\x6d", NULL, NULL, NULL },
   { 0x26, NULL, "\x82\xe8", NULL, "\x83\x8a", NULL, NULL, NULL },
   { 0x27, "+", "\x82\xea", NULL, "\x83\x8c", NULL, NULL, NULL },
   { 0x28, "*", "\x82\xaf", NULL, "\x83\x50", NULL, NULL, NULL },
   { 0x29, "}", "\x82\xde", "\x81\x76", "\x83\x80", "\x81\x76", "\x81\x76", NULL },
   { 0x2a, NULL, "\x82\xc2", "\x82\xc1", "\x83\x63", "\x83\x62", NULL, NULL },
   { 0x2b, NULL, "\x82\xb3", NULL, "\x83\x54", NULL, NULL, NULL },
   { 0x2c, NULL, "\x82\xbb", NULL, "\x83\x5c", NULL, NULL, NULL },
   { 0x2d, NULL, "\x82\xd0", NULL, "\x83\x71", NULL, NULL, NULL },
   { 0x2e, NULL, "\x82\xb1", NULL, "\x83\x52", NULL, NULL, NULL },
   { 0x2f, NULL, "\x82\xdd", NULL, "\x83\x7e", NULL, NULL, NULL },
   { 0x30, NULL, "\x82\xe0", NULL, "\x83\x82", NULL, NULL, NULL },
   { 0x31, "<", "\x82\xcb", "\x81\x41", "\x83\x6c", "\x81\x41", "\x81\x41", NULL },
   { 0x32, ">", "\x82\xe9", "\x81\x42", "\x83\x8b", "\x81\x42", "\x81\x42", NULL },
   { 0x33, "?", "\x82\xdf", "\x81\x45", "\x83\x81", "\x81\x45", "\x81\x45", NULL },
   { 0x34, "_", "\x82\xeb", "", "\x83\x8d", "", NULL, NULL },
   { 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

/* Pick the legend for a legend layer (see Keyboard_LegendLayer): 0 base,
 * 1 Shift, 2/3 hiragana plain/Shift, 4/5 katakana plain/Shift, 6/7 romaji
 * plain/Shift. */
static const char *osd_label_for(const struct osd_keyboard_key *k, int layer)
{
   const struct osd_legend *l;
   const char *label = k->label;

   if (layer)
   {
      for (l = osd_legends; l->code; l++)
         if (l->code == k->code)
         {
            if (layer >= 6)
            {
               if (layer == 7)
                  label = l->rom_shift ? l->rom_shift
                        : (l->shift ? l->shift : label);
               else
                  label = l->rom ? l->rom : label;
            }
            else
            {
               const char *plain = (layer >= 4) ? l->kata : l->hira;
               const char *shifted = (layer >= 4) ? l->kata_shift : l->hira_shift;
               if (layer == 1)
                  label = l->shift ? l->shift : label;
               else if (layer & 1)
                  label = shifted ? shifted : (plain ? plain : label);
               else
                  label = plain ? plain : label;
            }
            break;
         }
   }
   /* The key left of right Shift only types "_" together with Shift. */
   if (k->code == 0x34 && layer == 0)
      label = "";
   return label;
}

/* The layout is a set of horizontal lines (bands). A key belongs to every
 * band it overlaps by at least 8 pixels, so the tall RET/ENT keys span two
 * lines and the left/right cursor keys straddle two. Gamepad navigation uses
 * this so horizontal travel never leaves the current line. */
#define OSD_LINE_COUNT 6
static const int osd_line_top[OSD_LINE_COUNT] = { 2, 48, 82, 116, 150, 184 };

int WinDraw_OSDKeyLines(int index)
{
   int i, mask = 0;
   if (index < 0 || index >= WinDraw_OSDKeyCount())
      return 0;
   for (i = 0; i < OSD_LINE_COUNT; i++)
   {
      int lo = osd_keyboard_keys[index].y;
      int hi = lo + osd_keyboard_keys[index].h;
      if (osd_line_top[i] > lo) lo = osd_line_top[i];
      if (osd_line_top[i] + 32 < hi) hi = osd_line_top[i] + 32;
      if (hi - lo >= 8)
         mask |= 1 << i;
   }
   return mask;
}

/* ---- Appearance (core options): theme and opacity ---- */
struct osd_theme
{
   uint16_t surround, deck_outer, deck_inner;
   uint16_t face, bevel, highlight;
   uint16_t label, kana_label;
   uint16_t held, selected, led_border;
   /* Optional bevelled rim around the keyboard (flat look): ring colour, light
    * line (top/left), mid line, dark line (bottom). rim == 0 uses the plain
    * surround/deck rectangles. */
   int rim;
   uint16_t rim_ring, rim_light, rim_mid, rim_dark;
};

static const struct osd_theme osd_themes[2] = {
   /* Light: the original look. */
   { 0x2104, 0x4208, 0x3186,
     0xef7d, 0x0841, 0xffff,
     0x0000, 0x0014,
     0xfd20, 0xbf1f, 0x0841,
     0, 0, 0, 0, 0 },
   /* Dark: near-black keys with white legends, a darker bottom/right bevel
    * and a faint top highlight, inside a bevelled rim around a black deck. */
   { 0x1082, 0x1082, 0x1082,
     0x2104, 0x18c3, 0x2945,
     0xffff, 0x7d5f,
     0xb2c0, 0x0190, 0x4a49,
     1, 0x18c3, 0x5acb, 0x2945, 0x0000 }
};

/* Selection cursor colours (core options). Mode 0: theme default, 1: off,
 * 2: custom colour. */
/* Defaults match the core options: pale yellow tint, white frame. */
static int osd_tint_mode = 2, osd_frame_mode = 2;
static uint16_t osd_tint_color = 0xff93, osd_frame_color = 0xffff;

static int osd_parse_cursor(const char *value, uint16_t *color)
{
   unsigned long n;
   unsigned r, g, b;
   if (!value || !strcmp(value, "default"))
      return 0;
   if (!strcmp(value, "off"))
      return 1;
   n = strtoul(value, NULL, 16);
   r = (unsigned)((n >> 16) & 0xff);
   g = (unsigned)((n >> 8) & 0xff);
   b = (unsigned)(n & 0xff);
   *color = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
   return 2;
}

/* Values are "default", "off" or an RRGGBB hex colour. */
void WinDraw_SetOSDKeyboardCursor(const char *tint, const char *frame)
{
   osd_tint_mode  = osd_parse_cursor(tint,  &osd_tint_color);
   osd_frame_mode = osd_parse_cursor(frame, &osd_frame_color);
}

/* Black or white legend, whichever reads better on the given face colour. */
static uint16_t osd_contrast(uint16_t c)
{
   unsigned r = (((c >> 11) & 31) * 255) / 31;
   unsigned g = (((c >> 5) & 63) * 255) / 63;
   unsigned b = ((c & 31) * 255) / 31;
   return ((r * 30 + g * 59 + b * 11) / 100 > 140) ? 0x0000 : 0xffff;
}

/* Defaults match the core options: dark theme, 3D look. */
static int osd_theme_index = 1;
static int osd_opacity = 100;
static int osd_look = 1; /* 0 flat, 1 3D */
static int osd_jp_labels = 0; /* special keys with their Japanese captions */

void WinDraw_SetOSDKeyboardLabels(int japanese)
{
   osd_jp_labels = japanese ? 1 : 0;
}

static int osd_feedback = 2; /* press feedback: 0 off, 1 flash, 2 flash + key press */

void WinDraw_SetOSDKeyboardFeedback(int mode)
{
   osd_feedback = mode < 0 ? 0 : (mode > 2 ? 2 : mode);
}

/* Mix two RGB565 colours, t = 0..255 (0 = a, 255 = b). */
static uint16_t osd_blend565(uint16_t a, uint16_t b, int t)
{
   int r = ((a >> 11) & 31) + ((((b >> 11) & 31) - ((a >> 11) & 31)) * t) / 255;
   int g = ((a >> 5) & 63) + ((((b >> 5) & 63) - ((a >> 5) & 63)) * t) / 255;
   int bl = (a & 31) + (((b & 31) - (a & 31)) * t) / 255;
   return (uint16_t)((r << 11) | (g << 5) | bl);
}

void WinDraw_SetOSDKeyboardStyle(int opacity_percent, int dark_theme,
      int look_3d)
{
   osd_look = look_3d ? 1 : 0;
   if (opacity_percent < 0) opacity_percent = 0;
   if (opacity_percent > 100) opacity_percent = 100;
   osd_opacity = opacity_percent;
   osd_theme_index = dark_theme ? 1 : 0;
}

/* Cursor-key arrows are drawn, not typed: a 16 pixel arrow (triangular head
 * plus shaft) built for "up" and mirrored/rotated for the other directions.
 * code: 0x3b left, 0x3c up, 0x3d right, 0x3e down. cx/cy is the centre. */
static void osd_draw_arrow(uint16_t *dst, int cx, int cy, int code,
      uint16_t color)
{
   int dx, dy;
   for (dy = -8; dy <= 8; dy++)
      for (dx = -8; dx <= 8; dx++)
      {
         int u, v;
         switch (code)
         {
            case 0x3b: u = dy;  v = dx;  break; /* left  */
            case 0x3d: u = dy;  v = -dx; break; /* right */
            case 0x3e: u = dx;  v = -dy; break; /* down  */
            default:   u = dx;  v = dy;  break; /* up    */
         }
         if (u < 0) u = -u;
         if ((v >= -8 && v <= -1 && u <= v + 8) ||  /* head */
             (v >= 0 && v <= 7 && u <= 2))          /* shaft */
            dst[(cy + dy) * 800 + cx + dx] = color;
      }
}

static void osd_draw_frame(uint16_t *dst, int x, int y, int w, int h,
      int t, uint16_t color)
{
   osd_fill_rect(dst, 800, x, y, w, t, color);
   osd_fill_rect(dst, 800, x, y + h - t, w, t, color);
   osd_fill_rect(dst, 800, x, y, t, h, color);
   osd_fill_rect(dst, 800, x + w - t, y, t, h, color);
}

/* ---- 3D look ----
 * Raised keycaps built from a dark skirt, a chamfer ring (light on the top
 * and left, dark on the bottom and right, with a small specular tick in the
 * corner) and a face with a soft vertical gradient and a slightly darker
 * recessed "dish" under the legend, on a grey body around a black deck.
 * Gradients are ordered-dithered so they do not band in RGB565. */
struct osd_look3d
{
   uint32_t body_top, body_bottom, body_light, body_dark, deck, skirt;
   uint32_t ring_light, ring_dark, spec, face_top, face_bottom;
   int dish_percent;
};

static const struct osd_look3d osd_looks3d[2] = {
   /* Light theme */
   { 0x4a4a4a, 0x383838, 0x8c8c8c, 0x161616, 0x1e1e1e, 0x707070,
     0xffffff, 0xa8a8a8, 0xffffff, 0xf4f4f4, 0xd6d6d6, 95 },
   /* Dark theme, after the reference prototype */
   { 0x343434, 0x2a2a2a, 0x585858, 0x141414, 0x080808, 0x040404,
     0x5c5c5c, 0x181818, 0xc4c4c4, 0x323232, 0x3e3e3e, 86 }
};

static const unsigned char osd_bayer4[4][4] = {
   {  0,  8,  2, 10 },
   { 12,  4, 14,  6 },
   {  3, 11,  1,  9 },
   { 15,  7, 13,  5 }
};

static int osd_clamp8(int v)
{
   return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static uint16_t osd_pack(int r, int g, int b)
{
   r = osd_clamp8(r);
   g = osd_clamp8(g);
   b = osd_clamp8(b);
   return (uint16_t)((((r * 31 + 127) / 255) << 11) |
         (((g * 63 + 127) / 255) << 5) | ((b * 31 + 127) / 255));
}

static uint16_t osd_pack_dither(int x, int y, int r, int g, int b)
{
   int t = osd_bayer4[y & 3][x & 3];
   r = osd_clamp8(r + (t >> 1));
   g = osd_clamp8(g + (t >> 2));
   b = osd_clamp8(b + (t >> 1));
   return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static uint16_t osd_pack24(uint32_t c)
{
   return osd_pack((int)((c >> 16) & 0xff), (int)((c >> 8) & 0xff),
         (int)(c & 0xff));
}

static uint16_t osd_avg565(uint16_t a, uint16_t b)
{
   return (uint16_t)(((a & 0xf7de) >> 1) + ((b & 0xf7de) >> 1));
}

static int osd_lerp(int a, int b, int pos, int span)
{
   return span <= 1 ? a : a + ((b - a) * pos) / (span - 1);
}

/* Coverage of pixel (xx,yy) of a w x h rectangle with corner radius r (0-2):
 * 0 outside, 1 half covered (blended), 2 fully covered. */
static int osd_corner(int w, int h, int r, int xx, int yy)
{
   int ex = xx < w - 1 - xx ? xx : w - 1 - xx;
   int ey = yy < h - 1 - yy ? yy : h - 1 - yy;
   if (r <= 0 || ex >= r || ey >= r)
      return 2;
   if (r == 1)
      return 1;
   if (ex + ey == 0)
      return 0;
   if (ex + ey == 1)
      return 1;
   return 2;
}

static void osd_put(uint16_t *dst, int x, int y, uint16_t c, int cover)
{
   if (cover == 2)
      dst[y * 800 + x] = c;
   else if (cover == 1)
      dst[y * 800 + x] = osd_avg565(dst[y * 800 + x], c);
}

static void osd_round_fill(uint16_t *dst, int x, int y, int w, int h, int r,
      uint16_t color)
{
   int xx, yy;
   for (yy = 0; yy < h; yy++)
      for (xx = 0; xx < w; xx++)
         osd_put(dst, x + xx, y + yy, color, osd_corner(w, h, r, xx, yy));
}

static void osd_draw_body_3d(uint16_t *dst, int y_offset,
      const struct osd_look3d *lk)
{
   /* The frame is widened by 4 pixels on each side (there is spare room
    * there); vertically it must stay inside the keyboard area. */
   int bx = osd_scale_x(14) - 4, by = 188 + y_offset;
   int bw = osd_scale_x(772) + 8, bh = 226;
   int xx, yy;
   uint16_t light = osd_pack24(lk->body_light);
   uint16_t dark  = osd_pack24(lk->body_dark);

   for (yy = 0; yy < bh; yy++)
   {
      int r = osd_lerp((int)((lk->body_top >> 16) & 0xff),
            (int)((lk->body_bottom >> 16) & 0xff), yy, bh);
      int g = osd_lerp((int)((lk->body_top >> 8) & 0xff),
            (int)((lk->body_bottom >> 8) & 0xff), yy, bh);
      int b = osd_lerp((int)(lk->body_top & 0xff),
            (int)(lk->body_bottom & 0xff), yy, bh);
      for (xx = 0; xx < bw; xx++)
         osd_put(dst, bx + xx, by + yy,
               osd_pack_dither(bx + xx, by + yy, r, g, b),
               osd_corner(bw, bh, 2, xx, yy));
   }
   /* Edge lines: light on the top/left, dark on the bottom/right. */
   for (xx = 2; xx < bw - 2; xx++)
   {
      dst[by * 800 + bx + xx] = light;
      dst[(by + bh - 1) * 800 + bx + xx] = dark;
   }
   for (yy = 2; yy < bh - 2; yy++)
   {
      dst[(by + yy) * 800 + bx] = light;
      dst[(by + yy) * 800 + bx + bw - 1] = dark;
   }
   /* Black deck the keys sit in. */
   osd_round_fill(dst, osd_scale_x(17), 191 + y_offset, osd_scale_x(766), 218,
         2, osd_pack24(lk->deck));
}

/* One keycap. base != 0 uses the 565 colour in "face" (held, selected or a
 * custom tint) instead of the theme's face gradient. The face rectangle is
 * returned so legends and arrows can be centred in it. */
static void osd_draw_key_3d(uint16_t *dst, int x, int y, int w, int h,
      const struct osd_look3d *lk, int base, uint16_t face,
      int flash, uint16_t flash_color, int sink,
      int *fx, int *fy, int *fw, int *fh)
{
   int tall = (h >= 24);
   /* A pressed key sinks one pixel into its skirt. */
   int cx = x + 1, cy = y + 1 + sink, cw = w - 3, ch = tall ? h - 4 : h - 3;
   int ring = tall ? 2 : 1; /* chamfer width; short keys need the room */
   int ax = cx + ring, ay = cy + ring;
   int aw = cw - 2 * ring, ah = ch - 2 * ring;
   int tr, tg, tb, br, bg, bb, xx, yy;
   int use_dish = (ah >= 18 && aw >= 14);
   int lr, lg, lb, dr, dg, db;
   uint16_t spec = osd_pack24(lk->spec);

   if (base)
   {
      int r = (((face >> 11) & 31) * 255) / 31;
      int g = (((face >> 5) & 63) * 255) / 63;
      int b = ((face & 31) * 255) / 31;
      tr = r * 88 / 100;  tg = g * 88 / 100;  tb = b * 88 / 100;
      br = r * 108 / 100 + 4; bg = g * 108 / 100 + 4; bb = b * 108 / 100 + 4;
   }
   else
   {
      tr = (int)((lk->face_top >> 16) & 0xff);
      tg = (int)((lk->face_top >> 8) & 0xff);
      tb = (int)(lk->face_top & 0xff);
      br = (int)((lk->face_bottom >> 16) & 0xff);
      bg = (int)((lk->face_bottom >> 8) & 0xff);
      bb = (int)(lk->face_bottom & 0xff);
   }

   if (flash > 0)
   {
      /* Press feedback: blend the face gradient towards the flash colour. */
      int r = (((flash_color >> 11) & 31) * 255) / 31;
      int g = (((flash_color >> 5) & 63) * 255) / 63;
      int b = ((flash_color & 31) * 255) / 31;
      tr += (r * 88 / 100 - tr) * flash / 255;
      tg += (g * 88 / 100 - tg) * flash / 255;
      tb += (b * 88 / 100 - tb) * flash / 255;
      br += (r * 108 / 100 + 4 - br) * flash / 255;
      bg += (g * 108 / 100 + 4 - bg) * flash / 255;
      bb += (b * 108 / 100 + 4 - bb) * flash / 255;
   }

   /* Skirt, then the cap filled with the dark chamfer colour. */
   osd_round_fill(dst, x, y, w, h, 2, osd_pack24(lk->skirt));
   osd_round_fill(dst, cx, cy, cw, ch, 1, osd_pack24(lk->ring_dark));

   /* Chamfer: outer light line on the top/left, outer dark line on the
    * bottom/right; with a 2 pixel chamfer an inner, softer line follows. */
   lr = (int)((lk->ring_light >> 16) & 0xff);
   lg = (int)((lk->ring_light >> 8) & 0xff);
   lb = (int)(lk->ring_light & 0xff);
   dr = (int)((lk->ring_dark >> 16) & 0xff);
   dg = (int)((lk->ring_dark >> 8) & 0xff);
   db = (int)(lk->ring_dark & 0xff);
   for (xx = 1; xx < cw - 1; xx++)
      dst[cy * 800 + cx + xx] = osd_pack24(lk->ring_light);
   for (yy = 1; yy < ch - 1; yy++)
      dst[(cy + yy) * 800 + cx] = osd_pack24(lk->ring_light);
   if (ring == 2)
   {
      uint16_t l1 = osd_pack((lr + tr) / 2, (lg + tg) / 2, (lb + tb) / 2);
      uint16_t d1 = osd_pack((dr + br) / 2, (dg + bg) / 2, (db + bb) / 2);
      for (xx = 1; xx < cw - 2; xx++)
         dst[(cy + 1) * 800 + cx + xx] = l1;
      for (yy = 1; yy < ch - 2; yy++)
         dst[(cy + yy) * 800 + cx + 1] = l1;
      for (xx = 2; xx < cw - 1; xx++)
         dst[(cy + ch - 2) * 800 + cx + xx] = d1;
      for (yy = 2; yy < ch - 1; yy++)
         dst[(cy + yy) * 800 + cx + cw - 2] = d1;
   }

   for (yy = 0; yy < ah; yy++)
   {
      int r = osd_lerp(tr, br, yy, ah);
      int g = osd_lerp(tg, bg, yy, ah);
      int b = osd_lerp(tb, bb, yy, ah);
      for (xx = 0; xx < aw; xx++)
      {
         int pr = r, pg = g, pb = b;
         if (use_dish && xx >= 2 && xx < aw - 2 && yy >= 2 && yy < ah - 2 &&
               osd_corner(aw - 4, ah - 4, 1, xx - 2, yy - 2) == 2)
         {
            pr = r * lk->dish_percent / 100;
            pg = g * lk->dish_percent / 100;
            pb = b * lk->dish_percent / 100;
         }
         dst[(ay + yy) * 800 + ax + xx] =
               osd_pack_dither(ax + xx, ay + yy, pr, pg, pb);
      }
   }

   /* Specular tick in the top-left corner of the chamfer. */
   osd_put(dst, cx + 2, cy, spec, 2);
   osd_put(dst, cx + 1, cy + 1, spec, 2);
   osd_put(dst, cx, cy + 2, spec, 1);

   *fx = ax; *fy = ay; *fw = aw; *fh = ah;
}

/* Keycap LED: a small glossy lens straddling the bottom edge of the key. */
static void osd_draw_led_3d(uint16_t *dst, int x, int y, int w, int h,
      int green, int lit)
{
   int bx = x + w / 2 - 8, by = y + h - 7, xx, yy;
   int tr, tg, tb, br, bg, bb, dr, dg, db;

   if (green)
   {
      if (lit) { tr = 60; tg = 255; tb = 40; br = 10; bg = 190; bb = 2; dr = 250; dg = 255; db = 250; }
      else     { tr = 18; tg = 96;  tb = 18; br = 10; bg = 62;  bb = 10; dr = 84;  dg = 120; db = 84; }
   }
   else
   {
      if (lit) { tr = 255; tg = 44; tb = 40; br = 190; bg = 6; bb = 5; dr = 255; dg = 235; db = 238; }
      else     { tr = 104; tg = 14; tb = 14; br = 66;  bg = 8; bb = 8; dr = 120; dg = 84;  db = 84; }
   }
   osd_round_fill(dst, bx, by, 16, 8, 1, osd_pack(br * 40 / 100, bg * 40 / 100,
         bb * 40 / 100));
   for (yy = 0; yy < 6; yy++)
   {
      uint16_t c = osd_pack(osd_lerp(tr, br, yy, 6), osd_lerp(tg, bg, yy, 6),
            osd_lerp(tb, bb, yy, 6));
      for (xx = 0; xx < 14; xx++)
         dst[(by + 1 + yy) * 800 + bx + 1 + xx] = c;
   }
   for (yy = 3; yy <= 4; yy++)
      for (xx = 6; xx <= 9; xx++)
         dst[(by + yy) * 800 + bx + xx] = osd_pack(dr, dg, db);
}

/* Return-key glyph: stem on the right, shaft to the left, arrow head. */
static void osd_draw_return(uint16_t *dst, int cx, int cy, uint16_t color)
{
   int dx, dy;
   for (dy = -7; dy <= 7; dy++)
      for (dx = -8; dx <= 8; dx++)
      {
         int stem  = (dx >= 4 && dx <= 6 && dy >= -6 && dy <= 2);
         int shaft = (dy >= 0 && dy <= 2 && dx >= -4 && dx <= 6);
         int head  = (dx >= -8 && dx <= -3 &&
               (dy - 1 >= 0 ? dy - 1 : 1 - dy) <= dx + 8);
         if (stem || shaft || head)
            dst[(cy + dy) * 800 + cx + dx] = color;
      }
}

/* Bevelled rim of the flat dark look. Measured from the reference: the ring
 * colour fills the frame; the top has a light line on row 1 and a mid line on
 * row 2, the left a light line on column 0 and a mid line on column 1, the
 * right a mid line on its outermost column, and the bottom a mid line then a
 * dark line. The corners are slightly rounded: pixels outside the rounding are
 * left untouched so the game shows through, and the anti-aliased ones are
 * blended. The deck is the rectangle inside. */
static void osd_draw_rim_flat(uint16_t *dst, int y_offset,
      const struct osd_theme *th)
{
   int bx = osd_scale_x(14), by = 188 + y_offset;
   int bw = osd_scale_x(772), bh = 226;
   int x, y, i;
   /* Pixels outside the rounded corners (restored) and half-covered ones. */
   static const int outside[][2] = { {0,0}, {1,0}, {0,1} };
   int out_n = 3;
   uint16_t saved[8];
   int ox[8], oy[8], n = 0;
   int aax[3], aay[3];
   uint16_t aabg[3];

   for (i = 0; i < out_n; i++)
   {
      ox[n] = outside[i][0]; oy[n] = outside[i][1]; n++;
   }
   ox[n] = bw - 1; oy[n] = 0; n++;                 /* top-right */
   ox[n] = 0;      oy[n] = bh - 2; n++;            /* bottom-left */
   ox[n] = 0;      oy[n] = bh - 1; n++;
   ox[n] = 1;      oy[n] = bh - 1; n++;
   for (i = 0; i < n; i++)
      saved[i] = dst[(by + oy[i]) * 800 + bx + ox[i]];
   aax[0] = 2; aay[0] = 0;
   aax[1] = 1; aay[1] = 1;
   aax[2] = 2; aay[2] = bh - 1;
   for (i = 0; i < 3; i++)
      aabg[i] = dst[(by + aay[i]) * 800 + bx + aax[i]];

   osd_fill_rect(dst, 800, bx, by, bw, bh, th->rim_ring);
   for (x = 2; x < bw - 1; x++)
      dst[(by + 1) * 800 + bx + x] = th->rim_light;
   for (x = 1; x <= bw - 4; x++)
      dst[(by + 2) * 800 + bx + x] = th->rim_mid;
   for (y = 3; y <= bh - 3; y++)
      dst[(by + y) * 800 + bx] = th->rim_light;
   for (y = 2; y <= bh - 3; y++)
   {
      dst[(by + y) * 800 + bx + 1] = th->rim_mid;
      dst[(by + y) * 800 + bx + bw - 1] = th->rim_mid;
   }
   for (x = 2; x <= bw - 2; x++)
      dst[(by + bh - 2) * 800 + bx + x] = th->rim_mid;
   for (x = 3; x < bw; x++)
      dst[(by + bh - 1) * 800 + bx + x] = th->rim_dark;

   for (i = 0; i < n; i++)
      dst[(by + oy[i]) * 800 + bx + ox[i]] = saved[i];
   for (i = 0; i < 3; i++)
      dst[(by + aay[i]) * 800 + bx + aax[i]] = osd_avg565(aabg[i],
            dst[(by + aay[i]) * 800 + bx + aax[i]]);

   /* The deck the keys sit in. */
   osd_fill_rect(dst, 800, osd_scale_x(17), 191 + y_offset,
         osd_scale_x(766), 218, th->deck_inner);
}

/* Lighten an RGB565 colour towards white by pct percent. */
static uint16_t osd_lighten565(uint16_t c, int pct)
{
   int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
   r += ((31 - r) * pct) / 100;
   g += ((63 - g) * pct) / 100;
   b += ((31 - b) * pct) / 100;
   return (uint16_t)((r << 11) | (g << 5) | b);
}

/* ---- Japanese captions of the special keys ----
 * The real keycaps of these keys carry Japanese text. Each caption is one or
 * two lines of full-width Shift-JIS characters. The ROM font only has 16x16
 * full-width glyphs, far too large for a 30 pixel key, so they are scaled
 * down (area averaged, alpha blended) to whatever size fits. */
struct osd_special
{
   uint8_t code;
   const char *line1, *line2;
};

static const struct osd_special osd_specials[] = {
   { 0x5a, "\x82\xa9\x82\xc8", NULL },        /* KANA: kana */
   { 0x5b, "\x83\x8d\x81\x5b", "\x83\x7d\x8e\x9a" },          /* ROMAJI: roma-ji */
   { 0x5c, "\x83\x52\x81\x5b\x83\x68", "\x93\xfc\x97\xcd" },          /* CODE: code input */
   { 0x52, "\x8b\x4c\x8d\x86", "\x93\xfc\x97\xcd" },          /* KIGOU: symbol input */
   { 0x53, "\x93\x6f\x98\x5e", NULL },        /* TOUROKU: registration */
   { 0x5f, "\x82\xd0\x82\xe7", "\x82\xaa\x82\xc8" },          /* HIRAGANA */
   { 0x60, "\x91\x53\x8a\x70", NULL },        /* ZENKAKU: full width */
   { 0, NULL, NULL }
};

static const struct osd_special *osd_find_special(uint8_t code)
{
   const struct osd_special *sp;
   for (sp = osd_specials; sp->code; sp++)
      if (sp->code == code)
         return sp;
   return NULL;
}

/* Draw one 16x16 ROM glyph scaled to size x size (8..16), anti-aliased by
 * exact area coverage and blended over whatever is already there. */
static void osd_draw_glyph_scaled(uint16_t *dst, int x, int y, uint16_t sjis,
      int size, uint16_t color)
{
   uint32_t f = get_font_addr(sjis, 16);
   int dx, dy, i, j;

   if (f == (uint32_t)-1 || size < 1)
      return;
   for (dy = 0; dy < size; dy++)
   {
      int ylo = dy * 16, yhi = (dy + 1) * 16;
      for (dx = 0; dx < size; dx++)
      {
         int xlo = dx * 16, xhi = (dx + 1) * 16;
         long ink = 0;
         int alpha;
         for (j = ylo / size; j <= (yhi - 1) / size && j < 16; j++)
         {
            int lo = ylo > j * size ? ylo : j * size;
            int hi = yhi < (j + 1) * size ? yhi : (j + 1) * size;
            int wy = hi - lo;
            uint8_t row0 = FONT[f + j * 2], row1 = FONT[f + j * 2 + 1];
            if (wy <= 0)
               continue;
            for (i = xlo / size; i <= (xhi - 1) / size && i < 16; i++)
            {
               int l2 = xlo > i * size ? xlo : i * size;
               int h2 = xhi < (i + 1) * size ? xhi : (i + 1) * size;
               int wx = h2 - l2;
               int bit = i < 8 ? (row0 >> (7 - i)) & 1 : (row1 >> (15 - i)) & 1;
               if (wx > 0 && bit)
                  ink += (long)wx * wy;
            }
         }
         /* total area per destination pixel is 16 * 16 units; boost a little
          * so thin strokes stay legible after shrinking */
         alpha = (int)((ink * 255 * 5) / (256L * 4));
         if (alpha > 255)
            alpha = 255;
         if (alpha >= 20)
            dst[(y + dy) * 800 + x + dx] =
                  osd_blend565(dst[(y + dy) * 800 + x + dx], color, alpha);
      }
   }
}

/* Draw a special key's caption centred in the key. lock >= 0 means the key
 * has a keycap LED in its bottom rows, which is kept clear. */
static void osd_draw_special(uint16_t *dst, int x, int y, int w, int h,
      int fx, int fy, int fw, int fh, int lock, int sink,
      const struct osd_special *sp, uint16_t color)
{
   int c1 = (int)(strlen(sp->line1) / 2);
   int c2 = sp->line2 ? (int)(strlen(sp->line2) / 2) : 0;
   int maxc = c1 > c2 ? c1 : c2, lines = sp->line2 ? 2 : 1;
   int avail_w = w - 4;
   int avail_h = lock >= 0 ? (y + h - 8) - fy : fh;
   int size = avail_w / maxc, top, n, li;

   if (avail_h / lines < size)
      size = avail_h / lines;
   if (size > 12)
      size = 12;
   if (size < 8)
      size = 8;
   top = fy + (avail_h - lines * size) / 2 + sink;
   for (li = 0; li < lines; li++)
   {
      const char *line = li ? sp->line2 : sp->line1;
      int chars = (int)(strlen(line) / 2);
      int gx = fx + (fw - chars * size) / 2;
      for (n = 0; n < chars; n++)
         osd_draw_glyph_scaled(dst, gx + n * size, top + li * size,
               (uint16_t)(((unsigned char)line[n * 2] << 8) |
               (unsigned char)line[n * 2 + 1]), size, color);
   }
}

static void osd_draw_keyboard(uint16_t *dst, int y_offset)
{
   const struct osd_theme *th = &osd_themes[osd_theme_index];
   int i, x, y, w, h;
   uint8_t code;
   uint16_t old_color, old_bg;
   int old_fs, old_x, old_y;
   int layer;
   const char *label;

   /* Legends follow the emulated keyboard: Shift (physical, Joy2Key or
    * OSD-latched) and the KANA lock. */
   layer = Keyboard_LegendLayer();

   /* Keyboard surround and recessed deck. */
   if (osd_look)
      osd_draw_body_3d(dst, y_offset, &osd_looks3d[osd_theme_index]);
   else if (th->rim)
      osd_draw_rim_flat(dst, y_offset, th);
   else
   {
      osd_fill_rect(dst, 800, osd_scale_x(14), 188 + y_offset,
            osd_scale_x(772), 226, th->surround);
      osd_fill_rect(dst, 800, osd_scale_x(17), 191 + y_offset,
            osd_scale_x(766), 218, th->deck_outer);
      osd_fill_rect(dst, 800, osd_scale_x(20), 194 + y_offset,
            osd_scale_x(760), 212, th->deck_inner);
   }

   old_color = p6m.mcolor;
   old_bg    = p6m.mbcolor;
   old_fs    = p6m.mfs;
   old_x     = p6m.ml_x;
   old_y     = p6m.ml_y;
   p6m.sbp   = dst;
   p6m.mfs   = 16;

   for (i = 0; WinDraw_OSDKeyInfo(i, &x, &y, &w, &h, &code); i++)
   {
      uint16_t face = th->face;
      uint16_t text_color = th->label, kana_color = th->kana_label;
      int state, lock;
      int led_green = (code == 0x5f || code == 0x60); /* HIR, ZEN */
      int fx, fy, fw, fh;  /* area the legend is centred in */
      int state_face = 0;  /* 3D: face colour comes from a state */
      int flash = 0, sink = 0;

      if (code == 0)
         continue;

      /* Bit 0: gamepad/touch selection. Bit 1: key currently held down
       * (or a latched modifier). */
      state = Keyboard_OSDKeyState(i, code);
      lock = Keyboard_LockKeyState(code);
      if (state & 2)
      {
         face = th->held; /* held: orange */
         state_face = 1;
      }
      else if (state & 1)
      {
         /* Selected: theme blue by default, or the chosen tint (Off keeps
          * the normal key colour). */
         if (osd_tint_mode == 0)
         {
            face = th->selected;
            state_face = 1;
         }
         else if (osd_tint_mode == 2)
         {
            face = osd_tint_color;
            state_face = 1;
            text_color = kana_color = osd_contrast(face);
         }
      }

      /* Press feedback: a key touched, clicked or pressed from the pad
       * lights up and fades after release; in mode 2 it also sinks while
       * pressed. A held key (or latched modifier) stays lit. */
      if (osd_feedback > 0)
      {
         flash = (state & 2) ? 0 : Keyboard_OSDKeyFlash(code);
         if (osd_feedback >= 2 && ((state & 2) || flash >= 128))
            sink = 1;
      }

      if (osd_look)
         osd_draw_key_3d(dst, x, y, w, h, &osd_looks3d[osd_theme_index],
               state_face, face, flash, th->held, sink, &fx, &fy, &fw, &fh);
      else
      {
         if (flash > 0)
            face = osd_blend565(face, th->held, flash);
         /* Raised keycap with a dark lower/right bevel; a pressed key has a
          * thinner bevel and no highlight. */
         osd_fill_rect(dst, 800, x, y, w, h, th->bevel);
         osd_fill_rect(dst, 800, x, y, w - 2 + sink, h - 2 + sink, face);
         if (!(state & 2) && !sink)
         {
            /* The accent line follows the selection colours: a lighter shade
             * of the key's tint, or the frame colour when the tint is Off. */
            uint16_t accent = th->highlight;
            if (state & 1)
            {
               /* same fallback as the frame: both Off -> default frame */
               int fm = (osd_frame_mode == 1 && osd_tint_mode == 1)
                     ? 0 : osd_frame_mode;
               if (osd_tint_mode != 1)
                  accent = osd_lighten565(face, 35);
               else if (fm != 1)
                  accent = fm == 2 ? osd_frame_color : 0xf800;
            }
            osd_fill_rect(dst, 800, x + 2, y + 2, w - 6, 2, accent);
         }
         fx = x; fy = y; fw = w - 2 + sink; fh = h - 2 + sink;
      }
      if (state & 1)
      {
         /* Selection frame: red by default; if both the tint and the frame
          * are Off, fall back to the default so the cursor stays visible. */
         int mode = osd_frame_mode;
         if (mode == 1 && osd_tint_mode == 1)
            mode = 0;
         if (mode != 1)
            osd_draw_frame(dst, x, y, w, h, 2,
                  mode == 2 ? osd_frame_color : 0xf800);
      }
      if (lock >= 0 && osd_look)
         osd_draw_led_3d(dst, x, y, w, h, led_green, lock > 0);
      else if (lock >= 0)
      {
         /* Keycap LED of the lock keys, just above the key's bottom edge
          * like the real keyboard. Bright when the OS has lit it: red on
          * KANA, ROMAJI, CODE, CAPS and INS, green on HIRAGANA and ZENKAKU.
          * The key face itself is not tinted. */
         osd_fill_rect(dst, 800, x + w / 2 - 6, y + h - 7, 12, 7, th->led_border);
         osd_fill_rect(dst, 800, x + w / 2 - 5, y + h - 6, 10, 5,
               led_green ? (lock ? 0x07e0 : 0x0320)
                         : (lock ? 0xf800 : 0x6000));
      }

      if (code >= 0x3b && code <= 0x3e)
      {
         osd_draw_arrow(dst, fx + fw / 2, fy + fh / 2, code, text_color);
         continue;
      }
      if (osd_look && code == 0x1d)
      {
         osd_draw_return(dst, fx + fw / 2, fy + fh / 2, text_color);
         continue;
      }

      if (osd_jp_labels && osd_find_special(code))
      {
         osd_draw_special(dst, x, y, w, h, fx, fy, fw, fh, lock, sink,
               osd_find_special(code), text_color);
         continue;
      }

      label = osd_label_for(&osd_keyboard_keys[i], layer);
      if (label && label[0])
      {
         int len = (int)strlen(label);
         /* Kana legends are blue so the active mode is obvious; keys whose
          * legend does not change keep black text. */
         p6m.mcolor = (layer >= 2 && strcmp(label,
               osd_label_for(&osd_keyboard_keys[i], layer & 1)) != 0)
               ? kana_color : text_color;
         if (osd_look)
         {
            /* Transparent glyphs (background colour 0) so the face gradient
             * shows through; centred in the face. */
            p6m.mbcolor = 0;
            p6m.ml_x = fx + (fw - len * 8) / 2;
            p6m.ml_y = fh >= 16 ? fy + (fh - 16) / 2 : fy - (16 - fh) / 2;
            if (lock >= 0)
               p6m.ml_y -= 2; /* leave room for the keycap LED */
         }
         else if (h < 24)
         {
            /* Short keys (F1-F10 are 18 high): a small 8x8 legend, spaces
             * removed, drawn transparently in the 12 rows below the 2 pixel
             * accent line, so it never covers the accent line or the bevel. */
            char small[8];
            int sl = 0, si;
            for (si = 0; label[si] && sl < 7; si++)
               if (label[si] != ' ')
                  small[sl++] = label[si];
            small[sl] = 0;
            p6m.mfs = 8;
            p6m.mbcolor = 0;
            p6m.ml_x = fx + (fw - sl * 8) / 2;
            p6m.ml_y = y + 6 + sink;
            draw_str(small);
            p6m.mfs = 16;
            continue;
         }
         else
         {
            p6m.mbcolor = face;
            p6m.ml_x = x + (w - len * 8) / 2;
            p6m.ml_y = y + (h - 16) / 2;
            p6m.ml_y += sink;
         }
         draw_str((char *)label);
      }
   }

   /* Restore menu text state: this renderer shares the ROM-font machinery. */
   p6m.mcolor = old_color;
   p6m.mbcolor = old_bg;
   p6m.mfs = old_fs;
   p6m.ml_x = old_x;
   p6m.ml_y = old_y;
}

void WinDraw_DrawOSDKeyboard(uint16_t *dst, const uint16_t *src,
      unsigned src_width, unsigned src_height, int fullscreen)
{
   unsigned x, y, draw_w, draw_h, max_h;
   unsigned xoff, yoff;
   unsigned sx, sy;
   unsigned sx0, sy0, sx1, sy1;
   unsigned r, g, b;
   double x0, x1, y0, y1, wx, wy, weight;
   double sum_r, sum_g, sum_b;

   if (!dst || !src || !src_width || !src_height)
      return;

   /* Start with a clean presentation surface. Never modify ScrBuf: games
    * which do not redraw a still frame must not retain the keyboard. */
   memset(dst, 0, 800 * 512 * sizeof(uint16_t));

   max_h = fullscreen ? 512 : 286;
   if (src_width * max_h > src_height * 768)
   {
      draw_h = max_h;
      draw_w = (src_width * draw_h) / src_height;
   }
   else
   {
      draw_w = 768;
      draw_h = (src_height * draw_w) / src_width;
   }
   if (draw_w > 768) draw_w = 768;
   if (draw_h > max_h) draw_h = max_h;
   xoff = (768 - draw_w) / 2;
   yoff = (max_h - draw_h) / 2;

   /* Area resampling avoids the severe aliasing produced by nearest-neighbour
    * reduction, especially on the X68000's thin Japanese glyphs and UI lines.
    * Each destination pixel receives the weighted average of the source
    * pixels covered by its footprint. This is used for both up- and down-
    * scaling and preserves the source aspect ratio. */
   for (y = 0; y < draw_h; y++)
   {
      y0 = (double)y * (double)src_height / (double)draw_h;
      y1 = (double)(y + 1) * (double)src_height / (double)draw_h;
      sy0 = (unsigned)y0;
      sy1 = (unsigned)(y1 + 0.999999);
      for (x = 0; x < draw_w; x++)
      {
         x0 = (double)x * (double)src_width / (double)draw_w;
         x1 = (double)(x + 1) * (double)src_width / (double)draw_w;
         sx0 = (unsigned)x0;
         sx1 = (unsigned)(x1 + 0.999999);
         sum_r = sum_g = sum_b = weight = 0.0;

         for (sy = sy0; sy < sy1; sy++)
         {
            wy = ((y1 < (double)(sy + 1)) ? y1 : (double)(sy + 1)) -
                 ((y0 > (double)sy) ? y0 : (double)sy);
            for (sx = sx0; sx < sx1; sx++)
            {
               uint16_t pixel;
               pixel = src[sy * 800 + sx];
               wx = ((x1 < (double)(sx + 1)) ? x1 : (double)(sx + 1)) -
                    ((x0 > (double)sx) ? x0 : (double)sx);
               weight = wx * wy;
               sum_r += (double)((pixel >> 11) & 0x1f) * weight;
               sum_g += (double)((pixel >> 5) & 0x3f) * weight;
               sum_b += (double)(pixel & 0x1f) * weight;
            }
         }
         weight = (x1 - x0) * (y1 - y0);
         r = (unsigned)(sum_r / weight + 0.5);
         g = (unsigned)(sum_g / weight + 0.5);
         b = (unsigned)(sum_b / weight + 0.5);
         dst[(y + yoff) * 800 + x + xoff] =
               (uint16_t)((r << 11) | (g << 5) | b);
      }
   }

   if (osd_opacity < 100)
   {
      /* Compose the keyboard opaquely, then mix it with what was behind it
       * (the game in fullscreen layout, black otherwise). */
      static uint16_t background[OSD_KB_REGION_H * 800];
      int px, py, a = (osd_opacity * 256) / 100;
      uint16_t *region = dst + OSD_KB_REGION_Y * 800;

      memcpy(background, region, sizeof(background));
      osd_draw_keyboard(dst, OSD_KEYBOARD_Y_OFFSET);
      for (py = 0; py < OSD_KB_REGION_H; py++)
         for (px = 0; px < 768; px++)
         {
            uint16_t k = region[py * 800 + px];
            uint16_t b = background[py * 800 + px];
            unsigned r, g, bl;
            if (k == b)
               continue;
            r  = ((((k >> 11) & 0x1f) * a) + (((b >> 11) & 0x1f) * (256 - a))) >> 8;
            g  = ((((k >> 5)  & 0x3f) * a) + (((b >> 5)  & 0x3f) * (256 - a))) >> 8;
            bl = ((((k)       & 0x1f) * a) + (((b)       & 0x1f) * (256 - a))) >> 8;
            region[py * 800 + px] = (uint16_t)((r << 11) | (g << 5) | bl);
         }
   }
   else
      osd_draw_keyboard(dst, OSD_KEYBOARD_Y_OFFSET);
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
/* Optional MAME software-list labels. Original filenames remain authoritative. */
#include "mame_utf8_sjis.h"
extern int px68k_mame_names_mode(void);
extern const char *px68k_mame_names_system_dir(void);

typedef struct mame_label_entry {
   char *shortname;
   char *description;
   char *japanese;
   struct mame_label_entry *next;
} mame_label_entry;
static mame_label_entry *mame_labels[1024];
static int mame_labels_loaded;
static char mame_labels_dir[4096];

static unsigned mame_label_hash(const char *s)
{
   unsigned h = 5381;
   while (*s) h = ((h << 5) + h) ^ (unsigned char)*s++;
   return h % 1024;
}

static void mame_labels_clear(void)
{
   int i;
   for (i = 0; i < 1024; ++i)
   {
      mame_label_entry *e = mame_labels[i];
      while (e)
      {
         mame_label_entry *next = e->next;
         free(e->shortname);
         free(e->description);
         free(e->japanese);
         free(e);
         e = next;
      }
      mame_labels[i] = NULL;
   }
}

static char *mame_label_dup(const char *s, size_t n)
{
   char *r = (char *)malloc(n + 1);
   if (r) { memcpy(r, s, n); r[n] = 0; }
   return r;
}

static void mame_label_decode(char *s)
{
   char *r = s, *w = s;
   while (*r)
   {
      if (*r == '&')
      {
         const char *entity[] = { "&amp;", "&lt;", "&gt;", "&quot;", "&apos;" };
         const char chars[] = { '&', '<', '>', '"', '\'' };
         int i;
         for (i = 0; i < 5; ++i)
            if (!strncmp(r, entity[i], strlen(entity[i]))) break;
         if (i < 5) { *w++ = chars[i]; r += strlen(entity[i]); continue; }
      }
      *w++ = *r++;
   }
   *w = 0;
}

/* Convert UTF-8 MAME metadata into the Shift-JIS expected by draw_str(). */
static unsigned short mame_unicode_sjis(unsigned u)
{
   size_t lo = 0, hi = sizeof(mame_unicode_map)/sizeof(mame_unicode_map[0]);
   if (u >= 0x20 && u <= 0x7e) return (unsigned short)u;
   if (u >= 0xff61 && u <= 0xff9f) return (unsigned short)(0xa1 + u - 0xff61);
   while (lo < hi)
   {
      size_t mid = lo + (hi-lo)/2;
      if (mame_unicode_map[mid].unicode < u) lo = mid+1;
      else hi = mid;
   }
   if (lo < sizeof(mame_unicode_map)/sizeof(mame_unicode_map[0]) && mame_unicode_map[lo].unicode == u)
      return mame_unicode_map[lo].sjis;
   return (unsigned short)'?';
}

static char *mame_utf8_to_sjis(const char *src)
{
   size_t len = strlen(src), i = 0, out = 0;
   char *dst = (char *)malloc(len*2 + 1);
   if (!dst) return NULL;
   while (i < len)
   {
      unsigned c = (unsigned char)src[i++], u = c;
      unsigned short sj;
      int more = 0, j, valid = 1;
      if (c >= 0xc2 && c <= 0xdf) { u = c & 31; more = 1; }
      else if (c >= 0xe0 && c <= 0xef) { u = c & 15; more = 2; }
      else if (c >= 0xf0 && c <= 0xf4) { u = c & 7; more = 3; }
      else if (c >= 0x80) { u = '?'; }
      for (j = 0; j < more; ++j)
      {
         unsigned char b = (unsigned char)src[i];
         if (!b || (b & 0xc0) != 0x80) { valid = 0; break; }
         u = (u << 6) | (b & 63);
         ++i;
      }
      if (!valid || (more == 1 && u < 0x80) || (more == 2 && u < 0x800) ||
          (more == 3 && u < 0x10000) || (u >= 0xd800 && u <= 0xdfff)) u = '?';
      sj = mame_unicode_sjis(u);
      if (sj > 0xff) { dst[out++] = (char)(sj >> 8); dst[out++] = (char)(sj & 255); }
      else dst[out++] = (char)sj;
   }
   dst[out] = 0;
   return dst;
}

static void mame_labels_load(void)
{
   const char *dir = px68k_mame_names_system_dir();
   char path[4096], line[8192], shortname[256];
   mame_label_entry *current = NULL;
   FILE *f;
   if (!dir || !*dir) return;
   if (mame_labels_loaded && !strcmp(mame_labels_dir, dir)) return;
   mame_labels_clear();
   mame_labels_loaded = 1;
   snprintf(mame_labels_dir, sizeof(mame_labels_dir), "%s", dir);
   if (strlen(dir) + 24 >= sizeof(path)) return;
   snprintf(path, sizeof(path), "%s/MAME/hash/x68k_flop.xml", dir);
   f = fopen(path, "rb");
   if (!f) return;
   shortname[0] = 0;
   while (fgets(line, sizeof(line), f))
   {
      char *a, *b;
      a = strstr(line, "<software ");
      if (a)
      {
         a = strstr(a, "name=\"");
         shortname[0] = 0;
         current = NULL;
         if (a)
         {
            a += 6; b = strchr(a, '"');
            if (b && (size_t)(b-a) < sizeof(shortname))
            { memcpy(shortname, a, (size_t)(b-a)); shortname[b-a] = 0; }
         }
      }
      a = strstr(line, "<description>");
      if (a && shortname[0])
      {
         a += 13; b = strstr(a, "</description>");
         if (b && b > a && b-a < 1024)
         {
            unsigned h = mame_label_hash(shortname);
            mame_label_entry *e = (mame_label_entry *)calloc(1, sizeof(*e));
            if (e)
            {
               e->shortname = mame_label_dup(shortname, strlen(shortname));
               e->description = mame_label_dup(a, (size_t)(b-a));
               if (e->shortname && e->description)
               {
                  mame_label_decode(e->description);
                  { char *converted = mame_utf8_to_sjis(e->description);
                    if (converted) { free(e->description); e->description = converted; }
                    else { free(e->description); e->description = mame_label_dup(shortname, strlen(shortname)); } }
                  current = e;
                  e->next = mame_labels[h]; mame_labels[h] = e;
               }
               else { free(e->shortname); free(e->description); free(e); }
            }
         }
         shortname[0] = 0;
      }
      a = strstr(line, "<info ");
      if (a && current && strstr(a, "name=\"alt_title\""))
      {
         a = strstr(a, "value=\"");
         if (a)
         {
            a += 7; b = strchr(a, '"');
            if (b && b > a && b-a < 1024)
            {
               char *raw = mame_label_dup(a, (size_t)(b-a));
               if (raw)
               {
                  char *converted;
                  mame_label_decode(raw);
                  converted = mame_utf8_to_sjis(raw);
                  if (converted) { free(current->japanese); current->japanese = converted; }
                  free(raw);
               }
            }
         }
      }
      if (strstr(line, "</software>")) { shortname[0] = 0; current = NULL; }
   }
   fclose(f);
}

static const char *mame_explorer_name(const char *name, int directory)
{
   size_t n;
   char key[256];
   unsigned h;
   mame_label_entry *e;
   static char result[2048];
   int mode = px68k_mame_names_mode();
   if (directory || !mode) return name;
   n = strlen(name);
   if (n <= 4 || n >= sizeof(key) + 4 || strcasecmp(name+n-4, ".zip")) return name;
   mame_labels_load();
   n -= 4;
   if (n >= sizeof(key)) return name;
   memcpy(key, name, n); key[n] = 0;
   h = mame_label_hash(key);
   for (e = mame_labels[h]; e; e = e->next)
      if (!strcmp(e->shortname, key))
      {
         snprintf(result, sizeof(result), "%s.zip", (mode == 2 && e->japanese && *e->japanese) ? e->japanese : e->description);
         return result;
      }
   return name;
}

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
      const char *name = mame_explorer_name(mfl->name[selected], mfl->type[selected]);
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
      const char *name = mame_explorer_name(mfl->name[i + mfl->ptr], mfl->type[i + mfl->ptr]);
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
      const unsigned char *scan = (const unsigned char *)mame_explorer_name(mfl->name[selected], mfl->type[selected]);
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
