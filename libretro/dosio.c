/*	$Id: dosio.c,v 1.2 2003/12/05 18:07:15 nonaka Exp $	*/

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

#include <stdint.h>
#if !defined(USE_LIBRETRO_VFS) && !defined(_WIN32)
#include <sys/param.h>
#endif

#include "dosio.h"
#include "zipdisk.h"

#ifdef _WIN32
#define SLASH '\\'
#else
#define SLASH '/'
#endif

static char  curpath[MAX_PATH] = "";
static char *curfilep = curpath;

void plusyen(char *s, size_t len)
{
	size_t pos = strlen(s);

	if (pos)
	{
		if (s[pos-1] == SLASH)
			return;
	}
	if ((pos + 2) >= len)
		return;
	s[pos++] = SLASH;
	s[pos]   = '\0';
}

#ifdef USE_LIBRETRO_VFS

void *file_open(const char *filename)
{
	void *ret;

	if (string_is_empty(filename)) return NULL;
	if (px68k_is_zip_virtual_path(filename)) return px68k_zip_open(filename);
	if (path_is_directory(filename)) return NULL;

	ret = filestream_open(filename,
		RETRO_VFS_FILE_ACCESS_READ_WRITE |
		RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
		RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (!ret)
	{
		ret = filestream_open(filename,
			RETRO_VFS_FILE_ACCESS_READ,
			RETRO_VFS_FILE_ACCESS_HINT_NONE);
		if (!ret)
			return NULL;
	}

	return ret;
}

void *file_create(const char *filename)
{
	if (string_is_empty(filename)) return NULL;
	if (path_is_directory(filename)) return NULL;

	return filestream_open(filename,
		RETRO_VFS_FILE_ACCESS_READ_WRITE,
		RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

size_t file_lread(void* handle, void *data, size_t length)
{
	int64_t readsize;
	if (px68k_zip_owns(handle))
		return px68k_zip_read(handle, data, length);
	readsize = filestream_read((RFILE *)handle, data, (int64_t)length);
	if (readsize <= 0)
		return 0;
	return (size_t)readsize;
}

size_t file_lwrite(void *handle, void *data, size_t length)
{
	int64_t writesize;
	if (px68k_zip_owns(handle))
		return px68k_zip_write(handle, data, length);
	writesize = filestream_write((RFILE *)handle, data, (int64_t)length);
	if (writesize <= 0)
		return 0;
	return (size_t)writesize;
}

int64_t file_read(void *handle, void *buffer, int64_t length)
{
	return filestream_read((RFILE *)handle, buffer, length);
}

int64_t file_write(void *handle, void *buffer, int64_t length)
{
	return filestream_write((RFILE *)handle, buffer, length);
}

char *file_gets(void *handle, char *buffer, size_t length)
{
	return filestream_gets((RFILE *)handle, buffer, length);
}

int64_t file_tell(void *handle)
{
	if (px68k_zip_owns(handle))
		return px68k_zip_tell(handle);
	return filestream_tell((RFILE *)handle);
}

void file_rewind(void *handle)
{
	filestream_rewind((RFILE *)handle);
}

int file_eof(void *handle)
{
	return filestream_eof((RFILE *)handle);
}

#else /* !USE_LIBRETRO_VFS */

void *file_open(const char *filename)
{
	void *ret;
	if (px68k_is_zip_virtual_path(filename))
		return px68k_zip_open(filename);
	ret = create_file(filename, GENERIC_READ | GENERIC_WRITE,
	    OPEN_EXISTING);
	if (!ret)
	{
		if (!(ret = create_file(filename, GENERIC_READ,
		    OPEN_EXISTING)))
			return NULL;
	}
	return ret;
}

void *file_create(const char *filename)
{
	return create_file(curpath, GENERIC_READ | GENERIC_WRITE,
	    CREATE_ALWAYS);
}

size_t file_lread(void* handle, void *data, size_t length)
{
	int64_t readsize;
	if (px68k_zip_owns(handle))
		return px68k_zip_read(handle, data, length);
	if (read_file(handle, data, length, &readsize) == 0)
		return 0;
	return (size_t)readsize;
}

size_t file_lwrite(void *handle, void *data, size_t length)
{
	int64_t writesize;
	if (px68k_zip_owns(handle))
		return px68k_zip_write(handle, data, length);
	if (write_file(handle, data, length, &writesize) == 0)
		return 0;
	return (size_t)writesize;
}

#endif /* !USE_LIBRETRO_VFS */

void file_setcd(const char *exename)
{
	strncpy(curpath, exename, sizeof(curpath) - 1);
	plusyen(curpath, sizeof(curpath));
	curfilep  = curpath + strlen(exename) + 1;
	*curfilep = '\0';
}

void *file_open_c(const char *filename)
{
	strncpy(curfilep, filename, MAX_PATH - (curfilep - curpath));
	return file_open(curpath);
}

void *file_create_c(const char *filename)
{
	strncpy(curfilep, filename, MAX_PATH - (curfilep - curpath));
	return file_create(curpath);
}
