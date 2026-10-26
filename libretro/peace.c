/*
 * Copyright 2000 Masaru OKI
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
 * 3. The name of the author may not be used to endorse or promote products
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

#include <sys/stat.h>
#ifndef _WIN32
#include <sys/time.h>
#include <strings.h>
#endif
#include <sys/types.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "common.h"
#include "dosio.h"
#include "zipdisk.h"

#ifdef _WIN32
__declspec(dllimport) unsigned long __stdcall GetTickCount(void);
#endif

/*-----
 *
 * From PEACE - http://chiharu.hauN.org/peace/ja/
 *
 */
enum {
	HTYPE_MEMORY = 0x100,
	HTYPE_HEAP,
	HTYPE_PROCESS,
	HTYPE_THREAD,
	HTYPE_MODULE,
	HTYPE_FILE,
};

struct internal_handle
{
	void	*p;
	uint32_t flags;
	size_t	psize;
	uint32_t refcount;
	int	type;
};

struct internal_file
{
	int	fd;
};

#define ptrtohandle(h)							\
    ((struct internal_handle *)((char *)(h) - sizeof(struct internal_handle)))

#define isfixed(h)							\
    ((ptrtohandle(h)->p == (h)) ? 1 : 0)

#define sethandletype(h,t)						\
    do {								\
        if (isfixed(h))							\
	    ptrtohandle(h)->type = (t);					\
	else								\
	    (((struct internal_handle *)(h))->type = (t));		\
    } while (/* CONSTCOND */0)

#define handletype(h)							\
    ((isfixed(h)) ?							\
        ptrtohandle(h)->type : (((struct internal_handle *)(h))->type))

uint32_t FAKE_GetTickCount(void)
{
#ifdef _WIN32
	return GetTickCount();
#else
	struct timeval tv;

	gettimeofday(&tv, 0);
	return tv.tv_usec / 1000 + tv.tv_sec * 1000;
#endif
}

static void *local_lock(void *h)
{
	struct internal_handle *ih = h;

	if (isfixed(h))
		return h;
	ih->refcount++;

	return ih->p;
}

static int local_unlock(void *h)
{
	struct internal_handle *ih = h;

	if (isfixed(h) || ih->refcount == 0)
		return 0;

	if (--ih->refcount != 0) /* still locked? */
		return 1;

	/* unlocked */
	return 0;
}

static void *local_alloc(size_t bytes) 
{
	struct internal_handle *p = (struct internal_handle*)
		malloc(bytes + sizeof(struct internal_handle));
	if (p)
	{
		p->p        = &p[1];
		p->psize    = bytes;
		p->refcount = 0;
		p->type     = HTYPE_MEMORY;
		return p->p;
	}
	return 0;
}

static void *local_free(void *h)
{
	struct internal_handle *ih = h;

	if (h == 0)
		return NULL;
	if (!isfixed(h))
		return NULL;
	ih = (struct internal_handle *)((char *)h - sizeof(struct internal_handle));
	if (ih->p == &ih[1])
	{
		free(ih);
		return NULL;
	}
	return h;
}

int read_file(void* h, void *buf, size_t len, size_t *lp)
{
	struct internal_file *fp;
	if (!h)
		return 0;

	fp  = local_lock(h);
	*lp = read(fp->fd, buf, len);
	local_unlock(h);
	if (*lp <= 0)
		return 0;
	return 1;
}

int write_file(void* h, const void *buf, size_t len, size_t *lp)
{
	struct internal_file *fp;
	if (!h)
		return 0;

	fp  = local_lock(h);
	*lp = write(fp->fd, buf, len);
	local_unlock(h);
	if (*lp <= 0)
		return 0;
	return 1;
}

void* create_file(const char *filename, uint32_t rdwr,
	    uint32_t crmode)
{
	struct internal_file *fp;
	void* h;
	int fd, fmode = 0;
#ifdef _WIN32
 	fmode |=O_BINARY;
#endif
	switch (rdwr & (GENERIC_READ|GENERIC_WRITE))
	{
		case GENERIC_READ:
			fmode |= O_RDONLY;
			break;
		case GENERIC_WRITE:
			fmode |= O_WRONLY;
			break;
		case GENERIC_READ|GENERIC_WRITE:
			fmode |= O_RDWR;
		default:
			break;
	}
	switch (crmode)
	{
		case CREATE_ALWAYS:
			fmode |= O_CREAT;
			break;	
		case OPEN_EXISTING:
			break;
	}
	if ((fd = open(filename, fmode, 0644)) < 0)
		return NULL;

	h = local_alloc(sizeof(struct internal_file));
	sethandletype(h, HTYPE_FILE);
	fp     = local_lock(h);
	fp->fd = fd;
	local_unlock(h);
	return h;
}

