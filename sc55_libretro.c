/* Libretro integration glue for the Nuked-SC55 emulator. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "sc55.h"
#include "sc55_libretro.h"

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
static char saved_system[PATH_MAX];
static char saved_content[PATH_MAX];
static int32_t native_audio[4096 * 2];
static size_t native_count;
static double native_phase;
static unsigned native_rate = 66207;
static int volume_percent = 100;

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

void px68k_sc55_shutdown(void)
{
   if (unit) sc55_free(unit);
   unit = NULL;
   active_enabled = 0;
   native_count = 0;
   native_phase = 0.0;
}

void px68k_sc55_configure(int enabled, const char *system_dir, const char *content_dir)
{
   char sys[PATH_MAX], content[PATH_MAX];
   copy_path(sys, sizeof(sys), system_dir);
   copy_path(content, sizeof(content), content_dir);
   enabled = enabled != 0;
   if (!enabled)
   {
      if (unit || active_enabled) px68k_sc55_shutdown();
      saved_system[0] = saved_content[0] = '\0';
      return;
   }
   if (unit && active_enabled && !strcmp(saved_system, sys) && !strcmp(saved_content, content))
      return;
   px68k_sc55_shutdown();
   copy_path(saved_system, sizeof(saved_system), sys);
   copy_path(saved_content, sizeof(saved_content), content);
   if (load_unit()) active_enabled = 1;
   else fprintf(stderr, "PX68K SC-55: ROM set not found. Expected PrBoom-compatible filenames in the System Directory, System/prboom, or SC-55 subfolders.\n");
}

int px68k_sc55_enabled(void) { return active_enabled && unit != NULL; }

void px68k_sc55_set_volume(int value)
{
   if (value < 0) value = 0;
   if (value > 200) value = 200;
   volume_percent = value;
}

void px68k_sc55_midi_short(size_t msg)
{
   uint8_t bytes[3];
   size_t len = 3;
   if (!px68k_sc55_enabled()) return;
   bytes[0] = (uint8_t)(msg & 0xff);
   bytes[1] = (uint8_t)((msg >> 8) & 0xff);
   bytes[2] = (uint8_t)((msg >> 16) & 0xff);
   if ((bytes[0] & 0xf0) == 0xc0 || (bytes[0] & 0xf0) == 0xd0) len = 2;
   sc55_midi(unit, bytes, len);
}

void px68k_sc55_midi_long(const uint8_t *data, size_t len)
{
   if (px68k_sc55_enabled() && data && len) sc55_midi(unit, data, len);
}

static int16_t clamp16(int32_t x)
{
   if (x > 32767) return 32767;
   if (x < -32768) return -32768;
   return (int16_t)x;
}

void px68k_sc55_mix(int16_t *stereo, size_t frames)
{
   size_t i;
   double ratio;
   if (!px68k_sc55_enabled() || !stereo || !frames) return;
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
      l = (int32_t)(((int64_t)(l >> 14) * volume_percent) / 100);
      r = (int32_t)(((int64_t)(r >> 14) * volume_percent) / 100);
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
