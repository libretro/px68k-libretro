#ifndef _STATE_H
#define _STATE_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_inline.h>

#define PX68K_VERSION_MAJOR 0
#define PX68K_VERSION_MINOR 15
#define PX68K_VERSION_PATCH 0

#define PX68K_VERSION_NUMERIC ((PX68K_VERSION_MAJOR * 10000) + (PX68K_VERSION_MINOR * 100) + (PX68K_VERSION_PATCH))

typedef struct
{
   uint8_t *data;
   uint32_t loc;
   uint32_t len;
   uint32_t malloced;
   uint32_t initial_malloc; /* A setting! */

   /* Fast Save States exclude string labels from variables in the savestate, and are at least 20% faster.
    * Only used for internal savestates which will not be written to a file.
    * When loading, this is taken from the state header (not from the caller),
    * so a state is always read back in the format it was written in. */
   bool fastsavestates;

   /* Caller-owned buffer: never realloc'd or freed; writes past 'malloced'
    * fail instead of growing. */
   bool fixed;

   /* Dry run: only advance loc/len, touch no memory. Used to size states. */
   bool count_only;

   /* Load only: offset of the first section, and of the byte just past the
    * last section found. Sections are normally read back in the order they
    * were written, so the next lookup starts at sec_next. */
   uint32_t sec_base;
   uint32_t sec_next;
} StateMem;

#ifdef __cplusplus
extern "C" {
#endif

int PX68KSS_SaveSM(void *st, int, int, const void*, const void*, const void*);
int PX68KSS_LoadSM(void *st, int, int);

/* Helpers that set up a StateMem for each use. */
void PX68KSS_InitCount(StateMem *st);                       /* size query     */
void PX68KSS_InitWrite(StateMem *st, void *buf, uint32_t cap, bool fast);
void PX68KSS_InitRead(StateMem *st, const void *buf, uint32_t len);

/* True if the state in buf was written in the fast (label-less) format. */
bool PX68KSS_IsFastState(const void *buf, size_t len);

/* Flag for a single, >= 1 byte native-endian variable */
#define PX68KSTATE_RLSB            0x80000000
/* 32-bit native-endian elements */
#define PX68KSTATE_RLSB32          0x40000000
/* 16-bit native-endian elements */
#define PX68KSTATE_RLSB16          0x20000000
/* 64-bit native-endian elements */
#define PX68KSTATE_RLSB64          0x10000000

#define PX68KSTATE_BOOL		  0x08000000

/* Byte array whose stored length may be shorter than 'size'. On load, a
 * recorded length <= size is accepted and the remainder is zero-filled.
 * On save, 'size' is simply the number of bytes to write. */
#define PX68KSTATE_VARLEN          0x04000000

typedef struct
{
   void *v;		      /* Pointer to the variable/array */
   uint32_t size;		/* Length, in bytes, of the data to be saved EXCEPT:
                      *  In the case of PX68KSTATE_BOOL, it is the number 
                      *  of bool elements to save(bool is not always 1-byte).
                      *
                      *  If 0, the subchunk isn't saved.
                      */
   uint32_t flags;	/* Flags */
   const char *name;	/* Name */
} SFORMAT;

/* State-Section Descriptor */
struct SSDescriptor
{

   SFORMAT *sf;
   const char *name;
};

int PX68KSS_StateAction(void *st, int load, int data_only, SFORMAT *sf, const char *name, bool optional);

#ifdef __cplusplus
}
#endif

#endif
