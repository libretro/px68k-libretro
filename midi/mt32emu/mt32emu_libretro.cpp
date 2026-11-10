/* Munt MT-32 / CM-32L integration. The public core interface remains C89. */
#include "mt32emu_libretro.h"
#include "midiemu_libretro.h"
#include <stdio.h>
#include <libretro.h>
#include <string.h>

#define MT32EMU_API_TYPE 1
#include "mt32emu.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#ifdef _WIN32
#define PX_MUNT_SLASH '\\'
#else
#define PX_MUNT_SLASH '/'
#endif

static mt32emu_context px_munt_context;
static int px_munt_opened;
static int16_t px_munt_buffer[1024 * 2];
static retro_log_printf_t px_munt_log;

void px68k_mt32_set_log_callback(retro_log_printf_t callback)
{
   px_munt_log = callback;
}

static int px_munt_add_file(const char *machine, const char *dir, const char *name)
{
   char path[PATH_MAX * 2];
   if (!dir || !dir[0]) return 0;
   snprintf(path, sizeof(path), "%s%c%s", dir, PX_MUNT_SLASH, name);
   {
      mt32emu_return_code rc = mt32emu_add_machine_rom_file(px_munt_context, machine, path);
      if (rc > MT32EMU_RC_OK && px_munt_log)
         px_munt_log(RETRO_LOG_INFO, "[PX68K MIDI] Munt: loaded ROM file '%s' for %s.\n", path, machine);
      else if (rc < MT32EMU_RC_OK && px_munt_log)
         px_munt_log(RETRO_LOG_DEBUG, "[PX68K MIDI] Munt: could not use ROM file '%s' for %s (code %d).\n", path, machine, (int)rc);
      return rc > MT32EMU_RC_OK;
   }
}

static int px_munt_try_machine(const char *machine, const char *const *dirs,
      size_t dir_count, const char *const *control_names,
      const char *const *pcm_names)
{
   size_t i;
   mt32emu_report_handler_i report_handler;
   mt32emu_rom_info info;

   memset(&report_handler, 0, sizeof(report_handler));
   px_munt_context = mt32emu_create_context(report_handler, NULL);
   if (!px_munt_context) return 0;

   mt32emu_set_stereo_output_samplerate(px_munt_context, 44100.0);
   mt32emu_set_midi_event_queue_size(px_munt_context, 1024);
   mt32emu_configure_midi_event_queue_sysex_storage(px_munt_context, 32768);
   mt32emu_preallocate_reverb_memory(px_munt_context, MT32EMU_BOOL_TRUE);

   /* Machine-specific ROM identification lets Munt merge split ROM dumps
    * only when they belong to the selected hardware/ROM revision. */
   for (i = 0; i < dir_count; i++)
   {
      size_t n;
      for (n = 0; control_names[n]; n++)
         px_munt_add_file(machine, dirs[i], control_names[n]);
      for (n = 0; pcm_names[n]; n++)
         px_munt_add_file(machine, dirs[i], pcm_names[n]);
   }

   memset(&info, 0, sizeof(info));
   mt32emu_get_rom_info(px_munt_context, &info);
   if (info.control_rom_id && info.pcm_rom_id &&
       mt32emu_open_synth(px_munt_context) == MT32EMU_RC_OK)
   {
      px_munt_opened = 1;
      if (px_munt_log)
         px_munt_log(RETRO_LOG_INFO, "[PX68K MIDI] Munt: initialized %s.\n", machine);
      return 1;
   }

   mt32emu_free_context(px_munt_context);
   px_munt_context = NULL;
   return 0;
}

