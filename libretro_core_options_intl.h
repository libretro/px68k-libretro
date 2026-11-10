#ifndef LIBRETRO_CORE_OPTIONS_INTL_H__
#define LIBRETRO_CORE_OPTIONS_INTL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#pragma warning(disable:4566)
#endif

#include <libretro.h>

/*
 ********************************
 * VERSION: 2.0
 ********************************
 *
 * - 2.0: Add support for core options v2 interface
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* needs to be translated */
struct retro_core_option_v2_category option_cats_fr[] = {
   {
      "system",
      NULL,
      NULL,
   },
   {
      "video",
      NULL,
      NULL,
   },
   {
      "input",
      NULL,
      NULL,
   },
   {
      "p1_joy2key",
      NULL,
      NULL,
   },
   {
      "p2_joy2key",
      NULL,
      NULL,
   },
   {
      "audio",
      NULL,
      NULL,
   },
   {
      "media",
      NULL,
      NULL,
   },
   {
      "advanced",
      NULL,
      NULL,
   },

   { NULL, NULL, NULL },
};

struct retro_core_option_v2_definition option_defs_fr[] = {
   {
      "px68k_menufontsize",
      "Taille de la police du menu",
      NULL,
      NULL,
      NULL,
      "system",
      {
         { "normal", "Normale" },
         { "large",  "Grande" },
         { NULL,     NULL },
      },
      "normal"
   },
   {
      "px68k_cpu_emulation",
      "CPU Emulation",
      NULL,
      "Select the 68000 CPU emulator. Restart the core after changing this option.",
      NULL,
      "system",
      {
         { "C68K (Fast)", NULL },
         { "Musashi (Slower)", NULL },
         { NULL, NULL },
      },
      "C68K (Fast)"
   },
   {
      "px68k_cpuspeed",
      "Vitesse du CPU",
      NULL,
      "Configurez la vitesse du processeur. Peut être utilisé pour ralentir les jeux trop rapides ou pour accélérer les temps de chargement des disquettes.",
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
      "Taille de la RAM (Redémarrage requis)",
      NULL,
      "Définit la quantité de RAM à utiliser par le système.",
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
      "px68k_j2k_p1_up",
      "P1 Joy2Key UP",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "up"
   },
   {
      "px68k_j2k_p1_down",
      "P1 Joy2Key Down",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "down"
   },
   {
      "px68k_j2k_p1_left",
      "P1 Joy2Key Left",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "left"
   },
   {
      "px68k_j2k_p1_right",
      "P1 Joy2Key Right",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "right"
   },
   {
      "px68k_j2k_p1_select",
      "P1 Joy2Key Select",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "esc"
   },
   {
      "px68k_j2k_p1_start",
      "P1 Joy2Key Start",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "return"
   },
   {
      "px68k_j2k_p1_a",
      "P1 Joy2Key A",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p1_b",
      "P1 Joy2Key B",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p1_x",
      "P1 Joy2Key X",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p1_y",
      "P1 Joy2Key Y",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p1_l",
      "P1 Joy2Key L",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "opt1"
   },
   {
      "px68k_j2k_p1_r",
      "P1 Joy2Key R",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "opt2"
   },
   {
      "px68k_j2k_p1_l2",
      "P1 Joy2Key L2",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "menu"
   },
   {
      "px68k_j2k_p1_r2",
      "P1 Joy2Key R2",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "space"
   },
   {
      "px68k_j2k_p1_l3",
      "P1 Joy2Key L3",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "mouse_btn1"
   },
   {
      "px68k_j2k_p1_r3",
      "P1 Joy2Key R3",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "mouse_btn2"
   },
   {
      "px68k_j2k_p2_up",
      "P2 Joy2Key UP",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "up"
   },
   {
      "px68k_j2k_p2_down",
      "P2 Joy2Key Down",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "down"
   },
   {
      "px68k_j2k_p2_left",
      "P2 Joy2Key Left",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "left"
   },
   {
      "px68k_j2k_p2_right",
      "P2 Joy2Key Right",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "right"
   },
   {
      "px68k_j2k_p2_select",
      "P2 Joy2Key Select",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "esc"
   },
   {
      "px68k_j2k_p2_start",
      "P2 Joy2Key Start",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "return"
   },
   {
      "px68k_j2k_p2_a",
      "P2 Joy2Key A",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p2_b",
      "P2 Joy2Key B",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p2_x",
      "P2 Joy2Key X",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "x"
   },
   {
      "px68k_j2k_p2_y",
      "P2 Joy2Key Y",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "z"
   },
   {
      "px68k_j2k_p2_l",
      "P2 Joy2Key L",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "opt1"
   },
   {
      "px68k_j2k_p2_r",
      "P2 Joy2Key R",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "opt2"
   },
   {
      "px68k_j2k_p2_l2",
      "P2 Joy2Key L2",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "menu"
   },
   {
      "px68k_j2k_p2_r2",
      "P2 Joy2Key R2",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "space"
   },
   {
      "px68k_j2k_p2_l3",
      "P2 Joy2Key L3",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "touroku"
   },
   {
      "px68k_j2k_p2_r3",
      "P2 Joy2Key R3",
      NULL,
      "Select the X68000 keyboard key emitted when this logical Joy2Key action is activated in RetroArch Controls.",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "shift"
   },
   {
      "px68k_midi_emulation",
      "Internal MIDI Synthesizer",
      NULL,
      "Select the internal MIDI synthesizer instead of RetroArch MIDI output. SC-55 uses Nuked-SC55 ROMs; MT-32, CM-32L and CM-64 use compatible Munt ROMs in the System Directory or recognized subfolders.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "sc55",     "Roland SC-55" },
         { "mt32",     "Roland MT-32" },
         { "cm32l",    "Roland CM-32L" },
         { "cm64",     "Roland CM-64 (CM-32L sound module)" },
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_midiemu_volume",
      "MIDI Emulation Volume",
      NULL,
      "Adjust the volume of the active internal MIDI synthesizer relative to PX68K audio. 100% is the default level.",
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
      NULL,
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
      "MIDI Output Type (Restart)",
      NULL,
      NULL,
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
      "px68k_adpcm_lpf",
      "Filtre passe-bas ADPCM",
      NULL,
      "Active le filtre passe-bas ADPCM original. Le changement est immédiat, sans redémarrage du cœur.",
      NULL,
      "audio",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_adpcm_vol",
      "Volume ADPCM",
      NULL,
      "Règlage du volume du canal audio ADPCM.",
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
      "Volume OPM",
      NULL,
      "Règlage du volume du canal audio OPM.",
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
      "Volume Mercury",
      NULL,
      "Règlage du volume du canal sonore Mercury.",
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
      "Insertion automatique des disquettes archivées",
      NULL,
      "Contrôle l'insertion automatique d'une autre disquette d'une même archive ZIP au démarrage ou lors d'un changement de disquette.",
      NULL,
      "media",
      {
         { "Off",             "Désactivé" },
         { "Always",          "Toujours" },
         { "Boot Only",       "Démarrage uniquement" },
         { "At Disk Change",  "Au changement de disquette" },
         { NULL,               NULL },
      },
      "Off"
   },
   {
      "px68k_disk_drive",
      "Échange de disques sur le lecteur",
      NULL,
      "Par défaut, l'interface native de RetroArch, d'échange de disque dans le menu, échange le disque dans le lecteur FDD1. Modifiez cette option pour échanger des disques dans le lecteur FDD0.",
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
      "Enregistrer les chemins d'accès aux disquettes",
      NULL,
      "Lorsqu'elle est activée, les chemins d'accès aux disquettes précédemment chargés seront enregistrés pour chaque lecteur, puis chargés automatiquement au démarrage. Lorsqu'elle est désactivé, FDDx démarre à vide.",
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
      "Enregistrer les chemins d'accès aux disques durs",
      NULL,   
      "Lorsqu'elle est activée, les chemins d'accès aux disques durs précédemment chargés seront enregistrés pour chaque disque dur puis chargés automatiquement au démarrage. Lorsqu'elle est désactivée, HDDx démarre à vide.",
      NULL,
      "media",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL,       NULL }
      },
      "enabled"
   },
   {
      "px68k_rumble_on_disk_read",
      "Faire vibrer la manette pendant la lecture des disquettes",
      NULL,
      "Produit un effet de vibration par les manettes supportées pendant la lecture des disquettes.",
      NULL,
      "media",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL,       NULL }
      },
      "disabled"
   },

   /* from PX68K Menu */
   {
      "px68k_joy_mouse",
      "Joy2Mouse",
      NULL,
      "Ajoute le déplacement de la souris avec la croix directionnelle et le stick analogique gauche du joueur 1. Attribuez Mouse Btn1 et Mouse Btn2 aux commandes P1 Joy2Key pour utiliser les clics.",
      NULL,
      "input",
      {
         { "disabled", "désactivé" },
         { "enabled",  "activé" },
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_joy2mouse_analog_speed",
      "Joy2Mouse Analog speed",
      NULL,
      "Vitesse maximale du déplacement de la souris avec le stick analogique gauche, en unités souris par image.",
      NULL,
      "input",
      {
         { "1", NULL }, { "2", NULL }, { "4", NULL }, { "6", NULL },
         { "8", NULL }, { "10", NULL }, { "12", NULL }, { "16", NULL },
         { "20", NULL }, { "24", NULL }, { NULL, NULL },
      },
      "12"
   },
   {
      "px68k_joy2mouse_analog_deadzone",
      "Joy2Mouse Analog deadzone",
      NULL,
      "Zone morte du stick analogique en pourcentage. Augmentez cette valeur si le pointeur dérive lorsque le stick est relâché.",
      NULL,
      "input",
      {
         { "0", NULL }, { "5", NULL }, { "10", NULL }, { "12.5", NULL },
         { "15", NULL }, { "20", NULL }, { "25", NULL }, { "30", NULL },
         { "40", NULL }, { "50", NULL }, { NULL, NULL },
      },
      "12.5"
   },
   {
      "px68k_joy2mouse_digital_speed",
      "Joy2Mouse Digital speed",
      NULL,
      "Vitesse de déplacement de la souris avec la croix directionnelle, en unités souris par image.",
      NULL,
      "input",
      {
         { "1", NULL }, { "2", NULL }, { "3", NULL }, { "4", NULL },
         { "5", NULL }, { "6", NULL }, { "8", NULL }, { "10", NULL },
         { "12", NULL }, { NULL, NULL },
      },
      "6"
   },
   {
      "px68k_no_wait_mode",
      "Mode sans attente",
      NULL,
      "Lorsque ce mode est [activé], le cœur s'exécute aussi vite que possible. Cela peut provoquer une désynchronisation audio mais permet une avance rapide. Il est recommandé de définir ce paramètre à [désactivé].",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_frameskip",
      "Saut d'images",
      NULL,
      "Choisissez le nombre d'images à ignorer pour améliorer les performances au détriment de la fluidité visuelle.",
      NULL,
      "advanced",
      {
         { "Full Frame",      "Toutes les images" },
         { "1/2 Frame",       "1/2 image" },
         { "1/3 Frame",       "1/3 image" },
         { "1/4 Frame",       "1/4 image" },
         { "1/5 Frame",       "1/5 image" },
         { "1/6 Frame",       "1/6 image" },
         { "1/8 Frame",       "1/8 image" },
         { "1/16 Frame",      "1/16 image" },
         { "1/32 Frame",      "1/32 image" },
         { "1/60 Frame",      "1/60 image" },
         { "Auto Frame Skip", "Saut d'image automatique" },
         { NULL,              NULL },
      },
      "Full Frame"
   },
   {
      "px68k_push_video_before_audio",
      "Pousser la vidéo avant l'audio",
      NULL,
      "Privilégie la réduction de la latence vidéo à la latence audio.",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL,       NULL },
      },
      "disabled"
   },
   {
      "px68k_adjust_frame_rates",
      "Ajuster les fréquences d'images",
      NULL,
      "Pour la compatibilité avec les écrans modernes, ajuste légèrement les fréquences d'images signalées à l'interface afin de réduire les risques de latence audio. Désactivez pour utiliser les fréquences d'images actuelles.",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL,       NULL },
      },
      "enabled"
   },
   {
      "px68k_audio_desync_hack",
      "Hack de désynchronisation de l'audio",
      NULL,
      "Empêche la désynchronisation de l'audio en rejetant simplement tous les échantillons audio générés au-delà de la quantité demandée par tranche d'image. Forcez l'option 'Mode sans attente' sur [Activé], utilisez les options appropriées pour réguler correctement le contenu.",
      NULL,
      "advanced",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
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

struct retro_core_options_v2 options_fr = {
   option_cats_fr,
   option_defs_fr
};

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

/* RETRO_LANGUAGE_SLOVAK */

/* RETRO_LANGUAGE_PERSIAN */

/* RETRO_LANGUAGE_HEBREW */

/* RETRO_LANGUAGE_ASTURIAN */

/* RETRO_LANGUAGE_FINNISH */

#ifdef __cplusplus
}
#endif

#endif
