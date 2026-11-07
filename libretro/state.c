/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>
#include <compat/msvc.h>
#include <compat/strl.h>

#include "common.h"
#include "state.h"

#define SSEEK_END	2
#define SSEEK_CUR	1
#define SSEEK_SET	0

#define RLSB 		PX68KSTATE_RLSB	//0x80000000

/* Forward declaration */
int StateAction(StateMem *sm, int load, int data_only);

static INLINE void PX68K_en32lsb(uint8_t *buf, uint32_t morp)
{
   buf[0]=morp;
   buf[1]=morp>>8;
   buf[2]=morp>>16;
   buf[3]=morp>>24;
}

static INLINE uint32_t PX68K_de32lsb(const uint8_t *morp)
{
   return((uint32_t)morp[0]|((uint32_t)morp[1]<<8)|((uint32_t)morp[2]<<16)|((uint32_t)morp[3]<<24));
}

#ifdef MSB_FIRST
static INLINE void Endian_A64_Swap(void *src, uint32_t nelements)
{
   uint32_t i;
   uint8_t *nsrc = (uint8_t *)src;

   for(i = 0; i < nelements; i++)
   {
      unsigned z;
      uint8_t *base = &nsrc[i * 8];

      for(z = 0; z < 4; z++)
      {
         uint8_t tmp = base[z];

         base[z] = base[7 - z];
         base[7 - z] = tmp;
      }
   }
}

static INLINE void Endian_A32_Swap(void *src, uint32_t nelements)
{
   uint32_t i;
   uint8_t *nsrc = (uint8_t *)src;

   for(i = 0; i < nelements; i++)
   {
      uint8_t tmp1 = nsrc[i * 4];
      uint8_t tmp2 = nsrc[i * 4 + 1];

      nsrc[i * 4] = nsrc[i * 4 + 3];
      nsrc[i * 4 + 1] = nsrc[i * 4 + 2];

      nsrc[i * 4 + 2] = tmp2;
      nsrc[i * 4 + 3] = tmp1;
   }
}

void Endian_A16_Swap(void *src, uint32_t nelements)
{
   uint32_t i;
   uint8_t *nsrc = (uint8_t *)src;

   for(i = 0; i < nelements; i++)
   {
      uint8_t tmp = nsrc[i * 2];

      nsrc[i * 2] = nsrc[i * 2 + 1];
      nsrc[i * 2 + 1] = tmp;
   }
}

static INLINE void FlipByteOrder(uint8_t *src, uint32_t count)
{
   uint8_t *start=src;
   uint8_t *end=src+count-1;

   if((count&1) || !count)
      return;         /* This shouldn't happen. */

   count >>= 1;

   while(count--)
   {
      uint8_t tmp;

      tmp=*end;
      *end=*start;
      *start=tmp;
      end--;
      start++;
   }
}
#endif

/* Chunk header: 32-byte name + 32-bit little-endian payload size. */
#define CHUNK_HDR_SIZE (32 + 4)

static int32_t smem_read(StateMem *st, void *buffer, uint32_t len)
{
   if (len > st->len || st->loc > st->len - len)
      return 0;

   memcpy(buffer, st->data + st->loc, len);
   st->loc += len;

   return (int32_t)len;
}

static int32_t smem_write(StateMem *st, const void *buffer, uint32_t len)
{
   uint32_t end = st->loc + len;

   if (end < st->loc) /* 32-bit overflow */
      return 0;

   if (!st->count_only)
   {
      if (end > st->malloced)
      {
         uint8_t *newdata;
         uint32_t newsize;

         /* A caller-owned buffer can't grow: report failure instead. */
         if (st->fixed)
            return 0;

         newsize = (st->malloced >= 32768) ? st->malloced :
            (st->initial_malloc ? st->initial_malloc : 32768);

         while (newsize < end)
         {
            if (newsize > 0x7fffffffU)
               return 0;
            newsize *= 2;
         }

         newdata = (uint8_t *)realloc(st->data, newsize);
         if (!newdata)
            return 0;
         st->data     = newdata;
         st->malloced = newsize;
      }

      memcpy(st->data + st->loc, buffer, len);
   }

   st->loc = end;

   if (st->loc > st->len)
      st->len = st->loc;

   return (int32_t)len;
}

