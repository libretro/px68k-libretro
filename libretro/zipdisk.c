/* Floppy and HDF images read straight out of ZIP archives.
 *
 * Archives are parsed with libretro-common's rzip_archive (no zlib). A
 * member is addressed as "zip://@<slot>/<name>", the slot naming an
 * archive registered here; dosio routes file_* calls on such a path to
 * the memory-backed handles below, so the FDD and SASI code read members
 * exactly as they read files. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <zip/rzip_archive.h>

#include "common.h"
#include "dosio.h"
#include "prop.h"
#include "zipdisk.h"

#define ZIP_VIRTUAL_PREFIX     "zip://@"
#define ZIP_BROWSE_PREFIX      "zipbrowse://"
#define ZIP_SLOTS              32
#define ZIP_MEMBERS            4
#define ZIP_HANDLES            8
/* Largest member inflated: SASI addresses 2^21 256-byte sectors, and no
 * floppy format here comes near 8 MiB. */
#define ZIP_HDF_MAX            ((uint64_t)512 * 1024 * 1024)
#define ZIP_FLOPPY_MAX         ((uint64_t)8 * 1024 * 1024)

struct zip_member
{
   uint8_t *data;
   size_t   size;
   int      refs;
   int      resident;
   char     path[MAX_PATH];
};

struct zip_handle
{
   struct zip_member *m;
   size_t             pos;
};

/* The one archive kept open: its directory and sorted member lists. */
struct zip_open_archive
{
   rzip_archive_t *ar;
   void           *fp;
   int            *list[2];  /* entry indices: [0] floppies, [1] HDFs */
   int             num[2];
   int             slot;
};

static char                    *zip_archives[ZIP_SLOTS];
static unsigned                 zip_age[ZIP_SLOTS];
static unsigned                 zip_clock;
static int                      zip_pinned = -1;
static struct zip_open_archive  zo        = { NULL, NULL, { NULL, NULL }, { 0, 0 }, -1 };
static struct zip_member        zip_members[ZIP_MEMBERS];
static struct zip_handle        zip_handles[ZIP_HANDLES];

static const char *ext_of(const char *name)
{
   const char *dot = strrchr(name, '.');
   const char *sl  = strrchr(name, '/');
   if (!dot || (sl && sl > dot))
      return "";
   return dot + 1;
}

static int ext_is(const char *ext, const char *want)
{
   while (*want)
      if (tolower((unsigned char)*ext++) != *want++)
         return 0;
   return *ext == '\0';
}

static int is_hdf_name(const char *name)
{
   return ext_is(ext_of(name), "hdf");
}

static int is_floppy_name(const char *name)
{
   const char *e = ext_of(name);
   return ext_is(e, "xdf") || ext_is(e, "dim") || ext_is(e, "hdm")
       || ext_is(e, "img") || ext_is(e, "2hd") || ext_is(e, "dup")
       || ext_is(e, "d88") || ext_is(e, "88d");
}

/* Digit runs compare by value, so "Disk 2" sorts before "Disk 10". */
static int natural_cmp(const char *a, const char *b)
{
   while (*a && *b)
   {
      if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b))
      {
         const char *ea, *eb;
         size_t la, lb;
         while (*a == '0')
            a++;
         while (*b == '0')
            b++;
         for (ea = a; isdigit((unsigned char)*ea); ea++);
         for (eb = b; isdigit((unsigned char)*eb); eb++);
         la = (size_t)(ea - a);
         lb = (size_t)(eb - b);
         if (la != lb)
            return la < lb ? -1 : 1;
         while (a < ea)
         {
            if (*a != *b)
               return *a < *b ? -1 : 1;
            a++;
            b++;
         }
      }
      else
      {
         int ca = tolower((unsigned char)*a);
         int cb = tolower((unsigned char)*b);
         if (ca != cb)
            return ca < cb ? -1 : 1;
         a++;
         b++;
      }
   }
   return *a ? 1 : (*b ? -1 : 0);
}

static int entry_cmp(const void *x, const void *y)
{
   const rzip_entry_t *a = rzip_archive_entry(zo.ar, (uint32_t)*(const int*)x);
   const rzip_entry_t *b = rzip_archive_entry(zo.ar, (uint32_t)*(const int*)y);
   int r = natural_cmp(a->name, b->name);
   return r ? r : strcmp(a->name, b->name);
}

