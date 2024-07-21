#include <libretro.h>
#include <stdint.h>

#include "x68k/midi.h"
#include "prop.h"

#include "midiout.h"
#include "midiout_winmm.h"

#define MIDI_LIBRETRO 0
#define MIDI_WINMM    1

extern int libretro_supports_midi_output;

static int midi_driver = MIDI_LIBRETRO;

/* TODO: Make this proper menu label
 * for winmm midi driver backup. A value if 0 will mostly mean to use
 * Microsoft GS Synthesizer. 1/2 means other synths depending on which synth was loaded first.
 */
static int midi_winmm_port = 0;

/* libretro midi interface */

static void libretro_send_short_msg(uint32_t msg)
{
   if (libretro_supports_midi_output && midi_cb.output_enabled())
   {
      midi_cb.write(msg & 0xFF, 0);         /* status byte */
      midi_cb.write((msg >> 8) & 0xFF, 0);  /* note no. */
      midi_cb.write((msg >> 16) & 0xFF, 0); /* velocity */
   }
}

static void libretro_send_long_msg(const uint8_t *msg, size_t len)
{
   if (libretro_supports_midi_output && midi_cb.output_enabled())
   {
      int i;
      for (i = 0; i < len; i++)
         midi_cb.write(msg[i], 0);
   }
}

static int libretro_open(void)
{
   if (libretro_supports_midi_output && midi_cb.output_enabled())
   {
      return 1;
   }
   return 0;
}

static int libretro_flush(void)
{
   if (libretro_supports_midi_output && midi_cb.output_enabled())
   {
      midi_cb.flush();
   }
}

static int libretro_init(void)
{
   libretro_supports_midi_output = 0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_MIDI_INTERFACE, &midi_cb))
      libretro_supports_midi_output = 1;
}

/* midi interface */

void midi_out_short_msg(uint32_t msg)
{
   switch (midi_driver)
   {
#ifdef _WIN32
   case MIDI_WINMM:
      winmm_send_short_msg(msg);
      break;
#endif

   default:
      libretro_send_short_msg(msg);
      break;
   }
}

void midi_out_long_msg(uint8_t *msg, size_t len)
{
   switch (midi_driver)
   {
#ifdef _WIN32
   case MIDI_WINMM:
      winmm_send_long_msg(msg, len);
      break;
#endif

   default:
      libretro_send_long_msg(msg, len);
      break;
   }
}

int midi_out_open(void)
{
   switch (midi_driver)
   {
#ifdef _WIN32
   case MIDI_WINMM:
      log_cb(RETRO_LOG_INFO, "midi port: %s\n", winmm_device_name(midi_winmm_port));
      return winmm_device_open(midi_winmm_port);
#endif

   default:
      return libretro_open();
   }
}

void midi_out_close(void)
{
   switch (midi_driver)
   {
#ifdef _WIN32
   case MIDI_WINMM:
      winmm_device_close();
      break;
#endif

   default:
      break;
   }
}

void midi_out_flush(void)
{
   switch (midi_driver)
   {
#ifdef _WIN32
   case MIDI_WINMM:
      break;
#endif

   default:
      libretro_flush();
      break;
   }
}

void midi_interface_init(void)
{
   switch (midi_driver)
   {
#ifdef _WIN32
   case MIDI_WINMM:
      break;
#endif
   
   default:
      libretro_init();
      break;
   }
}

/* midi core options */

void update_variable_midi_interface(int running)
{
   struct retro_variable var;

   var.key = "px68k_midi_driver";
   var.value = NULL;

   if (!running && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "libretro"))
         midi_driver = MIDI_LIBRETRO;
      else if (!strcmp(var.value, "winmm"))
         midi_driver = MIDI_WINMM;
   }

   var.key = "px68k_winmm_midi_port";
   var.value = NULL;

   if (!running && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      midi_winmm_port = atoi(var.value);
   }

   var.key = "px68k_midi_output";
   var.value = NULL;

   if (!running && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         Config.MIDI_SW = 0;
      else if (!strcmp(var.value, "enabled"))
         Config.MIDI_SW = 1;
   }

   var.key = "px68k_midi_output_type";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "LA"))
         Config.MIDI_Type = 0;
      else if (!strcmp(var.value, "GM"))
         Config.MIDI_Type = 1;
      else if (!strcmp(var.value, "GS"))
         Config.MIDI_Type = 2;
      else if (!strcmp(var.value, "XG"))
         Config.MIDI_Type = 3;
   }
}

/* END OF MIDI INTERFACE */