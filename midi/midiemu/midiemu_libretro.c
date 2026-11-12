/* Common internal MIDI backend selection and audio mixing. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <libretro.h>
#include <rthreads/rthreads.h>
#include "sc55.h"
#include "midiemu_libretro.h"
#include "mt32emu_libretro.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#ifdef _WIN32
#define PXSLASH '\\'
#else
#define PXSLASH '/'
#endif

static sc55_t *unit;
static int active_enabled;
static int active_backend;
static char saved_system[PATH_MAX];
static char saved_content[PATH_MAX];
static int32_t native_audio[4096 * 2];
static size_t native_count;
static double native_phase;
static unsigned native_rate = 66207;
static int volume_percent = 100;
static retro_log_printf_t midi_log_cb;
static int attempted_emulation = -1;
static char attempted_system[PATH_MAX];
static char attempted_content[PATH_MAX];

#define MIDI_COMMAND_CAPACITY 2048
#define MIDI_PCM_CAPACITY 8192
#define MIDI_RENDER_MAX 1024

enum
{
   MIDI_COMMAND_SHORT = 1,
   MIDI_COMMAND_LONG,
   MIDI_COMMAND_RENDER,
   MIDI_COMMAND_BARRIER
};

struct midi_command
{
   int type;
   size_t msg;
   uint8_t *data;
   size_t len;
   size_t frames;
};

static struct midi_command midi_commands[MIDI_COMMAND_CAPACITY];
static size_t midi_command_read;
static size_t midi_command_write;
static size_t midi_command_count;
static int16_t midi_pcm[MIDI_PCM_CAPACITY * 2];
static size_t midi_pcm_read;
static size_t midi_pcm_write;
static size_t midi_pcm_count;
static slock_t *midi_worker_lock;
static scond_t *midi_worker_wake;
static scond_t *midi_worker_state;
static sthread_t *midi_worker_thread;
static int midi_worker_running;
static int midi_worker_stop;
static int midi_worker_paused;
static int midi_runahead_active;
static int midi_runahead_suppressed;
static int midi_render_pending;
static int midi_worker_volume = 100;
static void px68k_midiemu_render_sc55(int16_t *stereo, size_t frames, int volume);

static int midi_queue_push_locked(const struct midi_command *command)
{
   if (midi_command_count >= MIDI_COMMAND_CAPACITY)
      return 0;
   midi_commands[midi_command_write] = *command;
   midi_command_write = (midi_command_write + 1) % MIDI_COMMAND_CAPACITY;
   midi_command_count++;
   scond_signal(midi_worker_wake);
   return 1;
}

static void midi_worker_main(void *userdata)
{
   int16_t rendered[MIDI_RENDER_MAX * 2];
   (void)userdata;
   for (;;)
   {
      struct midi_command command;
      int render_volume;
      slock_lock(midi_worker_lock);
      while (!midi_command_count && !midi_worker_stop)
         scond_wait(midi_worker_wake, midi_worker_lock);
      if (midi_worker_stop)
      {
         slock_unlock(midi_worker_lock);
         break;
      }
      command = midi_commands[midi_command_read];
      render_volume = midi_worker_volume;
      memset(&midi_commands[midi_command_read], 0, sizeof(midi_commands[midi_command_read]));
      midi_command_read = (midi_command_read + 1) % MIDI_COMMAND_CAPACITY;
      midi_command_count--;
      scond_broadcast(midi_worker_state);
      slock_unlock(midi_worker_lock);

      if (command.type == MIDI_COMMAND_BARRIER)
      {
         slock_lock(midi_worker_lock);
         midi_worker_paused = 1;
         scond_broadcast(midi_worker_state);
         while (midi_worker_paused && !midi_worker_stop)
            scond_wait(midi_worker_wake, midi_worker_lock);
         slock_unlock(midi_worker_lock);
         continue;
      }
      if (command.type == MIDI_COMMAND_SHORT)
      {
         if (active_backend == PX68K_MIDIEMU_SC55 && unit)
         {
            uint8_t bytes[3];
            size_t len = 3;
            bytes[0] = (uint8_t)(command.msg & 0xff);
            bytes[1] = (uint8_t)((command.msg >> 8) & 0xff);
            bytes[2] = (uint8_t)((command.msg >> 16) & 0xff);
            if ((bytes[0] & 0xf0) == 0xc0 || (bytes[0] & 0xf0) == 0xd0) len = 2;
            sc55_midi(unit, bytes, len);
         }
         else
            px68k_mt32_midi_short(command.msg);
      }
      else if (command.type == MIDI_COMMAND_LONG)
      {
         if (active_backend == PX68K_MIDIEMU_SC55 && unit)
            sc55_midi(unit, command.data, command.len);
         else
            px68k_mt32_midi_long(command.data, command.len);
         free(command.data);
      }
      else if (command.type == MIDI_COMMAND_RENDER)
      {
         size_t frames = command.frames;
         size_t i;
         if (frames > MIDI_RENDER_MAX) frames = MIDI_RENDER_MAX;
         memset(rendered, 0, frames * 2 * sizeof(int16_t));
         if (active_backend == PX68K_MIDIEMU_SC55 && unit)
         {
            /* The SC-55 path below is shared with the synchronous fallback. */
            px68k_midiemu_render_sc55(rendered, frames, render_volume);
         }
         else
            px68k_mt32_mix(rendered, frames, render_volume);

         /* Never stall the emulation thread waiting for PCM space. If the
          * worker falls more than the ring capacity behind, drop this block. */
         slock_lock(midi_worker_lock);
         if (MIDI_PCM_CAPACITY - midi_pcm_count >= frames)
         {
            for (i = 0; i < frames; i++)
            {
               midi_pcm[midi_pcm_write * 2] = rendered[i * 2];
               midi_pcm[midi_pcm_write * 2 + 1] = rendered[i * 2 + 1];
               midi_pcm_write = (midi_pcm_write + 1) % MIDI_PCM_CAPACITY;
            }
            midi_pcm_count += frames;
         }
         midi_render_pending = 0;
         slock_unlock(midi_worker_lock);
      }
   }
}

