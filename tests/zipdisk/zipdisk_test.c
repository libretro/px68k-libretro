/* Regression test for libretro/zipdisk.c.
 *
 * Builds its archives at run time (stored and raw-deflated members) and
 * checks the member list, the memory-backed file handles behind dosio,
 * the archive slot table and malformed input. Exits non-zero on the
 * first failure. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/crc32.h>
#include <encodings/deflate.h>

#include "dosio.h"
#include "prop.h"
#include "zipdisk.h"

Win68Conf Config;

/* ---- the dosio calls zipdisk.c makes on the archive itself ---- */

void *file_open(const char *filename)
{
   return fopen(filename, "rb");
}

size_t file_seek(void *handle, long pointer, int16_t mode)
{
   int whence = mode == FSEEK_END ? SEEK_END : (mode == FSEEK_CUR ? SEEK_CUR : SEEK_SET);
   if (fseek((FILE*)handle, pointer, whence))
      return (size_t)-1;
   return (size_t)ftell((FILE*)handle);
}

size_t file_lread(void *handle, void *data, size_t length)
{
   return fread(data, 1, length, (FILE*)handle);
}

void file_close(void *handle)
{
   fclose((FILE*)handle);
}

/* ---- archive writer ---- */

struct zmember
{
   const char    *name;
   const uint8_t *data;
   size_t         len;
   int            deflate;
   uint32_t       claim_size;   /* nonzero: lie about the size in the directory */
};

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

