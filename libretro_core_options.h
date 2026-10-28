#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifndef HAVE_NO_LANGEXTRA
#include "libretro_core_options_intl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

#define MAX_CORE_OPTIONS (sizeof(option_defs_us) / sizeof(option_defs_us[0]))

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

struct retro_core_option_v2_category option_cats_us[] = {
   { "system",    "System",   "Change emulated hardware settings."                                    },
   { "audio",     "Audio",    "Change sound volumes and midi output type."                            },
   { "input",     "Input",    "Change controller types and button mapping."                           },
   { "media",     "Media",    "Change floppy disk media swapping options."                            },
   { "advanced",  "Advanced", "Change system-related advanced options for performance or aesthetics." },
   { NULL,        NULL,       NULL },
};

struct retro_core_option_v2_definition option_defs_us[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "px68k_menufontsize",
      "Menu Font Size",
      NULL,
      "Sets the built-in menu font size",
      NULL,
      "system",
      {
         { "normal", NULL },
         { "large",  NULL },
         { NULL,     NULL },
      },
      "normal"
   },
   {
      "px68k_cpuspeed",
      "CPU Speed",
      NULL,
      "Configure the CPU speed. Can be used to slow down games that run too fast or to speed up floppy loading times.",
      NULL,
      "system",
      {
         { "10Mhz",       NULL },
         { "16Mhz",       NULL },
         { "25Mhz",       NULL },
         { "33Mhz (OC)",  NULL },
         { "66Mhz (OC)",  NULL },
         { "100Mhz (OC)", NULL },
         { NULL,          NULL },
      },
      "10Mhz"
   },
   {
      "px68k_ramsize",
      "RAM Size (Restart)",
      NULL,
      "Sets the amount of RAM to be used by the system.",
      NULL,
      "system",
      {
         { "1MB",  NULL },
         { "2MB",  NULL },
         { "3MB",  NULL },
         { "4MB",  NULL },
         { "5MB",  NULL },
         { "6MB",  NULL },
         { "7MB",  NULL },
         { "8MB",  NULL },
         { "9MB",  NULL },
         { "10MB", NULL },
         { "11MB", NULL },
         { "12MB", NULL },
         { NULL,   NULL },
      },
      "2MB"
   },
   {
      "px68k_analog",
      "Use Analog",
      NULL,
      NULL,
      NULL,
      "input",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_joytype1",
      "P1 Joypad Type",
      NULL,
      "Set the joypad type for player 1.",
      NULL,
      "input",
      {
         { "Default (2 Buttons)",  NULL },
         { "CPSF-MD (8 Buttons)",  NULL },
         { "CPSF-SFC (8 Buttons)", NULL },
         { "Cyberstick (Digital)", NULL },
         { "Cyberstick (Analog)",  NULL },
         { NULL,                   NULL },
      },
      "Default (2 Buttons)"
   },
   {
      "px68k_joytype2",
      "P2 Joypad Type",
      NULL,
      "Set the joypad type for player 2.",
      NULL,
      "input",
      {
         { "Default (2 Buttons)",  NULL },
         { "CPSF-MD (8 Buttons)",  NULL },
         { "CPSF-SFC (8 Buttons)", NULL },
         { NULL,                   NULL },
      },
      "Default (2 Buttons)"
   },
   {
      "px68k_joy1_select",
      "P1 Joystick Select Mapping",
      NULL,
      "Assigns a keyboard key to joypad's SELECT button since some games use these keys as the Start or Insert Coin buttons.",
      NULL,
      "input",
      {
         { "Default", NULL },
         { "XF1",     NULL },
         { "XF2",     NULL },
         { "XF3",     NULL },
         { "XF4",     NULL },
         { "XF5",     NULL },
         { "OPT1",    NULL },
         { "OPT2",    NULL },
         { "F1",      NULL },
         { "F2",      NULL },
         { NULL,      NULL },
      },
      "Default"
   },
   {
      "px68k_sc55_emulation",
      "Roland SC-55 Emulation",
      NULL,
      "Use the internal Roland SC-55 synthesizer instead of RetroArch MIDI output. Requires compatible SC-55 ROM files in the System Directory or PrBoom-compatible subfolders.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_sc55_volume",
      "SC-55 Emulation Volume",
      NULL,
      "Adjust the volume of the internal Roland SC-55 synthesizer relative to the PX68K audio. 100% is the default level.",
      NULL,
      "audio",
      {
         { "0", "0%" },
         { "5", "5%" },
         { "10", "10%" },
         { "15", "15%" },
         { "20", "20%" },
         { "25", "25%" },
         { "30", "30%" },
         { "35", "35%" },
         { "40", "40%" },
         { "45", "45%" },
         { "50", "50%" },
         { "55", "55%" },
         { "60", "60%" },
         { "65", "65%" },
         { "70", "70%" },
         { "75", "75%" },
         { "80", "80%" },
         { "85", "85%" },
         { "90", "90%" },
         { "95", "95%" },
         { "100", "100%" },
         { "105", "105%" },
         { "110", "110%" },
         { "115", "115%" },
         { "120", "120%" },
         { "125", "125%" },
         { "130", "130%" },
         { "135", "135%" },
         { "140", "140%" },
         { "145", "145%" },
         { "150", "150%" },
         { "155", "155%" },
         { "160", "160%" },
         { "165", "165%" },
         { "170", "170%" },
         { "175", "175%" },
         { "180", "180%" },
         { "185", "185%" },
         { "190", "190%" },
         { "195", "195%" },
         { "200", "200%" },
         { NULL, NULL },
      },
      "100"
   },
   {
      "px68k_midi_output",
      "MIDI Output (Restart)",
      NULL,
      "Enable software MIDI.",
      NULL,
      "audio",
      {
         { "disabled", NULL},
         { "enabled",  NULL},
         { NULL,       NULL },
      },
      "enabled"
   },
   {
      "px68k_midi_output_type",
      "MIDI Output Type",
      NULL,
      "Sets MIDI output type.",
      NULL,
      "audio",
      {
         { "LA",       NULL },
         { "GM",       NULL },
         { "GS",       NULL },
         { "XG",       NULL },
         { NULL,       NULL },
      },
      "GM"
   },
   {
      "px68k_adpcm_vol",
      "ADPCM Volume",
      NULL,
      "Sets the volume of the ADPCM sound channel.",
      NULL,
      "audio",
      {
         { "0",  NULL },
         { "1",  NULL },
         { "2",  NULL },
         { "3",  NULL },
         { "4",  NULL },
         { "5",  NULL },
         { "6",  NULL },
         { "7",  NULL },
         { "8",  NULL },
         { "9",  NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { NULL, NULL },
      },
      "15"
   },
   {
      "px68k_opm_vol",
      "OPM Volume",
      NULL,
      "Sets the volume of the OPM sound channel.",
      NULL,
      "audio",
      {
         { "0",  NULL },
         { "1",  NULL },
         { "2",  NULL },
         { "3",  NULL },
         { "4",  NULL },
         { "5",  NULL },
         { "6",  NULL },
         { "7",  NULL },
         { "8",  NULL },
         { "9",  NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { NULL, NULL },
      },
      "12"
   },
#ifndef NO_MERCURY
   {
      "px68k_mercury_vol",
      "Mercury Volume",
      NULL,
      "Sets the volume of the Mercury sound channel.",
      NULL,
      "audio",
      {
         { "0",  NULL },
         { "1",  NULL },
         { "2",  NULL },
         { "3",  NULL },
         { "4",  NULL },
         { "5",  NULL },
         { "6",  NULL },
         { "7",  NULL },
         { "8",  NULL },
         { "9",  NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { NULL, NULL },
      },
      "13"
   },
#endif
   {
      "px68k_archive_floppies",
      "Auto Insert Floppies from Archive",
      NULL,
      "Controls whether another floppy from a ZIP archive is inserted automatically at boot or during disk changes.",
      NULL,
      "media",
      {
         { "Off",             NULL },
         { "Always",          NULL },
         { "Boot Only",       NULL },
         { "At Disk Change",  NULL },
         { NULL,               NULL },
      },
      "Off"
   },
   {
      "px68k_disk_drive",
      "Swap Disks on Drive",
      NULL,
      "By default using the native Disk Swap interface within RetroArch's menu will swap the disk in drive FDD1. Change this option to swap disks in drive FDD0.",
      NULL,
      "media",
      {
         { "FDD1", NULL },
         { "FDD0", NULL },
         { NULL,   NULL },
      },
      "FDD1"
   },
   {
      "px68k_save_fdd_path",
      "Save FDD Paths",
      NULL,
      "When enabled, last loaded fdd path will be saved for each drive and then auto-loaded on startup. When disabled, FDDx starts empty.",
      NULL,
      "media",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL,       NULL },
      },
      "enabled"
   },

   {
      "px68k_save_hdd_path",
      "Save HDD Paths",
      NULL,
      "When enabled, last loaded hdd path will be saved for each drive and then auto-loaded on startup. When disabled, HDDx starts empty.",
      NULL,
      "media",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL,       NULL },
      },
      "enabled"
   },

   {
      "px68k_rumble_on_disk_read",
      "Rumble on FDD Reads",
      NULL,
      "Produces rumble effect on supported devices when reading from floppy disks.",
      NULL,
      "media",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL,       NULL },
      },
      "disabled"
   },

   /* from PX68K Menu */
   {
      "px68k_joy_mouse",
      "Joy/Mouse",
      NULL,
      "Select [Mouse] or [Joypad] to control in-game mouse pointer.",
      NULL,
      "input",
      {
         { "Mouse",    NULL},
         { "Joystick", NULL}, /* unimplemented yet */
         { NULL,       NULL },
      },
      "Mouse"
   },
   {
      "px68k_vbtn_swap",
      "VBtn Swap",
      NULL,
      "Swaps TRIG1 and TRIG2 buttons when a 2-button gamepad is selected.",
      NULL,
      "input",
      {
         { "TRIG1 TRIG2", NULL},
         { "TRIG2 TRIG1", NULL},
         { NULL,          NULL },
      },
      "TRIG1 TRIG2"
   },
   {
      "px68k_no_wait_mode",
      "No Wait Mode",
      NULL,
      "When set to [enabled], core runs as fast as possible. Can cause audio desync. Setting this [disabled] is recommended.",
      NULL,
      "advanced",
      {
         { "disabled", NULL},
         { "enabled",  NULL},
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_frameskip",
      "Frames Skip",
      NULL,
      "Choose how many frames will be skipped to improve performance at the expense of visual smoothness.",
      NULL,
      "advanced",
      {
         { "Full Frame",      NULL },
         { "1/2 Frame",       NULL },
         { "1/3 Frame",       NULL },
         { "1/4 Frame",       NULL },
         { "1/5 Frame",       NULL },
         { "1/6 Frame",       NULL },
         { "1/8 Frame",       NULL },
         { "1/16 Frame",      NULL },
         { "1/32 Frame",      NULL },
         { "1/60 Frame",      NULL },
         { "Auto Frame Skip", NULL },
         { NULL,   NULL },
      },
      "Full Frame"
   },
   {
      "px68k_push_video_before_audio",
      "Push Video before Audio",
      NULL,
      "Prioritize reducing video latency over audio latency and/or stuttering.",
      NULL,
      "advanced",
      {
         { "disabled", NULL},
         { "enabled",  NULL},
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_adjust_frame_rates",
      "Adjust Frame Rates",
      NULL,
      "For compatibility with modern displays, slightly adjust frame rates reported to frontend in order to reduce the chances of audio stuttering.  Disable to use actual frame rates.",
      NULL,
      "advanced",
      {
         { "disabled", NULL},
         { "enabled",  NULL},
         { NULL,       NULL },
      },
      "enabled"
   },
   {
      "px68k_audio_desync_hack",
      "Audio Desync Hack",
      NULL,
      "Prevents audio from desynchronizing by simply discarding any audio samples generated past the requested amount per frame slice.  Forces 'No Wait Mode' to [enabled], use appropriate frontend settings to properly throttle content.",
      NULL,
      "advanced",
      {
         { "disabled", NULL},
         { "enabled",  NULL},
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_text_off",
      "Text Off",
      NULL,
      "TODO:",
      NULL,
      "advanced",
      {
         { "disabled", NULL},
         { "enabled",  NULL},
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_grp_off",
      "Grp Off",
      NULL,
      "TODO:",
      NULL,
      "advanced",
      {
         { "disabled", NULL},
         { "enabled",  NULL},
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_sp_off",
      "SP/BG Off",
      NULL,
      "TODO:",
      NULL,
      "advanced",
      {
         { "disabled", NULL},
         { "enabled",  NULL},
         { NULL,       NULL },
      },
      "disabled"
   },

   { NULL, NULL, NULL, NULL, NULL, NULL, { 0, 0 }, NULL },
};

struct retro_core_option_v2_definition option_defs_us[MAX_CORE_OPTIONS];

struct retro_core_options_v2 options_us = {
   option_cats_us,
   option_defs_us
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

#ifndef HAVE_NO_LANGEXTRA
struct retro_core_options_v2 *options_intl[RETRO_LANGUAGE_LAST] = {
   &options_us,      /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   &options_fr,      /* RETRO_LANGUAGE_FRENCH */
   NULL,           /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   NULL,           /* RETRO_LANGUAGE_ITALIAN */
   NULL,           /* RETRO_LANGUAGE_DUTCH */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,           /* RETRO_LANGUAGE_RUSSIAN */
   NULL,           /* RETRO_LANGUAGE_KOREAN */
   NULL,           /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,           /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,           /* RETRO_LANGUAGE_ESPERANTO */
   NULL,           /* RETRO_LANGUAGE_POLISH */
   NULL,           /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,           /* RETRO_LANGUAGE_ARABIC */
   NULL,           /* RETRO_LANGUAGE_GREEK */
   NULL,           /* RETRO_LANGUAGE_TURKISH */
   NULL,           /* RETRO_LANGUAGE_SLOVAK */
   NULL,           /* RETRO_LANGUAGE_PERSIAN */
   NULL,           /* RETRO_LANGUAGE_HEBREW */
   NULL,           /* RETRO_LANGUAGE_ASTURIAN */
   NULL,           /* RETRO_LANGUAGE_FINNISH */
};
#endif

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should only be called inside retro_set_environment().
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb,
      bool *categories_supported)
{
   unsigned version  = 0;
#ifndef HAVE_NO_LANGEXTRA
   unsigned language = 0;
#endif

   if (!environ_cb || !categories_supported)
      return;

   *categories_supported = false;

   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))
      version = 0;

   if (version >= 2)
   {
#ifndef HAVE_NO_LANGEXTRA
      struct retro_core_options_v2_intl core_options_intl;

      core_options_intl.us    = &options_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = options_intl[language];

      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL,
            &core_options_intl);
#else
      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
            &options_us);
#endif
   }
   else
   {
      size_t i, j;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_core_option_definition
            *option_v1_defs_us         = NULL;
#ifndef HAVE_NO_LANGEXTRA
      size_t num_options_intl          = 0;
      struct retro_core_option_v2_definition
            *option_defs_intl          = NULL;
      struct retro_core_option_definition
            *option_v1_defs_intl       = NULL;
      struct retro_core_options_intl
            core_options_v1_intl;
#endif
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine total number of options */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      if (version >= 1)
      {
         /* Allocate US array */
         option_v1_defs_us = (struct retro_core_option_definition *)
               calloc(num_options + 1, sizeof(struct retro_core_option_definition));

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            struct retro_core_option_v2_definition *option_def_us = &option_defs_us[i];
            struct retro_core_option_value *option_values         = option_def_us->values;
            struct retro_core_option_definition *option_v1_def_us = &option_v1_defs_us[i];
            struct retro_core_option_value *option_v1_values      = option_v1_def_us->values;

            option_v1_def_us->key           = option_def_us->key;
            option_v1_def_us->desc          = option_def_us->desc;
            option_v1_def_us->info          = option_def_us->info;
            option_v1_def_us->default_value = option_def_us->default_value;

            /* Values must be copied individually... */
            while (option_values->value)
            {
               option_v1_values->value = option_values->value;
               option_v1_values->label = option_values->label;

               option_values++;
               option_v1_values++;
            }
         }

#ifndef HAVE_NO_LANGEXTRA
         if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
             (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH) &&
             options_intl[language])
            option_defs_intl = options_intl[language]->definitions;

         if (option_defs_intl)
         {
            /* Determine number of intl options */
            while (true)
            {
               if (option_defs_intl[num_options_intl].key)
                  num_options_intl++;
               else
                  break;
            }

            /* Allocate intl array */
            option_v1_defs_intl = (struct retro_core_option_definition *)
                  calloc(num_options_intl + 1, sizeof(struct retro_core_option_definition));

            /* Copy parameters from option_defs_intl array */
            for (i = 0; i < num_options_intl; i++)
            {
               struct retro_core_option_v2_definition *option_def_intl = &option_defs_intl[i];
               struct retro_core_option_value *option_values           = option_def_intl->values;
               struct retro_core_option_definition *option_v1_def_intl = &option_v1_defs_intl[i];
               struct retro_core_option_value *option_v1_values        = option_v1_def_intl->values;

               option_v1_def_intl->key           = option_def_intl->key;
               option_v1_def_intl->desc          = option_def_intl->desc;
               option_v1_def_intl->info          = option_def_intl->info;
               option_v1_def_intl->default_value = option_def_intl->default_value;

               /* Values must be copied individually... */
               while (option_values->value)
               {
                  option_v1_values->value = option_values->value;
                  option_v1_values->label = option_values->label;

                  option_values++;
                  option_v1_values++;
               }
            }
         }

         core_options_v1_intl.us    = option_v1_defs_us;
         core_options_v1_intl.local = option_v1_defs_intl;

         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_v1_intl);
