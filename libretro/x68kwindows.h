#ifndef	__NP2_WIN32EMUL_H__
#define	__NP2_WIN32EMUL_H__

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#ifdef HAVE_C68k
#include "/m68000/c68k/core.h"
#endif

#ifndef FASTCALL
#define FASTCALL
#endif

#ifndef	MAX_PATH
#define	MAX_PATH	MAXPATHLEN
#endif

/*
 * DUMMY DEFINITION
 */

#ifndef	INLINE
#ifdef _WIN32
#define	INLINE	_inline
#else
#define	INLINE	inline
#endif
#endif

/* for dosio.c */
#define	GENERIC_READ			1
#define	GENERIC_WRITE			2

#define	OPEN_EXISTING			1
#define	CREATE_ALWAYS			2

/*
 * replace
 */
#define	timeGetTime()		FAKE_GetTickCount()

/*
 * prototype
 */
#ifdef __cplusplus
extern "C" {
#endif
int WritePrivateProfileString(const char *, const char *, const char *, const char*);
#ifdef __cplusplus
};
#endif

#include "peace.h"

#endif /* __NP2_WIN32EMUL_H__ */