static void put16(uint8_t *p, unsigned v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

static uint8_t *raw_deflate(const uint8_t *in, size_t len, size_t *out_len)
{
   size_t cap = len + len / 8 + 1024, rd = 0, wr = 0;
   uint8_t *out = (uint8_t*)malloc(cap);
   void *s      = rdeflate_new(6, -15);
   int r;
   rdeflate_set_in(s, in, len);
   rdeflate_set_out(s, out, cap);
   rdeflate_finish(s);
   r = rdeflate_process(s, &rd, &wr);
   rdeflate_free(s);
   if (r != RDEFLATE_PROCESS_END)
   {
      free(out);
      return NULL;
   }
   *out_len = wr;
   return out;
}

static int write_zip(const char *path, const struct zmember *m, int n)
{
   uint8_t hdr[46];
   uint32_t *off   = (uint32_t*)calloc((size_t)n, sizeof(uint32_t));
   uint32_t *csz   = (uint32_t*)calloc((size_t)n, sizeof(uint32_t));
   uint32_t *crc   = (uint32_t*)calloc((size_t)n, sizeof(uint32_t));
   uint32_t pos    = 0, cd_start, cd_len = 0;
   FILE *f         = fopen(path, "wb");
   int i;

   if (!f)
      return 0;
   for (i = 0; i < n; i++)
   {
      size_t nl = strlen(m[i].name), clen = m[i].len;
      uint8_t *cdata = NULL;
      const uint8_t *body = m[i].data;
      if (m[i].deflate)
      {
         cdata = raw_deflate(m[i].data, m[i].len, &clen);
         body  = cdata;
      }
      crc[i] = encoding_crc32(0, m[i].data, m[i].len);
      csz[i] = (uint32_t)clen;
      off[i] = pos;
      memset(hdr, 0, sizeof(hdr));
      put32(hdr, 0x04034b50u);
      put16(hdr + 4, 20);
      put16(hdr + 8, m[i].deflate ? 8 : 0);
      put32(hdr + 14, crc[i]);
      put32(hdr + 18, (uint32_t)clen);
      put32(hdr + 22, m[i].claim_size ? m[i].claim_size : (uint32_t)m[i].len);
      put16(hdr + 26, (unsigned)nl);
      fwrite(hdr, 1, 30, f);
      fwrite(m[i].name, 1, nl, f);
      if (clen)
         fwrite(body, 1, clen, f);
      pos += (uint32_t)(30 + nl + clen);
      free(cdata);
   }
   cd_start = pos;
   for (i = 0; i < n; i++)
   {
      size_t nl = strlen(m[i].name);
      memset(hdr, 0, sizeof(hdr));
      put32(hdr, 0x02014b50u);
      put16(hdr + 4, 20);
      put16(hdr + 6, 20);
      put16(hdr + 10, m[i].deflate ? 8 : 0);
      put32(hdr + 16, crc[i]);
      put32(hdr + 20, csz[i]);
      put32(hdr + 24, m[i].claim_size ? m[i].claim_size : (uint32_t)m[i].len);
      put16(hdr + 28, (unsigned)nl);
      put32(hdr + 42, off[i]);
      fwrite(hdr, 1, 46, f);
      fwrite(m[i].name, 1, nl, f);
      cd_len += (uint32_t)(46 + nl);
   }
   memset(hdr, 0, 22);
   put32(hdr, 0x06054b50u);
   put16(hdr + 8, (unsigned)n);
   put16(hdr + 10, (unsigned)n);
   put32(hdr + 12, cd_len);
   put32(hdr + 16, cd_start);
   fwrite(hdr, 1, 22, f);
   fclose(f);
   free(off);
   free(csz);
   free(crc);
   return 1;
}

static uint8_t *pattern(size_t len, unsigned seed)
{
   uint8_t *p = (uint8_t*)malloc(len ? len : 1);
   size_t i;
   uint32_t x = seed * 2654435761u + 1;
   for (i = 0; i < len; i++)
   {
      x    = x * 1103515245u + 12345u;
      /* Runs keep it compressible, the LCG keeps it from being trivial. */
      p[i] = (i & 64) ? (uint8_t)(x >> 24) : (uint8_t)(i >> 7);
   }
   return p;
}

static char tmpdir[512];

static const char *tpath(const char *name)
{
   static char buf[4][600];
   static int which;
   which = (which + 1) & 3;
   snprintf(buf[which], sizeof(buf[which]), "%s/%s", tmpdir, name);
   return buf[which];
}

/* Reads a whole member through the handle API, as the FDD loaders do. */
static int read_member(const char *vpath, const uint8_t *expect, size_t len)
{
   uint8_t *got;
   size_t n;
   void *h = px68k_zip_open(vpath);
   if (!h)
      return 0;
   got = (uint8_t*)malloc(len + 16);
   CHECK(px68k_zip_seek(h, 0, FSEEK_END) == len);
   CHECK(px68k_zip_seek(h, 0, FSEEK_SET) == 0);
   n = px68k_zip_read(h, got, len + 16);
   px68k_zip_close(h);
   n = (n == len && !memcmp(got, expect, len));
   free(got);
   return n;
}

static void test_listing_and_reads(void)
{
   uint8_t *a = pattern(1261568, 1), *b = pattern(737280, 2), *c = pattern(20000, 3);
   uint8_t *hdf = pattern(256 * 64, 4);
   struct zmember m[9];
   char vpath[256];
   int slot;

   m[0].name = "Game (Disk 10).xdf"; m[0].data = c; m[0].len = 20000;   m[0].deflate = 1; m[0].claim_size = 0;
   m[1].name = "Game (Disk 2).dim";  m[1].data = b; m[1].len = 737280;  m[1].deflate = 0; m[1].claim_size = 0;
   m[2].name = "readme.txt";         m[2].data = c; m[2].len = 100;     m[2].deflate = 0; m[2].claim_size = 0;
   m[3].name = "Game (Disk 1).XDF";  m[3].data = a; m[3].len = 1261568; m[3].deflate = 1; m[3].claim_size = 0;
   m[4].name = "sub/";               m[4].data = c; m[4].len = 0;       m[4].deflate = 0; m[4].claim_size = 0;
   m[5].name = "bad\"quote.d88";     m[5].data = c; m[5].len = 100;     m[5].deflate = 0; m[5].claim_size = 0;
   m[6].name = "system.hdf";         m[6].data = hdf; m[6].len = 256 * 64; m[6].deflate = 1; m[6].claim_size = 0;
   m[7].name = "game (disk 3).d88";  m[7].data = c; m[7].len = 5000;    m[7].deflate = 1; m[7].claim_size = 0;
   m[8].name = "sub/inner.hdm";      m[8].data = c; m[8].len = 300;     m[8].deflate = 0; m[8].claim_size = 0;
   CHECK(write_zip(tpath("set.zip"), m, 9));

   slot = px68k_zip_register_archive(tpath("set.zip"));
   CHECK(slot >= 0);
   CHECK(px68k_zip_register_archive(tpath("set.zip")) == slot);

   /* Floppies only, natural order, directories and quoted names left out. */
   CHECK(px68k_zip_count(slot, 0) == 5);
   CHECK(!strcmp(px68k_zip_name(slot, 0, 0), "Game (Disk 1).XDF"));
   CHECK(!strcmp(px68k_zip_name(slot, 0, 1), "Game (Disk 2).dim"));
   CHECK(!strcmp(px68k_zip_name(slot, 0, 2), "game (disk 3).d88"));
   CHECK(!strcmp(px68k_zip_name(slot, 0, 3), "Game (Disk 10).xdf"));
   CHECK(!strcmp(px68k_zip_name(slot, 0, 4), "sub/inner.hdm"));
   CHECK(px68k_zip_name(slot, 0, 5) == NULL);
   CHECK(px68k_zip_count(slot, 1) == 1);
   CHECK(!strcmp(px68k_zip_name(slot, 1, 0), "system.hdf"));

   /* Deflated and stored members come back byte-exact. */
   CHECK(px68k_zip_make_virtual_path(slot, "Game (Disk 1).XDF", vpath, sizeof(vpath)));
   CHECK(px68k_is_zip_virtual_path(vpath));
   CHECK(px68k_zip_virtual_slot(vpath) == slot);
   CHECK(read_member(vpath, a, 1261568));
   CHECK(px68k_zip_make_virtual_path(slot, "Game (Disk 2).dim", vpath, sizeof(vpath)));
   CHECK(read_member(vpath, b, 737280));
   CHECK(px68k_zip_make_virtual_path(slot, "sub/inner.hdm", vpath, sizeof(vpath)));
   CHECK(read_member(vpath, c, 300));
   CHECK(px68k_zip_make_virtual_path(slot, "nope.xdf", vpath, sizeof(vpath)));
   CHECK(px68k_zip_open(vpath) == NULL);

   free(a);
   free(b);
   free(c);
   free(hdf);
}

/* SASI reads a sector per open; writes must last for the session, and
 * accesses past the image end must fail the way a short file does. */
static void test_hdf_session(void)
{
   uint8_t *hdf = pattern(256 * 64, 4), sector[256], back[256];
   char vpath[256];
   int slot = px68k_zip_register_archive(tpath("set.zip"));
   void *h;

   CHECK(px68k_zip_make_virtual_path(slot, "system.hdf", vpath, sizeof(vpath)));
   CHECK(px68k_zip_preload(vpath));

   h = px68k_zip_open(vpath);
   CHECK(h && px68k_zip_owns(h));
   memset(sector, 0xa5, sizeof(sector));
   CHECK(px68k_zip_seek(h, 5 << 8, FSEEK_SET) == (5 << 8));
   CHECK(px68k_zip_write(h, sector, 256) == 256);
   CHECK(px68k_zip_tell(h) == (6 << 8));
   px68k_zip_close(h);

   h = px68k_zip_open(vpath);
   CHECK(px68k_zip_seek(h, 5 << 8, FSEEK_SET) == (5 << 8));
   CHECK(px68k_zip_read(h, back, 256) == 256 && !memcmp(back, sector, 256));
   CHECK(px68k_zip_seek(h, 4 << 8, FSEEK_SET) == (4 << 8));
   CHECK(px68k_zip_read(h, back, 256) == 256 && !memcmp(back, hdf + (4 << 8), 256));
   /* Past the end: the seek lands, the transfer is short. */
   CHECK(px68k_zip_seek(h, 64 << 8, FSEEK_SET) == (64 << 8));
   CHECK(px68k_zip_read(h, back, 256) == 0);
   CHECK(px68k_zip_write(h, sector, 256) == 0);
   CHECK(px68k_zip_seek(h, 63 << 8, FSEEK_SET) == (63 << 8));
   CHECK(px68k_zip_read(h, back, 512) == 256);
   CHECK(px68k_zip_seek(h, -1, FSEEK_SET) == (size_t)-1);
   px68k_zip_close(h);
   /* A closed handle stays ours (never mistaken for a file) and is inert. */
   CHECK(px68k_zip_owns(h));
   CHECK(px68k_zip_read(h, back, 256) == 0);
   CHECK(px68k_zip_seek(h, 0, FSEEK_SET) == (size_t)-1);
   free(hdf);
}

/* A slot a mounted drive refers to survives a full table; the slots that
 * get recycled serve their new archive, not the old one's members. */
static void test_slot_recycling(void)
{
   uint8_t *x = pattern(4096, 7), *y = pattern(4096, 8);
   struct zmember m;
   char name[64], vpath[256], keep_path[600];
   int i, keep, slot, first = -1;

   m.name = "disk.xdf"; m.data = x; m.len = 4096; m.deflate = 1; m.claim_size = 0;
   CHECK(write_zip(tpath("keep.zip"), &m, 1));
   snprintf(keep_path, sizeof(keep_path), "%s", tpath("keep.zip"));
   keep = px68k_zip_register_archive(keep_path);
   CHECK(keep >= 0);
   CHECK(px68k_zip_make_virtual_path(keep, "disk.xdf", Config.FDDImage[0], sizeof(Config.FDDImage[0])));

   m.data = y;
   for (i = 0; i < 100; i++)
   {
      snprintf(name, sizeof(name), "other%d.zip", i);
      if (i < 2)
         CHECK(write_zip(tpath(name), &m, 1));
      slot = px68k_zip_register_archive(tpath(name));
      CHECK(slot >= 0 && slot != keep);
      if (i == 0)
         first = slot;
      if (i == 0 || i == 1)
      {
         CHECK(px68k_zip_make_virtual_path(slot, "disk.xdf", vpath, sizeof(vpath)));
         CHECK(read_member(vpath, y, 4096));
      }
   }
   /* The drive's archive was never recycled. */
   CHECK(read_member(Config.FDDImage[0], x, 4096));
   CHECK(px68k_zip_register_archive(keep_path) == keep);

   /* other0.zip's slot was reused; registering it again serves it fresh. */
   slot = px68k_zip_register_archive(tpath("other0.zip"));
   CHECK(slot >= 0);
   CHECK(px68k_zip_make_virtual_path(slot, "disk.xdf", vpath, sizeof(vpath)));
   CHECK(read_member(vpath, y, 4096));
   (void)first;

   Config.FDDImage[0][0] = '\0';
   free(x);
   free(y);
}

static void test_rejects(void)
{
   uint8_t *z = (uint8_t*)calloc(9 * 1024 * 1024, 1), junk[300];
   struct zmember m[2];
   char vpath[256];
   FILE *f;
   int slot, i;

   /* A floppy larger than any format is refused before it is inflated. */
   m[0].name = "huge.xdf"; m[0].data = z; m[0].len = 9 * 1024 * 1024; m[0].deflate = 1; m[0].claim_size = 0;
   /* A directory size that does not match the data fails the extract. */
   m[1].name = "liar.xdf"; m[1].data = z; m[1].len = 1000; m[1].deflate = 1; m[1].claim_size = 0x7ffffff0u;
   CHECK(write_zip(tpath("bad.zip"), m, 2));
   slot = px68k_zip_register_archive(tpath("bad.zip"));
   CHECK(px68k_zip_make_virtual_path(slot, "huge.xdf", vpath, sizeof(vpath)));
   CHECK(px68k_zip_open(vpath) == NULL);
   CHECK(px68k_zip_make_virtual_path(slot, "liar.xdf", vpath, sizeof(vpath)));
   CHECK(px68k_zip_open(vpath) == NULL);
   free(z);

   /* Not a zip, empty, truncated. */
   for (i = 0; i < (int)sizeof(junk); i++)
      junk[i] = (uint8_t)(i * 37);
   f = fopen(tpath("junk.zip"), "wb");
   fwrite(junk, 1, sizeof(junk), f);
   fclose(f);
   f = fopen(tpath("empty.zip"), "wb");
   fclose(f);
   CHECK(px68k_zip_count(px68k_zip_register_archive(tpath("junk.zip")), 0) == 0);
   CHECK(px68k_zip_count(px68k_zip_register_archive(tpath("empty.zip")), 0) == 0);
   CHECK(px68k_zip_count(px68k_zip_register_archive(tpath("missing.zip")), 0) == 0);

   /* Malformed virtual paths. */
   CHECK(px68k_zip_virtual_slot("zip://@/a.xdf") < 0);
   CHECK(px68k_zip_virtual_slot("zip://@0") < 0);
   CHECK(px68k_zip_virtual_slot("zip://@0/") < 0);
   CHECK(px68k_zip_virtual_slot("zip://@99999999999/a.xdf") < 0);
   CHECK(px68k_zip_browse_slot("zipbrowse://") < 0);
   CHECK(px68k_zip_browse_slot("zipbrowse://0x") < 0);
   CHECK(px68k_zip_open("zip://@0/") == NULL);
}

int main(int argc, char **argv)
{
   snprintf(tmpdir, sizeof(tmpdir), "%s", argc > 1 ? argv[1] : ".");
   test_listing_and_reads();
   test_hdf_session();
   test_slot_recycling();
   test_rejects();
   px68k_zip_reset();
   CHECK(px68k_zip_count(0, 0) == 0);
   CHECK(px68k_zip_virtual_slot("zip://@0/a.xdf") < 0); /* unregistered */
   if (failures)
   {
      fprintf(stderr, "zipdisk_test: %d failure(s)\n", failures);
      return 1;
   }
   printf("zipdisk_test: all checks passed\n");
   return 0;
}
