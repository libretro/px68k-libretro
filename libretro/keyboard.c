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

/* Libretro software-keyboard visibility state. */
static int sw_keyboard_visible = 0;
extern retro_input_state_t input_state_cb;

/* Emulated key state and keyboard LEDs; see the notes further down. */
static uint8_t key_down_state[128];
uint8_t kbd_led = 0xff;
uint8_t kbd_led_seen = 0;
static uint8_t kbd_led_guess = 0xff;

/* Key repeat. On the real machine the keyboard itself repeats a held key by
 * re-sending its make code; the host only programs the timing (0x6n: delay =
 * 200 + n*100 ms, 0x7n: interval = 30 + n*n*5 ms; defaults 500 / 110 ms).
 * The core must therefore generate the repeats. The most recently pressed
 * key repeats; SHIFT/CTRL/OPT and the lock keys never do. */
#define KBD_REPEAT_NONE 0xff
uint16_t kbd_repeat_delay_ms    = 500;
uint16_t kbd_repeat_interval_ms = 110;
static int     kbd_repeat_opt_delay    = -1; /* -1 machine, 0 off, else ms */
static int     kbd_repeat_opt_interval = -1; /* -1 machine, else ms */
static uint8_t kbd_repeat_code         = KBD_REPEAT_NONE;
static long    kbd_repeat_timer_us     = 0;
static int     kbd_repeat_generating   = 0;

static void osd_reset_state(void);

