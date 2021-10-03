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

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "windows.h"
#include "mmsystem.h"

int WINAPI
MessageBox(HWND hWnd, LPCSTR str, LPCSTR title, UINT flags)
{

	(void)hWnd;
	(void)flags;

	printf("----- %s\n", title);
	printf("%s\n", str);
	printf("-----\n\n");

	return 0;
}

void WINAPI
PostQuitMessage(int m)
{

	exit(m);
}

DWORD WINAPI
FAKE_GetLastError(VOID)
{

	return NO_ERROR;
}

BOOL WINAPI
SetEndOfFile(HANDLE hFile)
{

	(void)hFile;

	return FALSE;
}

WINMMAPI MMRESULT WINAPI
midiOutPrepareHeader(HMIDIOUT hmo, LPMIDIHDR pmh, UINT cbmh)
{

	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return !MIDIERR_STILLPLAYING;	// (¤©
}

WINMMAPI MMRESULT WINAPI
midiOutUnprepareHeader(HMIDIOUT hmo, LPMIDIHDR pmh, UINT cbmh)
{

	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return MMSYSERR_NOERROR;
}

WINMMAPI MMRESULT WINAPI
midiOutShortMsg(HMIDIOUT hmo, DWORD dwMsg)
{

	(void)hmo;
	(void)dwMsg;
	return MMSYSERR_NOERROR;
}

WINMMAPI MMRESULT WINAPI
midiOutLongMsg(HMIDIOUT hmo, LPMIDIHDR pmh, UINT cbmh)
{

	(void)hmo;
	(void)pmh;
	(void)cbmh;
	return MMSYSERR_NOERROR;
}

WINMMAPI MMRESULT WINAPI
midiOutOpen(LPHMIDIOUT phmo, UINT uDeviceID, DWORD dwCallback,
    DWORD dwInstance, DWORD fdwOpen)
{

	(void)phmo;
	(void)uDeviceID;
	(void)dwCallback;
	(void)dwInstance;
	(void)fdwOpen;
	return !MMSYSERR_NOERROR;	// (¤£
}

WINMMAPI MMRESULT WINAPI
midiOutClose(HMIDIOUT hmo)
{

	(void)hmo;
	return MMSYSERR_NOERROR;
}

WINMMAPI MMRESULT WINAPI
midiOutReset(HMIDIOUT hmo)
{

	(void)hmo;
	return MMSYSERR_NOERROR;
}
