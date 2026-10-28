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

/* -------------------------------------------------------------------------- *
 *  WINUI.C - UI                                                              *
 * -------------------------------------------------------------------------- */

#include <stdint.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef _WIN32
#include <strings.h>
#endif

#include "common.h"
#include "keyboard.h"
#include "windraw.h"
#include "dswin.h"
#include "dosio.h"
#include "prop.h"
#include "status.h"
#include "joystick.h"
#include "mouse.h"
#include "winx68k.h"
#include "fdd.h"
#include "irqh.h"
#include "../m68000/m68000.h"
#include "crtc.h"
#include "mfp.h"
#include "fdc.h"
#include "disk_d88.h"
#include "dmac.h"
#include "ioc.h"
#include "rtc.h"
#include "sasi.h"
#include "bg.h"
#include "palette.h"
#include "crtc.h"
#include "pia.h"
#include "scc.h"
#include "midi.h"
#include "adpcm.h"
#include "mercury.h"
#include "tvram.h"
#include "winui.h"
#include "zipdisk.h"

#include "fmg_wrap.h"

#include <file/file_path.h>

#ifdef _WIN32
#define SLASH '\\'
#else
#define SLASH '/'
#endif

uint8_t	Debug_Text=1, Debug_Grp=1, Debug_Sp=1;

char		filepath[MAX_PATH] = ".";

char     cur_dir_str[MAX_PATH];
size_t   cur_dir_slen;

int menu_state = MS_KEY;
int mkey_y     = 0;
int mkey_pos   = 0;

struct menu_flist mfl;

/* Navigation history shared by filesystem folders and ZIP archives. A ZIP is
 * treated as a directory node: entering it pushes the containing directory and
 * selected item; '..' pops that exact frame and restores the selection.
 * ZIPs do not nest, so a drive never holds more than one frame; the
 * headroom only keeps a stale entry harmless. */
#define MFL_NAV_DEPTH 4
struct mfl_nav_frame
{
   char dir[MAX_PATH];
   char selected[MAX_PATH];
   int ptr;
   int y;
};
static struct mfl_nav_frame mfl_nav[2][MFL_NAV_DEPTH];
static int mfl_nav_depth[2];
static char mfl_nav_restore[2][MAX_PATH];
static const char mfl_slash[2] = { SLASH, '\0' };

static void menu_nav_push(int drv, const char *selected)
{
   int d;
   struct mfl_nav_frame *f;
   if (drv < 0 || drv >= 2) return;
   d = mfl_nav_depth[drv];
   if (d >= MFL_NAV_DEPTH) d = MFL_NAV_DEPTH - 1;
   f = &mfl_nav[drv][d];
   snprintf(f->dir, sizeof(f->dir), "%s", mfl.dir[drv]);
   snprintf(f->selected, sizeof(f->selected), "%s", selected ? selected : "");
   f->ptr = mfl.ptr;
   f->y = mfl.y;
   if (mfl_nav_depth[drv] < MFL_NAV_DEPTH)
      mfl_nav_depth[drv]++;
}

static int menu_nav_pop(int drv)
{
   struct mfl_nav_frame *f;
   int i, idx = -1;
   if (drv < 0 || drv >= 2 || mfl_nav_depth[drv] <= 0)
      return 0;
   f = &mfl_nav[drv][--mfl_nav_depth[drv]];
   snprintf(mfl_nav_restore[drv], sizeof(mfl_nav_restore[drv]), "%s", f->selected);
   snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]), "%s", f->dir);
   mfl.ptr = f->ptr;
   mfl.y = f->y;
   /* The list can be reordered or truncated, so restore by filename first. */
   if (f->selected[0])
      for (i = 0; i < mfl.num; i++)
         if (!strcmp(mfl.name[i], f->selected)) { idx = i; break; }
   if (idx >= 0)
   {
      mfl.ptr = idx >= WinDraw_ExplorerRows() ? idx - WinDraw_ExplorerRows() + 1 : 0;
      mfl.y = idx - mfl.ptr;
   }
   else if (mfl.ptr < 0 || mfl.ptr >= mfl.num || mfl.y < 0 || mfl.ptr + mfl.y >= mfl.num)
   {
      mfl.ptr = 0;
      mfl.y = 0;
   }
   return 1;
}

extern char base_dir[MAX_PATH];

/***** menu items *****/

#define MENU_NUM 5
#define MENU_WINDOW 7

int mval_y[] = {
	0,
	0,
	0,
	0,
	0
};

enum menu_id
{
	M_SYS,
	M_FD0,
	M_FD1,
	M_HD0,
	M_HD1
};