#ifdef USE_LIBRETRO_VFS

size_t file_seek(void* h, long pos, int16_t whence)
{
	int seek_position = -1;
	int64_t result = 0;

	if (px68k_zip_owns(h))
		return px68k_zip_seek(h, pos, whence);

	switch (whence)
	{
	case FSEEK_SET:
		seek_position = RETRO_VFS_SEEK_POSITION_START;
		break;
	case FSEEK_CUR:
		seek_position = RETRO_VFS_SEEK_POSITION_CURRENT;
		break;
	case FSEEK_END:
		seek_position = RETRO_VFS_SEEK_POSITION_END;
		break;
	}

	result = filestream_seek((RFILE *)h, (int64_t)pos, seek_position);
	if (result != -1)
	{
		result = filestream_tell((RFILE *)h);
	}
	return (size_t)result;
}

void file_close(void * h)
{
	if (px68k_zip_owns(h))
	{
		px68k_zip_close(h);
		return;
	}
	filestream_close((RFILE *)h);
}

#else /* !USE_LIBRETRO_VFS */

size_t file_seek(void* h, long pos, int16_t whence)
{
	struct internal_file *fp;
	int fd;
	if (px68k_zip_owns(h))
		return px68k_zip_seek(h, pos, whence);
	fp = local_lock(h);
	fd = fp->fd;
	local_unlock(h);
	return lseek(fd, pos, whence);
}

void file_close(void * h)
{
        if (px68k_zip_owns(h))
        {
                px68k_zip_close(h);
                return;
        }
        if (handletype(h) == HTYPE_FILE)
        {
		struct internal_file *fp = local_lock(h);
		close(fp->fd);
		local_unlock(h);
		local_free(h);
        }
}
#endif /* !USE_LIBRETRO_VFS */

#ifdef USE_LIBRETRO_VFS
/**
 * 1 : file, 2 : dir, 0 error;
*/

int wrap_vfs_stat(const char *path, int32_t *size)
{
	if (vfs_iface_info.iface)
		return vfs_iface_info.iface->stat(path, size);
	return retro_vfs_stat_impl(path, size);
}

DIRH *wrap_vfs_opendir(const char *filename)
{
	struct RDIR *dir;
	DIRH *output;

	dir = retro_opendir(filename);

	if (dir == NULL)
		return NULL;

	output         = (DIRH *)malloc(sizeof(DIRH));
	output->d_name = NULL;
	output->dir    = dir;

	return output;
}

int wrap_vfs_readdir(DIRH *rdir)
{
	if (retro_readdir(rdir->dir))
	{
		rdir->d_name = retro_dirent_get_name(rdir->dir);
		return rdir->d_name != NULL;
	}
	return 0;
}

void wrap_vfs_closedir(DIRH *rdir)
{
	if (rdir)
	{
		retro_closedir(rdir->dir);
		rdir->d_name = 0;
		free(rdir);
		rdir = NULL;
	}
}

size_t GetPrivateProfileString(
        const char *sect, const char *key,
        const char *defvalue, char *buf, size_t len,
        const char *inifile)
{
	char lbuf[256];
	FILEH *fp = NULL;

	if (sect == NULL
	 || key == NULL
	 || defvalue == NULL
	 || buf == NULL
	 || len == 0
	 || inifile == NULL)
		return 0;

	memset(buf, 0, len);

	if (!(fp = filestream_open(inifile,
		RETRO_VFS_FILE_ACCESS_READ,
		RETRO_VFS_FILE_ACCESS_HINT_NONE)))
	{
		goto nofile;
	}
	while (!file_eof(fp))
	{
		file_gets(fp, lbuf, sizeof(lbuf));
		/* XXX should be case insensitive */
		if (lbuf[0] == '['
			&& !strncasecmp(sect, &lbuf[1], strlen(sect))
			&& lbuf[strlen(sect) + 1] == ']')
			break;
	}
	if (file_eof(fp))
		goto notfound;
	while (!file_eof(fp))
	{
		file_gets(fp, lbuf, sizeof(lbuf));
		if (lbuf[0] == '[' && strchr(lbuf, ']'))
			goto notfound;
		/* XXX should be case insensitive */
		if (!strncasecmp(key, lbuf, strlen(key))
			&& lbuf[strlen(key)] == '=')
		{
			char *dst, *src;
			src = &lbuf[strlen(key) + 1];
			dst = buf;
			while (*src != '\r' && *src != '\n' && *src != '\0')
				*dst++ = *src++;
			*dst = '\0';
			file_close(fp);
			return strlen(buf);
		}
	}
notfound:
#ifdef DEBUG
	p6logd(("[%s]:%s not found\n", sect, key));
#endif
	file_close(fp);
nofile:
	strncpy(buf, defvalue, len);
	/* not include nul */
	return strlen(buf);
}