static int64_t zip_file_read(void *ud, uint64_t off, void *dst, size_t len)
{
   void *fp = ud;
   if (off > (uint64_t)LONG_MAX)
      return -1;
   if (file_seek(fp, (long)off, FSEEK_SET) != (size_t)off)
      return -1;
   return (int64_t)file_lread(fp, dst, len);
}

static void zip_close_archive(void)
{
   if (zo.ar)
      rzip_archive_close(zo.ar);
   if (zo.fp)
      file_close(zo.fp);
   free(zo.list[0]);
   free(zo.list[1]);
   zo.ar      = NULL;
   zo.fp      = NULL;
   zo.list[0] = NULL;
   zo.list[1] = NULL;
   zo.num[0]  = 0;
   zo.num[1]  = 0;
   zo.slot    = -1;
}

static int zip_open_slot(int slot)
{
   uint32_t i, n;
   size_t len;

   if (slot < 0 || slot >= ZIP_SLOTS || !zip_archives[slot])
      return 0;
   if (zo.slot == slot)
      return 1;
   zip_close_archive();

   if (!(zo.fp = file_open(zip_archives[slot])))
      return 0;
   len = file_seek(zo.fp, 0, FSEEK_END);
   if (len == (size_t)-1 || rzip_archive_open(&zo.ar, NULL, (uint64_t)len,
            zip_file_read, zo.fp) != RZIP_OK)
   {
      zo.ar = NULL;
      zip_close_archive();
      return 0;
   }

   n          = rzip_archive_num_entries(zo.ar);
   zo.list[0] = (int*)malloc((n ? n : 1) * sizeof(int));
   zo.list[1] = (int*)malloc((n ? n : 1) * sizeof(int));
   if (!zo.list[0] || !zo.list[1])
   {
      zip_close_archive();
      return 0;
   }
   for (i = 0; i < n; i++)
   {
      const rzip_entry_t *e = rzip_archive_entry(zo.ar, i);
      /* Names that cannot be passed through the quoted command line or
       * that would not fit a drive image path are left out. */
      if (!e || e->is_dir || !e->name[0]
            || strlen(e->name) >= PX68K_ZIP_NAME_MAX
            || strchr(e->name, '"'))
         continue;
      if (is_floppy_name(e->name))
         zo.list[0][zo.num[0]++] = (int)i;
      else if (is_hdf_name(e->name))
         zo.list[1][zo.num[1]++] = (int)i;
   }
   qsort(zo.list[0], (size_t)zo.num[0], sizeof(int), entry_cmp);
   qsort(zo.list[1], (size_t)zo.num[1], sizeof(int), entry_cmp);
   zo.slot = slot;
   return 1;
}

/* A slot still named by a mounted drive or the Disk Control table must
 * keep its archive. */
static int zip_slot_in_use(int slot)
{
   int i;
   if (slot == zip_pinned)
      return 1;
   for (i = 0; i < 2; i++)
      if (px68k_zip_virtual_slot(Config.FDDImage[i]) == slot)
         return 1;
   for (i = 0; i < 16; i++)
      if (px68k_zip_virtual_slot(Config.HDImage[i]) == slot)
         return 1;
   return 0;
}

int px68k_zip_register_archive(const char *archive)
{
   int i, slot = -1;
   char *copy;

   if (!archive || !archive[0])
      return -1;
   for (i = 0; i < ZIP_SLOTS; i++)
      if (zip_archives[i] && !strcmp(zip_archives[i], archive))
      {
         zip_age[i] = ++zip_clock;
         return i;
      }
   for (i = 0; i < ZIP_SLOTS && slot < 0; i++)
      if (!zip_archives[i])
         slot = i;
   /* Full: recycle the least recently used slot nothing refers to. */
   if (slot < 0)
      for (i = 0; i < ZIP_SLOTS; i++)
         if (!zip_slot_in_use(i) && (slot < 0 || zip_age[i] < zip_age[slot]))
            slot = i;
   if (slot < 0)
      return -1;

   if (!(copy = (char*)malloc(strlen(archive) + 1)))
      return -1;
   strcpy(copy, archive);
   if (zo.slot == slot)
      zip_close_archive();
   /* Members cached under the slot's old archive must not be served for
    * the new one. */
   for (i = 0; i < ZIP_MEMBERS; i++)
      if (zip_members[i].data && !zip_members[i].refs
            && px68k_zip_virtual_slot(zip_members[i].path) == slot)
      {
         free(zip_members[i].data);
         memset(&zip_members[i], 0, sizeof(zip_members[i]));
      }
   free(zip_archives[slot]);
   zip_archives[slot] = copy;
   zip_age[slot]      = ++zip_clock;
   return slot;
}

