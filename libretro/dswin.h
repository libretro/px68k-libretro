#ifndef _DSWIN_H
#define _DSWIN_H

#include "common.h"

typedef struct px68k_audio_diag {
   unsigned long produced, requested, copied, padded, dropped, resets, calls;
   int queued, peak_queued;
} px68k_audio_diag;
void audio_diag_get(px68k_audio_diag *out);
void audio_buffer_reset(void);
void DSound_Play(void);
void DSound_Stop(void);
void DSound_Send0(int32_t clock);

int audio_samples_avail(void);
void audio_samples_discard(int discard);
void raudio_callback(void *userdata, unsigned char *stream, int len);
int dswin_StateAction(StateMem *sm, int load, int data_only);

#endif /* _DSWIN_H */