static int midi_worker_start(void)
{
   if (midi_runahead_suppressed)
      return 1;
   midi_worker_lock = slock_new();
   midi_worker_wake = scond_new();
   midi_worker_state = scond_new();
   if (!midi_worker_lock || !midi_worker_wake || !midi_worker_state)
      goto error;
   midi_command_read = midi_command_write = midi_command_count = 0;
   midi_pcm_read = midi_pcm_write = midi_pcm_count = 0;
   midi_render_pending = 0;
   midi_runahead_active = 0;
   midi_worker_stop = midi_worker_paused = 0;
   midi_worker_volume = volume_percent;
   midi_worker_thread = sthread_create(midi_worker_main, NULL);
   if (!midi_worker_thread)
      goto error;
   midi_worker_running = 1;
   return 1;
error:
   if (midi_worker_state) scond_free(midi_worker_state);
   if (midi_worker_wake) scond_free(midi_worker_wake);
   if (midi_worker_lock) slock_free(midi_worker_lock);
   midi_worker_state = NULL;
   midi_worker_wake = NULL;
   midi_worker_lock = NULL;
   return 0;
}

static int midi_worker_enqueue(struct midi_command *command, int wait_for_space)
{
   int result = 0;
   if (!midi_worker_running) return 0;
   slock_lock(midi_worker_lock);
   if (midi_runahead_active || midi_worker_stop)
      goto done;
   while (midi_command_count >= MIDI_COMMAND_CAPACITY && wait_for_space &&
         !midi_runahead_active && !midi_worker_stop)
      scond_wait(midi_worker_state, midi_worker_lock);
   if (!midi_runahead_active && !midi_worker_stop)
      result = midi_queue_push_locked(command);
done:
   slock_unlock(midi_worker_lock);
   return result;
}

static void midi_worker_stop_and_join(void)
{
   if (!midi_worker_running)
   {
      midi_runahead_active = 0;
      midi_render_pending = 0;
      midi_pcm_read = midi_pcm_write = midi_pcm_count = 0;
      return;
   }
   slock_lock(midi_worker_lock);
   midi_runahead_active = 0;
   midi_worker_paused = 0;
   midi_render_pending = 0;
   midi_worker_stop = 1;
   scond_broadcast(midi_worker_wake);
   slock_unlock(midi_worker_lock);
   sthread_join(midi_worker_thread);
   midi_worker_thread = NULL;
   midi_worker_running = 0;
   while (midi_command_count)
   {
      struct midi_command *command = &midi_commands[midi_command_read];
      if (command->type == MIDI_COMMAND_LONG) free(command->data);
      midi_command_read = (midi_command_read + 1) % MIDI_COMMAND_CAPACITY;
      midi_command_count--;
   }
   scond_free(midi_worker_state);
   scond_free(midi_worker_wake);
   slock_free(midi_worker_lock);
   midi_worker_state = NULL;
   midi_worker_wake = NULL;
   midi_worker_lock = NULL;
}