void px68k_zip_pin(int slot)
{
   zip_pinned = slot;
}

void px68k_zip_reset(void)
{
   int i;
   zip_close_archive();
   for (i = 0; i < ZIP_HANDLES; i++)
      zip_handles[i].m = NULL;
   for (i = 0; i < ZIP_MEMBERS; i++)
   {
      free(zip_members[i].data);
      memset(&zip_members[i], 0, sizeof(zip_members[i]));
   }
   for (i = 0; i < ZIP_SLOTS; i++)
   {
      free(zip_archives[i]);
      zip_archives[i] = NULL;
      zip_age[i]      = 0;
   }
   zip_clock  = 0;
   zip_pinned = -1;
}

int px68k_zip_count(int slot, int hdd)
{
   if (!zip_open_slot(slot))
      return 0;
   return zo.num[hdd ? 1 : 0];
}

const char *px68k_zip_name(int slot, int hdd, int index)
{
   const rzip_entry_t *e;
   int l = hdd ? 1 : 0;
   if (!zip_open_slot(slot) || index < 0 || index >= zo.num[l])
      return NULL;
   e = rzip_archive_entry(zo.ar, (uint32_t)zo.list[l][index]);
   return e ? e->name : NULL;
}

int px68k_is_zip_virtual_path(const char *path)
{
   return path && !strncmp(path, ZIP_VIRTUAL_PREFIX, sizeof(ZIP_VIRTUAL_PREFIX) - 1);
}

/* Slot of "zip://@N/member", or -1; *member set to the name when given. */
static int split_virtual(const char *path, const char **member)
{
   const char *p;
   int slot = 0;
   if (!px68k_is_zip_virtual_path(path))
      return -1;
   p = path + sizeof(ZIP_VIRTUAL_PREFIX) - 1;
   if (!isdigit((unsigned char)*p))
      return -1;
   while (isdigit((unsigned char)*p) && slot < ZIP_SLOTS)
      slot = slot * 10 + (*p++ - '0');
   if (*p != '/' || !p[1] || slot >= ZIP_SLOTS || !zip_archives[slot])
      return -1;
   if (member)
      *member = p + 1;
   return slot;
}

int px68k_zip_virtual_slot(const char *path)
{
   return split_virtual(path, NULL);
}

int px68k_zip_make_virtual_path(int slot, const char *member, char *out, size_t out_size)
{
   int n;
   if (slot < 0 || slot >= ZIP_SLOTS || !zip_archives[slot] || !member || !out || !out_size)
      return 0;
   n = snprintf(out, out_size, ZIP_VIRTUAL_PREFIX "%d/%s", slot, member);
   return n > 0 && (size_t)n < out_size;
}

int px68k_zip_is_browse_path(const char *path)
{
   return path && !strncmp(path, ZIP_BROWSE_PREFIX, sizeof(ZIP_BROWSE_PREFIX) - 1);
}

int px68k_zip_browse_slot(const char *path)
{
   const char *p;
   int slot = 0;
   if (!px68k_zip_is_browse_path(path))
      return -1;
   p = path + sizeof(ZIP_BROWSE_PREFIX) - 1;
   if (!isdigit((unsigned char)*p))
      return -1;
   while (isdigit((unsigned char)*p) && slot < ZIP_SLOTS)
      slot = slot * 10 + (*p++ - '0');
   if (*p || slot >= ZIP_SLOTS || !zip_archives[slot])
      return -1;
   return slot;
}

int px68k_zip_get_archive_path(int slot, char *out, size_t out_size)
{
   int n;
   if (slot < 0 || slot >= ZIP_SLOTS || !zip_archives[slot] || !out || !out_size)
      return 0;
   n = snprintf(out, out_size, "%s", zip_archives[slot]);
   return n >= 0 && (size_t)n < out_size;
}

