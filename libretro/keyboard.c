/* 
 * Copyright (c) 2003,2008 NONAKA Kimihiro
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
#include "joystick.h"
#include "prop.h"
#include "keyboard.h"
#include "mfp.h"
#include "windraw.h"

#include "libretro.h"

uint8_t	KeyBufWP;
uint8_t	KeyBufRP;
uint8_t	KeyBuf[KeyBufSize];
uint8_t	KeyIntFlag = 0;

void Keyboard_Init(void)
{
	KeyBufWP = 0;
	KeyBufRP = 0;
	memset(KeyBuf, 0, KeyBufSize);
	KeyIntFlag = 0;
}

/*
 *	てーぶる類
 */

#define	NC	0
#define KEYTABLE_MAX 512

static uint8_t KeyTable[KEYTABLE_MAX] = {
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		0x0f,0x10,  NC,  NC,  NC,0x1d,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,0x01,  NC,  NC,  NC,  NC,
		0x35,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
		0x09,0x0a,0x28,0x27,0x31,0x0c,0x32,0x33,
		0x0b,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
		0x09,0x0a,0x28,0x27,0x31,0x0c,0x32,0x33,
		0x1b,0x1e,0x2e,0x2c,0x20,0x13,0x21,0x22,
		0x23,0x18,0x24,0x25,0x26,0x30,0x2f,0x10,
		0x1a,0x11,0x14,0x1f,0x15,0x17,0x2d,0x12,
		0x2b,0x16,0x2a,0x1c,0x0e,0x29,0x0d,0x34,
		0x1b,0x1e,0x2e,0x2c,0x20,0x13,0x21,0x22,
		0x23,0x18,0x24,0x25,0x26,0x30,0x2f,0x19,
		0x1a,0x11,0x14,0x1f,0x15,0x17,0x2d,0x12,
		0x2b,0x16,0x2a,0x1c,0x0e,0x29,0x0d,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,


		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		0x0f,0x10,0x1d,  NC,  NC,0x1d,  NC,  NC,
		  NC,0x3c,0x3e,0x3d,0x3b,  NC,  NC,  NC,
		  NC,  NC,0x63,0x01,  NC,  NC,  NC,  NC,
		  NC,  NC,0x56,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		0x70,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		0x36,0x3b,0x3c,0x3d,0x3e,0x39,0x38,0x3a,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,0x5e,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,0x61,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,0x36,0x3b,0x3c,
		0x3d,0x3e,0x39,0x38,0x3a,  NC,0x5e,0x37,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,0x41,0x46,  NC,0x42,0x51,0x40,
		0x4f,0x4b,0x4c,0x4d,0x47,0x48,0x49,0x43,
		0x44,0x45,  NC,  NC,  NC,  NC,0x63,0x64,
		0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,0x70,0x70,0x71,0x71,0x5d,  NC,0x55,
		0x55,0x55,0x55,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,  NC,
		  NC,  NC,  NC,  NC,  NC,  NC,  NC,0x37
};

/* P6K: PX68K_KEYBOARD */
void send_keycode(uint8_t code, int flag)
{
	uint8_t newwp;

	if (code != NC) {
		newwp = ((KeyBufWP + 1) & (KeyBufSize - 1));
		if (newwp != KeyBufRP) {
			KeyBuf[KeyBufWP] = code | ((flag == P6K_UP)? 0x80 : 0);
			KeyBufWP = newwp;
		}
	}
}