/* Max # of characters is 15. */
char menu_item_key[][15] = {
	"SYSTEM",
	"FDD0",
	"FDD1",
	"HDD0",
	"HDD1",
	"",
	"",
	"uhyo",
	""
};

/* Max # of characters is 30. */
/* Max # of items including terminater `""' in each line is 15. */
char menu_items[][15][30] = {
	{"RESET", "NMI RESET", "QUIT", ""},
	{"dummy", "EJECT", ""},
	{"dummy", "EJECT", ""},
	{"dummy", "EJECT", ""},
	{"dummy", "EJECT", ""}
};

static void menu_system(int v)
{
	switch (v)
   {
      case 0 :
         WinX68k_Reset();
         break;
      case 1:
         IRQH_Int(7, NULL);
         break;
   }
}

static void upper(char *s)
{
	while (*s != '\0')
   {
      if (*s >= 'a' && *s <= 'z')
         *s = 'A' + *s - 'a';
      s++;
   }
}

/* Dynamically sized directory index. Navigation uses indices and never scans
 * the filesystem during cursor movement, including accelerated 50-line jumps. */
static void menu_list_clear(void)
{
   int i;
   for (i = 0; i < mfl.num; ++i)
      free(mfl.name[i]);
   free(mfl.name);
   free(mfl.type);
   mfl.name = NULL;
   mfl.type = NULL;
   mfl.capacity = 0;
   mfl.num = 0;
}

static int menu_list_add(const char *name, char type)
{
   char **names;
   char *types;
   char *copy;
   int cap;
   if (mfl.num == mfl.capacity)
   {
      cap = mfl.capacity ? mfl.capacity * 2 : 256;
      if (cap <= mfl.capacity) return 0;
      names = (char **)realloc(mfl.name, (size_t)cap * sizeof(char *));
      if (!names) return 0;
      mfl.name = names;
      types = (char *)realloc(mfl.type, (size_t)cap * sizeof(char));
      if (!types) return 0;
      mfl.type = types;
      mfl.capacity = cap;
   }
   copy = (char *)malloc(strlen(name) + 1);
   if (!copy) return 0;
   strcpy(copy, name);
   mfl.name[mfl.num] = copy;
   mfl.type[mfl.num] = type;
   ++mfl.num;
   return 1;
}

struct menu_sort_entry { char *name; char type; };
static int menu_sort_compare(const void *aa, const void *bb)
{
   const struct menu_sort_entry *a = (const struct menu_sort_entry *)aa;
   const struct menu_sort_entry *b = (const struct menu_sort_entry *)bb;
   if (!strcmp(a->name, "..")) return -1;
   if (!strcmp(b->name, "..")) return 1;
   if (a->type != b->type) return (int)b->type - (int)a->type;
   return strcasecmp(a->name, b->name);
}
static void menu_list_sort(void)
{
   struct menu_sort_entry *entries;
   int i;
   if (mfl.num < 2) return;
   entries = (struct menu_sort_entry *)malloc((size_t)mfl.num * sizeof(*entries));
   if (!entries) return;
   for (i = 0; i < mfl.num; ++i)
   {
      entries[i].name = mfl.name[i];
      entries[i].type = mfl.type[i];
   }
   qsort(entries, (size_t)mfl.num, sizeof(*entries), menu_sort_compare);
   for (i = 0; i < mfl.num; ++i)
   {
      mfl.name[i] = entries[i].name;
      mfl.type[i] = entries[i].type;
   }
   free(entries);
}

static void menu_nav_restore_cursor(int drv)
{
   int i;
   if (drv < 0 || drv >= 2) return;
   if (mfl_nav_restore[drv][0])
      for (i = 0; i < mfl.num; i++)
         if (!strcmp(mfl.name[i], mfl_nav_restore[drv]))
         {
            mfl.ptr = i >= WinDraw_ExplorerRows() ? i - WinDraw_ExplorerRows() + 1 : 0;
            mfl.y = i - mfl.ptr;
            mfl_nav_restore[drv][0] = '\0';
            return;
         }
   if (mfl.ptr < 0 || mfl.ptr >= mfl.num || mfl.y < 0 || mfl.ptr + mfl.y >= mfl.num)
      mfl.ptr = mfl.y = 0;
   mfl_nav_restore[drv][0] = '\0';
}

static int menu_name_is_zip(const char *name)
{
   const char *dot = name ? strrchr(name, '.') : NULL;
   if (!dot || strlen(dot) != 4)
      return 0;
   return (tolower((unsigned char)dot[1]) == 'z' &&
           tolower((unsigned char)dot[2]) == 'i' &&
           tolower((unsigned char)dot[3]) == 'p');
}