static struct zip_member *zip_member_get(const char *virtual_path)
{
   const rzip_entry_t *e;
   struct zip_member *m = NULL;
   const char *member;
   uint8_t *data;
   size_t size;
   int i, slot, idx, resident;

   if (strlen(virtual_path) >= MAX_PATH)
      return NULL;
   for (i = 0; i < ZIP_MEMBERS; i++)
      if (zip_members[i].data && !strcmp(zip_members[i].path, virtual_path))
         return &zip_members[i];

   if ((slot = split_virtual(virtual_path, &member)) < 0 || !zip_open_slot(slot))
      return NULL;
   if ((idx = rzip_archive_find(zo.ar, member)) < 0)
      return NULL;
   e        = rzip_archive_entry(zo.ar, (uint32_t)idx);
   resident = is_hdf_name(member);
   if (!e || e->is_dir || e->size > (resident ? ZIP_HDF_MAX : ZIP_FLOPPY_MAX))
      return NULL;

   for (i = 0; i < ZIP_MEMBERS && !m; i++)
      if (!zip_members[i].data)
         m = &zip_members[i];
   if (!m)
      return NULL;

   if (rzip_archive_extract(zo.ar, (uint32_t)idx, &data, &size) != RZIP_OK)
      return NULL;
   m->data     = data;
   m->size     = size;
   m->refs     = 0;
   m->resident = resident;
   strcpy(m->path, virtual_path);
   return m;
}

int px68k_zip_preload(const char *virtual_path)
{
   return virtual_path && zip_member_get(virtual_path) != NULL;
}

void *px68k_zip_open(const char *virtual_path)
{
   struct zip_member *m;
   int i;
   for (i = 0; i < ZIP_HANDLES; i++)
      if (!zip_handles[i].m)
         break;
   if (i == ZIP_HANDLES || !(m = zip_member_get(virtual_path)))
      return NULL;
   m->refs++;
   zip_handles[i].m   = m;
   zip_handles[i].pos = 0;
   return &zip_handles[i];
}

int px68k_zip_owns(const void *handle)
{
   int i;
   for (i = 0; i < ZIP_HANDLES; i++)
      if (handle == (const void*)&zip_handles[i])
         return 1;
   return 0;
}

size_t px68k_zip_seek(void *handle, long pos, int mode)
{
   struct zip_handle *h = (struct zip_handle*)handle;
   long base;
   if (!h->m)
      return (size_t)-1;
   switch (mode)
   {
      case FSEEK_CUR:
         base = (long)h->pos;
         break;
      case FSEEK_END:
         base = (long)h->m->size;
         break;
      default:
         base = 0;
         break;
   }
   if (pos < 0 && (unsigned long)-pos > (unsigned long)base)
      return (size_t)-1;
   h->pos = (size_t)(base + pos);
   return h->pos;
}

size_t px68k_zip_read(void *handle, void *data, size_t len)
{
   struct zip_handle *h = (struct zip_handle*)handle;
   if (!h->m || h->pos >= h->m->size)
      return 0;
   if (len > h->m->size - h->pos)
      len = h->m->size - h->pos;
   memcpy(data, h->m->data + h->pos, len);
   h->pos += len;
   return len;
}

/* Writes land in the cached member only; they are gone after
 * px68k_zip_reset(). The image never grows. */
size_t px68k_zip_write(void *handle, const void *data, size_t len)
{
   struct zip_handle *h = (struct zip_handle*)handle;
   if (!h->m || h->pos >= h->m->size)
      return 0;
   if (len > h->m->size - h->pos)
      len = h->m->size - h->pos;
   memcpy(h->m->data + h->pos, data, len);
   h->pos += len;
   return len;
}

int64_t px68k_zip_tell(void *handle)
{
   return (int64_t)((struct zip_handle*)handle)->pos;
}

void px68k_zip_close(void *handle)
{
   struct zip_handle *h = (struct zip_handle*)handle;
   struct zip_member *m = h->m;
   h->m = NULL;
   if (m && --m->refs <= 0 && !m->resident)
   {
      free(m->data);
      memset(m, 0, sizeof(*m));
   }
}
