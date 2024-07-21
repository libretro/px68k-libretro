#include <libretro.h>
#include <stdint.h>
#include "x68k/midi.h"
#include "midiout.h"
#include "prop.h"

extern struct retro_midi_interface midi_cb;
extern retro_environment_t environ_cb;
extern int libretro_supports_midi_output;

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

static int libretro_open(void **phmo)
{
   if (libretro_supports_midi_output && midi_cb.output_enabled())
   {
      *phmo = &midi_cb;
      return 0;
   }
   return 1;
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

void midi_out_short_msg(size_t msg)
{
   libretro_send_short_msg(msg);
}

void midi_out_long_msg(uint8_t *s, size_t len)
{
   libretro_send_long_msg(s, len);
}

int midi_out_open(void **phmo)
{
   return libretro_open(phmo);
}

void midi_out_flush(void)
{
   libretro_flush();
}

void midi_interface_init(void)
{
   libretro_init();
}

/* midi core options */

void update_variable_midi_interface(int running)
{
   struct retro_variable var;

   var.key = "px68k_midi_output";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
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