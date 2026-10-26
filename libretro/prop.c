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
#include "dosio.h"
#include "prop.h"
#include "zipdisk.h"

static uint8_t initialized = 0;

Win68Conf Config;

extern char filepath[MAX_PATH];
extern char winx68k_ini[2048];

static void LoadDefaults(void)
{
	int i;

	Config.MenuFontSize = 0; /* start with default normal menu size */
	Config.FrameRate = 1;
	filepath[0] = 0;
	Config.OPM_VOL = 12;
	Config.PCM_VOL = 15;
	Config.MCR_VOL = 13;
	Config.BufferSize = 50;
	Config.WindowFDDStat = 1;
	Config.Sound_LPF = 1;
	Config.SoundROMEO = 1;
	Config.MIDI_SW = 1;
	Config.MIDI_Reset = 1;
	Config.MIDI_Type = 1;
	Config.XVIMode = 0;
	Config.ToneMap = 0;
	Config.ToneMapFile[0] = 0;
	Config.MIDIDelay = Config.BufferSize*5;
	Config.MIDIAutoDelay = 1;
	Config.VbtnSwap = 0;
	Config.JoyOrMouse = 1;

	Config.NoWaitMode = 0;
	Config.AdjustFrameRates = 1;
	Config.AudioDesyncHack = 0;

	for (i = 0; i < 2; i++)
		Config.FDDImage[i][0] = '\0';

	for (i = 0; i < 16; i++)
		Config.HDImage[i][0] = '\0';

	initialized = 1;
}

void LoadConfig(void)
{
	int	i;
	char	buf[MAX_PATH];

	/* Because we are not loading defauts for most items from a config file,
	 * directly set default config at first call
	 */
	if (!initialized)
		LoadDefaults();

	GetPrivateProfileString("WinX68k", "StartDir", "", buf, MAX_PATH, winx68k_ini);
	if (buf[0] != 0)
		strncpy(filepath, buf, sizeof(filepath));
	else
		filepath[0] = 0;

	if (Config.save_fdd_path)
		for (i = 0; i < 2; i++)
		{
			sprintf(buf, "FDD%d", i);
			GetPrivateProfileString("WinX68k", buf, "", Config.FDDImage[i], MAX_PATH, winx68k_ini);
		}

	if (Config.save_hdd_path)
		for (i=0; i<16; i++)
		{
			sprintf(buf, "HDD%d", i);
			GetPrivateProfileString("WinX68k", buf, "", Config.HDImage[i], MAX_PATH, winx68k_ini);
		}
}


void SaveConfig(void)
{
	int	i;
	char	buf[MAX_PATH];

	WritePrivateProfileString("WinX68k", "StartDir", filepath, winx68k_ini);

	if (Config.save_fdd_path)
		for (i = 0; i < 2; i++)
		{
			sprintf(buf, "FDD%d", i);
			/* A ZIP member's slot means nothing to a later session. */
			WritePrivateProfileString("WinX68k", buf,
				px68k_is_zip_virtual_path(Config.FDDImage[i]) ? "" : Config.FDDImage[i],
				winx68k_ini);
		}

	if (Config.save_hdd_path)
		for (i=0; i<16; i++)
		{
			sprintf(buf, "HDD%d", i);
			WritePrivateProfileString("WinX68k", buf,
				px68k_is_zip_virtual_path(Config.HDImage[i]) ? "" : Config.HDImage[i],
				winx68k_ini);
		}
}