static uint8_t get_x68k_keycode(uint32_t wp)
{
	if (wp < KEYTABLE_MAX/2)
		return KeyTable[wp];

	switch (wp) {
	case RETROK_UP:
		return 0x3c;
	case RETROK_DOWN:
		return 0x3e;
	case RETROK_LEFT:
		return 0x3b;
	case RETROK_RIGHT:
		return 0x3d;

#define RETROK_KP_0 RETROK_KP0
#define RETROK_KP_1 RETROK_KP1
#define RETROK_KP_2 RETROK_KP2
#define RETROK_KP_3 RETROK_KP3
#define RETROK_KP_4 RETROK_KP4
#define RETROK_KP_5 RETROK_KP5
#define RETROK_KP_6 RETROK_KP6
#define RETROK_KP_7 RETROK_KP7
#define RETROK_KP_8 RETROK_KP8
#define RETROK_KP_9 RETROK_KP9
#define RETROK_NUMLOCKCLEAR RETROK_NUMLOCK

	case RETROK_KP_0:
		return 0x4f;
	case RETROK_KP_1:
		return 0x4b;
	case RETROK_KP_2:
		return 0x4c;
	case RETROK_KP_3:
		return 0x4d;
	case RETROK_KP_4:
		return 0x47;
	case RETROK_KP_5:
		return 0x48;
	case RETROK_KP_6:
		return 0x49;
	case RETROK_KP_7:
		return 0x43;
	case RETROK_KP_8:
		return 0x44;
	case RETROK_KP_9:
		return 0x45;
	case RETROK_NUMLOCKCLEAR:
		return 0x3f;

	case RETROK_F1:
		return 0x63;
	case RETROK_F2:
		return 0x64;
	case RETROK_F3:
		return 0x65;
	case RETROK_F4:
		return 0x66;
	case RETROK_F5:
		return 0x67;
	case RETROK_F6:
		return 0x68;
	case RETROK_F7:
		return 0x69;
	case RETROK_F8:
		return 0x6a;
	case RETROK_F9:
		return 0x6b;
	case RETROK_F10:
		return 0x6c;
	case RETROK_LSHIFT:
	case RETROK_RSHIFT:
		return 0x70;
	case RETROK_LCTRL:
	case RETROK_RCTRL:
		return 0x71;
	case RETROK_KP_DIVIDE:
		return 0x40;
	case RETROK_KP_MULTIPLY:
		return 0x41;
	case RETROK_KP_MINUS:
		return 0x42;
	case RETROK_KP_PLUS:
		return 0x46;
	case RETROK_KP_ENTER:
		return 0x4e;
	case RETROK_INSERT:
		return 0x5e;
	case RETROK_HOME:
		return 0x36;
	case RETROK_END:
		return 0x3a;
	case RETROK_PAGEUP:
		return 0x38;
	case RETROK_PAGEDOWN:
		return 0x39;
	default:
		return -1;
	}
}

void Keyboard_KeyDown(uint32_t wp)
{
	uint8_t newwp;
	uint8_t code = get_x68k_keycode(wp);
	if (code < 0)
		return;

	send_keycode(code, P6K_DOWN);

	switch (wp) {
	case RETROK_UP:
		if (!(JoyKeyState&JOY_DOWN))
			JoyKeyState |= JOY_UP;
		break;

	case RETROK_DOWN:
		if (!(JoyKeyState&JOY_UP))
			JoyKeyState |= JOY_DOWN;
		break;

	case RETROK_LEFT:
		if (!(JoyKeyState&JOY_RIGHT))
			JoyKeyState |= JOY_LEFT;
		break;

	case RETROK_RIGHT:
		if (!(JoyKeyState&JOY_LEFT))
			JoyKeyState |= JOY_RIGHT;
		break;
	case RETROK_a:
		JoyKeyState |= (JOY_LEFT | JOY_RIGHT);	/* [RUN] */
		break;

	case RETROK_s:
		JoyKeyState |= (JOY_UP | JOY_DOWN);	/* [SELECT] */
		break;

	case RETROK_z:
		JoyKeyState |= JOY_TRG1;
		break;

	case RETROK_x:
		JoyKeyState |= JOY_TRG2;
		break;
	}
}

void Keyboard_KeyUp(uint32_t wp)
{
	uint8_t newwp;
	uint8_t code = get_x68k_keycode(wp);
	if (code < 0)
		return;
	send_keycode(code, P6K_UP);

	switch(wp) {
	case RETROK_UP:
		JoyKeyState &= ~JOY_UP;
		break;

	case RETROK_DOWN:
		JoyKeyState &= ~JOY_DOWN;
		break;

	case RETROK_LEFT:
		JoyKeyState &= ~JOY_LEFT;
		break;

	case RETROK_RIGHT:
		JoyKeyState &= ~JOY_RIGHT;
		break;

	case RETROK_a:
		JoyKeyState &= ~(JOY_LEFT | JOY_RIGHT);	/* [RUN] */
		break;

	case RETROK_s:
		JoyKeyState &= ~(JOY_UP | JOY_DOWN);	/* [SELECT] */
		break;

	case RETROK_z:
		JoyKeyState &= ~JOY_TRG1;
		break;

	case RETROK_x:
		JoyKeyState &= ~JOY_TRG2;
		break;
	}

}

/*
 *	Key Check
 *	1フレーム中に4回（2400bps/10bit/60fpsとすれば、だが）呼ばれて、MFPにデータを送る
 */

void
Keyboard_Int(void)
{
	if (KeyBufRP != KeyBufWP)
	{
		if (!KeyIntFlag)
		{
			LastKey = KeyBuf[KeyBufRP];
			KeyBufRP = ((KeyBufRP+1)&(KeyBufSize-1));
			KeyIntFlag = 1;
			MFP_Int(3);
		}
	}
	else if (!KeyIntFlag)
		LastKey = 0;
}

int Keyboard_IsSwKeyboard(void)
{
	return 0;
}