void px68k_midiemu_state_load_begin(void)
{
   struct midi_command command;
   if (!midi_worker_running)
      return;
   memset(&command, 0, sizeof(command));
   command.type = MIDI_COMMAND_BARRIER;
   slock_lock(midi_worker_lock);
   while (midi_command_count >= MIDI_COMMAND_CAPACITY && !midi_worker_stop)
      scond_wait(midi_worker_state, midi_worker_lock);
   if (!midi_worker_stop)
   {
      midi_queue_push_locked(&command);
      while (!midi_worker_paused && !midi_worker_stop)
         scond_wait(midi_worker_state, midi_worker_lock);
      midi_pcm_read = midi_pcm_write = midi_pcm_count = 0;
      midi_render_pending = 0;
      midi_worker_paused = 0;
      scond_broadcast(midi_worker_wake);
   }
   slock_unlock(midi_worker_lock);
}

void px68k_midiemu_runahead_begin(void)
{
   struct midi_command command;
   if (!midi_worker_running)
   {
      midi_runahead_active = 1;
      return;
   }
   memset(&command, 0, sizeof(command));
   command.type = MIDI_COMMAND_BARRIER;
   slock_lock(midi_worker_lock);
   midi_runahead_active = 1;
   while (midi_command_count >= MIDI_COMMAND_CAPACITY && !midi_worker_stop)
      scond_wait(midi_worker_state, midi_worker_lock);
   if (!midi_worker_stop)
   {
      midi_queue_push_locked(&command);
      while (!midi_worker_paused && !midi_worker_stop)
         scond_wait(midi_worker_state, midi_worker_lock);
      midi_render_pending = 0;
   }
   slock_unlock(midi_worker_lock);
}

void px68k_midiemu_runahead_end(void)
{
   if (!midi_worker_running)
   {
      midi_runahead_active = 0;
      return;
   }
   slock_lock(midi_worker_lock);
   midi_runahead_active = 0;
   midi_worker_paused = 0;
   scond_broadcast(midi_worker_wake);
   slock_unlock(midi_worker_lock);
}

/* The second-instance run-ahead core is only used to produce speculative
 * frames. Its synth must not emit MIDI or spend time rendering audio. */
void px68k_midiemu_runahead_suppress(void)
{
   midi_runahead_suppressed = 1;
   midi_worker_stop_and_join();
   midi_pcm_read = midi_pcm_write = midi_pcm_count = 0;
   midi_render_pending = 0;
}

/* Called for a newly initialized core instance, before MIDI configuration. */
void px68k_midiemu_runahead_reset(void)
{
   midi_runahead_suppressed = 0;
   midi_runahead_active = 0;
}

void px68k_midiemu_set_log_callback(retro_log_printf_t callback)
{
   midi_log_cb = callback;
   px68k_mt32_set_log_callback(callback);
}

static void copy_path(char *out, size_t cap, const char *in)
{
   if (!in) in = "";
   snprintf(out, cap, "%s", in);
}

static int load_file_slot(sc55_t *dev, int slot, const char *path)
{
   FILE *f = fopen(path, "rb");
   long size;
   unsigned char *buf;
   int ok;
   if (!f) return 0;
   if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET) != 0)
   { fclose(f); return 0; }
   buf = (unsigned char*)malloc((size_t)size);
   if (!buf) { fclose(f); return 0; }
   if (fread(buf, 1, (size_t)size, f) != (size_t)size)
   { free(buf); fclose(f); return 0; }
   fclose(f);
   ok = sc55_load_rom(dev, slot, buf, (size_t)size);
   free(buf);
   return ok;
}

static int try_named_set(int model, const char *const names[SC55_ROM_COUNT],
      const char *const *dirs, size_t ndirs)
{
   sc55_t *dev = sc55_new(model);
   int slot;
   size_t d;
   char path[PATH_MAX * 2];
   if (!dev) return 0;
   for (slot = 0; slot < SC55_ROM_COUNT; slot++)
   {
      int loaded = 0;
      if (!names[slot]) continue;
      for (d = 0; d < ndirs && !loaded; d++)
      {
         if (!dirs[d] || !dirs[d][0]) continue;
         snprintf(path, sizeof(path), "%s%c%s", dirs[d], PXSLASH, names[slot]);
         loaded = load_file_slot(dev, slot, path);
      }
      if (!loaded) { sc55_free(dev); return 0; }
   }
   sc55_reset(dev);
   unit = dev;
   native_rate = sc55_rate(unit);
   native_count = 0;
   native_phase = 0.0;
   return 1;
}

