/* 
 * Copyright (c) 2003 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by NONAKA Kimihiro.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	__NP2_DOSIO_H__
#define	__NP2_DOSIO_H__

#include "common.h"
#include "appftype.h"

#define		FILEH		HANDLE

#define		FSEEK_SET	0
#define		FSEEK_CUR	1
#define		FSEEK_END	2

#ifdef __cplusplus
extern "C" {
#endif
							// DOSIO:関数の準備
void dosio_init(void);
void dosio_term(void);
							// ファイル操作
FILEH file_open(LPSTR filename);
FILEH file_create(LPSTR filename, int ftype);
DWORD file_seek(FILEH handle, long pointer, short mode);
DWORD file_lread(FILEH handle, void *data, DWORD length);
DWORD file_lwrite(FILEH handle, void *data, DWORD length);
WORD file_read(FILEH handle, void *data, WORD length);
short file_close(FILEH handle);
short file_attr(LPSTR filename);
							// カレントファイル操作
void file_setcd(LPSTR exename);
FILEH file_open_c(LPSTR filename);
FILEH file_create_c(LPSTR filename, int ftype);
short file_attr_c(LPSTR filename);

LPSTR getFileName(LPSTR filename);
void plusyen(LPSTR str, int len);
void cutyen(LPSTR str);

int kanji1st(LPSTR str, int pos);
int ex_a2i(LPSTR str, int min, int max);

#ifdef __cplusplus
};
#endif

#endif	/* __NP2_DOSIO_H__ */