uint32_t GetPrivateProfileInt(
	const char *sect, const char *key, int defvalue,
    const char *inifile)
{
	char lbuf[256];
	FILEH *fp = NULL;

	if (sect == NULL
		|| key == NULL
		|| inifile == NULL)
		return 0;

	fp = filestream_open(inifile,
		RETRO_VFS_FILE_ACCESS_READ,
		RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (fp == NULL)
		goto nofile;
	while (!file_eof(fp))
	{
		file_gets(fp, lbuf, sizeof(lbuf));
		/* XXX should be case insensitive */
		if (lbuf[0] == '['
			&& !strncasecmp(sect, &lbuf[1], strlen(sect))
			&& lbuf[strlen(sect) + 1] == ']')
			break;
	}
	if (file_eof(fp))
		goto notfound;
	while (!file_eof(fp))
	{
		file_gets(fp, lbuf, sizeof(lbuf));
		if (lbuf[0] == '[' && strchr(lbuf, ']'))
			goto notfound;
		/* XXX should be case insensitive */
		if (!strncasecmp(key, lbuf, strlen(key))
			&& lbuf[strlen(key)] == '=')
		{
			int value;
			sscanf(&lbuf[strlen(key) + 1], "%d", &value);
			file_close(fp);
			return value;
		}
	}
notfound:
	file_close(fp);
nofile:
	return defvalue;
}

#else /* !USE_LIBRETRO_VFS */

size_t GetPrivateProfileString(const char *sect, const char *key,
		const char *defvalue,
		char *buf, size_t len, const char *inifile)
{
	char lbuf[256];
	FILE *fp;

	if (sect     == NULL
	 || key      == NULL
	 || defvalue == NULL
	 || buf      == NULL
	 || len      == 0
	 || inifile  == NULL)
		return 0;

	memset(buf, 0, len);

	if (!(fp = fopen(inifile, "r")))
		goto nofile;
	while (!feof(fp))
	{
		fgets(lbuf, sizeof(lbuf), fp);
		/* XXX should be case insensitive */
		if (lbuf[0] == '['
		    && !strncasecmp(sect, &lbuf[1], strlen(sect))
		    && lbuf[strlen(sect) + 1] == ']')
			break;
	}
	if (feof(fp))
		goto notfound;
	while (!feof(fp))
	{
		fgets(lbuf, sizeof(lbuf), fp);
		if (lbuf[0] == '[' && strchr(lbuf, ']'))
			goto notfound;
		/* XXX should be case insensitive */
		if (!strncasecmp(key, lbuf, strlen(key))
		    && lbuf[strlen(key)] == '=') {
			char *dst, *src;
			src = &lbuf[strlen(key) + 1];
			dst = buf;
			while (*src != '\r' && *src != '\n' && *src != '\0')
				*dst++ = *src++;
			*dst = '\0';
			fclose(fp);
			return strlen(buf);
		}
	}
notfound:
	fclose(fp);
nofile:
	strncpy(buf, defvalue, len);
	/* not include nul */
	return strlen(buf);
}

unsigned int GetPrivateProfileInt(const char *sect, const char *key, signed int defvalue, const char *inifile)
{
	char lbuf[256];
	FILE *fp;

	if (sect    == NULL
	 || key     == NULL
	 || inifile == NULL)
		return 0;

	if (!(fp = fopen(inifile, "r")))
		goto nofile;

	while (!feof(fp))
	{
		fgets(lbuf, sizeof(lbuf), fp);
		/* XXX should be case insensitive */
		if (lbuf[0] == '['
		    && !strncasecmp(sect, &lbuf[1], strlen(sect))
		    && lbuf[strlen(sect) + 1] == ']')
			break;
	}
	if (feof(fp))
		goto notfound;
	while (!feof(fp))
	{
		fgets(lbuf, sizeof(lbuf), fp);
		if (lbuf[0] == '[' && strchr(lbuf, ']'))
			goto notfound;
		/* XXX should be case insensitive */
		if (!strncasecmp(key, lbuf, strlen(key))
		    && lbuf[strlen(key)] == '=')
		{
			int value;
			sscanf(&lbuf[strlen(key) + 1], "%d", &value);
			fclose(fp);
			return value;
		}
	}
notfound:
	fclose(fp);
nofile:
	return defvalue;
}

#endif /* !USE_LIBRETRO_VFS */