static int load_unit(void)
{
   char sys_prboom[PATH_MAX], content_sc55[PATH_MAX], sys_sc55[PATH_MAX];
   char sys_sc_55[PATH_MAX], sys_roland[PATH_MAX], sys_nuked[PATH_MAX];
   char pr_sc55[PATH_MAX], pr_sc_55[PATH_MAX], pr_roland[PATH_MAX], pr_nuked[PATH_MAX];
   const char *dirs[16];
   size_t n = 0;
   static const char *const mk2[SC55_ROM_COUNT] =
      { "rom1.bin", "rom2.bin", "rom_sm.bin", "waverom1.bin", "waverom2.bin", NULL };
   static const char *const mk1[SC55_ROM_COUNT] =
      { "sc55_rom1.bin", "sc55_rom2.bin", NULL, "sc55_waverom1.bin", "sc55_waverom2.bin", "sc55_waverom3.bin" };
   snprintf(sys_prboom, sizeof(sys_prboom), "%s%cprboom", saved_system, PXSLASH);
   snprintf(content_sc55, sizeof(content_sc55), "%s%csc55", saved_content, PXSLASH);
   snprintf(sys_sc55, sizeof(sys_sc55), "%s%csc55", saved_system, PXSLASH);
   snprintf(sys_sc_55, sizeof(sys_sc_55), "%s%csc-55", saved_system, PXSLASH);
   snprintf(sys_roland, sizeof(sys_roland), "%s%croland", saved_system, PXSLASH);
   snprintf(sys_nuked, sizeof(sys_nuked), "%s%cnuked", saved_system, PXSLASH);
   snprintf(pr_sc55, sizeof(pr_sc55), "%s%csc55", sys_prboom, PXSLASH);
   snprintf(pr_sc_55, sizeof(pr_sc_55), "%s%csc-55", sys_prboom, PXSLASH);
   snprintf(pr_roland, sizeof(pr_roland), "%s%croland", sys_prboom, PXSLASH);
   snprintf(pr_nuked, sizeof(pr_nuked), "%s%cnuked", sys_prboom, PXSLASH);
   if (saved_content[0]) dirs[n++] = saved_content;
   if (saved_system[0]) dirs[n++] = sys_prboom;
   if (saved_system[0]) dirs[n++] = saved_system;
   if (saved_content[0]) dirs[n++] = content_sc55;
   if (saved_system[0]) dirs[n++] = sys_sc55;
   if (saved_system[0]) dirs[n++] = sys_sc_55;
   if (saved_system[0]) dirs[n++] = sys_roland;
   if (saved_system[0]) dirs[n++] = sys_nuked;
   if (saved_system[0]) dirs[n++] = pr_sc55;
   if (saved_system[0]) dirs[n++] = pr_sc_55;
   if (saved_system[0]) dirs[n++] = pr_roland;
   if (saved_system[0]) dirs[n++] = pr_nuked;
   if (try_named_set(SC55_MODEL_MK2, mk2, dirs, n)) return 1;
   if (try_named_set(SC55_MODEL_MK1, mk1, dirs, n)) return 1;
   return 0;
}

void px68k_midiemu_shutdown(void)
{
   midi_worker_stop_and_join();
   if (unit) sc55_free(unit);
   unit = NULL;
   px68k_mt32_shutdown();
   active_backend = PX68K_MIDIEMU_DISABLED;
   active_enabled = 0;
   native_count = 0;
   native_phase = 0.0;
   attempted_emulation = -1;
   attempted_system[0] = '\0';
   attempted_content[0] = '\0';
}