static int menu_create_zip_flist(int drv)
{
   int slot, count, i;
   if (!px68k_zip_is_browse_path(mfl.dir[drv]))
      return 0;
   slot = px68k_zip_browse_slot(mfl.dir[drv]);
   menu_list_clear();
   if (slot < 0) return 1;
   if (!menu_list_add("..", 1)) return 1;
   count = px68k_zip_count(slot, 0);
   for (i = 0; i < count; ++i)
      if (!menu_list_add(px68k_zip_name(slot, 0, i), 0)) break;
   mfl.ptr = 0;
   mfl.y = 0;
   return 1;
}

/* Directory enumeration is spread over menu frames. ZIP enumeration and the
 * boot-time navigation stack reconstruction remain synchronous. */
#ifdef USE_LIBRETRO_VFS
static DIRH *menu_scan_dir;
#else
#include <dirent.h>
static DIR *menu_scan_dir;
#endif
static int menu_scan_active;
static int menu_scan_drv;
static int menu_scan_restore_ptr, menu_scan_restore_y;
static char menu_scan_support[32];

static void menu_scan_close(void)
{
   if (menu_scan_dir)
   {
#ifdef USE_LIBRETRO_VFS
      wrap_vfs_closedir(menu_scan_dir);
#else
      closedir(menu_scan_dir);
#endif
      menu_scan_dir = NULL;
   }
   menu_scan_active = 0;
}

/* Returns nonzero while more directory entries remain. */
static int menu_scan_step(int budget)
{
   int count = 0;
   while (menu_scan_active && count < budget)
   {
      const char *n;
      char path[MAX_PATH];
      int is_dir = 0;
      int len;
      char ext[4];
      char *p;
#ifdef USE_LIBRETRO_VFS
      int st_size = 0;
      int mode;
      if (!wrap_vfs_readdir(menu_scan_dir)) break;
      n = menu_scan_dir->d_name;
#else
      struct dirent *dent = readdir(menu_scan_dir);
      struct stat st;
      if (!dent) break;
      n = dent->d_name;
#endif
      ++count;
      if (!strcmp(n, ".") || !strcmp(n, "..")) continue;
      len = (int)strlen(n);
      if (len >= 4 && n[len - 4] == '.')
      {
         memcpy(ext, n + len - 3, 3);
         ext[3] = '\0';
         upper(ext);
         p = strstr(menu_scan_support, ext);
         if (p && (p - menu_scan_support) % 3 == 0)
         {
            /* Accepted file extension: still check whether it's a directory. */
         }
         else p = NULL;
      }
      else p = NULL;
      /* A stat is needed for directories, including those without a supported
       * extension. Avoid path buffer overflow for unusually long names. */
      if (strlen(mfl.dir[menu_scan_drv]) + (size_t)len >= sizeof(path)) continue;
      snprintf(path, sizeof(path), "%s%s", mfl.dir[menu_scan_drv], n);
#ifdef USE_LIBRETRO_VFS
      mode = wrap_vfs_stat(path, &st_size);
      is_dir = !!(mode & STAT_IS_DIRECTORY);
#else
      if (stat(path, &st) != 0) continue;
      is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
#endif
      if (!is_dir && !p) continue;
      if (!menu_list_add(n, (char)is_dir)) break;
   }
   if (menu_scan_active && count < budget)
   {
      menu_scan_close();
      menu_list_sort();
      {
         int had_nav_restore = mfl_nav_restore[menu_scan_drv][0] != 0;
         if (had_nav_restore)
            menu_nav_restore_cursor(menu_scan_drv);
         else
         {
            /* Restore the saved filesystem cursor only AFTER the completed
             * directory has been sorted. The caller may have restored mfl.ptr
             * while scanning, so checking whether it is zero is unreliable. */
            int idx = menu_scan_restore_ptr + menu_scan_restore_y;
            int rows = WinDraw_ExplorerRows();
            if (idx >= 0 && idx < mfl.num)
            {
               mfl.ptr = menu_scan_restore_ptr;
               mfl.y = menu_scan_restore_y;
               if (mfl.ptr < 0) mfl.ptr = 0;
               if (mfl.ptr > idx) mfl.ptr = idx;
               if (rows < 1) rows = 1;
               if (mfl.y < 0 || mfl.y >= rows)
               {
                  mfl.ptr = idx >= rows ? idx - rows + 1 : 0;
                  mfl.y = idx - mfl.ptr;
               }
            }
            else
               mfl.ptr = mfl.y = 0;
         }
      }
   }
   return menu_scan_active;
}

