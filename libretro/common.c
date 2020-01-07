// ---------------------------------------------------------------------------------------
//  COMMON - 標準ヘッダ群（COMMON.H）とエラーダイアログ表示とか
// ---------------------------------------------------------------------------------------
//#include	<windows.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef DEBUG
#ifdef ANDROID
#include <android/log.h>
#endif
#endif

#include "libretro.h"
extern retro_log_printf_t log_cb;

//#include	"sstp.h"

//extern HWND hWndMain;
extern const char PrgTitle[];

// P6L: PX68K_LOG
//      ~ ~   ~
#define P6L_LEN 256
char p6l_buf[P6L_LEN];

void Error(const char* s)
{
	printf("%s Error: %s\n", PrgTitle, s);

//	SSTP_SendMes(SSTPMES_ERROR);

//	MessageBox(hWndMain, s, title, MB_ICONERROR | MB_OK);
}

// log for debug
void p6logd(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsnprintf(p6l_buf, P6L_LEN, fmt, args);
	va_end(args);

#ifdef DEBUG
#if defined(ANDROID)
	__android_log_write(ANDROID_LOG_DEBUG, "Tag", p6l_buf);
#elif defined(PSP)
	printf("%s", p6l_buf);
#else
	printf("%s", p6l_buf);
#endif
#else
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s", p6l_buf);
#endif
}
