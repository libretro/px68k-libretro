#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

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
   { "system",      "System",      "Change emulated hardware settings."                              },
   { "video",       "Video",       "Change video output and aspect ratio settings."                  },
   { "input",       "Input",       "Change controller types and button mapping."                     },
   { "p1_joy2key",  "P1 Joy2Key",  "Map Player 1 RetroPad inputs to X68000 keyboard keys."            },
   { "p2_joy2key",  "P2 Joy2Key",  "Map Player 2 RetroPad inputs to X68000 keyboard keys."            },
   { "audio",       "Audio",       "Change sound volumes and midi output type."                       },
   { "media",       "Media",       "Change floppy disk media swapping options."                        },
   { "menu",      "Menu",     "Configure the built-in menu and navigation."                           },
   { "advanced",  "Advanced", "Change system-related advanced options for performance or aesthetics." },
   { NULL,        NULL,       NULL },
};

#define PX68K_JOY2KEY_VALUES \
   { "none", "None" }, \
   { "menu", "PX68K Menu" }, \
   { "osd_keyboard", "OSD Keyboard" }, \
   { "esc", "Escape" }, \
   { "1", "1" }, \
   { "2", "2" }, \
   { "3", "3" }, \
   { "4", "4" }, \
   { "5", "5" }, \
   { "6", "6" }, \
   { "7", "7" }, \
   { "8", "8" }, \
   { "9", "9" }, \
   { "0", "0" }, \
   { "minus", "-" }, \
   { "equals", "^" }, \
   { "yen", "Yen" }, \
   { "backspace", "Backspace" }, \
   { "tab", "Tab" }, \
   { "q", "Q" }, \
   { "w", "W" }, \
   { "e", "E" }, \
   { "r", "R" }, \
   { "t", "T" }, \
   { "y", "Y" }, \
   { "u", "U" }, \
   { "i", "I" }, \
   { "o", "O" }, \
   { "p", "P" }, \
   { "at", "@" }, \
   { "lbracket", "[" }, \
   { "return", "Return" }, \
   { "a", "A" }, \
   { "s", "S" }, \
   { "d", "D" }, \
   { "f", "F" }, \
   { "g", "G" }, \
   { "h", "H" }, \
   { "j", "J" }, \
   { "k", "K" }, \
   { "l", "L" }, \
   { "semicolon", ";" }, \
   { "colon", ":" }, \
   { "rbracket", "]" }, \
   { "z", "Z" }, \
   { "x", "X" }, \
   { "c", "C" }, \
   { "v", "V" }, \
   { "b", "B" }, \
   { "n", "N" }, \
   { "m", "M" }, \
   { "comma", "," }, \
   { "period", "." }, \
   { "slash", "/" }, \
   { "underquote", "Underscore" }, \
   { "space", "Space" }, \
   { "home", "Home" }, \
   { "delete", "Delete" }, \
   { "roll_up", "Roll Up" }, \
   { "roll_down", "Roll Down" }, \
   { "undo", "Undo" }, \
   { "left", "Cursor Left" }, \
   { "up", "Cursor Up" }, \
   { "right", "Cursor Right" }, \
   { "down", "Cursor Down" }, \
   { "clear", "Clear" }, \
   { "kp_divide", "Keypad /" }, \
   { "kp_multiply", "Keypad *" }, \
   { "kp_minus", "Keypad -" }, \
   { "kp7", "Keypad 7" }, \
   { "kp8", "Keypad 8" }, \
   { "kp9", "Keypad 9" }, \
   { "kp_plus", "Keypad +" }, \
   { "kp4", "Keypad 4" }, \
   { "kp5", "Keypad 5" }, \
   { "kp6", "Keypad 6" }, \
   { "kp_equals", "Keypad =" }, \
   { "kp1", "Keypad 1" }, \
   { "kp2", "Keypad 2" }, \
   { "kp3", "Keypad 3" }, \
   { "kp_enter", "Keypad Enter" }, \
   { "kp0", "Keypad 0" }, \
   { "kp_comma", "Keypad ," }, \
   { "kp_period", "Keypad ." }, \
   { "symbol", "Symbol" }, \
   { "touroku", "Touroku" }, \
   { "help", "Help" }, \
   { "xf1", "XF1" }, \
   { "xf2", "XF2" }, \
   { "xf3", "XF3" }, \
   { "xf4", "XF4" }, \
   { "xf5", "XF5" }, \
   { "kana", "Kana" }, \
   { "roman", "Roman" }, \
   { "enter_code", "Enter Code" }, \
   { "capslock", "Caps Lock" }, \
   { "insert", "Insert" }, \
   { "hiragana", "Hiragana" }, \
   { "fullwidth", "Full-width" }, \
   { "break", "Break" }, \
   { "copy", "Copy" }, \
   { "f1", "F1" }, \
   { "f2", "F2" }, \
   { "f3", "F3" }, \
   { "f4", "F4" }, \
   { "f5", "F5" }, \
   { "f6", "F6" }, \
   { "f7", "F7" }, \
   { "f8", "F8" }, \
   { "f9", "F9" }, \
   { "f10", "F10" }, \
   { "shift", "Shift" }, \
   { "ctrl", "Ctrl" }, \
   { "opt1", "OPT.1" }, \
   { "opt2", "OPT.2" }, \
   { NULL, NULL }

