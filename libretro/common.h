#ifndef _LIBRETRO_WINX68K_COMMON_H
#define _LIBRETRO_WINX68K_COMMON_H

#ifdef _WIN32
#include "windows.h"
#endif

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include "../win32api/windows.h"
#endif

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#define	SUCCESS		0
#define	FAILURE		1

#undef FASTCALL
#define FASTCALL

#define STDCALL
#define	LABEL
#define	__stdcall

#ifdef PSP
#ifdef MAX_PATH
#undef MAX_PATH
#endif
#define MAX_PATH 256
#endif

typedef unsigned char	UINT8;
typedef unsigned short	UINT16;
typedef unsigned int	UINT32;
typedef signed char	INT8;
typedef signed short	INT16;
typedef signed int	INT32;

typedef union {
	struct {
		BYTE l;
		BYTE h;
	} b;
	WORD w;
} PAIR;

#ifdef __cplusplus
extern "C" {
#endif

void Error(const char* s);
void p6logd(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
