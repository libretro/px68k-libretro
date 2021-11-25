/*  Copyright 2005 Guillaume Duhamel
    Copyright 2005-2006 Theo Berkau

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <string.h>

#ifndef ALIGNED
#ifdef _MSC_VER
#define ALIGNED(x) __declspec(align(x))
#else
#define ALIGNED(x) __attribute__((aligned(x)))
#endif
#endif

#ifndef STDCALL
#ifdef _MSC_VER
#define STDCALL __stdcall
#else
#define STDCALL
#endif
#endif

#ifndef FASTCALL
#ifdef __MINGW32__
#define FASTCALL __attribute__((fastcall))
#elif defined (__i386__)
#define FASTCALL __attribute__((regparm(3)))
#else
#define FASTCALL
#endif
#endif

#ifndef INLINE
#ifdef _MSC_VER
#define INLINE _inline
#else
#define INLINE inline
#endif 
#endif

#ifdef GEKKO
/* Wii have both stdint.h and "yabause" definitions of fixed
size types */
#include <gccore.h>
typedef unsigned long pointer;

#else /* ! GEKKO */

#ifdef HAVE_STDINT_H

#include <stdint.h>
typedef uint8_t u8;
typedef  int8_t s8;
typedef uint16_t u16;
typedef  int16_t s16;
typedef uint32_t u32;
typedef  int32_t s32;
typedef uint64_t u64;
typedef  int64_t s64;
typedef uintptr_t pointer;

#else  /* !HAVE_STDINT_H */

typedef unsigned char u8;
typedef unsigned short u16;

typedef signed char s8;
typedef signed short s16;

#if defined(__LP64__)
// Generic 64-bit
typedef unsigned int u32;
typedef unsigned long u64;
typedef unsigned long pointer;

typedef signed int s32;
typedef signed long s64;

#elif defined(_MSC_VER)
typedef unsigned long u32;
typedef unsigned __int64 u64;
typedef unsigned long long u64;
#ifdef _WIN64
typedef __int64 pointer;
#else
typedef unsigned long pointer;
#endif

typedef signed long s32;
typedef __int64 s64;
typedef signed long long s64;

#else
/* 32-bit Linux GCC/MINGW/etc */
typedef unsigned long u32;
typedef unsigned long long u64;
typedef unsigned long pointer;

typedef signed long s32;
typedef signed long long s64;
#endif

#endif  /* !HAVE_STDINT_H */

#endif /* !GEKKO */

#ifdef HAVE_LIBMINI18N
#include "mini18n.h"
#else
#ifndef _
#define _(a) (a)
#endif
#endif

/* Minimum/maximum values */

#undef MIN
#undef MAX
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#define MAX(a,b)  ((a) > (b) ? (a) : (b))

/*
 * Any of these can be left undefined if there is no platform-specific
 * optimization for them; the defaults below will then be used instead.
 */

/* Defaults: */

#ifdef __GNUC__

#ifndef UNUSED
#define UNUSED __attribute ((unused))
#endif

#ifdef DEBUG
#define USED_IF_DEBUG
#else
#define USED_IF_DEBUG __attribute ((unused))
#endif

#ifdef SMPC_DEBUG
#define USED_IF_SMPC_DEBUG
#else
#define USED_IF_SMPC_DEBUG __attribute ((unused))
#endif

/* LIKELY(x) indicates that x is likely to be true (nonzero);
 * UNLIKELY(x) indicates that x is likely to be false (zero).
 * Use like: "if (UNLIKELY(a < b)) {...}" */
#define LIKELY(x) (__builtin_expect(!!(x), 1))
#define UNLIKELY(x) (__builtin_expect(!!(x), 0))

#else

#define UNUSED
#define USED_IF_DEBUG
#define USED_IF_SMPC_DEBUG
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)

#endif

#ifdef USE_16BPP
typedef u16 pixel_t;
#else
typedef u32 pixel_t;
#endif

#ifdef _MSC_VER
#define snprintf sprintf_s
#endif

#endif
