#ifndef _LIBRETRO_WINX68K_COMMON_H
#define _LIBRETRO_WINX68K_COMMON_H

#include "compiler.h"

#include <string.h>

#include <libretro.h>
extern retro_log_printf_t log_cb;

#include "state.h"
#include "state_inline.h"

#undef FASTCALL
#define FASTCALL

#ifdef PSP
#ifdef MAX_PATH
#undef MAX_PATH
#endif
#define MAX_PATH 256
#endif

#endif
