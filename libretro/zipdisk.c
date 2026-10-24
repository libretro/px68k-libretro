#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <limits.h>
#include <zlib.h>
#include "zipdisk.h"
#include "dosio.h"

#define ZIP_VIRTUAL_PREFIX "zip://"
#define ZIP_BROWSE_PREFIX "zipbrowse://"
#define ZIP_EOCD 0x06054b50u
#define ZIP_CENTRAL 0x02014b50u
#define ZIP_LOCAL 0x04034b50u
#define ZIP_ARCHIVE_PATH_MAX 4096

static char zip_archives[PX68K_ZIP_ARCHIVE_SLOTS][ZIP_ARCHIVE_PATH_MAX];
static int current_archive_slot = -1;

static uint16_t rd16(const uint8_t *p)
{
   return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
      ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int px68k_zip_register_archive(const char *archive)
{
   int i, free_slot = -1;
   if (!archive || !archive[0])
      return -1;
   for (i = 0; i < PX68K_ZIP_ARCHIVE_SLOTS; i++)
   {
      if (zip_archives[i][0] && !strcmp(zip_archives[i], archive))
      {
         current_archive_slot = i;
         return i;
      }
      if (!zip_archives[i][0] && free_slot < 0)
         free_slot = i;
   }
   if (free_slot < 0)
      return -1;
   snprintf(zip_archives[free_slot], sizeof(zip_archives[free_slot]), "%s", archive);
   current_archive_slot = free_slot;
   return free_slot;
}

void px68k_zip_set_archive(const char *archive)
{
   if (!archive)
   {
      current_archive_slot = -1;
      return;
   }
   (void)px68k_zip_register_archive(archive);
}

int px68k_zip_current_slot(void)
{
   return current_archive_slot;
}

static uint8_t *zip_read_archive_file(const char *path, size_t *size)
{
   void *f = file_open(path);
   uint8_t *b;
   int64_t n;
   if (!f)
      return NULL;
   file_seek(f, 0, FSEEK_END);
   n = file_tell(f);
   file_seek(f, 0, FSEEK_SET);
   if (n < 22 || (uint64_t)n > 0x7fffffffU)
   {
      file_close(f);
      return NULL;
   }
   b = (uint8_t*)malloc((size_t)n);
   if (!b)
   {
      file_close(f);
      return NULL;
   }
   if (file_lread(f, b, (size_t)n) != (size_t)n)
   {
      free(b);
      file_close(f);
      return NULL;
   }
   file_close(f);
   *size = (size_t)n;
   return b;
}

static int find_eocd(const uint8_t *b, size_t n)
{
   size_t i, start = n > 65557 ? n - 65557 : 0;
   if (n < 22)
      return -1;
   for (i = n - 22;; i--)
   {
      if (rd32(b + i) == ZIP_EOCD)
         return (int)i;
      if (i == start)
         break;
   }
   return -1;
}

static int ends_disk_ext(const char *name)
{
   const char *p = strrchr(name, '.');
   char ext[8];
   size_t i;
   if (!p)
      return 0;
   p++;
   for (i = 0; i < sizeof(ext) - 1 && p[i]; i++)
      ext[i] = (char)tolower((unsigned char)p[i]);
   ext[i] = 0;
   return !strcmp(ext, "xdf") || !strcmp(ext, "dim") ||
      !strcmp(ext, "hdm") || !strcmp(ext, "img") ||
      !strcmp(ext, "2hd") || !strcmp(ext, "dup") ||
      !strcmp(ext, "d88") || !strcmp(ext, "88d");
}

int px68k_is_zip_virtual_path(const char *path)
{
   return path && !strncmp(path, ZIP_VIRTUAL_PREFIX, strlen(ZIP_VIRTUAL_PREFIX));
}

int px68k_zip_is_browse_path(const char *path)
{
   return path && !strncmp(path, ZIP_BROWSE_PREFIX, strlen(ZIP_BROWSE_PREFIX));
}

int px68k_zip_browse_slot(const char *path)
{
   char *end;
   long slot;
   if (!px68k_zip_is_browse_path(path))
      return -1;
   slot = strtol(path + strlen(ZIP_BROWSE_PREFIX), &end, 10);
   if (end == path + strlen(ZIP_BROWSE_PREFIX) || *end ||
       slot < 0 || slot >= PX68K_ZIP_ARCHIVE_SLOTS || !zip_archives[slot][0])
      return -1;
   return (int)slot;
}

int px68k_zip_get_archive_path(int slot, char *out, size_t out_size)
{
   int n;
   if (slot < 0 || slot >= PX68K_ZIP_ARCHIVE_SLOTS ||
       !zip_archives[slot][0] || !out || !out_size)
      return 0;
   n = snprintf(out, out_size, "%s", zip_archives[slot]);
   return n >= 0 && (size_t)n < out_size;
}

int px68k_zip_make_virtual_path(int slot, const char *member, char *out, size_t out_size)
{
   int n;
   if (slot < 0 || slot >= PX68K_ZIP_ARCHIVE_SLOTS ||
       !zip_archives[slot][0] || !member || !out || !out_size)
      return 0;
   n = snprintf(out, out_size, "zip://@%d/%s", slot, member);
   return n >= 0 && (size_t)n < out_size;
}

static int zip_list_internal(int slot, char *names, size_t name_stride, int capacity)
{
   size_t n = 0, pos;
   uint8_t *b;
   int eocd, count = 0, i;
   uint32_t cdsize, cdoff;
   uint16_t entries;
   const char *archive;

   if (slot < 0 || slot >= PX68K_ZIP_ARCHIVE_SLOTS || !zip_archives[slot][0] ||
       !names || name_stride == 0 || capacity <= 0)
      return 0;
   archive = zip_archives[slot];
   b = zip_read_archive_file(archive, &n);
   if (!b)
      return 0;
   eocd = find_eocd(b, n);
   if (eocd < 0)
   {
      free(b);
      return 0;
   }
   entries = rd16(b + eocd + 10);
   cdsize = rd32(b + eocd + 12);
   cdoff = rd32(b + eocd + 16);
   if ((uint64_t)cdoff + cdsize > n)
   {
      free(b);
      return 0;
   }
   pos = cdoff;
   for (i = 0; i < entries && count < capacity; i++)
   {
      uint16_t nl, xl, cl, flags;
      if (pos + 46 > n || rd32(b + pos) != ZIP_CENTRAL)
         break;
      flags = rd16(b + pos + 8);
      nl = rd16(b + pos + 28);
      xl = rd16(b + pos + 30);
      cl = rd16(b + pos + 32);
      if (pos + 46u + nl + xl + cl > n)
         break;
      if (!(flags & 1) && nl > 0 && nl < PX68K_ZIP_NAME_MAX &&
          b[pos + 46 + nl - 1] != '/' &&
          !memchr(b + pos + 46, 0, nl))
      {
         char name[PX68K_ZIP_NAME_MAX];
         memcpy(name, b + pos + 46, nl);
         name[nl] = 0;
         if (ends_disk_ext(name) && !strstr(name, "../") &&
             strncmp(name, "../", 3) && !strchr(name, '\\'))
         {
            /* The caller may use a smaller UI filename buffer than the ZIP
             * reader maximum. Always terminate within that caller-provided
             * stride instead of assuming every row is 4096 bytes. */
            snprintf(names + (size_t)count * name_stride, name_stride, "%s", name);
            count++;
         }
      }
      pos += 46u + nl + xl + cl;
   }
   free(b);
   return count;
}

int px68k_zip_list_slot(int slot, char *names, size_t name_stride, int capacity)
{
   return zip_list_internal(slot, names, name_stride, capacity);
}

int px68k_zip_list(const char *archive, char names[PX68K_ZIP_MAX_ENTRIES][PX68K_ZIP_NAME_MAX])
{
   int slot = px68k_zip_register_archive(archive);
   return zip_list_internal(slot, &names[0][0], PX68K_ZIP_NAME_MAX, PX68K_ZIP_MAX_ENTRIES);
}

static int split_virtual(const char *v, char *archive, size_t alen, const char **member)
{
   const char *p;
   if (!px68k_is_zip_virtual_path(v))
      return 0;
   p = v + strlen(ZIP_VIRTUAL_PREFIX);
   if (*p == '@')
   {
      char *end;
      long slot = strtol(p + 1, &end, 10);
      if (end == p + 1 || *end != '/' || slot < 0 ||
          slot >= PX68K_ZIP_ARCHIVE_SLOTS || !zip_archives[slot][0])
         return 0;
      snprintf(archive, alen, "%s", zip_archives[slot]);
      *member = end + 1;
      return **member != 0;
   }
   if (current_archive_slot < 0 || !zip_archives[current_archive_slot][0])
      return 0;
   snprintf(archive, alen, "%s", zip_archives[current_archive_slot]);
   *member = p;
   return **member != 0;
}

int px68k_zip_read_virtual(const char *virtual_path, uint8_t **data, size_t *size)
{
   char archive[ZIP_ARCHIVE_PATH_MAX];
   const char *member;
   size_t n = 0, pos;
   uint8_t *b, *out;
   int eocd, found = 0, i;
   uint32_t cdoff;
   uint16_t entries;
   if (!data || !size || !split_virtual(virtual_path, archive, sizeof(archive), &member))
      return 0;
   *data = NULL;
   *size = 0;
   b = zip_read_archive_file(archive, &n);
   if (!b)
      return 0;
   eocd = find_eocd(b, n);
   if (eocd < 0)
   {
      free(b);
      return 0;
   }
   entries = rd16(b + eocd + 10);
   cdoff = rd32(b + eocd + 16);
   if ((uint64_t)cdoff > n)
   {
      free(b);
      return 0;
   }
   pos = cdoff;
   for (i = 0; i < entries; i++)
   {
      uint16_t nl, xl, cl;
      if (pos + 46 > n || rd32(b + pos) != ZIP_CENTRAL)
         break;
      nl = rd16(b + pos + 28);
      xl = rd16(b + pos + 30);
      cl = rd16(b + pos + 32);
      if (pos + 46u + nl + xl + cl > n)
         break;
      if (strlen(member) == nl && !memcmp(b + pos + 46, member, nl))
      {
         uint16_t method = rd16(b + pos + 10);
         uint16_t flags = rd16(b + pos + 8);
         uint32_t csize = rd32(b + pos + 20);
         uint32_t usize = rd32(b + pos + 24);
         uint32_t loff = rd32(b + pos + 42);
         uint16_t lnl, lxl;
         uint8_t *src;
         if ((flags & 1) || (uint64_t)loff + 30 > n || rd32(b + loff) != ZIP_LOCAL)
            break;
         lnl = rd16(b + loff + 26);
         lxl = rd16(b + loff + 28);
         if ((uint64_t)loff + 30 + lnl + lxl + csize > n)
            break;
         src = b + loff + 30 + lnl + lxl;
         out = (uint8_t*)malloc(usize ? usize : 1);
         if (!out)
            break;
         if (method == 0)
         {
            if (csize != usize)
            {
               free(out);
               break;
            }
            memcpy(out, src, usize);
         }
         else if (method == 8)
         {
            z_stream zs;
            int zr;
            memset(&zs, 0, sizeof(zs));
            zs.next_in = src;
            zs.avail_in = csize;
            zs.next_out = out;
            zs.avail_out = usize;
            if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)
            {
               free(out);
               break;
            }
            zr = inflate(&zs, Z_FINISH);
            inflateEnd(&zs);
            if (zr != Z_STREAM_END || zs.total_out != usize)
            {
               free(out);
               break;
            }
         }
         else
         {
            free(out);
            break;
         }
         *data = out;
         *size = usize;
         found = 1;
         break;
      }
      pos += 46u + nl + xl + cl;
   }
   free(b);
   return found;
}
