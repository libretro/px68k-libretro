#ifndef _WINX68K_WINDRAW_H
#define _WINX68K_WINDRAW_H

#include <stdint.h>

extern uint16_t WinDraw_Pal16B, WinDraw_Pal16R, WinDraw_Pal16G;

void WinDraw_Init(void);
void WinDraw_Cleanup(void);
void FASTCALL WinDraw_Draw(void);
void WinDraw_DrawLine(void);
void WinDraw_DrawOSDKeyboard(uint16_t *dst, const uint16_t *src,
      unsigned src_width, unsigned src_height, int fullscreen);
int WinDraw_OSDKeyCount(void);
int WinDraw_OSDKeyLines(int index);
void WinDraw_SetOSDKeyboardStyle(int opacity_percent, int dark_theme,
      int look_3d);
void WinDraw_SetOSDKeyboardCursor(const char *tint, const char *frame);
void WinDraw_SetOSDKeyboardFeedback(int mode);
void WinDraw_SetOSDKeyboardLabels(int japanese);
int WinDraw_OSDKeyInfo(int index, int *x, int *y, int *w, int *h,
      uint8_t *code);

int WinDraw_MenuInit(void);
void WinDraw_DrawMenu(int menu_state, int mkey_pos, int mkey_y, int *mval_y);

extern struct menu_flist mfl;

void WinDraw_DrawMenufile(struct menu_flist *mfl);
int WinDraw_ExplorerRows(void);
void WinDraw_ClearMenuBuffer(void);

#endif /* _WINX68K_WINDRAW_H */
