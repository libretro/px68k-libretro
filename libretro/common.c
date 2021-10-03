// ---------------------------------------------------------------------------------------
//  COMMON - 標準ヘッダ群（COMMON.H）とエラーダイアログ表示とか
// ---------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>

#include "libretro.h"
extern retro_log_printf_t log_cb;

#define P6L_LEN 256
char p6l_buf[P6L_LEN];

void Error(const char* s)
{
   if (log_cb)
      log_cb(RETRO_LOG_ERROR, "Error: %s\n", s);
}

// log for debug
void p6logd(const char *fmt, ...)
{
   va_list args;

   va_start(args, fmt);
   vsnprintf(p6l_buf, P6L_LEN, fmt, args);
   va_end(args);

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s", p6l_buf);
}