void px68k_midiemu_configure(int emulation, const char *system_dir, const char *content_dir)
{
   char sys[PATH_MAX], content[PATH_MAX];
   copy_path(sys, sizeof(sys), system_dir);
   copy_path(content, sizeof(content), content_dir);
   if (emulation < PX68K_MIDIEMU_DISABLED || emulation > PX68K_MIDIEMU_CM32L)
      emulation = PX68K_MIDIEMU_DISABLED;
   if (emulation == PX68K_MIDIEMU_DISABLED)
   {
      if (unit || active_enabled || active_backend != PX68K_MIDIEMU_DISABLED) px68k_midiemu_shutdown();
      saved_system[0] = saved_content[0] = '\0';
      attempted_emulation = -1;
      attempted_system[0] = attempted_content[0] = '\0';
      return;
   }
   if (active_enabled && active_backend == emulation &&
         !strcmp(saved_system, sys) && !strcmp(saved_content, content))
      return;
   if (attempted_emulation == emulation &&
         !strcmp(attempted_system, sys) && !strcmp(attempted_content, content))
      return;
   px68k_midiemu_shutdown();
   copy_path(saved_system, sizeof(saved_system), sys);
   copy_path(saved_content, sizeof(saved_content), content);
   attempted_emulation = emulation;
   copy_path(attempted_system, sizeof(attempted_system), sys);
   copy_path(attempted_content, sizeof(attempted_content), content);
   if (emulation == PX68K_MIDIEMU_SC55)
   {
      if (load_unit())
      {
         active_enabled = 1;
         active_backend = emulation;
         if (!midi_worker_start() && midi_log_cb)
            midi_log_cb(RETRO_LOG_WARN, "[PX68K MIDI] Worker thread unavailable; using synchronous synthesis.\n");
         if (midi_log_cb)
            midi_log_cb(RETRO_LOG_INFO, "[PX68K MIDI] SC-55 ROM set loaded.\n");
      }
      else if (midi_log_cb)
         midi_log_cb(RETRO_LOG_WARN, "[PX68K MIDI] SC-55 ROM set not found. Check the System Directory and content directory for the documented ROM filenames.\n");
   }
   else if (px68k_mt32_configure(emulation, sys, content))
   {
      active_enabled = 1;
      active_backend = emulation;
      if (!midi_worker_start() && midi_log_cb)
         midi_log_cb(RETRO_LOG_WARN, "[PX68K MIDI] Worker thread unavailable; using synchronous synthesis.\n");
   }
}

int px68k_midiemu_enabled(void)
{
   if (!active_enabled) return 0;
   if (active_backend == PX68K_MIDIEMU_SC55) return unit != NULL;
   return px68k_mt32_enabled();
}

void px68k_midiemu_set_volume(int value)
{
   if (value < 0) value = 0;
   if (value > 200) value = 200;
   volume_percent = value;
   if (midi_worker_running)
   {
      slock_lock(midi_worker_lock);
      midi_worker_volume = value;
      slock_unlock(midi_worker_lock);
   }
}

void px68k_midiemu_midi_short(size_t msg)
{
   struct midi_command command;
   if (midi_runahead_suppressed || !px68k_midiemu_enabled()) return;
   if (midi_worker_running)
   {
      memset(&command, 0, sizeof(command));
      command.type = MIDI_COMMAND_SHORT;
      command.msg = msg;
      midi_worker_enqueue(&command, 1);
      return;
   }
   if (midi_runahead_active) return;
   if (active_backend != PX68K_MIDIEMU_SC55)
   {
      px68k_mt32_midi_short(msg);
      return;
   }
   {
      uint8_t bytes[3];
      size_t len = 3;
      bytes[0] = (uint8_t)(msg & 0xff);
      bytes[1] = (uint8_t)((msg >> 8) & 0xff);
      bytes[2] = (uint8_t)((msg >> 16) & 0xff);
      if ((bytes[0] & 0xf0) == 0xc0 || (bytes[0] & 0xf0) == 0xd0) len = 2;
      sc55_midi(unit, bytes, len);
   }
}

void px68k_midiemu_midi_long(const uint8_t *data, size_t len)
{
   if (midi_runahead_suppressed || !px68k_midiemu_enabled() || !data || !len) return;
   if (midi_worker_running)
   {
      struct midi_command command;
      uint8_t *copy = (uint8_t*)malloc(len);
      if (!copy) return;
      memcpy(copy, data, len);
      memset(&command, 0, sizeof(command));
      command.type = MIDI_COMMAND_LONG;
      command.data = copy;
      command.len = len;
      if (!midi_worker_enqueue(&command, 1)) free(copy);
      return;
   }
   if (midi_runahead_active) return;
   if (active_backend == PX68K_MIDIEMU_SC55) sc55_midi(unit, data, len);
   else px68k_mt32_midi_long(data, len);
}

static int16_t clamp16(int32_t x)
{
   if (x > 32767) return 32767;
   if (x < -32768) return -32768;
   return (int16_t)x;
}