static void menu_create_flist(int v)
{
   int drv = WinUI_get_drv_num(mkey_y);
   if (drv < 0) return;
   if (v == 1)
   {
      menu_scan_close();
      if (drv < 2)
      {
         FDD_EjectFD(drv);
         Config.FDDImage[drv][0] = '\0';
      }
      else Config.HDImage[drv - 2][0] = '\0';
      return;
   }
   menu_scan_close();
   if (drv < 2 && menu_create_zip_flist(drv)) return;
   menu_scan_drv = drv;
   menu_scan_restore_ptr = mfl.ptr;
   menu_scan_restore_y = mfl.y;
   snprintf(menu_scan_support, sizeof(menu_scan_support), "%s",
         drv >= 2 ? "HDF" : "D8888DHDMDUP2HDDIMXDFIMGZIP");
   menu_list_clear();
#ifdef USE_LIBRETRO_VFS
   menu_scan_dir = wrap_vfs_opendir(mfl.dir[drv]);
#else
   menu_scan_dir = opendir(mfl.dir[drv]);
#endif
   if (!menu_scan_dir)
   {
      snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]), "%s%c", base_dir, SLASH);
#ifdef USE_LIBRETRO_VFS
      menu_scan_dir = wrap_vfs_opendir(mfl.dir[drv]);
#else
      menu_scan_dir = opendir(mfl.dir[drv]);
#endif
   }
   mfl.ptr = mfl.y = 0;
   if (!menu_scan_dir) return;
   if (strcmp(mfl.dir[drv], cur_dir_str)) menu_list_add("..", 1);
   menu_scan_active = 1;
}

/* Only the boot-time stack reconstruction needs a synchronous listing. */
static void menu_create_flist_sync(void)
{
   while (menu_scan_active) menu_scan_step(1024);
}


/* Rebuild the legacy filesystem cursor stack for a path reached implicitly at
 * boot. Normally this stack is populated by entering each directory in the
 * browser. A boot-mounted archive skips those steps, so leaving its parent
 * directory otherwise restores stale cursor positions on subsequent '..'. */
static void menu_seed_legacy_folder_stack(int drv, const char *folder_path)
{
   char path[MAX_PATH];
   char current[MAX_PATH];
   char saved_dir[MAX_PATH];
   char *components[MAX_PATH / 2];
   int count = 0, i;
   char *p;
   size_t len;

   if (drv < 0 || drv >= 4 || !folder_path || !folder_path[0]) return;
   snprintf(saved_dir, sizeof(saved_dir), "%s", mfl.dir[drv]);
   snprintf(path, sizeof(path), "%s", folder_path);
   len = strlen(path);
   while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\'))
      path[--len] = '\0';

   current[0] = '\0';
   p = path;
#ifdef _WIN32
   if (p[0] && p[1] == ':') {
      current[0] = p[0]; current[1] = ':'; current[2] = SLASH; current[3] = '\0';
      p += 2;
   } else
#endif
   if (*p == '/' || *p == '\\') {
      current[0] = SLASH; current[1] = '\0';
      p++;
   }
   while (*p == '/' || *p == '\\') p++;
   while (*p) {
      char *start = p;
      size_t n;
      while (*p && *p != '/' && *p != '\\') p++;
      n = (size_t)(p - start);
      if (n && !(n == 1 && start[0] == '.')) {
         if (count >= (int)(sizeof(components) / sizeof(components[0]))) { mfl.stackptr = 0; snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]), "%s", saved_dir); return; }
         components[count++] = start;
         if (*p) *p++ = '\0';
      } else if (*p) p++;
      while (*p == '/' || *p == '\\') p++;
   }

   mfl.stackptr = 0;
   for (i = 0; i < count; i++) {
      int j, found = -1;
      size_t clen;
      if (!current[0]) snprintf(current, sizeof(current), ".%c", SLASH);
      else {
         len = strlen(current);
         if (current[len - 1] != '/' && current[len - 1] != '\\')
            strncat(current, mfl_slash, sizeof(current) - strlen(current) - 1);
      }
      snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]), "%s", current);
      menu_create_flist(0);
      menu_create_flist_sync();
      for (j = 0; j < mfl.num; j++)
         if (mfl.type[j] && !strcmp(mfl.name[j], components[i])) { found = j; break; }
      if (found < 0 || mfl.stackptr >= 256) {
         mfl.stackptr = 0;
         snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]), "%s", saved_dir);
         return;
      }
      mfl.ptr = found >= WinDraw_ExplorerRows() ? found - WinDraw_ExplorerRows() + 1 : 0;
      mfl.y = found - mfl.ptr;
      mfl.stack[0][mfl.stackptr] = mfl.ptr;
      mfl.stack[1][mfl.stackptr] = mfl.y;
      mfl.stackptr = (mfl.stackptr + 1) % 256;
      clen = strlen(current);
      if (clen + strlen(components[i]) + 2 >= sizeof(current)) {
         mfl.stackptr = 0;
         snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]), "%s", saved_dir);
         return;
      }
      strcat(current, components[i]);
      strncat(current, mfl_slash, sizeof(current) - strlen(current) - 1);
   }
   snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]), "%s", saved_dir);
}