static int px_munt_load_roms(int emulation, const char *system_dir,
      const char *content_dir)
{
   static const char *const mt32_machines[] = {
      "mt32_1_04", "mt32_1_05", "mt32_1_06", "mt32_1_07",
      "mt32_bluer", "mt32_2_03", "mt32_2_04", "mt32_2_06", "mt32_2_07", NULL
   };
   static const char *const cm32l_machines[] = {
      "cm32l_1_00", "cm32l_1_02", NULL
   };
   static const char *const mt32_control[] = {
      "MT32_CONTROL.ROM", "mt32_control.rom", "MT32_CONTROL.BIN", NULL
   };
   static const char *const mt32_pcm[] = {
      "MT32_PCM.ROM", "mt32_pcm.rom", "MT32_PCM.BIN", NULL
   };
   static const char *const cm32l_control[] = {
      "CM32L_CONTROL.ROM", "cm32l_control.rom", "CM-32L_CONTROL.ROM",
      "CM32L_CONTROL.BIN", NULL
   };
   static const char *const cm32l_pcm[] = {
      "CM32L_PCM.ROM", "cm32l_pcm.rom", "CM-32L_PCM.ROM",
      "CM32L_PCM.BIN", NULL
   };
   static char system_subdirs[6][PATH_MAX];
   static char content_subdirs[4][PATH_MAX];
   const char *dirs[12];
   const char *const *machines;
   const char *const *control_names;
   const char *const *pcm_names;
   size_t dir_count = 0;
   size_t i;

   if (emulation == PX68K_MIDIEMU_MT32)
   {
      machines = mt32_machines;
      control_names = mt32_control;
      pcm_names = mt32_pcm;
   }
   else
   {
      machines = cm32l_machines;
      control_names = cm32l_control;
      pcm_names = cm32l_pcm;
   }

   if (system_dir && system_dir[0])
   {
      static const char *const subdir_names[6] = {
         "mt32", "cm32l", "cm64", "roland", "munt", "prboom"
      };
      dirs[dir_count++] = system_dir;
      for (i = 0; i < 6; i++)
      {
         snprintf(system_subdirs[i], sizeof(system_subdirs[i]), "%s%c%s",
               system_dir, PX_MUNT_SLASH, subdir_names[i]);
         dirs[dir_count++] = system_subdirs[i];
      }
   }
   if (content_dir && content_dir[0])
   {
      static const char *const content_names[4] = {
         "mt32", "cm32l", "cm64", "roland"
      };
      dirs[dir_count++] = content_dir;
      for (i = 0; i < 4; i++)
      {
         snprintf(content_subdirs[i], sizeof(content_subdirs[i]), "%s%c%s",
               content_dir, PX_MUNT_SLASH, content_names[i]);
         dirs[dir_count++] = content_subdirs[i];
      }
   }

   if (px_munt_log)
      px_munt_log(RETRO_LOG_INFO, "[PX68K MIDI] Munt: searching %u location(s) for %s ROMs.\n",
            (unsigned)dir_count, emulation == PX68K_MIDIEMU_MT32 ? "MT-32" : "CM-32L-compatible");
   for (i = 0; machines[i]; i++)
      if (px_munt_try_machine(machines[i], dirs, dir_count,
            control_names, pcm_names))
         return 1;
   return 0;
}

int px68k_mt32_configure(int emulation, const char *system_dir, const char *content_dir)
{
   px68k_mt32_shutdown();
   if (emulation != PX68K_MIDIEMU_MT32 &&
       emulation != PX68K_MIDIEMU_CM32L)
      return 0;

   if (px_munt_load_roms(emulation, system_dir, content_dir))
      return 1;
   if (px_munt_log)
   {
      if (emulation == PX68K_MIDIEMU_MT32)
         px_munt_log(RETRO_LOG_WARN,
               "[PX68K MIDI] Munt: no compatible MT-32 ROM pair found (expected MT32_CONTROL.ROM + MT32_PCM.ROM).\n"
               "Searched the System Directory root, mt32/, cm32l/, cm64/, roland/, munt/, prboom/, and the content directory/subdirectories.\n");
      else
         px_munt_log(RETRO_LOG_WARN,
               "[PX68K MIDI] Munt: no compatible CM-32L ROM pair found (expected CM32L_CONTROL.ROM + CM32L_PCM.ROM).\n"
               "Searched the System Directory root, mt32/, cm32l/, cm64/, roland/, munt/, prboom/, and the content directory/subdirectories.\n");
   }
   return 0;
}

int px68k_mt32_enabled(void)
{
   return px_munt_opened && px_munt_context != NULL;
}

void px68k_mt32_midi_short(size_t msg)
{
   if (px68k_mt32_enabled())
      mt32emu_play_msg(px_munt_context, (mt32emu_bit32u)(msg & 0x00ffffffu));
}

void px68k_mt32_midi_long(const uint8_t *data, size_t len)
{
   if (px68k_mt32_enabled() && data && len)
      mt32emu_play_sysex(px_munt_context, data, (mt32emu_bit32u)len);
}

void px68k_mt32_mix(int16_t *stereo, size_t frames, int volume_percent)
{
   size_t done = 0;
   if (!px68k_mt32_enabled() || !stereo || !frames) return;
   if (volume_percent < 0) volume_percent = 0;
   if (volume_percent > 200) volume_percent = 200;
   while (done < frames)
   {
      size_t count = frames - done;
      size_t i;
      if (count > 1024) count = 1024;
      mt32emu_render_bit16s(px_munt_context, px_munt_buffer, (mt32emu_bit32u)count);
      for (i = 0; i < count * 2; i++)
      {
         int32_t sample = (int32_t)(((int64_t)px_munt_buffer[i] * volume_percent) / 100);
         int32_t mixed = (int32_t)stereo[done * 2 + i] + sample;
         if (mixed > 32767) mixed = 32767;
         if (mixed < -32768) mixed = -32768;
         stereo[done * 2 + i] = (int16_t)mixed;
      }
      done += count;
   }
}

void px68k_mt32_shutdown(void)
{
   if (px_munt_context)
   {
      if (px_munt_opened)
         mt32emu_close_synth(px_munt_context);
      mt32emu_free_context(px_munt_context);
   }
   px_munt_context = NULL;
   px_munt_opened = 0;
}