#define PX68K_P1_JOY2KEY_VALUES \
   { "none", "None" }, \
   { "menu", "PX68K Menu" }, \
   { "osd_keyboard", "OSD Keyboard" }, \
   { "mouse_btn1", "Mouse Btn1" }, \
   { "mouse_btn2", "Mouse Btn2" }, \
   { "esc", "Escape" }, \
   { "1", "1" }, \
   { "2", "2" }, \
   { "3", "3" }, \
   { "4", "4" }, \
   { "5", "5" }, \
   { "6", "6" }, \
   { "7", "7" }, \
   { "8", "8" }, \
   { "9", "9" }, \
   { "0", "0" }, \
   { "minus", "-" }, \
   { "equals", "^" }, \
   { "yen", "Yen" }, \
   { "backspace", "Backspace" }, \
   { "tab", "Tab" }, \
   { "q", "Q" }, \
   { "w", "W" }, \
   { "e", "E" }, \
   { "r", "R" }, \
   { "t", "T" }, \
   { "y", "Y" }, \
   { "u", "U" }, \
   { "i", "I" }, \
   { "o", "O" }, \
   { "p", "P" }, \
   { "at", "@" }, \
   { "lbracket", "[" }, \
   { "return", "Return" }, \
   { "a", "A" }, \
   { "s", "S" }, \
   { "d", "D" }, \
   { "f", "F" }, \
   { "g", "G" }, \
   { "h", "H" }, \
   { "j", "J" }, \
   { "k", "K" }, \
   { "l", "L" }, \
   { "semicolon", ";" }, \
   { "colon", ":" }, \
   { "rbracket", "]" }, \
   { "z", "Z" }, \
   { "x", "X" }, \
   { "c", "C" }, \
   { "v", "V" }, \
   { "b", "B" }, \
   { "n", "N" }, \
   { "m", "M" }, \
   { "comma", "," }, \
   { "period", "." }, \
   { "slash", "/" }, \
   { "underquote", "Underscore" }, \
   { "space", "Space" }, \
   { "home", "Home" }, \
   { "delete", "Delete" }, \
   { "roll_up", "Roll Up" }, \
   { "roll_down", "Roll Down" }, \
   { "undo", "Undo" }, \
   { "left", "Cursor Left" }, \
   { "up", "Cursor Up" }, \
   { "right", "Cursor Right" }, \
   { "down", "Cursor Down" }, \
   { "clear", "Clear" }, \
   { "kp_divide", "Keypad /" }, \
   { "kp_multiply", "Keypad *" }, \
   { "kp_minus", "Keypad -" }, \
   { "kp7", "Keypad 7" }, \
   { "kp8", "Keypad 8" }, \
   { "kp9", "Keypad 9" }, \
   { "kp_plus", "Keypad +" }, \
   { "kp4", "Keypad 4" }, \
   { "kp5", "Keypad 5" }, \
   { "kp6", "Keypad 6" }, \
   { "kp_equals", "Keypad =" }, \
   { "kp1", "Keypad 1" }, \
   { "kp2", "Keypad 2" }, \
   { "kp3", "Keypad 3" }, \
   { "kp_enter", "Keypad Enter" }, \
   { "kp0", "Keypad 0" }, \
   { "kp_comma", "Keypad ," }, \
   { "kp_period", "Keypad ." }, \
   { "symbol", "Symbol" }, \
   { "touroku", "Touroku" }, \
   { "help", "Help" }, \
   { "xf1", "XF1" }, \
   { "xf2", "XF2" }, \
   { "xf3", "XF3" }, \
   { "xf4", "XF4" }, \
   { "xf5", "XF5" }, \
   { "kana", "Kana" }, \
   { "roman", "Roman" }, \
   { "enter_code", "Enter Code" }, \
   { "capslock", "Caps Lock" }, \
   { "insert", "Insert" }, \
   { "hiragana", "Hiragana" }, \
   { "fullwidth", "Full-width" }, \
   { "break", "Break" }, \
   { "copy", "Copy" }, \
   { "f1", "F1" }, \
   { "f2", "F2" }, \
   { "f3", "F3" }, \
   { "f4", "F4" }, \
   { "f5", "F5" }, \
   { "f6", "F6" }, \
   { "f7", "F7" }, \
   { "f8", "F8" }, \
   { "f9", "F9" }, \
   { "f10", "F10" }, \
   { "shift", "Shift" }, \
   { "ctrl", "Ctrl" }, \
   { "opt1", "OPT.1" }, \
   { "opt2", "OPT.2" }, \
   { NULL, NULL }

