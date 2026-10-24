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
      "audio",
      NULL,
      NULL,
   },
   {
      "input",
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
      "px68k_analog",
      "Utiliser l'analogique",
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
      "Type de manette du joueur 1",
      NULL,
      "Définit le type de manette du joueur 1.",
      NULL,
      "input",
      {
         { "Default (2 Buttons)",  "Défaut (2 Boutons)" },
         { "CPSF-MD (8 Buttons)",  "CPSF-MD (8 Boutons)" },
         { "CPSF-SFC (8 Buttons)", "CPSF-SFC (8 Boutons)" },
         { "Cyberstick (Digital)", NULL },
         { "Cyberstick (Analog)",  NULL },
         { NULL,                   NULL },
      },
      "Default (2 Buttons)"
   },
   {
      "px68k_joytype2",
      "Type de manette du joueur 2",
      NULL,
      "Définit le type de manette du joueur 2.",
      NULL,
      "input",
      {
         { "Default (2 Buttons)",  "Défaut (2 Boutons)" },
         { "CPSF-MD (8 Buttons)",  "CPSF-MD (8 Boutons)" },
         { "CPSF-SFC (8 Buttons)", "CPSF-SFC (8 Boutons)" },
         { NULL,                   NULL },
      },
      "Default (2 Buttons)"
   },
   {
      "px68k_joy1_select",
      "Mappage de la manette du joueur 1",
      NULL,
      "Attribue une touche du clavier au bouton SELECT de la manette, car certains jeux utilisent ces touches comme bouton Démarrer ou Insérer une pièce.",
      NULL,
      "input",
      {
         { "Default", "Défaut" },
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
      "Insertion automatique des disquettes d'archive",
      NULL,
      "Contrôle l'insertion automatique d'une autre disquette d'une archive ZIP au démarrage ou lors d'un changement de disque.",
      NULL,
      "media",
      {
         { "Off",             NULL },
         { "Always",          NULL },
         { "Boot Only",       NULL },
         { "At Disk Change",  NULL },
         { NULL,               NULL },
      },
      "Always"
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
      "Manette / souris",
      NULL,
      "Sélectionner la [souris] ou la [manette] pour contrôler le pointeur de souris dans les jeux.",
      NULL,
      "input",
      {
         { "Mouse",    "Souris" },
         { "Joystick", "Manette" }, /* unimplemented yet */
         { NULL,       NULL },
      },
      "Mouse"
   },
   {
      "px68k_vbtn_swap",
      "Echange des boutons",
      NULL,
      "Echange le BOUTON1 et le BOUTON2 quand une manette 2 boutons est sélectionné.",
      NULL,
      "input",
      {
         { "TRIG1 TRIG2", "BOUTON1 BOUTON2" },
         { "TRIG2 TRIG1", "BOUTON2 BOUTON1" },
         { NULL,          NULL },
      },
      "TRIG1 TRIG2"
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