static int32_t smem_seek(StateMem *st, uint32_t offset, int whence)
{
   switch(whence)
   {
      case SSEEK_SET:
         st->loc = offset;
         break;
      case SSEEK_END:
         st->loc = st->len - offset;
         break;
      case SSEEK_CUR:
         st->loc += offset;
         break;
   }

   if(st->loc > st->len)
   {
      st->loc = st->len;
      return(-1);
   }

   return(0);
}

/* Bounds-checked forward skip. */
static int smem_skip(StateMem *st, uint32_t n)
{
   if (n > st->len || st->loc > st->len - n)
   {
      st->loc = st->len;
      return -1;
   }
   st->loc += n;
   return 0;
}

static int smem_write32le(StateMem *st, uint32_t b)
{
   uint8_t s[4];
   s[0]=b;
   s[1]=b>>8;
   s[2]=b>>16;
   s[3]=b>>24;
   return((smem_write(st, s, 4)<4)?0:4);
}

static int smem_read32le(StateMem *st, uint32_t *b)
{
   uint8_t s[4];

   if(smem_read(st, s, 4) < 4)
      return(0);

   *b = s[0] | (s[1] << 8) | (s[2] << 16) | ((uint32_t)s[3] << 24);

   return(4);
}

void PX68KSS_InitCount(StateMem *st)
{
   memset(st, 0, sizeof(*st));
   st->count_only = true;
}

void PX68KSS_InitWrite(StateMem *st, void *buf, uint32_t cap, bool fast)
{
   memset(st, 0, sizeof(*st));
   st->data           = (uint8_t*)buf;
   st->malloced       = cap;
   st->fixed          = true;
   st->fastsavestates = fast;
}

void PX68KSS_InitRead(StateMem *st, const void *buf, uint32_t len)
{
   memset(st, 0, sizeof(*st));
   st->data = (uint8_t*)buf; /* never written to while loading */
   st->len  = len;
}

static bool header_magic_ok(const uint8_t *header)
{
   return !memcmp(header, "PX68KSVESTATE", 13) || !memcmp(header, "PX68KSVST", 9);
}

bool PX68KSS_IsFastState(const void *buf, size_t len)
{
   const uint8_t *header = (const uint8_t*)buf;
   return buf && len >= 32 && header_magic_ok(header) && (header[24] & 1);
}

#ifdef MSB_FIRST
static void SwapFlagged(const SFORMAT *sf, int32_t bytesize)
{
   if(sf->flags & PX68KSTATE_BOOL) { }
   else if(sf->flags & PX68KSTATE_RLSB64)
      Endian_A64_Swap(sf->v, bytesize / sizeof(uint64_t));
   else if(sf->flags & PX68KSTATE_RLSB32)
      Endian_A32_Swap(sf->v, bytesize / sizeof(uint32_t));
   else if(sf->flags & PX68KSTATE_RLSB16)
      Endian_A16_Swap(sf->v, bytesize / sizeof(uint16_t));
   else if(sf->flags & RLSB)
      FlipByteOrder((uint8_t*)sf->v, bytesize);
}
#endif

static bool SubWrite(StateMem *st, SFORMAT *sf)
{
   /* Size can sometimes be zero, so also check for the text name.
    * These two should both be zero only at the end of a struct. */
   while(sf->size || sf->name)
   {
      int32_t bytesize;
      bool ok = true;

      if(!sf->size || !sf->v)
      {
         sf++;
         continue;
      }

      /* Link to another struct.	*/
      if(sf->size == (uint32_t)~0)
      {
         if(!SubWrite(st, (SFORMAT *)sf->v))
            return false;

         sf++;
         continue;
      }

      bytesize = sf->size;

      /* exclude text labels from fast savestates */
      if (!st->fastsavestates)
      {
         char nameo[1 + 256];
         int slen      = strlcpy(nameo + 1, sf->name, 255);
         nameo[256]    = 0;
         nameo[0]      = slen;

         if (!smem_write(st, nameo, 1 + (uint8_t)nameo[0]))
            return false;
      }

      if (!smem_write32le(st, bytesize))
         return false;

#ifdef MSB_FIRST
      /* Flip the byte order... */
      SwapFlagged(sf, bytesize);
#endif

      /* Special case for the evil bool type,
       * to convert bool to 1-byte elements. */
      if(sf->flags & PX68KSTATE_BOOL)
      {
         int32_t bool_monster;
         if (st->count_only)
            ok = smem_write(st, NULL, (uint32_t)bytesize) != 0 || !bytesize;
         else
            for (bool_monster = 0; ok && bool_monster < bytesize; bool_monster++)
            {
               uint8_t tmp_bool = ((bool *)sf->v)[bool_monster];
               ok = smem_write(st, &tmp_bool, 1) == 1;
            }
      }
      else
         ok = smem_write(st, (const uint8_t *)sf->v, bytesize) == bytesize;

#ifdef MSB_FIRST
      /* Now restore the original byte order. */
      SwapFlagged(sf, bytesize);
#endif
      if (!ok)
         return false;
      sf++;
   }

   return true;
}