struct _menu_func {
	void (*func)(int v);
	int imm;
};

struct _menu_func menu_func[] = {
	{menu_system, 0},
	{menu_create_flist, 0},
	{menu_create_flist, 0},
	{menu_create_flist, 0},
	{menu_create_flist, 0}
};

int WinUI_get_drv_num(int key)
{
	char *s = menu_item_key[key];

	if (!strncmp("FDD", s, 3))
		return strcmp("FDD0", s)?
			(strcmp("FDD1", s)? -1 : 1) : 0;
	return strcmp("HDD0", s)?
		(strcmp("HDD1", s)? -1: 3) : 2;
}

/* TODO/FIXME - we need a way of iterating over all drives for Windows */
#ifdef _WIN32
#define CUR_DIR_STR "c:\\"
#else
#define CUR_DIR_STR "/"
#endif

/******************************************************************************
 * init
 ******************************************************************************/
void plusyen(char *s, size_t len);
void WinUI_Init(void)
{
	int i;

   if(filepath[0] != 0)
      strcpy(cur_dir_str, filepath);
   else
      strcpy(cur_dir_str, CUR_DIR_STR);

   plusyen(cur_dir_str, sizeof(cur_dir_str));
   cur_dir_slen = strlen(cur_dir_str);

   for (i = 0; i < 4; i++)
	   strcpy(mfl.dir[i], cur_dir_str);
}

/* ex. ./hoge/.. -> ./
 * ( ./ ---down hoge dir--> ./hoge ---up hoge dir--> ./hoge/.. ) */
static void shortcut_dir(int drv)
{
   int i, found = 0;
   /* len is larger than 2 */
   size_t len = strlen(mfl.dir[drv]);
   char *p    = mfl.dir[drv] + len - 2;
   for (i = len - 2; i >= 0; i--)
   {
      if (*p == SLASH/*'/'*/)
      {
         found = 1;
         break;
      }
      p--;
   }
#ifdef _WIN32
   if (found && strcmp(p, "\\..\\"))
#else
      if (found && strcmp(p, "/../"))
#endif
         *(p + 1) = '\0';
      else
#ifdef _WIN32
         strcat(mfl.dir[drv], "..\\");
#else
   strcat(mfl.dir[drv], "../");
#endif
}