#else
         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, option_v1_defs_us);
#endif
      }
      else
      {
         /* Allocate arrays */
         variables  = (struct retro_variable *)calloc(num_options + 1,
               sizeof(struct retro_variable));
         values_buf = (char **)calloc(num_options, sizeof(char *));

         if (!variables || !values_buf)
            goto error;

         /* Copy parameters from option_defs_us array */
         for (i = 0; i < num_options; i++)
         {
            const char *key                        = option_defs_us[i].key;
            const char *desc                       = option_defs_us[i].desc;
            const char *default_value              = option_defs_us[i].default_value;
            struct retro_core_option_value *values = option_defs_us[i].values;
            size_t buf_len                         = 3;
            size_t default_index                   = 0;

            values_buf[i] = NULL;

            /* Skip options that are irrelevant when using the
             * old style core options interface */
            if ((strcmp(key, "fceumm_show_adv_system_options") == 0) ||
                (strcmp(key, "fceumm_advance_sound_options") == 0))
               continue;

            if (desc)
            {
               size_t num_values = 0;

               /* Determine number of values */
               while (true)
               {
                  if (values[num_values].value)
                  {
                     /* Check if this is the default value */
                     if (default_value)
                        if (strcmp(values[num_values].value, default_value) == 0)
                           default_index = num_values;

                     buf_len += strlen(values[num_values].value);
                     num_values++;
                  }
                  else
                     break;
               }

               /* Build values string */
               if (num_values > 0)
               {
                  buf_len += num_values - 1;
                  buf_len += strlen(desc);

                  values_buf[i] = (char *)calloc(buf_len, sizeof(char));
                  if (!values_buf[i])
                     goto error;

                  strcpy(values_buf[i], desc);
                  strcat(values_buf[i], "; ");

                  /* Default value goes first */
                  strcat(values_buf[i], values[default_index].value);

                  /* Add remaining values */
                  for (j = 0; j < num_values; j++)
                  {
                     if (j != default_index)
                     {
                        strcat(values_buf[i], "|");
                        strcat(values_buf[i], values[j].value);
                     }
                  }
               }
            }

            variables[option_index].key   = key;
            variables[option_index].value = values_buf[i];
            option_index++;
         }

         /* Set variables */
         environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
      }

error:
      /* Clean up */

      if (option_v1_defs_us)
      {
         free(option_v1_defs_us);
         option_v1_defs_us = NULL;
      }

#ifndef HAVE_NO_LANGEXTRA
      if (option_v1_defs_intl)
      {
         free(option_v1_defs_intl);
         option_v1_defs_intl = NULL;
      }
#endif

      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
