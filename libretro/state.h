#ifndef _STATE_H
#define _STATE_H

#include <stdint.h>

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
} StateMem;

#ifdef __cplusplus
extern "C" {
#endif

int PX68KSS_SaveSM(void *st, int, int, const void*, const void*, const void*);
int PX68KSS_LoadSM(void *st, int, int);

/* Flag for a single, >= 1 byte native-endian variable */
#define PX68KSTATE_RLSB            0x80000000
/* 32-bit native-endian elements */
#define PX68KSTATE_RLSB32          0x40000000
/* 16-bit native-endian elements */
#define PX68KSTATE_RLSB16          0x20000000
/* 64-bit native-endian elements */
#define PX68KSTATE_RLSB64          0x10000000

#define PX68KSTATE_BOOL		  0x08000000

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
