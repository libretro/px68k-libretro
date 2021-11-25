// ---------------------------------------------------------------------------------------
//  COMMON - 標準ヘッダ群（COMMON.H）とエラーダイアログ表示とか
// ---------------------------------------------------------------------------------------
#include <stdarg.h>
#include <libretro.h>
extern retro_log_printf_t log_cb;

void Error(const char* s)
{
   if (log_cb)
      log_cb(RETRO_LOG_ERROR, "Error: %s\n", s);
}
