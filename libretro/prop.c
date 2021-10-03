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

/* -------------------------------------------------------------------------- *
 *  PROP.C - 各種設定用プロパティシートと設定値管理                           *
 * -------------------------------------------------------------------------- */

#include <sys/stat.h>

#include "common.h"
#include "winx68k.h"
#include "keyboard.h"
#include "fileio.h"
#include "prop.h"

BYTE	LastCode = 0;
BYTE	KEYCONFFILE[] = "xkeyconf.dat";

int	CurrentHDDNo = 0;

BYTE ini_title[] = "WinX68k";

BYTE initialized = 0;

static const char MIDI_TYPE_NAME[4][3] = {
	"LA", "GM", "GS", "XG"
};

BYTE KeyTableBk[512];

Win68Conf Config;
Win68Conf ConfBk;

#ifndef MAX_BUTTON
#define MAX_BUTTON 32
#endif

extern char filepath[MAX_PATH];
extern char winx68k_ini[MAX_PATH];
extern int winx, winy;
extern char joyname[2][MAX_PATH];
extern char joybtnname[2][MAX_BUTTON][MAX_PATH];
extern BYTE joybtnnum[2];

#define CFGLEN MAX_PATH

#if 0
static long solveHEX(char *str) {

	long	ret;
	int		i;
	char	c;

	ret = 0;
	for (i=0; i<8; i++) {
		c = *str++;
		if ((c >= '0') && (c <= '9')) {
			c -= '0';
		}
		else if ((c >= 'A') && (c <= 'F')) {
			c -= '7';
		}
		else if ((c >= 'a') && (c <= 'f')) {
			c -= 'W';
		}
		else {
			break;
		}
		ret <<= 4;
		ret += (long) c;
	}
	return(ret);
}
#endif

static char *makeBOOL(BYTE value) {

	if (value) {
		return("true");
	}
	return("false");
}

static BYTE Aacmp(char *cmp, char *str) {

	char	p;

	while(*str) {
		p = *cmp++;
		if (!p) {
			break;
		}
		if (p >= 'a' && p <= 'z') {
			p -= 0x20;
		}
		if (p != *str++) {
			return(-1);
		}
	}
	return(0);
}

static BYTE solveBOOL(char *str) {

	if ((!Aacmp(str, "TRUE")) || (!Aacmp(str, "ON")) ||
		(!Aacmp(str, "+")) || (!Aacmp(str, "1")) ||
		(!Aacmp(str, "ENABLE"))) {
		return(1);
	}
	return(0);
}

#ifdef __LIBRETRO__
extern char retro_system_conf[512];
extern char slash;

#endif

int
set_modulepath(char *path, size_t len)
{
	struct stat sb;
	char *homepath;
        strcpy(path,retro_system_conf);
        sprintf(winx68k_ini, "%s%cconfig",retro_system_conf,slash);
        return 0;
}

static void LoadDefaults(void)
{
	int i;
	int j;

	Config.MenuFontSize = 0; // start with default normal menu size
	winx = 0;
	winy = 0;
	Config.FrameRate = 1;
	filepath[0] = 0;
	Config.OPM_VOL = 12;
	Config.PCM_VOL = 15;
	Config.MCR_VOL = 13;
	Config.SampleRate = 44100;
	Config.BufferSize = 50;
	Config.MouseSpeed = 10;
	Config.WindowFDDStat = 1;
	Config.FullScrFDDStat = 1;
	Config.DSAlert = 1;
	Config.Sound_LPF = 1;
	Config.SoundROMEO = 1;
	Config.MIDI_SW = 1;
	Config.MIDI_Reset = 1;
	Config.MIDI_Type = 1;
	Config.JoySwap = 0;
	Config.JoyKey = 0;
	Config.JoyKeyReverse = 0;
	Config.JoyKeyJoy2 = 0;
	Config.SRAMWarning = 1;
	Config.LongFileName = 1;
	Config.WinDrvFD = 1;
	Config.WinStrech = 1;
	Config.DSMixing = 0;
	Config.XVIMode = 0;
	Config.CDROM_ASPI = 1;
	Config.CDROM_SCSIID = 6;
	Config.CDROM_ASPI_Drive = 0;
	Config.CDROM_IOCTRL_Drive = 16;
	Config.CDROM_Enable = 1;
	Config.SSTP_Enable = 0;
	Config.SSTP_Port = 11000;
	Config.ToneMap = 0;
	Config.ToneMapFile[0] = 0;
	Config.MIDIDelay = Config.BufferSize*5;
	Config.MIDIAutoDelay = 1;
	Config.VkeyScale = 4;
	Config.VbtnSwap = 0;
	Config.JoyOrMouse = 1;
	Config.HwJoyAxis[0] = 0;
	Config.HwJoyAxis[1] = 1;
	Config.HwJoyHat = 0;

	for (i = 0; i < 8; i++)
		Config.HwJoyBtn[i] = i;

	Config.NoWaitMode = 0;
	Config.PushVideoBeforeAudio = 0;
	Config.AdjustFrameRates = 1;
	Config.AudioDesyncHack = 0;

	for (i = 0; i < 2; i++)
		for (j = 0; j < 8; j++)
			Config.JOY_BTN[i][j] = j;

	for (i = 0; i < 2; i++)
		Config.FDDImage[i][0] = '\0';

	for (i = 0; i < 16; i++)
		Config.HDImage[i][0] = '\0';

	initialized = 1;
}

void LoadConfig(void)
{
	LoadDefaults();
}