static void px68k_midiemu_render_sc55(int16_t *stereo, size_t frames, int volume)
{
   size_t i;
   double ratio;
   if (!unit || !stereo || !frames) return;
   ratio = (double)native_rate / 44100.0;
   if (ratio <= 0.0) return;
   /* Keep one native sample on the left of the interpolation point. */
   if (native_phase >= (double)(native_count > 1 ? native_count - 1 : 0))
   {
      size_t drop = (size_t)native_phase;
      if (drop >= native_count) drop = native_count ? native_count - 1 : 0;
      if (drop)
      {
         memmove(native_audio, native_audio + drop * 2, (native_count - drop) * 2 * sizeof(int32_t));
         native_count -= drop;
         native_phase -= (double)drop;
      }
   }
   {
      double last_needed = native_phase + (double)(frames - 1) * ratio + 1.0;
      size_t need = last_needed >= 0.0 ? (size_t)last_needed + 1 : 2;
      if (need > 4095) need = 4095;
      if (need > native_count)
      {
         size_t add = need - native_count;
         if (native_count + add > 4096) add = 4096 - native_count;
         if (add && sc55_run(unit, native_audio + native_count * 2, add) == add)
            native_count += add;
      }
   }
   for (i = 0; i < frames; i++)
   {
      size_t idx = (size_t)native_phase;
      double frac = native_phase - (double)idx;
      int32_t l, r;
      if (idx + 1 >= native_count) break;
      l = (int32_t)((double)native_audio[idx * 2] * (1.0 - frac) + (double)native_audio[(idx + 1) * 2] * frac);
      r = (int32_t)((double)native_audio[idx * 2 + 1] * (1.0 - frac) + (double)native_audio[(idx + 1) * 2 + 1] * frac);
      /* Scale after converting to the 16-bit mixer domain to avoid overflow. */
      l = (int32_t)(((int64_t)(l >> 14) * volume) / 100);
      r = (int32_t)(((int64_t)(r >> 14) * volume) / 100);
      stereo[i * 2] = clamp16((int32_t)stereo[i * 2] + l);
      stereo[i * 2 + 1] = clamp16((int32_t)stereo[i * 2 + 1] + r);
      native_phase += ratio;
   }
   if (native_phase >= 1.0)
   {
      size_t drop = (size_t)native_phase;
      if (drop >= native_count) drop = native_count ? native_count - 1 : 0;
      if (drop)
      {
         memmove(native_audio, native_audio + drop * 2, (native_count - drop) * 2 * sizeof(int32_t));
         native_count -= drop;
         native_phase -= (double)drop;
      }
   }
}


void px68k_midiemu_mix(int16_t *stereo, size_t frames)
{
   size_t i;
   if (midi_runahead_suppressed || !px68k_midiemu_enabled() || !stereo || !frames) return;
   if (midi_worker_running)
   {
      struct midi_command command;
      size_t count;
      slock_lock(midi_worker_lock);
      if (midi_runahead_active)
      {
         slock_unlock(midi_worker_lock);
         return;
      }
      count = frames < midi_pcm_count ? frames : midi_pcm_count;
      for (i = 0; i < count; i++)
      {
         int32_t left = (int32_t)stereo[i * 2] + midi_pcm[midi_pcm_read * 2];
         int32_t right = (int32_t)stereo[i * 2 + 1] + midi_pcm[midi_pcm_read * 2 + 1];
         stereo[i * 2] = clamp16(left);
         stereo[i * 2 + 1] = clamp16(right);
         midi_pcm_read = (midi_pcm_read + 1) % MIDI_PCM_CAPACITY;
      }
      midi_pcm_count -= count;

      /* At most one render block may be queued or executing. Without this
       * limit, fast-forward can fill the command queue with stale render
       * requests until MIDI event submission blocks the emulation thread. */
      if (!midi_render_pending && midi_command_count < MIDI_COMMAND_CAPACITY &&
            !midi_worker_stop)
      {
         memset(&command, 0, sizeof(command));
         command.type = MIDI_COMMAND_RENDER;
         command.frames = frames > MIDI_RENDER_MAX ? MIDI_RENDER_MAX : frames;
         midi_render_pending = 1;
         if (!midi_queue_push_locked(&command))
            midi_render_pending = 0;
      }
      slock_unlock(midi_worker_lock);
      return;
   }
   if (midi_runahead_active) return;
   if (active_backend != PX68K_MIDIEMU_SC55)
   {
      px68k_mt32_mix(stereo, frames, volume_percent);
      return;
   }
   px68k_midiemu_render_sc55(stereo, frames, volume_percent);
}