int WinUI_Menu(int first)
{
   int i, n, ii;
   int cursor0;
   uint8_t joy;
   int menu_redraw  = 0;
   int pad_changed  = 0;
   int mfile_redraw = 0;

   if (first)
   {
      menu_state    = MS_KEY;
      mkey_y        = 0;
      mkey_pos      = 0;
      menu_redraw   = 1;
      first         = 0;
      /*  The screen is not rewritten without any key actions,
       * so draw screen first. */
      WinDraw_ClearMenuBuffer();
      WinDraw_DrawMenu(menu_state, mkey_pos, mkey_y, mval_y);
   }

   /* Advance one bounded batch each frame; the menu continues to redraw. */
   if (menu_scan_active)
   {
      menu_scan_step(256);
      if (menu_state == MS_FILE) mfile_redraw = 1;
   }
   cursor0 = mkey_y;
   joy     = get_joy_downstate();
   reset_joy_downstate();

   if (menu_scan_active && menu_state == MS_FILE)
      joy |= JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT | JOY_TRG1;

   if (!(joy & JOY_UP))
   {
      switch (menu_state)
      {
         case MS_KEY:
            if (mkey_y > 0)
               mkey_y--;
            if (mkey_pos > mkey_y)
               mkey_pos--;
            break;
         case MS_VALUE:
            if (mval_y[mkey_y] > 0)
            {
               mval_y[mkey_y]--;

               /* do something immediately */
               if (menu_func[mkey_y].imm)
                  menu_func[mkey_y].func(mval_y[mkey_y]);

               menu_redraw = 1;
            }
            break;
         case MS_FILE:
            if (mfl.y == 0)
            {
               if (mfl.ptr > 0)
                  mfl.ptr--;
            }
            else
               mfl.y--;
            mfile_redraw = 1;
            break;
      }
   }

   if (!(joy & JOY_DOWN))
   {
      switch (menu_state)
      {
         case MS_KEY:
            if (mkey_y < MENU_NUM - 1)
               mkey_y++;
            if (mkey_y > mkey_pos + MENU_WINDOW - 1)
               mkey_pos++;
            break;
         case MS_VALUE:
            if (menu_items[mkey_y][mval_y[mkey_y] + 1][0] != '\0')
            {
               mval_y[mkey_y]++;

               if (menu_func[mkey_y].imm)
                  menu_func[mkey_y].func(mval_y[mkey_y]);

               menu_redraw = 1;
            }
            break;
         case MS_FILE:
            if (mfl.y >= WinDraw_ExplorerRows() - 1)
            {
               if (mfl.ptr + WinDraw_ExplorerRows() < mfl.num
                     && mfl.ptr < mfl.num - WinDraw_ExplorerRows())
                  mfl.ptr++;
            }
            else if (mfl.y + 1 < mfl.num)
               mfl.y++;
            mfile_redraw = 1;
            break;
      }
   }

   if (!(joy & JOY_LEFT))
   {
      switch (menu_state)
      {
         case MS_KEY:
            break;
         case MS_VALUE:
            if (mval_y[mkey_y] > 0)
            {
               mval_y[mkey_y]-=Config.MenuLinesJump;
               if (mval_y[mkey_y]<0)
                  mval_y[mkey_y] = 0;

               /* do something immediately */
               if (menu_func[mkey_y].imm)
                  menu_func[mkey_y].func(mval_y[mkey_y]);

               menu_redraw = 1;
            }
            break;
         case MS_FILE:
            if (mfl.y == 0)
            {
               if (mfl.ptr > 0)
               {
                  mfl.ptr-=Config.MenuLinesJump;
                  if (mfl.ptr < 0 )
                     mfl.ptr = 0;
               }
            }
            else
            {
               mfl.y-=Config.MenuLinesJump;
               if (mfl.y<0)
               {
                  if (mfl.ptr > 0)
                     mfl.ptr += mfl.y;
                  if (mfl.ptr < 0)
                     mfl.ptr = 0;
                  mfl.y = 0;
               }
            }
            mfile_redraw = 1;
            break;
      }
   }

   if (!(joy & JOY_RIGHT))
   {
      switch (menu_state)
      {
         case MS_KEY:
            break;
         case MS_VALUE:
            for (ii = 0; ii<Config.MenuLinesJump; ii++)
            {
               if (menu_items[mkey_y][mval_y[mkey_y] + 1][0] != '\0')
               {
                  mval_y[mkey_y]++;

                  if (menu_func[mkey_y].imm)
                     menu_func[mkey_y].func(mval_y[mkey_y]);

                  menu_redraw = 1;
               }
            }
            break;
         case MS_FILE:
            for (ii=0; ii<Config.MenuLinesJump; ii++)
            {
               if (mfl.y >= WinDraw_ExplorerRows() - 1)
               {
                  if (mfl.ptr + WinDraw_ExplorerRows() < mfl.num
                        && mfl.ptr < mfl.num - WinDraw_ExplorerRows())
                     mfl.ptr++;
               } else if (mfl.y + 1 < mfl.num)
                  mfl.y++;
               mfile_redraw = 1;
            }
            break;
      }
   }

   if (!(joy & JOY_TRG1))
   {
      int drv, y;
      switch (menu_state)
      {
         case MS_KEY:
            menu_state  = MS_VALUE;
            menu_redraw = 1;
            break;
         case MS_VALUE:
            menu_func[mkey_y].func(mval_y[mkey_y]);

            if (menu_state == MS_HWJOY_SET)
            {
               menu_redraw = 1;
               break;
            }

            /* get back key_mode if value is set.
             * go file_mode if value is filename. */
            menu_state = MS_KEY;
            menu_redraw = 1;

            drv = WinUI_get_drv_num(mkey_y);
            if (drv >= 0)
            {
               if (mval_y[mkey_y] == 0)
               {
                  /* Start in the archive containing the currently inserted
                   * floppy, rather than StartDir, when that floppy is a ZIP
                   * member. This makes changing disks from the internal menu
                   * open the same archive's member list. */
                  if (drv < 2 && px68k_is_zip_virtual_path(Config.FDDImage[drv]))
                  {
                     int slot = px68k_zip_virtual_slot(Config.FDDImage[drv]);
                     if (slot >= 0)
                     {
                        char archive_path[MAX_PATH];
                        snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]),
                              "zipbrowse://%d", slot);
                        /* Seed the same navigation stack used by ordinary
                         * folders, so '..' returns to the archive's parent
                         * and selects the archive itself. */
                        mfl_nav_depth[drv] = 0;
                        if (px68k_zip_get_archive_path(slot, archive_path,
                                 sizeof(archive_path)))
                        {
                           char *slash = strrchr(archive_path, '/');
                           char *backslash = strrchr(archive_path, '\\');
                           char archive_name[MAX_PATH];
                           if (!slash || (backslash && backslash > slash))
                              slash = backslash;
                           archive_name[0] = '\0';
                           if (slash)
                           {
                              snprintf(archive_name, sizeof(archive_name), "%s", slash + 1);
                              if (slash == archive_path)
                                 slash[1] = '\0';
                              else
                              {
                                 *slash = '\0';
                                 strncat(archive_path, mfl_slash,
                                       sizeof(archive_path) - strlen(archive_path) - 1);
                              }
                              snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]), "%s", archive_path);
                              menu_nav_push(drv, archive_name);
                              snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]), "zipbrowse://%d", slot);
                           }
                        }
                        mfl.ptr = 0;
                        mfl.y = 0;
                        /* The boot path bypasses normal directory entry, so
                         * seed the original filesystem stack from the archive
                         * parent path. Keep the ZIP-specific frame above. */
                        menu_seed_legacy_folder_stack(drv, archive_path);
                     }
                  }
                  /* Refresh the file list for the selected drive/path. */
                  menu_func[mkey_y].func(0);
                  menu_state     = MS_FILE;
                  menu_redraw    = 0; /* reset */
                  mfile_redraw   = 1;
               }
               else 
                  mval_y[mkey_y] = 0;
            }
            else if (!strcmp("SYSTEM", menu_item_key[mkey_y]))
            {
               if (mval_y[mkey_y] == 2)
                  return WUM_EMU_QUIT;
               return WUM_MENU_END;
            }
            break;
         case MS_FILE:
            if ((drv = WinUI_get_drv_num(mkey_y)) < 0)
               break;
            y = mfl.ptr + mfl.y;

            /* A ZIP's '..' is handled exactly like a filesystem folder's '..'. */
            if (drv < 2 && px68k_zip_is_browse_path(mfl.dir[drv]) &&
                !strcmp(mfl.name[y], ".."))
            {
               if (menu_nav_pop(drv))
               {
                  /* Filesystem parent scans asynchronously. Keep the saved ZIP
                   * filename until the scan finishes and sorting is complete. */
                  menu_func[mkey_y].func(0);
               }
               else
               {
                  /* No parent frame (e.g. a stale browse path): return safely
                   * to the configured start directory instead of corrupting the cursor. */
                  snprintf(mfl.dir[drv], sizeof(mfl.dir[drv]), "%s", cur_dir_str);
                  mfl.ptr = mfl.y = 0;
                  menu_func[mkey_y].func(0);
               }
               mfile_redraw = 1;
               break;
            }

            /* Selecting a ZIP opens its internal floppy-image list. */
            if (drv < 2 && !px68k_zip_is_browse_path(mfl.dir[drv]) &&
                !mfl.type[y] && menu_name_is_zip(mfl.name[y]))
            {
               char archive_path[MAX_PATH];
               int slot;
               snprintf(archive_path, sizeof(archive_path), "%s%s", mfl.dir[drv], mfl.name[y]);
               slot = px68k_zip_register_archive(archive_path);
               if (slot >= 0)
               {
                  menu_nav_push(drv, mfl.name[y]);
                  snprintf(mfl.dir[drv], MAX_PATH, "zipbrowse://%d", slot);
                  mfl.ptr = 0;
                  mfl.y = 0;
                  menu_func[mkey_y].func(0);
                  mfile_redraw = 1;
               }
               break;
            }

            /* file loaded */
            if (mfl.type[y])
            {
               /* Keep the original PX68K navigation stack for real filesystem
                * directories. The ZIP-specific history is only for entering
                * and leaving an archive; mixing both stacks causes the cursor
                * to reset when going up more than one directory level. */
               if (!strcmp(mfl.name[y], ".."))
               {
                  /* Remember the child directory by name before leaving it.
                   * The parent is loaded incrementally, so numeric cursor
                   * positions alone cannot reliably restore the selection. */
                  {
                     char child[MAX_PATH];
                     size_t end = strlen(mfl.dir[drv]);
                     size_t start;
                     while (end > 0 && (mfl.dir[drv][end - 1] == '/' ||
                                        mfl.dir[drv][end - 1] == '\\')) --end;
                     start = end;
                     while (start > 0 && mfl.dir[drv][start - 1] != '/' &&
                            mfl.dir[drv][start - 1] != '\\') --start;
                     if (end > start && end - start < sizeof(child))
                     {
                        memcpy(child, mfl.dir[drv] + start, end - start);
                        child[end - start] = '\0';
                        snprintf(mfl_nav_restore[drv],
                              sizeof(mfl_nav_restore[drv]), "%s", child);
                     }
                  }
                  shortcut_dir(drv);
                  mfl.stack[0][mfl.stackptr] = mfl.stack[1][mfl.stackptr] = 0;
                  mfl.stackptr = (mfl.stackptr - 1) % 256;
                  if (mfl.stackptr < 0) mfl.stackptr += 256;
                  mfl.ptr = mfl.stack[0][mfl.stackptr];
                  mfl.y = mfl.stack[1][mfl.stackptr];
               }
               else
               {
                  strncat(mfl.dir[drv], mfl.name[y], sizeof(mfl.dir[drv]) - strlen(mfl.dir[drv]) - 1);
#ifdef _WIN32
                  strncat(mfl.dir[drv], "\\", sizeof(mfl.dir[drv]) - strlen(mfl.dir[drv]) - 1);
#else
                  strncat(mfl.dir[drv], "/", sizeof(mfl.dir[drv]) - strlen(mfl.dir[drv]) - 1);
#endif
                  mfl.stack[0][mfl.stackptr] = mfl.ptr;
                  mfl.stack[1][mfl.stackptr] = mfl.y;
                  mfl.stackptr = (mfl.stackptr + 1) % 256;
                  mfl.ptr = 0;
                  mfl.y = 0;
               }
               menu_func[mkey_y].func(0);
               mfile_redraw = 1;
            } else {
               /* file operation */
               if (strlen(mfl.name[y]) != 0)
               {
                  char tmpstr[MAX_PATH];
                  int zip_slot = -1;
                  if (drv < 2 && px68k_zip_is_browse_path(mfl.dir[drv]))
                  {
                     zip_slot = px68k_zip_browse_slot(mfl.dir[drv]);
                     if (zip_slot < 0 || !px68k_zip_make_virtual_path(zip_slot, mfl.name[y], tmpstr, sizeof(tmpstr)))
                        break;
                  }
                  else
                  {
                     snprintf(tmpstr, sizeof(tmpstr), "%s%s", mfl.dir[drv], mfl.name[y]);
                  }
                  if (drv < 2)
                  {
                     if (zip_slot >= 0)
                     {
                        /* Update both the emulator drives and RetroArch's
                         * Disk Control table/index from the same ZIP list. */
                        px68k_disk_menu_sync_zip(zip_slot, mfl.name[y], drv);
                     }
                     else
                     {
                        FDD_SetFD(drv, tmpstr, 0);
                        strncpy(Config.FDDImage[drv], tmpstr, sizeof(Config.FDDImage[drv]) - 1);
                        Config.FDDImage[drv][sizeof(Config.FDDImage[drv]) - 1] = '\0';
                     }
                  }
                  else
                     strcpy(Config.HDImage[drv - 2], tmpstr);
               }
               menu_state  = MS_KEY;
               menu_redraw = 1;
            }
            /* Preserve the original PX68K cursor restoration after the list
             * refresh. menu_create_flist() resets mfl.ptr, so without this
             * the saved parent-folder cursor is lost on the next '..'.
             * ZIP entry/exit paths above handle their own filename-based
             * restoration and intentionally bypass this block. */
            mfl.ptr = mfl.stack[0][mfl.stackptr];
            mfl.y = mfl.stack[1][mfl.stackptr];
            break;
         case MS_HWJOY_SET:
            /* Go back keymode
             * if TRG1 of v-pad or hw keyboard was pushed. */
            if (!pad_changed)
            {
               menu_state  = MS_KEY;
               menu_redraw = 1;
            }
            break;
      }
   }

   if (!(joy & JOY_TRG2))
   {
      switch (menu_state)
      {
         case MS_FILE:
            menu_scan_close();
            menu_state  = MS_VALUE;
            /* reset position of file cursor */
            mfl.y       = 0;
            mfl.ptr     = 0;
            menu_redraw = 1;
            break;
         case MS_VALUE:
            menu_state  = MS_KEY;
            menu_redraw = 1;
            break;
         case MS_HWJOY_SET:
            /* Go back keymode
             * if TRG2 of v-pad or hw keyboard was pushed. */
            if (!pad_changed)
            {
               menu_state  = MS_KEY;
               menu_redraw = 1;
            }
            break;
      }
   }

   if (pad_changed)
      menu_redraw = 1;

   if (cursor0 != mkey_y)
      menu_redraw = 1;

   if (mfile_redraw)
   {
      WinDraw_DrawMenufile(&mfl);
      mfile_redraw = 0;
   }

   if (menu_redraw)
   {
      WinDraw_ClearMenuBuffer();
      WinDraw_DrawMenu(menu_state, mkey_pos, mkey_y, mval_y);
   }

   return 0;
}
