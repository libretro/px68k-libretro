#ifndef _WINX68K_WINUI_H
#define _WINX68K_WINUI_H

#include <stdint.h>
#include "common.h"

extern	uint8_t	Debug_Text, Debug_Grp, Debug_Sp;

extern char cur_dir_str[];
extern size_t cur_dir_slen;

void WinUI_Init(void);
int WinUI_Menu(int first);

#define WUM_MENU_END 1
#define WUM_EMU_QUIT 2

enum MenuState
{
   MS_KEY,
   MS_VALUE,
   MS_FILE,
   MS_HWJOY_SET
};

#define MFL_MAX 4000

struct menu_flist
{
	char name[MFL_MAX][MAX_PATH];
	char type[MFL_MAX];
	char dir[4][MAX_PATH];
	int ptr;
	int num;
	int y;
	int stack[2][MFL_MAX];
	int stackptr;
};

extern char menu_item_key[][15];
extern char menu_items[][15][30];

int WinUI_get_drv_num(int key);

/* Defined in libretro.c: mounts a ZIP member picked in the menu and
 * rebuilds the Disk Control table from the same archive. */
void px68k_disk_menu_sync_zip(int slot, const char *member, int drive);

#endif /* _WINX68K_WINUI_H */
