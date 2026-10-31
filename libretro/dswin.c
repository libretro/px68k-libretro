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

#include        <stdint.h>
#include	"common.h"
#include	"dswin.h"
#include	"prop.h"
#include	"adpcm.h"
#include	"mercury.h"
#include	"fmg_wrap.h"

#define PCMBUF_SIZE 2*2*48000

static uint8_t pcmbuffer[PCMBUF_SIZE];
/* Stereo signed 16-bit PCM; occupancy is counted in frames. */
#define PCM_CAPACITY (PCMBUF_SIZE / 4)
static int pcm_read = 0, pcm_write = 0, pcm_count = 0;
static int32_t snd_precounter = 0;

uint8_t *pbsp = pcmbuffer;
uint8_t *pbrp = pcmbuffer, *pbwp = pcmbuffer;
uint8_t *pbep = &pcmbuffer[PCMBUF_SIZE];

void audio_buffer_reset(void)
{
   pcm_read = pcm_write = pcm_count = 0;
   pbrp = pbwp = pbsp;
   memset(pcmbuffer, 0, sizeof(pcmbuffer));
}

int dswin_StateAction(StateMem *sm, int load, int data_only)
{
   SFORMAT StateRegs[] = { SFVAR(snd_precounter), SFEND };
   int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs,
                                 "X68K_DSND", false);
   if (load && ret)
      audio_buffer_reset();
   return ret;
}

void DSound_Play(void)
{
	ADPCM_SetVolume((uint8_t)Config.PCM_VOL);
	OPM_SetVolume((uint8_t)Config.OPM_VOL);	
}

void DSound_Stop(void)
{
	ADPCM_SetVolume(0);
	OPM_SetVolume(0);	
}

/* The synthesizers accept a wrapping buffer. Never hand them more than
 * the contiguous writable span, and never overwrite unread PCM. */
static void sound_send(int length)
{
   while (length > 0)
   {
      int n = PCM_CAPACITY - pcm_write;
      int free_frames = PCM_CAPACITY - pcm_count;
      if (n > length) n = length;
      if (n > free_frames) n = free_frames;
      if (n == 0)
      {
         /* Preserve emulator time when producer outruns consumer. */
         int drop = length;
         if (drop > pcm_count) drop = pcm_count;
         pcm_read = (pcm_read + drop) % PCM_CAPACITY;
         pcm_count -= drop;
         pbrp = pbsp + pcm_read * 4;
         continue;
      }
      ADPCM_Update((int16_t *)(pbsp + pcm_write * 4), n, pbsp, pbep);
      OPM_Update((int16_t *)(pbsp + pcm_write * 4), n, pbsp, pbep);
      pcm_write = (pcm_write + n) % PCM_CAPACITY;
      pcm_count += n;
      pbwp = pbsp + pcm_write * 4;
      length -= n;
   }
}

void DSound_Send0(int32_t clock)
{
   int length = 0;
   snd_precounter += (44100 * clock);
   while (snd_precounter >= 10000000L)
   {
      length++;
      snd_precounter -= 10000000L;
   }
   if (length > 0) sound_send(length);
}

int audio_samples_avail(void)
{
   return pcm_count;
}

void audio_samples_discard(int discard)
{
   if (discard <= 0) return;
   if (discard > pcm_count) discard = pcm_count;
   pcm_read = (pcm_read + discard) % PCM_CAPACITY;
   pcm_count -= discard;
   pbrp = pbsp + pcm_read * 4;
}

void raudio_callback(void *userdata, unsigned char *stream, int len)
{
   uint8_t *out = (uint8_t *)userdata;
   int frames, n;
   (void)stream;
   if (!out || len <= 0) return;
   /* Callers supply stereo frames. Handle any malformed byte remainder. */
   frames = len / 4;
   while (frames > 0)
   {
      /* Never advance synthesizers to satisfy frontend demand.
       * Only emulated CPU time may produce sound. Missing audio is silence. */
      if (pcm_count == 0)
         break;
      n = PCM_CAPACITY - pcm_read;
      if (n > pcm_count) n = pcm_count;
      if (n > frames) n = frames;
      if (n <= 0) break;
      memcpy(out, pbsp + pcm_read * 4, n * 4);
      out += n * 4;
      audio_samples_discard(n);
      frames -= n;
   }
   if (frames > 0) memset(out, 0, frames * 4);
   if (len & 3) memset((uint8_t *)userdata + (len & ~3), 0, len & 3);
}
