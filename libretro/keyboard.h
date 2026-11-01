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
