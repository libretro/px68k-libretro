#ifndef _WINX68K_KEYBOARD_H
#define _WINX68K_KEYBOARD_H

#include <stdint.h>
#include "common.h"

#define KeyBufSize 128

#define P6K_UP   1
#define P6K_DOWN 2

extern	uint8_t	KeyBuf[KeyBufSize];
extern	uint8_t	KeyBufWP;
extern	uint8_t	KeyBufRP;
extern	uint8_t	KeyIntFlag;

void Keyboard_Init(void);
void Keyboard_KeyDown(uint32_t vkcode);
void Keyboard_KeyUp(uint32_t vkcode);
void Keyboard_Int(void);
void send_keycode(uint8_t code, int flag);
int Keyboard_get_key_ptr(int x, int y);
int Keyboard_IsSwKeyboard(void);
void Keyboard_ToggleSwKeyboard(void);
void Keyboard_OSDPadUpdate(unsigned port, uint32_t pad);
void Keyboard_OSDPointerUpdate(void);
void Keyboard_OSDReleaseAll(void);
void Keyboard_SetOSDHover(int enabled);
int Keyboard_OSDKeyFlash(uint8_t code);
int Keyboard_OSDKeyState(int idx, uint8_t code);
int Keyboard_KeyIsDown(uint8_t code);
int Keyboard_LockKeyState(uint8_t code);
int Keyboard_LegendLayer(void);
void Keyboard_HostCommand(uint8_t data);
void Keyboard_SetRepeatOptions(int delay_ms, int interval_ms);
void Keyboard_RepeatTick(unsigned frame_us);
extern uint16_t kbd_repeat_delay_ms;
extern uint16_t kbd_repeat_interval_ms;
extern uint8_t kbd_led;
extern uint8_t kbd_led_seen;

/* https://gamesx.com/wiki/doku.php?id=x68000:keycodes */
#define	KBD_XF1		0x55
#define	KBD_XF2		0x56
#define	KBD_XF3		0x57
#define	KBD_XF4		0x58
#define	KBD_XF5		0x59
#define	KBD_F1		0x63
#define	KBD_F2		0x64
#define	KBD_OPT1	0x72
#define	KBD_OPT2	0x73

#endif /* _WINX68K_KEYBOARD_H */