static int WriteStateChunk(StateMem *st, const char *sname, SFORMAT *sf)
{
   uint32_t data_start_pos;
   uint32_t end_pos;

   uint8_t sname_tmp[32];
   size_t sname_len = strlen(sname);

   memset(sname_tmp, 0, sizeof(sname_tmp));
   memcpy((char *)sname_tmp, sname, (sname_len < 32) ? sname_len : 32);

   if (!smem_write(st, sname_tmp, 32))
      return 0;

   /* We'll come back and write this later. */
   if (!smem_write32le(st, 0))
      return 0;

   data_start_pos = st->loc;

   if(!SubWrite(st, sf))
      return(0);

   end_pos = st->loc;

   smem_seek(st, data_start_pos - 4, SSEEK_SET);
   smem_write32le(st, end_pos - data_start_pos);
   smem_seek(st, end_pos, SSEEK_SET);

   /* A zero-length chunk is legal; keep the return value non-zero. */
   return (int)(end_pos - data_start_pos) + 1;
}

static SFORMAT *FindSF(const char *name, SFORMAT *sf, bool FastSaveStates)
{
   /* Size can sometimes be zero, so also check for the text name.
    * These two should both be zero only at the end of a struct. */
   while(sf->size || sf->name)
   {
      if(!sf->size || !sf->v)
      {
         sf++;
         continue;
      }

      /* Link to another SFORMAT structure. */
      if (sf->size == (uint32_t)~0)
      {
         SFORMAT *temp_sf = FindSF(name, (SFORMAT*)sf->v, FastSaveStates);
         if (temp_sf)
            return temp_sf;
      }
      else
      {
         /* for fast savestates, we no longer have the
          * text label in the state, and need to assume
          * that it is the correct one. */
         if (FastSaveStates)
            return sf;

         if (!strcmp(sf->name, name))
            return sf;
      }

      sf++;
   }

   return NULL;
}

static int ReadStateChunk(StateMem *st, SFORMAT *sf, int size)
{
   uint32_t temp = st->loc;
   uint32_t end  = temp + (uint32_t)size;

   uint32_t recorded_size;  /* In bytes */
   uint8_t toa[1 + 256];    /* Don't change to char unless
                               cast toa[0] to unsigned to
                               smem_read() and other places. */
   toa[0] = 0;
   toa[1] = 0;

   while (st->loc < end)
   {
      SFORMAT *tmp           = NULL;

      /* exclude text labels from fast savestates */
      if (!st->fastsavestates)
      {

         if(smem_read(st, toa, 1) != 1)
            return(0);

         if(toa[0] && smem_read(st, toa + 1, toa[0]) != toa[0])
            return 0;

         toa[1 + toa[0]] = 0;
      }

      if (smem_read32le(st, &recorded_size) != 4)
         return 0;

      tmp = FindSF((char*)toa + 1, sf, st->fastsavestates);

      /* Fix for unnecessary name checks, when we find
       * it in the first slot, don't recheck that slot again.
       * Also necessary for fast savestates to work. */
      if (tmp == sf)
         sf++;

      if(tmp)
      {
         uint32_t expected_size = tmp->size;	/* In bytes */

         if ((tmp->flags & PX68KSTATE_VARLEN) && recorded_size <= expected_size)
         {
            /* Variable-length byte array: take what was stored and clear
             * the rest, so the result is the full array as it was saved. */
            if (recorded_size
                  && smem_read(st, (uint8_t *)tmp->v, recorded_size) != (int32_t)recorded_size)
               return 0;
            if (expected_size > recorded_size)
               memset((uint8_t *)tmp->v + recorded_size, 0,
                     expected_size - recorded_size);
         }
         else if(recorded_size != expected_size)
         {
            if(smem_skip(st, recorded_size) < 0)
               return(0);
         }
         else
         {
            if (expected_size
                  && smem_read(st, (uint8_t *)tmp->v, expected_size) != (int32_t)expected_size)
               return 0;

            if(tmp->flags & PX68KSTATE_BOOL)
            {
               int32_t bool_monster;
               /* Converting downwards is necessary
                * for the case of sizeof(bool) > 1 */
               for(bool_monster = expected_size - 1; bool_monster >= 0; bool_monster--)
                  ((bool *)tmp->v)[bool_monster] = ((uint8_t *)tmp->v)[bool_monster];
            }

#ifdef MSB_FIRST
            SwapFlagged(tmp, expected_size);
#endif
         }
      }
      else
      {
         if(smem_skip(st, recorded_size) < 0)
            return(0);
      }
   }

   return 1;
}