#ifndef HAVE_NO_LANGEXTRA
#include "libretro_core_options_intl.h"
#endif

struct retro_core_option_v2_definition option_defs_us[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */


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
      "px68k_bg_wait",
      "BG/Sprite RAM Wait States",
      NULL,
      "Emulate the slow CPU access to the sprite RAM without changing the CPU speed. Sprite table changes made in the middle of the screen (raster sprite multiplexing) are shown a few lines later, as one block, like on real hardware. Fixes sprites that are cut off in Puyo Puyo.",
      NULL,
      "system",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL,       NULL },
      },
      "enabled"
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
      "px68k_aspect_ratio",
      "Aspect Ratio",
      NULL,
      "Aspect ratio of the displayed picture. '4:3' stretches every picture to fill a 4:3 screen. 'X68000' reproduces what a real X68000 and its 4:3 monitor show: the size of the picture is computed from the CRTC and dot clock timing, so modes that use only part of the scan line or of the screen height (e.g. Cotton 512*256 Hi uses 75% of the width) get black borders. '1:1' shows square pixels at the core's raw resolution.",
      NULL,
      "video",
      {
         { "4:3",    "4:3 (fill screen)" },
         { "x68000", "X68000 (CRT timing)" },
         { "1:1",    "1:1 (square pixels)" },
         { NULL,     NULL },
      },
      "4:3"
   },
   {
      "px68k_adjust_frame_rates",
      "Adjust Frame Rates",
      NULL,
      "For compatibility with modern displays, changes the 15KHz mode from 61.46 to 60Hz. Disable to get the real machine timing.",
      NULL,
      "video",
      {
         { "disabled", NULL},
         { "enabled",  NULL},
         { NULL,       NULL },
      },
      "enabled"
   },
   {
      "px68k_frameskip",
      "Frames Skip",
      NULL,
      "Choose how many frames will be skipped to improve performance at the expense of visual smoothness.",
      NULL,
      "video",
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
   /* from PX68K Menu */
   {
      "px68k_joy_mouse",
      "Joy2Mouse",
      NULL,
      "Adds P1 D-pad and left analog stick mouse movement. Assign Mouse Btn1 and Mouse Btn2 to P1 Joy2Key inputs to use them as mouse clicks.",
      NULL,
      "input",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL,        NULL },
      },
      "disabled"
   },
   {
      "px68k_joy2mouse_analog_speed",
      "Joy2Mouse Analog speed",
      NULL,
      "Maximum mouse movement speed for the left analog stick, in mouse units per frame.",
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
      "Analog stick deadzone percentage. Increase this if the mouse pointer drifts when the stick is released.",
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
      "Mouse movement speed for the D-pad, in mouse units per frame.",
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
      "px68k_osd_keyboard_theme",
      "OSD Keyboard Theme",
      NULL,
      "Style and color theme for the OSD keyboard.",
      NULL,
      "input",
      {
         { "flat_light", "Flat Light" },
         { "flat_dark",  "Flat Dark" },
         { "3D_light", "3D Light" },
         { "3D_dark",  "3D Dark" },
         { NULL, NULL },
      },
      "3D_dark"
   },
   {
      "px68k_osd_keyboard_labels",
      "OSD Keyboard Special Key Labels",
      NULL,
      "Latin: the keys are labelled KAN, ROM, COD, KIG, TOR, HIR and ZEN. Japanese: they show the captions printed on a real X68000 keyboard.",
      NULL,
      "input",
      {
         { "latin",    "Latin" },
         { "japanese", "Japanese" },
         { NULL, NULL },
      },
      "latin"
   },
   {
      "px68k_osd_keyboard_layout",
      "OSD Keyboard Layout",
      NULL,
      "Choose how the game is displayed while the OSD keyboard is open.",
      NULL,
      "input",
      {
         { "game_above", "Game above keyboard" },
         { "fullscreen", "Fullscreen behind keyboard" },
         { NULL, NULL },
      },
      "fullscreen"
   },
   {
      "px68k_osd_keyboard_opacity",
      "OSD Keyboard Opacity",
      NULL,
      "Opacity of the on-screen keyboard, from 0% to 100% in 5% steps (right raises it, left lowers it). Lower values let the game show through when the layout is 'Fullscreen behind keyboard'; with 'Game above keyboard' the keyboard fades towards black. At 0% the keyboard is invisible but still works.",
      NULL,
      "input",
      {
         { "0", "0%" }, { "5", "5%" }, { "10", "10%" }, { "15", "15%" },
         { "20", "20%" }, { "25", "25%" }, { "30", "30%" }, { "35", "35%" },
         { "40", "40%" }, { "45", "45%" }, { "50", "50%" }, { "55", "55%" },
         { "60", "60%" }, { "65", "65%" }, { "70", "70%" }, { "75", "75%" },
         { "80", "80%" }, { "85", "85%" }, { "90", "90%" }, { "95", "95%" },
         { "100", "100%" },
         { NULL, NULL },
      },
      "100"
   },
   {
      "px68k_osd_keyboard_cursor_tint",
      "OSD Keyboard Active Key Tint",
      NULL,
      "Face colour of the currently selected key.",
      NULL,
      "input",
      {
         { "off", "Off" }, { "ffffff", "White" },
         { "a0a0a0", "Grey" }, { "000000", "Black" }, { "ff9999", "Red (pale)" },
         { "ff0000", "Red (vivid)" }, { "8e0b0b", "Red (deep)" }, { "ffc999", "Orange (pale)" },
         { "ff7700", "Orange (vivid)" }, { "8e480b", "Orange (deep)" }, { "fff199", "Yellow (pale)" },
         { "ffdd00", "Yellow (vivid)" }, { "8e7c0b", "Yellow (deep)" }, { "99ff99", "Green (pale)" },
         { "00ff00", "Green (vivid)" }, { "0b8e0b", "Green (deep)" }, { "99ffff", "Cyan (pale)" },
         { "00ffff", "Cyan (vivid)" }, { "0b8e8e", "Cyan (deep)" }, { "99b8ff", "Blue (pale)" },
         { "004cff", "Blue (vivid)" }, { "0b328e", "Blue (deep)" }, { "cf99ff", "Purple (pale)" },
         { "8800ff", "Purple (vivid)" }, { "510b8e", "Purple (deep)" }, { "ff99e0", "Magenta (pale)" },
         { "ff00b2", "Magenta (vivid)" }, { "8e0b67", "Magenta (deep)" },
         { NULL, NULL },
      },
      "fff199"
   },
   {
      "px68k_osd_keyboard_cursor_frame",
      "OSD Keyboard Selection Frame",
      NULL,
      "Colour of the square drawn around the selected key.",
      NULL,
      "input",
      {
         { "off", "Off" }, { "ffffff", "White" },
         { "a0a0a0", "Grey" }, { "000000", "Black" }, { "ff9999", "Red (pale)" },
         { "ff0000", "Red (vivid)" }, { "8e0b0b", "Red (deep)" }, { "ffc999", "Orange (pale)" },
         { "ff7700", "Orange (vivid)" }, { "8e480b", "Orange (deep)" }, { "fff199", "Yellow (pale)" },
         { "ffdd00", "Yellow (vivid)" }, { "8e7c0b", "Yellow (deep)" }, { "99ff99", "Green (pale)" },
         { "00ff00", "Green (vivid)" }, { "0b8e0b", "Green (deep)" }, { "99ffff", "Cyan (pale)" },
         { "00ffff", "Cyan (vivid)" }, { "0b8e8e", "Cyan (deep)" }, { "99b8ff", "Blue (pale)" },
         { "004cff", "Blue (vivid)" }, { "0b328e", "Blue (deep)" }, { "cf99ff", "Purple (pale)" },
         { "8800ff", "Purple (vivid)" }, { "510b8e", "Purple (deep)" }, { "ff99e0", "Magenta (pale)" },
         { "ff00b2", "Magenta (vivid)" }, { "8e0b67", "Magenta (deep)" },
         { NULL, NULL },
      },
      "ffffff"
   },
   {
      "px68k_osd_keyboard_hover",
      "OSD Keyboard Mouse Hover",
      NULL,
      "When enabled, the selected key follows the mouse cursor as it moves over the on-screen keyboard. The selection only changes when the mouse actually moves, so it never fights gamepad navigation.",
      NULL,
      "input",
      {
         { "enabled",  "Enabled" },
         { "disabled", "Disabled" },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "px68k_osd_keyboard_feedback",
      "OSD Keyboard Press Feedback",
      NULL,
      "Visual reaction when a key is touched, clicked or pressed from the pad. Flash: the key lights up and fades out after release, so even a quick tap is visible. Flash + key press: the key also sinks while pressed.",
      NULL,
      "input",
      {
         { "off",   "Off" },
         { "flash", "Flash" },
         { "full",  "Flash + key press" },
         { NULL, NULL },
      },
      "full"
   },
   {
      "px68k_key_repeat_delay",
      "Key Repeat Delay",
      NULL,
      "Time a key must be held before it starts repeating. 'Machine' follows whatever the X68000 software programs into the keyboard (500 ms by default).",
      NULL,
      "input",
      {
         { "machine", "Machine (X68000 setting)" },
         { "off",     "Off (no key repeat)" },
         { "200",  "200 ms" }, { "300",  "300 ms" }, { "400",  "400 ms" },
         { "500",  "500 ms" }, { "700",  "700 ms" }, { "1000", "1000 ms" },
         { NULL, NULL },
      },
      "machine"
   },
   {
      "px68k_key_repeat_interval",
      "Key Repeat Interval",
      NULL,
      "Time between repeats once a held key is repeating. 'Machine' follows the X68000 setting (110 ms by default).",
      NULL,
      "input",
      {
         { "machine", "Machine (X68000 setting)" },
         { "30",  "30 ms" },  { "50",  "50 ms" },  { "75",  "75 ms" },
         { "100", "100 ms" }, { "150", "150 ms" }, { "200", "200 ms" },
         { "300", "300 ms" },
         { NULL, NULL },
      },
      "machine"
   },

   {
      "px68k_j2k_p1_up",
      "P1 Joy2Key UP",
      NULL,
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "xf1"
   },
   {
      "px68k_j2k_p1_a",
      "P1 Joy2Key A",
      NULL,
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
      NULL,
      "p1_joy2key",
      {
         PX68K_P1_JOY2KEY_VALUES
      },
      "osd_keyboard"
   },
   {
      "px68k_j2k_p1_l3",
      "P1 Joy2Key L3",
      NULL,
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
      NULL,
      "p2_joy2key",
      {
         PX68K_JOY2KEY_VALUES
      },
      "xf1"
   },
   {
      "px68k_j2k_p2_a",
      "P2 Joy2Key A",
      NULL,
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select a keyboard Key to assign to this logical Joy2Key action, it can then be mapped in Controls (exit and reopen the menu to refresh J2K key labels there).",
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
      "Select the internal MIDI synthesizer instead of RetroArch MIDI output. SC-55 uses Nuked-SC55 ROMs; MT-32 and CM-32L use compatible Munt ROMs in the System Directory or recognized subfolders.",
      NULL,
      "audio",
      {
         { "disabled", NULL },
         { "sc55",     "Roland SC-55" },
         { "mt32",     "Roland MT-32" },
         { "cm32l",    "Roland CM-32L" },
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
      "px68k_midi_state_restore",
      "MIDI Restore On State Load",
      NULL,
      "When a state is loaded, the MIDI synthesizer still holds whatever it was last sent. Choose what to restore so the music sounds as it did when the state was saved. 'Synth Setup' re-sends the sound standard (LA/GM/GS/XG), SysEx parameters and each channel's bank, program, volume, pan, effects and pitch bend. 'Setup And Output' also switches back to the selected internal synthesizer or external MIDI output that was in use at the time, when available. Has no effect on Run-Ahead.",
      NULL,
      "audio",
      {
         { "disabled",     NULL },
         { "setup",        "Synth Setup" },
         { "setup_output", "Setup And Output" },
         { NULL,           NULL },
      },
      "setup_output"
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
      "px68k_adpcm_lpf",
      "ADPCM Low-Pass Filter",
      NULL,
      "Enable the original ADPCM low-pass filter.",
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
      "px68k_mame_names",
      "MAME Software List Names",
      NULL,
      "Show English descriptions or Japanese alt_title labels from MAME x68k_flop.xml for matching ZIP filenames in the internal explorer. Requires MAME/hash/x68k_flop.xml in the system directory.",
      NULL,
      "menu",
      {
         { "disabled", NULL },
         { "english", "English" },
         { "japanese", "Japanese" },
         { NULL, NULL },
      },
      "disabled"
   },
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

   {
      "px68k_menufontsize",
      "Menu Font Size",
      NULL,
      "Sets the built-in menu font size",
      NULL,
      "menu",
      {
         { "normal", NULL },
         { "large",  NULL },
         { NULL,     NULL },
      },
      "normal"
   },
   {
      "px68k_menu_lines_jump",
      "Lines Jump (with left-right)",
      NULL,
      "Number of entries to jump when pressing Left or Right in the built-in menu.",
      NULL,
      "menu",
      {
         { "5", NULL }, { "10", NULL }, { "15", NULL },
         { "20", NULL }, { "25", NULL }, { "30", NULL },
         { "35", NULL }, { "40", NULL }, { "45", NULL },
         { "50", NULL }, { NULL, NULL },
      },
      "10"
   },
   {
      "px68k_text_off",
      "Text Off",
      NULL,
      "Disables the text layer, used for characters, text overlays, and some interface elements.",
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
      "Disables the graphics layer, typically used for bitmap graphics and backgrounds.",
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
      "Disables the sprite and tile-based background layers, commonly used for game characters, objects, and scrolling backgrounds.",
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