void Keyboard_Init(void)
{
	osd_reset_state();
	memset(key_down_state, 0, sizeof(key_down_state));
	kbd_led = 0xff;
	kbd_led_seen = 0;
	kbd_led_guess = 0xff;
	kbd_repeat_delay_ms = 500;
	kbd_repeat_interval_ms = 110;
	kbd_repeat_code = KBD_REPEAT_NONE;
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
/* Emulated key state, fed by every source (physical keyboard, Joy2Key and the
 * OSD keyboard) because they all go through send_keycode(). */
/* Keyboard LEDs. The OS drives them with a byte written to the MFP data
 * register: bit 7 set, then bit 6 zenkaku, 5 hiragana, 4 INS, 3 CAPS, 2 code
 * input, 1 romaji, 0 kana, where a 0 bit means "lit". That byte is the
 * authoritative lock-key state, so the OSD follows what the guest believes.
 * Until the guest has sent one, a local guess toggles on each lock-key make
 * so the display still responds on software that never programs the LEDs. */
#define KBD_LED_KANA     0
#define KBD_LED_ROMAJI   1
#define KBD_LED_CODE     2
#define KBD_LED_CAPS     3
#define KBD_LED_INS      4
#define KBD_LED_HIRAGANA 5
#define KBD_LED_ZENKAKU  6

void Keyboard_HostCommand(uint8_t data)
{
	if (data & 0x80)
	{
		kbd_led      = data;
		kbd_led_seen = 1;
	}
	else if ((data & 0xf0) == 0x60)
		kbd_repeat_delay_ms = (uint16_t)(200 + (data & 0x0f) * 100);
	else if ((data & 0xf0) == 0x70)
		kbd_repeat_interval_ms =
			(uint16_t)(30 + (data & 0x0f) * (data & 0x0f) * 5);
}

/* Core options: delay_ms -1 = follow the machine, 0 = repeat off, else ms.
 * interval_ms -1 = follow the machine, else ms. */
void Keyboard_SetRepeatOptions(int delay_ms, int interval_ms)
{
	kbd_repeat_opt_delay    = delay_ms;
	kbd_repeat_opt_interval = interval_ms;
	if (delay_ms == 0)
		kbd_repeat_code = KBD_REPEAT_NONE;
}

static int repeat_delay_ms(void)
{
	return kbd_repeat_opt_delay < 0 ? (int)kbd_repeat_delay_ms
			: kbd_repeat_opt_delay;
}

static int repeat_interval_ms(void)
{
	return kbd_repeat_opt_interval < 0 ? (int)kbd_repeat_interval_ms
			: kbd_repeat_opt_interval;
}

static int key_can_repeat(uint8_t code)
{
	if (code >= 0x70 && code <= 0x73)
		return 0; /* SHIFT, CTRL, OPT.1, OPT.2 */
	if (code >= 0x5a && code <= 0x60)
		return 0; /* lock keys */
	return 1;
}

/* Called once per emulated frame with the frame duration. */
void Keyboard_RepeatTick(unsigned frame_us)
{
	long interval_us;
	int generated = 0;

	if (kbd_repeat_code == KBD_REPEAT_NONE)
		return;
	if (!key_down_state[kbd_repeat_code])
	{
		kbd_repeat_code = KBD_REPEAT_NONE;
		return;
	}
	interval_us = (long)repeat_interval_ms() * 1000L;
	if (interval_us < 1000L)
		interval_us = 1000L;
	kbd_repeat_timer_us -= (long)frame_us;
	while (kbd_repeat_timer_us <= 0 && generated < 3)
	{
		kbd_repeat_generating = 1;
		send_keycode(kbd_repeat_code, P6K_DOWN);
		kbd_repeat_generating = 0;
		kbd_repeat_timer_us += interval_us;
		generated++;
	}
	if (kbd_repeat_timer_us < 0)
		kbd_repeat_timer_us = 0; /* never build up a backlog */
}

static int lock_key_bit(uint8_t code)
{
	switch (code)
	{
		case 0x5a: return KBD_LED_KANA;
		case 0x5b: return KBD_LED_ROMAJI;
		case 0x5c: return KBD_LED_CODE;
		case 0x5d: return KBD_LED_CAPS;
		case 0x5e: return KBD_LED_INS;
		case 0x5f: return KBD_LED_HIRAGANA;
		case 0x60: return KBD_LED_ZENKAKU;
	}
	return -1;
}

static int led_on(int bit)
{
	return !(((kbd_led_seen ? kbd_led : kbd_led_guess) >> bit) & 1);
}

int Keyboard_KeyIsDown(uint8_t code)
{
	return code < 128 && key_down_state[code];
}

/* -1: not a lock key, 0: LED off, 1: LED on. */
int Keyboard_LockKeyState(uint8_t code)
{
	int bit = lock_key_bit(code);
	return bit < 0 ? -1 : led_on(bit);
}

/* Legend layer: 0 alphanumeric, 1 shifted, 2/3 hiragana (plain/shifted),
 * 4/5 katakana (plain/shifted), 6/7 romaji (plain/shifted). While the romaji
 * LED is lit the punctuation keys type Japanese punctuation; otherwise kana
 * legends apply while KANA is lit. */
int Keyboard_LegendLayer(void)
{
	int shift = Keyboard_KeyIsDown(0x70) ? 1 : 0;
	if (led_on(KBD_LED_ROMAJI))
		return 6 + shift;
	if (led_on(KBD_LED_KANA))
		return (led_on(KBD_LED_HIRAGANA) ? 2 : 4) + shift;
	return shift;
}

void send_keycode(uint8_t code, int flag)
{
	uint8_t newwp;

	if (code < 128)
	{
		int was_down = key_down_state[code];

		if (flag == P6K_UP)
		{
			key_down_state[code] = 0;
			if (code == kbd_repeat_code)
				kbd_repeat_code = KBD_REPEAT_NONE;
		}
		else
		{
			int bit = lock_key_bit(code);
			if (bit >= 0 && !was_down)
				kbd_led_guess ^= (uint8_t)(1 << bit);
			key_down_state[code] = 1;
			if (!was_down && !kbd_repeat_generating && key_can_repeat(code) &&
					repeat_delay_ms() > 0)
			{
				kbd_repeat_code     = code;
				kbd_repeat_timer_us = (long)repeat_delay_ms() * 1000L;
			}
		}
	}

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

/* ------------------------------------------------------------------------
 * OSD keyboard interaction (gamepad navigation, touch/pointer, key events)
 * ------------------------------------------------------------------------ */

#define OSD_PORTS          2
#define OSD_POINTERS       4
#define OSD_OUT_WIDTH      768
#define OSD_OUT_HEIGHT     512
#define OSD_REPEAT_DELAY   18  /* frames before a held direction repeats */
#define OSD_REPEAT_RATE    4   /* frames between repeats */
#define OSD_POINTER_SLOP   6   /* pixels of forgiveness around keycaps */
#define OSD_FLASH_STEP     24  /* feedback fade per frame (about 190 ms) */
#define OSD_CODE_BS        0x0f
#define OSD_CODE_SPACE     0x35
#define OSD_CODE_Q         0x11

#define OSD_PAD_UP      (1u << RETRO_DEVICE_ID_JOYPAD_UP)
#define OSD_PAD_DOWN    (1u << RETRO_DEVICE_ID_JOYPAD_DOWN)
#define OSD_PAD_LEFT    (1u << RETRO_DEVICE_ID_JOYPAD_LEFT)
#define OSD_PAD_RIGHT   (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT)
#define OSD_PAD_DIRS    (OSD_PAD_UP | OSD_PAD_DOWN | OSD_PAD_LEFT | OSD_PAD_RIGHT)
#define OSD_PAD_CONFIRM (1u << RETRO_DEVICE_ID_JOYPAD_B)
#define OSD_PAD_BS      (1u << RETRO_DEVICE_ID_JOYPAD_A)
#define OSD_PAD_SPACE   (1u << RETRO_DEVICE_ID_JOYPAD_X)
#define OSD_PAD_CLEAR   (1u << RETRO_DEVICE_ID_JOYPAD_Y)

static int      osd_sel = -1;           /* selected key (table index) */
static int      osd_pref_x;             /* remembered column */
static int      osd_line;               /* current line (see WinDraw_OSDKeyLines) */
static uint8_t  osd_refs[128];          /* OSD sources holding each scancode */
static uint8_t  osd_latched[128];       /* modifier latched by a tap */
static uint32_t osd_pad_prev[OSD_PORTS];
static uint32_t osd_pad_block[OSD_PORTS];
static uint32_t osd_dir_prev[OSD_PORTS];
static int      osd_dir_timer[OSD_PORTS];
static int      osd_pad_code[OSD_PORTS];
static int      osd_aux_code[OSD_PORTS][2];
static int      osd_ptr_down[OSD_POINTERS];
static int      osd_ptr_wait[OSD_POINTERS];
static int      osd_ptr_code[OSD_POINTERS];
static uint8_t  osd_flash[128];         /* press feedback strength, by scancode */
static int      osd_hover_enabled = 1;
static int      osd_hover_have_last;
static int      osd_hover_last_x, osd_hover_last_y;

static void osd_code_down(int code)
{
	if (code <= 0 || code >= 128)
		return;
	if (!osd_refs[code])
		send_keycode((uint8_t)code, P6K_DOWN);
	if (osd_refs[code] < 255)
		osd_refs[code]++;
}

static void osd_code_up(int code)
{
	if (code <= 0 || code >= 128 || !osd_refs[code])
		return;
	osd_refs[code]--;
	if (!osd_refs[code])
		send_keycode((uint8_t)code, P6K_UP);
}

static int osd_is_modifier(int code)
{
	/* SHIFT, CTRL, OPT.1, OPT.2 */
	return code >= 0x70 && code <= 0x73;
}

static int osd_key_code(int idx)
{
	uint8_t code;
	if (!WinDraw_OSDKeyInfo(idx, NULL, NULL, NULL, NULL, &code))
		return -1;
	return code ? (int)code : -1;
}

static int osd_line_lowest(int mask)
{
	int l;
	for (l = 0; l < 31; l++)
		if (mask & (1 << l))
			return l;
	return 0;
}

static int osd_line_highest(int mask)
{
	int l, last = 0;
	for (l = 0; l < 31; l++)
		if (mask & (1 << l))
			last = l;
	return last;
}

static void osd_select(int idx)
{
	int x, y, w, h;
	if (!WinDraw_OSDKeyInfo(idx, &x, &y, &w, &h, NULL))
		return;
	osd_sel    = idx;
	osd_pref_x = x + w / 2;
	osd_line   = osd_line_lowest(WinDraw_OSDKeyLines(idx));
}

static void osd_ensure_selection(void)
{
	int i;
	if (osd_sel >= 0 && osd_key_code(osd_sel) > 0)
		return;
	for (i = 0; i < WinDraw_OSDKeyCount(); i++)
		if (osd_key_code(i) == OSD_CODE_Q)
		{
			osd_select(i);
			return;
		}
	for (i = 0; i < WinDraw_OSDKeyCount(); i++)
		if (osd_key_code(i) > 0)
		{
			osd_select(i);
			return;
		}
}

static int osd_range_dist(int v, int lo, int len)
{
	if (v < lo)
		return lo - v;
	if (v >= lo + len)
		return v - (lo + len - 1);
	return 0;
}

/* Navigation follows the layout's horizontal lines. Left/Right only ever
 * visit keys that belong to the current line, so the cursor cannot slip to a
 * neighbouring row; at the end of a line it simply stops. Up/Down jump to the
 * adjacent line (past the whole key for the tall RET/ENT) and pick the key
 * closest to the remembered column. */
static void osd_move(int dx, int dy)
{
	int i, n, best = -1;
	int x, y, w, h, cx, cy, mask;
	long best_score = 0;

	osd_ensure_selection();
	if (osd_sel < 0 || !WinDraw_OSDKeyInfo(osd_sel, &x, &y, &w, &h, NULL))
		return;
	cx   = x + w / 2;
	cy   = y + h / 2;
	mask = WinDraw_OSDKeyLines(osd_sel);
	n    = WinDraw_OSDKeyCount();

	if (dx)
	{
		if (!(mask & (1 << osd_line)))
			osd_line = osd_line_lowest(mask);
		for (i = 0; i < n; i++)
		{
			int kx, ky, kw, kh, kcx;
			long score;

			if (i == osd_sel || osd_key_code(i) <= 0 ||
					!(WinDraw_OSDKeyLines(i) & (1 << osd_line)))
				continue;
			WinDraw_OSDKeyInfo(i, &kx, &ky, &kw, &kh, NULL);
			kcx = kx + kw / 2;
			if (dx > 0 ? (kcx <= cx + 2) : (kcx >= cx - 2))
				continue;
			score = kcx > cx ? kcx - cx : cx - kcx;
			if (best < 0 || score < best_score)
			{
				best       = i;
				best_score = score;
			}
		}
		if (best >= 0)
		{
			int keep_line = osd_line;
			osd_select(best);
			osd_line = keep_line; /* stay on the line we were travelling */
		}
	}
	else
	{
		int target = dy < 0 ? osd_line_lowest(mask) - 1
				: osd_line_highest(mask) + 1;
		if (target < 0 || target > 30)
			return;
		for (i = 0; i < n; i++)
		{
			int kx, ky, kw, kh, kcx;
			long score;

			if (i == osd_sel || osd_key_code(i) <= 0 ||
					!(WinDraw_OSDKeyLines(i) & (1 << target)))
				continue;
			WinDraw_OSDKeyInfo(i, &kx, &ky, &kw, &kh, NULL);
			kcx = kx + kw / 2;
			score = 4L * osd_range_dist(osd_pref_x, kx, kw) +
					(kcx > osd_pref_x ? kcx - osd_pref_x : osd_pref_x - kcx);
			if (best < 0 || score < best_score)
			{
				best       = i;
				best_score = score;
			}
		}
		if (best >= 0)
		{
			int keep_x = osd_pref_x;
			osd_select(best);
			osd_pref_x = keep_x; /* vertical travel keeps the column */
			osd_line   = target;
		}
	}
	(void)cy;
}

/* Press the key at idx. Returns the scancode that must be released later,
 * or -1 when nothing is held (latched modifier, dummy key). */
static int osd_key_press(int idx)
{
	int code = osd_key_code(idx);
	if (code <= 0 || code >= 128)
		return -1;
	osd_flash[code] = 255;
	if (osd_is_modifier(code))
	{
		/* A tap latches/unlatches the modifier so it can be combined with
		 * the next key without a second button or finger. */
		if (osd_latched[code])
		{
			osd_latched[code] = 0;
			osd_code_up(code);
		}
		else
		{
			osd_latched[code] = 1;
			osd_code_down(code);
		}
		return -1;
	}
	osd_code_down(code);
	return code;
}

static void osd_clear_latches(void)
{
	int code;
	for (code = 0x70; code <= 0x73; code++)
		if (osd_latched[code])
		{
			osd_latched[code] = 0;
			osd_code_up(code);
		}
}

static void osd_reset_state(void)
{
	int i;
	memset(osd_refs, 0, sizeof(osd_refs));
	memset(osd_latched, 0, sizeof(osd_latched));
	for (i = 0; i < OSD_PORTS; i++)
	{
		osd_pad_prev[i]    = 0;
		osd_pad_block[i]   = 0;
		osd_dir_prev[i]    = 0;
		osd_dir_timer[i]   = 0;
		osd_pad_code[i]    = -1;
		osd_aux_code[i][0] = -1;
		osd_aux_code[i][1] = -1;
	}
	for (i = 0; i < OSD_POINTERS; i++)
	{
		osd_ptr_down[i] = 0;
		osd_ptr_wait[i] = 0;
		osd_ptr_code[i] = -1;
	}
}

/* Release every key the OSD holds (including latched modifiers). Safe to
 * call at any time; the keyboard stays open. */
void Keyboard_OSDReleaseAll(void)
{
	int code, i;
	for (code = 1; code < 128; code++)
		if (osd_refs[code])
			send_keycode((uint8_t)code, P6K_UP);
	memset(osd_refs, 0, sizeof(osd_refs));
	memset(osd_latched, 0, sizeof(osd_latched));
	memset(osd_flash, 0, sizeof(osd_flash));
	for (i = 0; i < OSD_PORTS; i++)
	{
		osd_pad_code[i]    = -1;
		osd_aux_code[i][0] = -1;
		osd_aux_code[i][1] = -1;
	}
	for (i = 0; i < OSD_POINTERS; i++)
	{
		osd_ptr_down[i] = 0;
		osd_ptr_code[i] = -1;
	}
}

/* Core option: should the selection follow the mouse cursor? */
void Keyboard_SetOSDHover(int enabled)
{
	osd_hover_enabled = enabled ? 1 : 0;
}

/* Press feedback strength 0..255 for a scancode: 255 while pressed, then
 * fading to 0 over a few frames after release. Used by the renderer. */
int Keyboard_OSDKeyFlash(uint8_t code)
{
	return code < 128 ? osd_flash[code] : 0;
}

/* Bit 0: selected, bit 1: held/latched. Used by the renderer. */
int Keyboard_OSDKeyState(int idx, uint8_t code)
{
	int state = 0;
	if (idx == osd_sel)
		state |= 1;
	if (Keyboard_KeyIsDown(code))
		state |= 2;
	return state;
}

static void osd_aux(unsigned port, uint32_t pressed, uint32_t released,
      uint32_t bit, int slot, int code)
{
	if (pressed & bit)
	{
		osd_code_down(code);
		osd_aux_code[port][slot] = code;
		if (code > 0 && code < 128)
			osd_flash[code] = 255;
	}
	if ((released & bit) && osd_aux_code[port][slot] >= 0)
	{
		osd_code_up(osd_aux_code[port][slot]);
		osd_aux_code[port][slot] = -1;
	}
}

/* "pad" is the RetroPad bitmask of one port with the Joy2Key OSD-keyboard
 * and menu buttons already removed, so the toggle keeps priority over every
 * navigation role. D-pad/left stick move, B presses, A = BS, X = SPACE,
 * Y = release latched modifiers. Called once per frame per port. */
void Keyboard_OSDPadUpdate(unsigned port, uint32_t pad)
{
	uint32_t pressed, released, dirs, newdirs;
	int moves = 0;

	if (port >= OSD_PORTS || !sw_keyboard_visible)
		return;

	/* Buttons already held when the keyboard opened are ignored until
	 * released, so the opening gesture cannot press a key. */
	osd_pad_block[port] &= pad;
	pad &= ~osd_pad_block[port];

	pressed  = pad & ~osd_pad_prev[port];
	released = osd_pad_prev[port] & ~pad;
	osd_pad_prev[port] = pad;

	osd_ensure_selection();

	dirs = pad & OSD_PAD_DIRS;
	if ((dirs & OSD_PAD_LEFT) && (dirs & OSD_PAD_RIGHT))
		dirs &= ~(OSD_PAD_LEFT | OSD_PAD_RIGHT);
	if ((dirs & OSD_PAD_UP) && (dirs & OSD_PAD_DOWN))
		dirs &= ~(OSD_PAD_UP | OSD_PAD_DOWN);
	newdirs = dirs & ~osd_dir_prev[port];
	if (newdirs)
	{
		osd_dir_timer[port] = OSD_REPEAT_DELAY;
		moves = 1;
		dirs  = newdirs | (dirs & osd_dir_prev[port]);
	}
	else if (dirs && --osd_dir_timer[port] <= 0)
	{
		osd_dir_timer[port] = OSD_REPEAT_RATE;
		moves = 1;
	}
	osd_dir_prev[port] = pad & OSD_PAD_DIRS;
	if (moves)
	{
		if (dirs & OSD_PAD_LEFT)  osd_move(-1, 0);
		if (dirs & OSD_PAD_RIGHT) osd_move(+1, 0);
		if (dirs & OSD_PAD_UP)    osd_move(0, -1);
		if (dirs & OSD_PAD_DOWN)  osd_move(0, +1);
	}

	if (pressed & OSD_PAD_CONFIRM)
		osd_pad_code[port] = osd_key_press(osd_sel);
	if ((released & OSD_PAD_CONFIRM) && osd_pad_code[port] >= 0)
	{
		osd_code_up(osd_pad_code[port]);
		osd_pad_code[port] = -1;
	}

	osd_aux(port, pressed, released, OSD_PAD_BS,    0, OSD_CODE_BS);
	osd_aux(port, pressed, released, OSD_PAD_SPACE, 1, OSD_CODE_SPACE);

	if (pressed & OSD_PAD_CLEAR)
		osd_clear_latches();
}

static int osd_hit_test(int px, int py)
{
	int i, n = WinDraw_OSDKeyCount();
	int best = -1;
	long best_d = (long)OSD_POINTER_SLOP * OSD_POINTER_SLOP + 1;

	for (i = 0; i < n; i++)
	{
		int x, y, w, h, ddx, ddy;
		long d;
		if (osd_key_code(i) <= 0)
			continue;
		WinDraw_OSDKeyInfo(i, &x, &y, &w, &h, NULL);
		ddx = osd_range_dist(px, x, w);
		ddy = osd_range_dist(py, y, h);
		if (!ddx && !ddy)
			return i;
		d = (long)ddx * ddx + (long)ddy * ddy;
		if (d < best_d)
		{
			best_d = d;
			best   = i;
		}
	}
	return best;
}

/* Touchscreen / mouse via the libretro pointer device. Pointer coordinates
 * span the visible video area (-0x7fff..0x7fff), which is exactly the
 * 768x512 OSD frame while the keyboard is open. Up to four simultaneous
 * contacts are tracked so a finger can hold a key while another taps. */
void Keyboard_OSDPointerUpdate(void)
{
	int i;

	if (!sw_keyboard_visible || !input_state_cb)
		return;

	/* Press feedback: held keys stay lit, released keys fade out. */
	for (i = 1; i < 128; i++)
		if (osd_flash[i])
		{
			if (Keyboard_KeyIsDown((uint8_t)i))
				osd_flash[i] = 255;
			else
				osd_flash[i] = osd_flash[i] > OSD_FLASH_STEP
					? (uint8_t)(osd_flash[i] - OSD_FLASH_STEP) : 0;
		}

	for (i = 0; i < OSD_POINTERS; i++)
	{
		int pressed = input_state_cb(0, RETRO_DEVICE_POINTER, (unsigned)i,
				RETRO_DEVICE_ID_POINTER_PRESSED) ? 1 : 0;

		if (osd_ptr_wait[i])
		{
			/* Contact that began before the keyboard opened. */
			if (!pressed)
				osd_ptr_wait[i] = 0;
			continue;
		}

		if (pressed && !osd_ptr_down[i])
		{
			int rx = input_state_cb(0, RETRO_DEVICE_POINTER, (unsigned)i,
					RETRO_DEVICE_ID_POINTER_X);
			int ry = input_state_cb(0, RETRO_DEVICE_POINTER, (unsigned)i,
					RETRO_DEVICE_ID_POINTER_Y);
			int sx = (int)(((long)(rx + 32767) * OSD_OUT_WIDTH) / 65534L);
			int sy = (int)(((long)(ry + 32767) * OSD_OUT_HEIGHT) / 65534L);
			int idx = (rx < -32767 || rx > 32767 || ry < -32767 || ry > 32767)
				? -1 : osd_hit_test(sx, sy);

			osd_ptr_down[i] = 1;
			osd_ptr_code[i] = -1;
			if (idx >= 0)
			{
				osd_select(idx);
				osd_ptr_code[i] = osd_key_press(idx);
			}
		}
		else if (!pressed && osd_ptr_down[i])
		{
			if (osd_ptr_code[i] >= 0)
				osd_code_up(osd_ptr_code[i]);
			osd_ptr_code[i] = -1;
			osd_ptr_down[i] = 0;
		}
	}

	/* Hover: the selection follows the pointer, but only when the pointer
	 * actually moves. A resting mouse therefore never fights the gamepad,
	 * and a touchscreen's stale position never moves the cursor. */
	{
		int rx = input_state_cb(0, RETRO_DEVICE_POINTER, 0,
				RETRO_DEVICE_ID_POINTER_X);
		int ry = input_state_cb(0, RETRO_DEVICE_POINTER, 0,
				RETRO_DEVICE_ID_POINTER_Y);
		int any_down = 0;

		for (i = 0; i < OSD_POINTERS; i++)
			if (osd_ptr_down[i])
				any_down = 1;

		if (!osd_hover_have_last)
		{
			/* First frame after opening: just learn where the pointer is. */
			osd_hover_have_last = 1;
			osd_hover_last_x = rx;
			osd_hover_last_y = ry;
		}
		else if (rx != osd_hover_last_x || ry != osd_hover_last_y)
		{
			osd_hover_last_x = rx;
			osd_hover_last_y = ry;
			if (osd_hover_enabled && !any_down &&
					rx >= -32767 && rx <= 32767 &&
					ry >= -32767 && ry <= 32767)
			{
				int sx = (int)(((long)(rx + 32767) * OSD_OUT_WIDTH) / 65534L);
				int sy = (int)(((long)(ry + 32767) * OSD_OUT_HEIGHT) / 65534L);
				int idx = osd_hit_test(sx, sy);
				if (idx >= 0 && idx != osd_sel)
					osd_select(idx);
			}
		}
	}
}

void Keyboard_ToggleSwKeyboard(void)
{
	int i;
	if (sw_keyboard_visible)
	{
		/* Closing must never leave a key stuck in the emulated machine. */
		Keyboard_OSDReleaseAll();
		sw_keyboard_visible = 0;
		return;
	}
	for (i = 0; i < OSD_PORTS; i++)
	{
		osd_pad_block[i] = 0xffffffffu; /* ignore buttons held right now */
		osd_pad_prev[i]  = 0;
		osd_dir_prev[i]  = 0;
		osd_dir_timer[i] = 0;
	}
	for (i = 0; i < OSD_POINTERS; i++)
		osd_ptr_wait[i] = 1;
	osd_hover_have_last = 0;
	memset(osd_flash, 0, sizeof(osd_flash));
	osd_ensure_selection();
	sw_keyboard_visible = 1;
}

int Keyboard_IsSwKeyboard(void)
{
	return sw_keyboard_visible;
}