/* Find a section by name. Sections are almost always read back in the order
 * they were written, so the search starts just past the previous hit and only
 * wraps to the beginning if that fails. (Previously every lookup rescanned
 * the section list from the start, which is quadratic in the section count.)
 * On success st->loc is at the start of the section's payload. */
static int FindSection(StateMem *st, const char *name, uint32_t *size_out)
{
   int pass;
   uint32_t start[2];
   uint32_t stop[2];

   start[0] = st->sec_next; stop[0] = st->len;
   start[1] = st->sec_base; stop[1] = st->sec_next;

   for (pass = 0; pass < 2; pass++)
   {
      uint32_t pos = start[pass];

      while (pos < stop[pass] && st->len - pos >= CHUNK_HDR_SIZE)
      {
         const uint8_t *h = st->data + pos;
         uint32_t size    = PX68K_de32lsb(h + 32);

         if (size > st->len - pos - CHUNK_HDR_SIZE)
            break; /* truncated or corrupt */

         if (!strncmp((const char*)h, name, 32))
         {
            st->loc      = pos + CHUNK_HDR_SIZE;
            st->sec_next = st->loc + size;
            *size_out    = size;
            return 1;
         }

         pos += CHUNK_HDR_SIZE + size;
      }
   }

   return 0;
}

static int PX68KSS_StateAction_internal(StateMem *st, int load, int data_only,
      struct SSDescriptor *section, bool optional)
{
   if(load)
   {
      uint32_t size = 0;

      /* Not found: an optional section (one added after older states were
       * written) simply leaves the current values alone. */
      if (!FindSection(st, section->name, &size))
         return optional ? 1 : 0;

      if(!ReadStateChunk(st, section->sf, (int)size))
         return 0;
   }
   else
   {
      if(!WriteStateChunk(st, section->name, section->sf))
         return(0);
   }

   return(1);
}

int PX68KSS_StateAction(void *st_p, int load, int data_only, SFORMAT *sf, const char *name, bool optional)
{
   struct SSDescriptor love;
   StateMem *st   = (StateMem*)st_p;

   love.sf        = sf;
   love.name      = name;

   return PX68KSS_StateAction_internal(st, load, 0, &love, optional);
}

int PX68KSS_SaveSM(void *st_p, int a, int b, const void*c, const void*d, const void*e)
{
   uint32_t sizy;
   uint8_t header[32];
   StateMem *st = (StateMem*)st_p;
   static const char *header_magic = "PX68KSVST";

   memset(header, 0, sizeof(header));
   memcpy(header, header_magic, 9);

   PX68K_en32lsb(header + 16, PX68K_VERSION_NUMERIC);
   /* header[24] bit 0: state uses the fast (label-less) layout. Older states
    * have 0 here, which is exactly "labelled", so they still load. */
   header[24] = st->fastsavestates ? 1 : 0;
   if (!smem_write(st, header, 32))
      return 0;

   if(!StateAction(st, 0, 0))
      return(0);

   sizy = st->loc;
   smem_seek(st, 16 + 4, SSEEK_SET);
   smem_write32le(st, sizy);
   smem_seek(st, sizy, SSEEK_SET);

   return(1);
}

int PX68KSS_LoadSM(void *st_p, int a, int b)
{
   uint8_t header[32];
   uint32_t stateversion;
   StateMem *st = (StateMem*)st_p;

   if (smem_read(st, header, 32) != 32)
      return(0);

   if(!header_magic_ok(header))
      return(0);

   stateversion = PX68K_de32lsb(header + 16);

   /* Always trust the layout the state says it has, not what the caller
    * guessed: a labelled state read as label-less (or the reverse) would
    * silently scramble every variable. */
   st->fastsavestates = (header[24] & 1) != 0;
   st->sec_base       = st->loc;
   st->sec_next       = st->loc;

   /* 'load' is any non-zero value; guard against a zero version field. */
   return(StateAction(st, stateversion ? (int)stateversion : 1, 0));
}
