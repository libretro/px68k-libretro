#ifndef _WINX68K_CONFIG_H
#define _WINX68K_CONFIG_H

#include <stdint.h>
#include "common.h"

typedef struct
{
	uint32_t BufferSize;
	int OPM_VOL;
	int PCM_VOL;
	int MCR_VOL;
	int WindowFDDStat;
	int MIDI_SW;
	int MIDI_Type;
	int MIDI_Reset;
	char HDImage[16][MAX_PATH];
	int ToneMap;
	char ToneMapFile[MAX_PATH];
	int XVIMode;
	int Sound_LPF;
	int SoundROMEO;
	int MIDIDelay;
	int MIDIAutoDelay;
	char FDDImage[2][MAX_PATH];
	int VbtnSwap;
	int JoyOrMouse;
	int NoWaitMode;
	uint8_t FrameRate;
	int AdjustFrameRates;
	int AudioDesyncHack;
	int MenuFontSize; /* font size of menu, 0 = normal, 1 = large */
	int MenuLinesJump; /* Left/Right navigation step, 5 to 50 */
	int joy1_select_mapping; /* used for keyboard to joypad map for P1 Select */
	int save_fdd_path;
	int save_hdd_path;
	/* Cpu clock in MHz */
	int clockmhz;
	/* RAM Size = size * 1024 * 1024 */
	int ram_size;
	/* Set controller type for each player to use
	 * 0 = Standard 2-buttons gamepad
	 * 1 = CPSF-MD (8 Buttons
	 * 2 = CPSF-SFC (8 Buttons) */
	int JOY_TYPE[2];
} Win68Conf;

extern Win68Conf Config;

void LoadConfig(void);
void SaveConfig(void);
void PropPage_Init(void);

#endif /* _WINX68K_CONFIG_H */
