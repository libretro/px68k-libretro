#ifndef DSWIN_H__
#define DSWIN_H__

#include "common.h"

int DSound_Init(unsigned long rate, unsigned long length);
int DSound_Cleanup(void);

void DSound_Play(void);
void DSound_Stop(void);
void FASTCALL DSound_Send0(long clock);

void DS_SetVolumeOPM(long vol);
void DS_SetVolumeADPCM(long vol);
void DS_SetVolumeMercury(long vol);

int audio_samples_avail(void);
void audio_samples_discard(int discard);
void raudio_callback(void *userdata, unsigned char *stream, int len);

#endif /* DSWIN_H__ */
