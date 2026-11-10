#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#ifndef _WIN32
#include <strings.h>
#endif

#include <libretro.h>
#include <libretro_core_options.h>
#include <string/stdstring.h>
#ifdef USE_LIBRETRO_VFS
#include <streams/file_stream_transforms.h>
#endif

#include "libretro/winx68k.h"
#include "libretro/dosio.h"
#include "libretro/zipdisk.h"
#include "midi/midiemu/midiemu_libretro.h"
#include "libretro/dswin.h"
#include "libretro/windraw.h"
#include "libretro/joystick.h"
#include "libretro/keyboard.h"
#include "libretro/prop.h"
#include "libretro/status.h"
#include "libretro/timer.h"
#include "libretro/mouse.h"
#include "libretro/winui.h"
#include "fmgen/fmg_wrap.h"
#include "m68000/m68000.h"
#include "x68k/adpcm.h"
#include "x68k/fdd.h"
#include "x68k/sram.h"
#include "x68k/sysport.h"
#include "x68k/x68kmemory.h"
#ifndef NO_MERCURY
#include "x68k/mercury.h"
#endif
#include "x68k/mfp.h"
#include "x68k/gvram.h"
#include "x68k/tvram.h"
#include "x68k/crtc.h"
#include "x68k/dmac.h"
#include "x68k/irqh.h"
#include "x68k/palette.h"
#include "x68k/bg.h"
#include "x68k/pia.h"
#include "x68k/ioc.h"
#include "x68k/scsi.h"
#include "x68k/sasi.h"
#include "x68k/fdc.h"
#include "x68k/rtc.h"
#include "x68k/scc.h"

#ifdef _WIN32
#define SLASH '\\'
#else
#define SLASH '/'
#endif

#define MODE_HIGH_ACTUAL 55.46 /* floor((10*100*1000^2 / VSYNC_HIGH)) / 100 */
#define MODE_NORM_ACTUAL 61.46 /* floor((10*100*1000^2 / VSYNC_NORM)) / 100 */
#define MODE_HIGH_COMPAT 55.5  /* 31.50 kHz - commonly used  */
#define MODE_NORM_COMPAT 60.00 /* Optional frontend-compatible 15 kHz presentation rate.
                     * Native X68000 CRTC timing remains unchanged. */
enum
{
   MODES_ACTUAL,
   MODES_COMPAT,
   MODE_NORM = 0,
   MODE_HIGH,
   MODES
};

const float framerates[2][2] = {
   { MODE_NORM_ACTUAL, MODE_HIGH_ACTUAL },
   { MODE_NORM_COMPAT, MODE_HIGH_COMPAT }
};

/*
 * Refresh rate follows the CRTC timing (R00, R04, R20) instead of the two
 * fixed 61.46/55.46 Hz values: real modes run at e.g. 61.23, 60.46, 59.59,
 * 57.89 or 53.49 Hz (verified against XM6 type G).
 *
 * "actual" is the true rate; with Adjust Frame Rates enabled the frontend is
 * given a nearby friendlier rate and the audio clock is scaled to match.
 */
/* The rates must be exact, not rounded: the sound renderer produces
 * 44100 * frame_clocks / 10MHz samples per emulated frame and the frontend
 * consumes 44100 / fps per frame.  Rounding fps to 2 decimals made the two
 * differ by up to 0.016%, which is a missing sample every few frames and is
 * heard as crackling. */
static double px68k_fps_actual(void)
{
   return 1e7 / (double)CRTC_FrameClocks();
}

static double px68k_fps_compat(double actual)
{
   if (actual >= 58.0)
      return 60.0;
   return floor(actual * 2.0 + 0.5) / 2.0;
}

static double px68k_current_framerate(void)
{
   double a = px68k_fps_actual();
   return Config.AdjustFrameRates ? px68k_fps_compat(a) : a;
}

/* frame period the frontend was last told about, 100ns units */
static uint32_t applied_frame_clk = 0;

char	winx68k_dir[2048];
char	winx68k_ini[2048];

uint16_t	VLINE_TOTAL = 567;
uint32_t	VLINE = 0;
uint32_t	vline = 0;

#define SOUNDRATE 44100.0
/* Audio frame budget is accumulated fractionally to avoid rounding drift. */

static int firstcall          = 1;

static uint32_t old_ram_size     = 0;
static int old_clkdiv         = 0;

static int oldrw=0,oldrh      = 0;
static char RPATH[512];
static char RETRO_DIR[512];
static const char *retro_save_directory;
static const char *retro_system_directory;
static const char *retro_browse_directory;
const char *retro_content_directory;
char retro_system_conf[512]; /* system directory path */
char retro_browse_conf[512]; /* file browser default path */
char base_dir[MAX_PATH];

static uint8_t Core_Key_State[512];
static uint8_t Core_old_Key_State[512];

static bool joypad1 = true, joypad2 = true;
static unsigned controller_device[2] = {
   RETRO_DEVICE_JOYPAD, RETRO_DEVICE_JOYPAD
};
static bool controller_device_set[2];
static float joy_mouse_analog_speed = 12.0f;
static float joy_mouse_deadzone = 12.5f;
static float joy_mouse_digital_speed = 6.0f;

static int controller_type[2];

#define PX68K_DEVICE_CPSF_MD      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define PX68K_DEVICE_CPSF_SFC     RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 2)
#define PX68K_DEVICE_CYBERSTICK_D RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 3)
#define PX68K_DEVICE_CYBERSTICK_A RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 4)
#define PX68K_DEVICE_JOY2KEY      RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 5)


static char CMDFILE[512];

/* Args for experimental_cmdline */
static char ARGUV[64][1024];
static unsigned char ARGUC = 0;

/* Args for Core */
static char XARGV[64][1024];
static const char* xargv_cmd[64];
static int PARAMCOUNT     = 0;

static uint8_t DispFrame  = 0;
static int FrameSkipCount = 0;
static int FrameSkipQueue = 0;
static int ClkUsed        = 0;

uint32_t retrow           = 800;
/* Display aspect of the active picture, derived from CRTC timing
 * (see WinDraw_Draw). 4:3 means the window fills the whole screen. */
float    retro_aspect      = 4.0f / 3.0f;
/* core option px68k_aspect_ratio: 0 = X68000 (CRT timing), 1 = 4:3 (default), 2 = 1:1 */
int      px68k_aspect_mode = 1;
uint32_t retroh           = 600;
/* Keep the full OSD keyboard visible even when the emulated mode is low-res. */
static bool osd_video_geometry_active = false;
static uint16_t osd_video_buffer[800 * 512];
#define OSD_VIDEO_WIDTH  768
#define OSD_VIDEO_HEIGHT 512

/* Geometry the frontend should currently use: the keyboard's fixed 768x512
 * 4:3 reference while it is visible, otherwise the emulated mode with the
 * aspect ratio chosen by the "Aspect Ratio" option / CRTC timing. */
static void px68k_fill_geometry(struct retro_game_geometry *g)
{
   g->max_width  = 800;
   g->max_height = 600;
   if (osd_video_geometry_active)
   {
      g->base_width   = OSD_VIDEO_WIDTH;
      g->base_height  = OSD_VIDEO_HEIGHT;
      g->aspect_ratio = 4.0f / 3.0f;
   }
   else
   {
      g->base_width   = retrow;
      g->base_height  = retroh;
      g->aspect_ratio = retro_aspect;
   }
}
int CHANGEAV              = 0;
int CHANGEAV_TIMING       = 0; /* Separate change of geometry from change of refresh rate */
int VID_MODE              = MODE_NORM; /* what framerate we start in */
static double FRAMERATE;
uint32_t libretro_supports_input_bitmasks = 0;
 
static int16_t soundbuf[1024 * 2];
/* Legacy timer module linkage; no longer used to gate emulated frames. */
unsigned int total_usec = (unsigned int)-1;
static int soundbuf_size;
static double audio_frame_fraction = 0.0;
/* Experimental FinalBurn-style fixed audio output cadence: the sound
 * renderer receives enough clock ticks to synthesize the output block
 * implied by the advertised FPS. Emulated CPU/video clocks are unchanged. */
static double fb_audio_clock_remainder = 0.0;
static int fb_audio_last_mode = -1;
static int fb_audio_last_adjust = -1;
static int fb_audio_clock(int clock)
{
   double ratio, scaled;
   int result;
   double actual = px68k_fps_actual();
   if (!Config.AdjustFrameRates)
   {
      fb_audio_clock_remainder = 0.0;
      fb_audio_last_mode = (int)(actual * 100.0);
      fb_audio_last_adjust = 0;
      return clock;
   }
   if (fb_audio_last_mode != (int)(actual * 100.0f) || fb_audio_last_adjust != Config.AdjustFrameRates)
   {
      fb_audio_clock_remainder = 0.0;
      fb_audio_last_mode = (int)(actual * 100.0f);
      fb_audio_last_adjust = Config.AdjustFrameRates;
   }
   ratio = actual / px68k_fps_compat(actual);
   scaled = (double)clock * ratio + fb_audio_clock_remainder;
   result = (int)scaled;
   fb_audio_clock_remainder = scaled - (double)result;
   return result;
}

static int next_audio_frame_size(void)
{
   int frames;
   if (FRAMERATE <= 0.0)
      return 0;
   audio_frame_fraction += (double)SOUNDRATE / FRAMERATE;
   frames = (int)audio_frame_fraction;
   audio_frame_fraction -= frames;
   if (frames > 1024) frames = 1024;
   if (frames < 0) frames = 0;
   return frames;
}

uint16_t *videoBuffer;

enum {
   menu_out,
   menu_enter,
   menu_in
};

static int menu_mode = menu_out;

static retro_video_refresh_t video_cb;
static retro_environment_t environ_cb;
/* Explorer-only setting: no changes to archive paths or disk handling. */
int px68k_mame_names_mode(void)
{
   struct retro_variable var = { "px68k_mame_names", NULL };
   if (!environ_cb || !environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) || !var.value) return 0;
   if (!strcmp(var.value, "japanese")) return 2;
   if (!strcmp(var.value, "english") || !strcmp(var.value, "enabled")) return 1;
   return 0;
}

const char *px68k_mame_names_system_dir(void)
{
   return retro_system_directory;
}


static retro_input_poll_t input_poll_cb;
static retro_set_rumble_state_t rumble_cb;
retro_input_state_t input_state_cb;
retro_audio_sample_t audio_cb;
retro_audio_sample_batch_t audio_batch_cb;
retro_log_printf_t log_cb;
#ifdef USE_LIBRETRO_VFS
struct retro_vfs_interface_info vfs_iface_info;
#endif

static unsigned no_content;

static bool opt_rumble_enabled = false;

#define MAX_DISKS 10

typedef enum
{
   FDD0 = 0,
   FDD1 = 1
} disk_drive;

/* .dsk swap support */
struct disk_control_interface_t
{
   unsigned dci_version;                        /* disk control interface version, 0 = use old interface */
   unsigned total_images;                       /* total number if disk images */
   unsigned index;                              /* currect disk index */
   disk_drive cur_drive;                        /* current active drive */
   bool inserted[2];                            /* tray state for FDD0/FDD1, 0 = disk ejected, 1 = disk inserted */

   char path[MAX_DISKS][MAX_PATH];              /* disk image paths */
   char label[MAX_DISKS][MAX_PATH];             /* disk image base name w/o extension */

   unsigned g_initial_disc;                     /* initial disk index */
   char g_initial_disc_path[MAX_PATH];          /* initial disk path */
};

static struct disk_control_interface_t disk;
static struct retro_disk_control_callback dskcb;
static struct retro_disk_control_ext_callback dskcb_ext;

static struct retro_input_descriptor input_descs[64];


static bool is_path_absolute(const char* path)
{
   if (path[0] == SLASH)
      return true;

#ifdef _WIN32
   if ((path[0] >= 'a' && path[0] <= 'z') ||
      (path[0]  >= 'A' && path[0] <= 'Z'))
   {
      if (path[1] == ':')
         return true;
   }
#endif
   return false;
}

static void extract_basename(char *buf, const char *path, size_t size)
{
   const char *base;
   char *ext;
   base = strrchr(path, '/');
   if (!base)
      base = strrchr(path, '\\');
   if (!base)
      base = path;

   if (*base == '\\' || *base == '/')
      base++;

   strncpy(buf, base, size - 1);
   buf[size - 1] = '\0';

   ext = strrchr(buf, '.');
   if (ext)
      *ext = '\0';
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   char *base = NULL;

   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
      buf[0] = '\0';
}

/* BEGIN MIDI INTERFACE */
#include "x68k/midi.h"
#include "midi/midi_shadow.h"
#include "libretro/mmsystem.h"
static int libretro_supports_midi_output = 0;
static struct retro_midi_interface midi_cb = { 0 };
static bool libretro_supports_option_categories = 0;

/* While a setup is being re-created on load, don't record it as new input. */
static bool midi_replaying;

/* 0 = off, 1 = re-create the synth setup, 2 = also switch back to the output
 * (internal MIDI synthesizer or external MIDI) that was in use when the state was saved. */
static int midi_restore_mode = 2;
static int midi_emulation_selected = PX68K_MIDIEMU_DISABLED;

/* Raw output: straight to whichever destination is active. */
static void midi_emit_short(size_t msg)
{
   if (px68k_midiemu_enabled())
   {
      px68k_midiemu_midi_short(msg);
      return;
   }
   if (libretro_supports_midi_output && midi_cb.output_enabled())
   {
      uint8_t status = msg & 0xFF;

      midi_cb.write(status, 0);
      midi_cb.write((msg >> 8) & 0xFF, 0);
      /* Program change and channel pressure have ONE data byte. Sending a
       * third (the unused 0) made a following running-status reader treat it
       * as a second message, e.g. a program change to program 0 right after
       * every program change. */
      if ((status & 0xF0) != 0xC0 && (status & 0xF0) != 0xD0)
         midi_cb.write((msg >> 16) & 0xFF, 0);
   }
}

static void midi_emit_long(const uint8_t *s, size_t len)
{
   if (px68k_midiemu_enabled())
   {
      px68k_midiemu_midi_long(s, len);
      return;
   }
   if (libretro_supports_midi_output && midi_cb.output_enabled())
   {
      size_t i;
      for (i = 0; i < len; i++)
         midi_cb.write(s[i], 0);
   }
}

/* Called by the emulated MIDI board. */
void midi_out_short_msg(size_t msg)
{
   if (!midi_replaying)
      MIDIShadow_Short(msg);
   midi_emit_short(msg);
}

void midi_out_long_msg(uint8_t *s, size_t len)
{
   if (!midi_replaying)
      MIDIShadow_Long(s, len);
   midi_emit_long(s, len);
}

/* 0 = none, 1 = external MIDI, 2..4 = SC-55 / MT-32 / CM-32L. */
static int midi_current_source(void)
{
   if (!Config.MIDI_SW)
      return 0;
   if (px68k_midiemu_enabled())
      return midi_emulation_selected + 1;
   if (libretro_supports_midi_output && midi_cb.output_enabled())
      return 1;
   return 0;
}

/* Switch to the output that was in use when a state was saved, if it can be
 * had. Returns 1 if the destination changed. Never turns MIDI itself on: the
 * guest probes the board at startup, so that can't be changed mid-run. */
static int midi_apply_saved_source(int saved)
{
   int now = midi_current_source();

   if (!Config.MIDI_SW || saved == 0 || saved == now)
      return 0;

   /* Older states used source 5 for CM-64. Restore that selection as CM-32L,
    * since Munt's CM-64 option used the same LA module and ROMs. */
   if (saved == 5)
      saved = PX68K_MIDIEMU_CM32L + 1;

   if (saved >= PX68K_MIDIEMU_SC55 + 1 && saved <= PX68K_MIDIEMU_CM32L + 1)
   {
      int emulation = saved - 1;
      px68k_midiemu_configure(emulation, retro_system_directory, retro_content_directory);
      if (!px68k_midiemu_enabled())
         return 0; /* Required ROMs are unavailable: stay on what we have. */
      midi_emulation_selected = emulation;
   }
   else if (saved == 1)
   {
      if (!(libretro_supports_midi_output && midi_cb.output_enabled()))
         return 0;
      px68k_midiemu_configure(PX68K_MIDIEMU_DISABLED, retro_system_directory, retro_content_directory);
   }
   else
      return 0;

   MIDI_ReopenOutput();
   return 1;
}

/* After loading a normal state: put the synth back into the sound setup the
 * state was saved with (mode, programs, volumes, SysEx parameters), and
 * optionally the output that was in use. */
static void midi_restore_after_load(void)
{
   int saved = MIDIShadow_GetLoadedSource();
   int source_changed;

   /* -1: older state, it says nothing about the synth. */
   if (!midi_restore_mode || saved < 0 || !Config.MIDI_SW)
      return;

   source_changed = (midi_restore_mode >= 2) ? midi_apply_saved_source(saved) : 0;

   if (!source_changed && !MIDIShadow_NeedsReplay())
      return;

   midi_replaying = true;
   MIDI_SendModuleReset();               /* the standard the state was using (LA/GM/GS/XG) */
   MIDIShadow_Replay(midi_emit_short, midi_emit_long);
   midi_replaying = false;

   if (libretro_supports_midi_output && midi_cb.output_enabled())
      midi_cb.flush();
}


/* A non-NULL handle is required by x68k/midi.c before it will dispatch
 * MIDI bytes. Use a private sentinel for the internal synthesizer: no
 * external MIDI handle is involved in this case. */
static int px68k_midiemu_midi_handle;

int midi_out_open(void **phmo)
{
   if (px68k_midiemu_enabled())
   {
      if (phmo) *phmo = &px68k_midiemu_midi_handle;
      return 0;
   }
   if (libretro_supports_midi_output && midi_cb.output_enabled())
   {
      *phmo = &midi_cb;
      return 0;
   }
   return 1;
}

static void update_variable_midi_interface(int running)
{
   struct retro_variable var;

   var.key = "px68k_midi_output";
   var.value = NULL;

   if (!running && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         Config.MIDI_SW = 0;
      else if (!strcmp(var.value, "enabled"))
         Config.MIDI_SW = 1;
   }

   var.key = "px68k_midi_state_restore";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         midi_restore_mode = 0;
      else if (!strcmp(var.value, "setup"))
         midi_restore_mode = 1;
      else
         midi_restore_mode = 2;
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

/* Stop notes that may outlive the emulated frame stream in a physical or
 * software MIDI destination. This is deliberately an output-side panic: it
 * does not modify the emulated MIDI board state, so normal save states and
 * Run-Ahead state restoration remain unchanged. */
static void midi_output_panic(void)
{
   unsigned ch;

   if (!Config.MIDI_SW)
      return;

   /* Raw output: this is not something the game sent, so it must not be
    * recorded as part of the synth's setup. */
   for (ch = 0; ch < 16; ch++)
   {
      /* Release sustain, then stop held notes and any remaining voices. */
      midi_emit_short(((size_t)0 << 16) | ((size_t)64 << 8) | (0xB0 | ch));
      midi_emit_short(((size_t)0 << 16) | ((size_t)123 << 8) | (0xB0 | ch));
      midi_emit_short(((size_t)0 << 16) | ((size_t)120 << 8) | (0xB0 | ch));
   }

   if (libretro_supports_midi_output && midi_cb.output_enabled())
      midi_cb.flush();
}

static void midi_interface_init(void)
{
   libretro_supports_midi_output = 0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_MIDI_INTERFACE, &midi_cb))
      libretro_supports_midi_output = 1;
}

/* END OF MIDI INTERFACE */

static void update_variable_disk_drive_swap(void)
{
   struct retro_variable var =
   {
      "px68k_disk_drive",
      NULL
   };

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "FDD0") == 0)
         disk.cur_drive = FDD0;
      else
         disk.cur_drive = FDD1;
   }
}

static bool set_eject_state(bool ejected)
{
   /* Refresh the active drive from the px68k_disk_drive option here too, not
    * only in get_eject_state(): the frontend's swap sequence is
    * set_eject_state(true) -> set_image_index() -> set_eject_state(false)
    * and may never call get_eject_state(), so without this the disk change
    * would keep targeting the previous drive (default FDD1) and choosing the
    * drive from the option would have no effect. */
   update_variable_disk_drive_swap();

   if (disk.index == disk.total_images)
      return true; /* Frontend is trying to set "no disk in tray" */

   if (ejected)
   {
      FDD_EjectFD(disk.cur_drive);
      Config.FDDImage[disk.cur_drive][0] = '\0';
   }
   else
   {
      strcpy(Config.FDDImage[disk.cur_drive], disk.path[disk.index]);
      FDD_SetFD(disk.cur_drive, Config.FDDImage[disk.cur_drive], 0);
   }
   disk.inserted[disk.cur_drive] = !ejected;
   return true;
}

static bool get_eject_state(void)
{
   update_variable_disk_drive_swap();
   return !disk.inserted[disk.cur_drive];
}

/* Controls automatic pairing of floppy images from a ZIP archive.
 * 0 = Always, 1 = Boot Only, 2 = At Disk Change, 3 = Off. */
static int get_archive_floppy_mode(void)
{
   struct retro_variable var = { "px68k_archive_floppies", NULL };

   if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "Boot Only"))
         return 1;
      if (!strcmp(var.value, "At Disk Change"))
         return 2;
      if (!strcmp(var.value, "Always"))
         return 0;
   }

   return 3; /* default: Off */
}

static unsigned get_image_index(void)
{
   return disk.index;
}

static bool set_image_index(unsigned index)
{
   disk.index = index;
   return true;
}

static unsigned get_num_images(void)
{
   return disk.total_images;
}

/* Fills the Disk Control table with up to MAX_DISKS floppy members of a
 * ZIP, in sorted order starting at member @first, and keeps the archive's
 * slot from being recycled while the table refers to it. */
static int zip_fill_disk_table(int slot, int first)
{
   unsigned old_total = disk.total_images;
   int total          = px68k_zip_count(slot, 0);
   int n              = 0;
   int i;

   for (i = first; i < total && n < MAX_DISKS; i++)
   {
      const char *name = px68k_zip_name(slot, 0, i);
      if (!name || !px68k_zip_make_virtual_path(slot, name, disk.path[n],
               sizeof(disk.path[n])))
         break;
      snprintf(disk.label[n], sizeof(disk.label[n]), "%s", name);
      n++;
   }
   for (i = n; i < (int)old_total && i < MAX_DISKS; i++)
   {
      disk.path[i][0]  = '\0';
      disk.label[i][0] = '\0';
   }
   disk.total_images = (unsigned)n;
   px68k_zip_pin(slot);
   return n;
}

/* Keep the internal PX68K file menu and RetroArch Disk Control backed by the
 * same ZIP member list. The table holds MAX_DISKS members, so for a larger
 * set it is the window holding the chosen disk and the one after it. */
void px68k_disk_menu_sync_zip(int slot, const char *member, int drive)
{
   int total, selected = -1, first = 0, count, i;

   if (drive < 0 || drive > 1 || !member || !*member)
      return;

   total = px68k_zip_count(slot, 0);
   for (i = 0; i < total; i++)
   {
      const char *name = px68k_zip_name(slot, 0, i);
      if (name && !strcmp(name, member))
      {
         selected = i;
         break;
      }
   }
   if (selected < 0)
      return;

   if (selected > MAX_DISKS - 2)
      first = selected - (MAX_DISKS - 2);
   if (total > MAX_DISKS && first > total - MAX_DISKS)
      first = total - MAX_DISKS;
   if ((count = zip_fill_disk_table(slot, first)) <= 0)
      return;
   selected -= first;

   disk.index     = (unsigned)selected;
   disk.cur_drive = (disk_drive)drive;
   FDD_SetFD(drive, disk.path[selected], 0);
   snprintf(Config.FDDImage[drive], sizeof(Config.FDDImage[drive]), "%s",
         disk.path[selected]);
   disk.inserted[drive] = true;

   /* Boot Only pairs disks only during initial ZIP launch. At Disk Change
    * pairs on manual insertion but not at boot. Off never pairs a second disk. */
   {
      int archive_mode = get_archive_floppy_mode();
      if (drive == FDD0 && archive_mode != 1 && archive_mode != 3)
      {
         if (selected + 1 < count)
         {
            FDD_SetFD(FDD1, disk.path[selected + 1], 0);
            snprintf(Config.FDDImage[FDD1], sizeof(Config.FDDImage[FDD1]), "%s",
                  disk.path[selected + 1]);
            disk.inserted[FDD1] = true;
         }
         else
         {
            FDD_EjectFD(FDD1);
            Config.FDDImage[FDD1][0] = '\0';
            disk.inserted[FDD1] = false;
         }
      }
   }
}

static bool add_image_index(void)
{
   if (disk.total_images >= MAX_DISKS)
      return false;

   disk.total_images++;
   return true;
}

static bool replace_image_index(unsigned index, const struct retro_game_info *info)
{
   char image[MAX_PATH];
   strcpy(disk.path[index], info->path);
   extract_basename(image, info->path, sizeof(image));
   snprintf(disk.label[index], sizeof(disk.label), "%s", image);
   return true;
}

static bool disk_set_initial_image(unsigned index, const char *path)
{
   if (string_is_empty(path))
      return false;

   disk.g_initial_disc = index;
   strncpy(disk.g_initial_disc_path, path, sizeof(disk.g_initial_disc_path));

   return true;
}

static bool disk_get_image_path(unsigned index, char *path, size_t len)
{
   if (len < 1)
      return false;

   if (index < disk.total_images)
   {
      if (!string_is_empty(disk.path[index]))
      {
         strncpy(path, disk.path[index], len);
         return true;
      }
   }

   return false;
}

static bool disk_get_image_label(unsigned index, char *label, size_t len)
{
   if (len < 1)
      return false;

   if (index < disk.total_images)
   {
      if (!string_is_empty(disk.label[index]))
      {
         strncpy(label, disk.label[index], len);
         return true;
      }
   }

   return false;
}

static void attach_disk_swap_interface(void)
{
   dskcb.set_eject_state = set_eject_state;
   dskcb.get_eject_state = get_eject_state;
   dskcb.set_image_index = set_image_index;
   dskcb.get_image_index = get_image_index;
   dskcb.get_num_images  = get_num_images;
   dskcb.add_image_index = add_image_index;
   dskcb.replace_image_index = replace_image_index;

   environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &dskcb);
}

void attach_disk_swap_interface_ext(void)
{
   dskcb_ext.set_eject_state = set_eject_state;
   dskcb_ext.get_eject_state = get_eject_state;
   dskcb_ext.set_image_index = set_image_index;
   dskcb_ext.get_image_index = get_image_index;
   dskcb_ext.get_num_images  = get_num_images;
   dskcb_ext.add_image_index = add_image_index;
   dskcb_ext.replace_image_index = replace_image_index;
   dskcb_ext.set_initial_image = NULL;
   dskcb_ext.get_image_path = disk_get_image_path;
   dskcb_ext.get_image_label = disk_get_image_label;

   environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &dskcb_ext);
}

static void disk_swap_interface_init(void)
{
   unsigned i;
   disk.dci_version  = 0;
   disk.total_images = 0;
   disk.index        = 0;
   disk.cur_drive    = FDD1;
   disk.inserted[0]  = false;
   disk.inserted[1]  = false;

   disk.g_initial_disc         = 0;
   disk.g_initial_disc_path[0] = '\0';

   for (i = 0; i < MAX_DISKS; i++)
   {
      disk.path[i][0]  = '\0';
      disk.label[i][0] = '\0';
   }

   if (environ_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION, &disk.dci_version) && (disk.dci_version >= 1))
      attach_disk_swap_interface_ext();
   else
      attach_disk_swap_interface();
}
/* end .dsk swap support */

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

static int loadcmdfile(char *argv)
{
   int res  = 0;
   FILE *fp = fopen(argv, "r");

   if (fp)
   {
      if (fgets(CMDFILE, 512, fp) != NULL)
         res = 1;
      fclose(fp);
   }

   return res;
}

static size_t handle_extension(char *path, char *ext)
{
   size_t len = strlen(path);
   if (len >= 4 &&
         path[len - 4] == '.' &&
         path[len - 3] == ext[0] &&
         path[len - 2] == ext[1] &&
         path[len - 1] == ext[2])
      return 1;
   return 0;
}

static void parse_cmdline(const char *argv)
{
   char *p, *p2, *start_of_word;
   int c, c2;
   static char buffer[512 * 4];
   enum states { DULL, IN_WORD, IN_STRING } state = DULL;

   strcpy(buffer, argv);
   strcat(buffer, " \0");

   for (p = buffer; *p != '\0'; p++)
   {
      c = (unsigned char) *p; /* convert to unsigned char for is* functions */
      switch (state)
      {
         case DULL: /* not in a word, not in a double quoted string */
            if (isspace(c)) /* still not in a word, so ignore this char */
               continue;
            /* not a space -- if it's a double quote we go to IN_STRING, else to IN_WORD */
            if (c == '"')
            {
               state = IN_STRING;
               start_of_word = p + 1; /* word starts at *next* char, not this one */
               continue;
            }
            state = IN_WORD;
            start_of_word = p; /* word starts here */
            continue;
         case IN_STRING:
            /* we're in a double quoted string, so keep going until we hit a close " */
            if (c == '"')
            {
               /* word goes from start_of_word to p-1
                *... do something with the word ... */
               for (c2 = 0, p2 = start_of_word; p2 < p; p2++, c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUV[ARGUC][c2] = '\0'; /* terminate: ARGUV is reused across
                                         * loads, a shorter token would keep
                                         * the previous game's path tail */

               ARGUC++;

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_STRING or we handled the end above */
         case IN_WORD:
            /* we're in a word, so keep going until we get to a space */
            if (isspace(c))
            {
               /* word goes from start_of_word to p-1
                *... do something with the word ... */
               for (c2 = 0, p2 = start_of_word; p2 <p; p2++, c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUV[ARGUC][c2] = '\0'; /* terminate (see IN_STRING above) */

               ARGUC++;

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_WORD or we handled the end above */
      }
   }
}


static const char *m3u_zip_separator(const char *path)
{
   const char *p;
   for (p = path; *p; p++)
      if (p[0] == '.' && p[1] && p[2] && p[3] && p[4] == '#'
            && tolower((unsigned char)p[1]) == 'z'
            && tolower((unsigned char)p[2]) == 'i'
            && tolower((unsigned char)p[3]) == 'p')
         return p + 4;
   return NULL;
}

static bool read_m3u(const char *file)
{
   unsigned index = 0;
   char line[4096];
   FILE *f = fopen(file, "r");

   if (!f)
      return false;

   while (fgets(line, sizeof(line), f)
         && index < sizeof(disk.path) / sizeof(disk.path[0]))
   {
      char entry[2048];
      char name[MAX_PATH];
      char image_label[MAX_PATH];
      char *label;
      size_t len;
      const char *zip_sep;

      /* Trim line endings and surrounding whitespace before interpreting it. */
      len = strlen(line);
      while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'
               || isspace((unsigned char)line[len - 1])))
         line[--len] = '\0';
      while (line[0] && isspace((unsigned char)line[0]))
         memmove(line, line + 1, strlen(line));
      if (!line[0] || line[0] == '#')
         continue;

      /* Accept quoted entries as well as normal M3U paths. */
      len = strlen(line);
      if (len >= 2 && line[0] == '"' && line[len - 1] == '"')
      {
         memmove(line, line + 1, len - 2);
         line[len - 2] = '\0';
      }
      if (!line[0])
         continue;

      /* RetroArch's optional |label suffix is not part of the path. */
      label = strchr(line, '|');
      if (label)
      {
         *label++ = '\0';
         while (*label && isspace((unsigned char)*label))
            label++;
      }
      len = strlen(line);
      while (len && isspace((unsigned char)line[len - 1]))
         line[--len] = '\0';
      if (!len || len >= sizeof(entry))
         continue;
      strcpy(entry, line);

      if (is_path_absolute(entry))
      {
         if (snprintf(name, sizeof(name), "%s", entry) >= (int)sizeof(name))
            continue;
      }
      else if (base_dir[0])
      {
         if (snprintf(name, sizeof(name), "%s%c%s", base_dir, SLASH, entry)
               >= (int)sizeof(name))
            continue;
      }
      else if (snprintf(name, sizeof(name), "%s", entry) >= (int)sizeof(name))
         continue;

      zip_sep = m3u_zip_separator(name);
      if (zip_sep)
      {
         char archive[MAX_PATH];
         int slot;
         size_t archive_len = (size_t)(zip_sep - name);
         if (!archive_len || archive_len >= sizeof(archive) || !zip_sep[1])
            continue;
         memcpy(archive, name, archive_len);
         archive[archive_len] = '\0';
         slot = px68k_zip_register_archive(archive);
         if (slot < 0
               || !px68k_zip_make_virtual_path(slot, zip_sep + 1,
                  disk.path[index], sizeof(disk.path[index]))
               || !px68k_zip_preload(disk.path[index]))
         {
            disk.path[index][0] = '\0';
            continue;
         }
         /* M3U entries can refer to several archives. Keep every archive
          * referenced by the disk table alive for the whole playlist. */
         px68k_zip_pin_playlist(slot);
         extract_basename(image_label, zip_sep + 1, sizeof(image_label));
      }
      else
      {
         if (strlen(name) >= sizeof(disk.path[index]))
            continue;
         strcpy(disk.path[index], name);
         extract_basename(image_label, name, sizeof(image_label));
      }

      if (label && *label)
         snprintf(disk.label[index], sizeof(disk.label[index]), "%s", label);
      else
         snprintf(disk.label[index], sizeof(disk.label[index]), "%s", image_label);
      index++;
   }

   disk.total_images = index;
   fclose(f);
   return disk.total_images != 0;
}

static void Add_Option(const char* option)
{
   static int first = 0;

   if(first == 0)
   {
      PARAMCOUNT = 0;
      first++;
   }

   strcpy(XARGV[PARAMCOUNT++], option);
}

static int retro_load_game_internal(const char *argv)
{
   if (strlen(argv) > strlen("cmd"))
   {
      int res = 0;
      if (handle_extension((char*)argv, "cmd") || handle_extension((char*)argv, "CMD"))
      {
         int i;

         if (!(res = loadcmdfile((char*)argv)))
         {
            if (log_cb)
               log_cb(RETRO_LOG_ERROR, "%s\n", "[libretro]: failed to read cmd file ...");
            return 0;
         }

         parse_cmdline(CMDFILE);

         /* handle relative paths, append content dir if needed */
         for (i = 1; i < ARGUC; i++)
         {
            if (!is_path_absolute(ARGUV[i]))
            {
               char tmp[2048] = { 0 };
               strcpy(tmp, ARGUV[i]);
               ARGUV[i][0] = '\0';
               sprintf(ARGUV[i], "%s%c%s", base_dir, SLASH, tmp);
            }
         }
      }
      else if (handle_extension((char*)argv, "m3u") || handle_extension((char*)argv, "M3U"))
      {
         if (!read_m3u((char*)argv))
         {
            if (log_cb)
               log_cb(RETRO_LOG_ERROR, "%s\n", "[libretro]: failed to read m3u file ...");
            return 0;
         }

         if(disk.total_images > 1)
         {
            sprintf((char*)argv, "%s \"%s\" \"%s\"", "px68k", disk.path[0], disk.path[1]);
            disk.inserted[1] = true;
         }
         else
            sprintf((char*)argv, "%s \"%s\"", "px68k", disk.path[0]);

         disk.inserted[0] = true;
         parse_cmdline(argv);
      }
   }

   return 1;
}

#define MEM_SIZE 0xc00000

static int WinX68k_Init(void)
{
	IPL  = (uint8_t*)malloc(0x40000);
	MEM  = (uint8_t*)malloc(MEM_SIZE);
	FONT = (uint8_t*)malloc(0xc0000);

   if (MEM)
		memset(MEM, 0, MEM_SIZE);

	if (MEM && FONT && IPL)
	{
	  	/* m68000_init(); */
		return 1;
	}
	return 0;
}

static void WinX68k_SCSICheck(void)
{
	static const uint8_t SCSIIMG[] = {
		0x00, 0xfc, 0x00, 0x14,				/* $fc0000 SCSI boot entry address */
		0x00, 0xfc, 0x00, 0x16,				/* $fc0004 IOCS vector setting entry
address (always before "Human" 8 bytes) */
		0x00, 0x00, 0x00, 0x00,				/* $fc0008 ? */
		0x48, 0x75, 0x6d, 0x61,				/* $fc000c ↓ */
		0x6e, 0x36, 0x38, 0x6b,				/* $fc0010 ID "Human68k"	(always just
before start-up entry point) */
		0x4e, 0x75,							/* $fc0014 "rts"		(start-up entry point)
*/
		0x23, 0xfc, 0x00, 0xfc, 0x00, 0x2a,	/* $fc0016 ↓		(IOCS vector setting
entry point) */
		0x00, 0x00, 0x07, 0xd4,				/* $fc001c "move.l #$fc002a, $7d4.l" */
		0x74, 0xff,							   /* $fc0020 "moveq #-1, d2" */
		0x4e, 0x75,							   /* $fc0022 "rts" */
		0x44, 0x55, 0x4d, 0x4d, 0x59, 0x20,	/* $fc0024 ID "DUMMY " */
		0x70, 0xff,							/* $fc002a "moveq #-1, d0"	(SCSI IOCS call
entry point) */
		0x4e, 0x75,							/* $fc002c "rts" */
	};

	int i;
	uint16_t *p1, *p2;
	int scsi = 0;
	for (i = 0x30600; i < 0x30c00; i += 2)
   {
		p1 = (uint16_t *)(&IPL[i]);
		p2 = p1 + 1;
		/* IPL still holds the raw (unswapped) ROM bytes here.  The SCSI
		 * boot pointer word 0x00fc reads as 0xfc00 through a 16-bit load
		 * on little-endian but as 0x00fc on big-endian. */
#ifdef MSB_FIRST
		if (*p1 == 0x00fc && *p2 == 0x0000)
#else
		if (*p1 == 0xfc00 && *p2 == 0x0000)
#endif
      {
			scsi = 1;
			break;
		}
	}

	/* SCSI model time */
	if (scsi)
   {
		memset(IPL, 0, 0x2000);				      /* main is 8kb */
		memset(&IPL[0x2000], 0xff, 0x1e000);	/* remaining is 0xff */
		memcpy(IPL, SCSIIMG, sizeof(SCSIIMG));	/* fake­SCSI BIOS */
	}
   else /* SASI model sees the IPL as it is */
      memcpy(IPL, &IPL[0x20000], 0x20000);
}

#define	NELEMENTS(array)	((int)(sizeof(array) / sizeof(array[0])))

static int WinX68k_LoadROMs(void)
{
	static const char *BIOSFILE[] = {
		"iplrom.dat", "iplrom30.dat", "iplromco.dat", "iplromxv.dat"
	};
	static const char FONTFILE[] = "cgrom.dat";
	static const char FONTFILETMP[] = "cgrom.tmp";
	void *fp;
	int i;
	uint8_t tmp;

	for (fp = 0, i = 0; fp == 0 && i < NELEMENTS(BIOSFILE); ++i)
		fp = file_open_c((char *)BIOSFILE[i]);

	if (fp == 0)
	{
		if (log_cb)
			log_cb(RETRO_LOG_ERROR, "[PX68K] Error: BIOS ROM image can't be found.\n");
		return 0;
	}

	file_lread(fp, &IPL[0x20000], 0x20000);
	file_close(fp);

   /* if SCSI IPL, SCSI BIOS is established around $fc0000 */
	WinX68k_SCSICheck();

	/* The CPU fetches opcodes from IPL via *(uint16_t*)PC, which is a
	 * native load.  On little-endian the ROM (stored MSB-first) must be
	 * byte-swapped so those loads yield the correct 68000 words; on
	 * big-endian the raw ROM order is already correct, so skip the swap
	 * (doing it would feed the CPU byte-swapped opcodes -> no boot). */
#ifndef MSB_FIRST
	for (i = 0; i < 0x40000; i += 2) {
		tmp = IPL[i];
		IPL[i] = IPL[i + 1];
		IPL[i + 1] = tmp;
	}
#endif

	fp = file_open_c((char *)FONTFILE);
	if (fp == 0)
   {
		/* cgrom.tmp present? */
		fp = file_open_c((char *)FONTFILETMP);
		/* font creation XXX - Font ROM image can't be found */
		if (fp == 0)
			return 0;
	}
	file_lread(fp, FONT, 0xc0000);
	file_close(fp);

	return 1;
}

static  void WinX68k_Cleanup(void)
{
	if (IPL)
		free(IPL);
	if (MEM)
		free(MEM);
	if (FONT)
		free(FONT);
	IPL  = NULL;
	MEM  = NULL;
	FONT = NULL;
}

void WinX68k_Reset(void)
{
   OPM_Reset();

   m68000_reset();
   /* Reset vector byte order follows the ROM layout used by this platform. */
#ifdef MSB_FIRST
   m68000_set_reg(M68K_A7, (IPL[0x30000]<<24)|(IPL[0x30001]<<16)|(IPL[0x30002]<<8)|IPL[0x30003]);
   m68000_set_reg(M68K_PC, (IPL[0x30004]<<24)|(IPL[0x30005]<<16)|(IPL[0x30006]<<8)|IPL[0x30007]);
#else
   m68000_set_reg(M68K_A7, (IPL[0x30001]<<24)|(IPL[0x30000]<<16)|(IPL[0x30003]<<8)|IPL[0x30002]);
   m68000_set_reg(M68K_PC, (IPL[0x30005]<<24)|(IPL[0x30004]<<16)|(IPL[0x30007]<<8)|IPL[0x30006]);
#endif

   Memory_Init();
   CRTC_Init();
   DMA_Init();
   MFP_Init();
   FDC_Init();
   FDD_Reset();
   SASI_Init();
   SCSI_Init();
   IOC_Init();
   SCC_Init();
   PIA_Init();
   RTC_Init();
   TVRAM_Init();
   GVRAM_Init();
   BG_Init();
   Pal_Init();
   IRQH_Init();
   MIDI_Init();
   Keyboard_Init();

   m68000_ICountBk = 0;
   ICount = 0;

   DSound_Stop();
   SRAM_VirusCheck();
#if 0
   CDROM_Init();
#endif
   DSound_Play();
}

extern char filepath[MAX_PATH];
static int pmain(int argc, char *argv[])
{
	strcpy(winx68k_dir, retro_system_conf);
	sprintf(winx68k_ini, "%s%cconfig", retro_system_conf, SLASH);

   file_setcd(winx68k_dir);

   LoadConfig();

   /* if available, use retro_browse_conf to set StartDir config */
   if (retro_browse_conf[0] != 0)
      strcpy(filepath, retro_browse_conf);

   if (!WinDraw_MenuInit())
   {
      WinX68k_Cleanup();
      WinDraw_Cleanup();
      return 1;
   }

   StatBar_Show(Config.WindowFDDStat);
   WinUI_Init();

#if 0
   /* TODO: CLean this up */
   if (!WinX68k_Init())
   {
      WinX68k_Cleanup();
      WinDraw_Cleanup();
      return 1;
   }
#endif

   m68000_init();

   if (!WinX68k_LoadROMs())
   {
      WinX68k_Cleanup();
      WinDraw_Cleanup();
      return 1;
   }

   /* before moving to WinDraw_Init() */
   Keyboard_Init();
   WinDraw_Init();

   ADPCM_Init();
#if 0
   /* TODO: CLean this up */
   OPM_Init(4000000/*3579545*/);
#endif
#ifndef	NO_MERCURY
   Mcry_Init(winx68k_dir);
#endif

   FDD_Init();
   SysPort_Init();
   Mouse_Init();
   Joystick_Init();
   SRAM_Init();
   /* FIXME: actually, this sets initial register values rather than suppose to set soft-reset values */
   WinX68k_Reset();


   MIDI_Init();
   MIDI_SetMimpiMap(Config.ToneMapFile);	/* ToneMap file usage */
   MIDI_EnableMimpiDef(Config.ToneMap);

   ADPCM_SetVolume((uint8_t)Config.PCM_VOL);
   OPM_SetVolume((uint8_t)Config.OPM_VOL);
#ifndef	NO_MERCURY
   Mcry_SetVolume((uint8_t)Config.MCR_VOL);
#endif
   DSound_Play();

   /* apply defined command line settings */
   /* Detach any disk/HD image inherited from a previously loaded game.
    * With save_fdd_path / save_hdd_path enabled, LoadConfig() above
    * restored the previous game's FDD/HD images from the .ini (and the
    * global Config keeps them across loads anyway).  If we only overwrote
    * the single slot this game provides, a floppy game would still boot
    * the previous game's HDD image (SASI/SCSI boots before FDD), or keep
    * its second floppy in drive 1.  Clear everything first so ONLY the
    * media set from this game's command line below stays attached.
    * Skipped for argc==0 (no content) to preserve standalone behaviour. */
   if (argc >= 2)
   {
      int _d;
      for (_d = 0; _d < 2;  _d++) Config.FDDImage[_d][0] = '\0';
      for (_d = 0; _d < 16; _d++) Config.HDImage[_d][0]  = '\0';
   }

   if(argc == 3 && argv[1][0] == '-' && argv[1][1] == 'h')
      strcpy(Config.HDImage[0], argv[2]);
   else
   {
      switch (argc)
      {
         case 3:
            strcpy(Config.FDDImage[1], argv[2]);
         case 2:
            strcpy(Config.FDDImage[0], argv[1]);
            break;
         case 0:
            /* start menu when running without content */
            /* menu_mode = menu_enter; */
            break;
      }
   }

   FDD_SetFD(0, Config.FDDImage[0], 0);
   FDD_SetFD(1, Config.FDDImage[1], 0);

   return 1;
}

static int pre_main(void)
{
   int i = 0;
   int Only1Arg;

   for (i = 0; i < 64; i++)
      xargv_cmd[i] = NULL;

   if (no_content)
   {
      PARAMCOUNT = 0;
      goto run_pmain;
   }

   Only1Arg = (strcmp(ARGUV[0], "px68k") == 0) ? 0 : 1;

   if (Only1Arg)
   {
      int cfgload = 0;

      Add_Option("px68k");

      if (strlen(RPATH) >= strlen("hdf"))
      {
         if (!strcasecmp(&RPATH[strlen(RPATH) - strlen("hdf")], "hdf"))
         {
            Add_Option("-h");
            cfgload = 1;
         }
      }

      Add_Option(RPATH);
   }
   else
   {
      /* Pass all cmdline args */
      for (i = 0; i < ARGUC; i++)
         Add_Option(ARGUV[i]);
   }

   for (i = 0; i < PARAMCOUNT; i++)
      xargv_cmd[i] = (char*)(XARGV[i]);

run_pmain:
   pmain(PARAMCOUNT, (char **)xargv_cmd);

   if (PARAMCOUNT)
      xargv_cmd[PARAMCOUNT - 2] = NULL;
   
   return 0;
}

static void retro_set_controller_descriptors(void)
{
   static const char *default_labels[16] = {
      "A", "B", "X", "Y", "Select", "Start", "Right", "Left",
      "Up", "Down", "R", "L", "R2", "L2", "R3", "L3"
   };
   static const char *cyber_d_labels[16] = {
      "A", "B", "C", "D", "Select", "Start",
      "Right", "Left", "Up", "Down", "E2", "E1",
      NULL, NULL, NULL, NULL
   };
   static const char *cyber_a_labels[16] = {
      "A", "B", "C", "D", "Select", "Start",
      NULL, NULL, NULL, NULL, "E2", "E1",
      NULL, NULL, NULL, NULL
   };
   static char dynamic_labels[2][16][80];
   unsigned port;
   unsigned count = 0;

   memset(input_descs, 0, sizeof(input_descs));

   for (port = 0; port < 2; port++)
   {
      unsigned i;
      const char **labels = default_labels;
      if ((port == 0 && !joypad1) || (port == 1 && !joypad2))
         continue;

      switch (Config.JOY_TYPE[port])
      {
         case PAD_CYBERSTICK_D: labels = cyber_d_labels; break;
         case PAD_CYBERSTICK_A: labels = cyber_a_labels; break;
         default: break;
      }

      for (i = 0; i < 16 && count < 63; i++)
      {
         unsigned retropad_button = Joystick_GetJoy2KeyRetropadId(i);
         const char *label = labels[i];
         const char *native_label =
            Joystick_NativeLabel(Config.JOY_TYPE[port], retropad_button);

         /* CPSF labels come from the same table that drives the emulated
          * signals, so the descriptors always match the real behaviour. */
         if (native_label)
            label = native_label;

         {
            int action = Joystick_GetJoy2KeyAction(
                  Config.JOY_TYPE[port], retropad_button);
            if (action >= 0)
            {
               const char *key_label =
                  Joystick_GetKeyMappingLabel(port, (unsigned)action);
               if (action == 13 && !strcmp(key_label, "Menu"))
                  snprintf(dynamic_labels[port][i], sizeof(dynamic_labels[port][i]),
                        "PX68K Menu (J2K %s [Menu])",
                        Joystick_GetJoy2KeyActionName((unsigned)action));
               else
                  snprintf(dynamic_labels[port][i], sizeof(dynamic_labels[port][i]),
                        "J2K %s [%s]",
                        Joystick_GetJoy2KeyActionName((unsigned)action), key_label);
               label = dynamic_labels[port][i];
            }
         }

         input_descs[count].port = port;
         input_descs[count].device = RETRO_DEVICE_JOYPAD;
         input_descs[count].index = 0;
         input_descs[count].id = retropad_button;
         input_descs[count].description = label;
         count++;
      }

      if (port == 0 && Config.JOY_TYPE[port] == PAD_CYBERSTICK_A)
      {
         static const struct retro_input_descriptor cyber_analog_descs[] = {
            { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
               RETRO_DEVICE_ID_ANALOG_X, "CyberStick Stick X" },
            { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
               RETRO_DEVICE_ID_ANALOG_Y, "CyberStick Stick Y" },
            { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
               RETRO_DEVICE_ID_ANALOG_Y, "CyberStick Throttle (Right Y)" },
            { 0, 0, 0, 0, NULL }
         };
         unsigned j;
         for (j = 0; cyber_analog_descs[j].description && count < 63; j++)
         {
            input_descs[count] = cyber_analog_descs[j];
            count++;
         }
      }
   }

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &input_descs);
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   unsigned base_device;
   int joy_type = PAD_NONE;

   if (port >= 2)
      return;

   base_device = device & RETRO_DEVICE_MASK;

   if (base_device == RETRO_DEVICE_JOYPAD)
   {
      joy_type = PAD_2BUTTON;
      if (device == PX68K_DEVICE_CPSF_MD)
         joy_type = PAD_CPSF_MD;
      else if (device == PX68K_DEVICE_CPSF_SFC)
         joy_type = PAD_CPSF_SFC;
      else if (device == PX68K_DEVICE_CYBERSTICK_D)
         joy_type = PAD_CYBERSTICK_D;
      else if (device == PX68K_DEVICE_CYBERSTICK_A && port == 0)
         joy_type = PAD_CYBERSTICK_A;
      else if (device == PX68K_DEVICE_JOY2KEY)
         joy_type = PAD_JOY2KEY;
      else if (device != RETRO_DEVICE_JOYPAD &&
               device != PX68K_DEVICE_CPSF_MD &&
               device != PX68K_DEVICE_CPSF_SFC &&
               device != PX68K_DEVICE_CYBERSTICK_D &&
               device != PX68K_DEVICE_CYBERSTICK_A &&
               device != PX68K_DEVICE_JOY2KEY)
      {
         if (log_cb)
            log_cb(RETRO_LOG_ERROR, "[libretro]: Unknown joypad subclass %u, using RetroPad.\n", device);
      }
   }
   else if (device != RETRO_DEVICE_KEYBOARD && device != RETRO_DEVICE_NONE)
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "[libretro]: Unsupported controller device %u.\n", device);
      return;
   }

   controller_device[port] = device;
   controller_device_set[port] = true;
   Config.JOY_TYPE[port] = joy_type;
   controller_type[port] = joy_type;
   if (port == 0)
      joypad1 = (base_device == RETRO_DEVICE_JOYPAD);
   else
      joypad2 = (base_device == RETRO_DEVICE_JOYPAD);

   /* Changing one logical port's device must not reset the other port's
    * PIA strobe state. Joystick_Init() is reserved for core initialization. */
   retro_set_controller_descriptors();
}

/* The device chosen through the libretro controller interface is the single
 * source of truth for a logical port. If anything else overwrote
 * Config.JOY_TYPE, restore it so a port can never silently fall back to a
 * 2-button joystick. */
static void sync_controller_types(void)
{
   unsigned port;
   int changed = 0;

   for (port = 0; port < 2; port++)
   {
      if (!controller_device_set[port])
         continue;
      if (Config.JOY_TYPE[port] != controller_type[port])
      {
         Config.JOY_TYPE[port] = controller_type[port];
         changed = 1;
      }
   }
   if (changed)
      retro_set_controller_descriptors();
}

void retro_set_environment(retro_environment_t cb)
{
   int nocontent = 1;

   /* Controller types are listed per emulated port. Keep the generic
    * RetroPad first, followed by the supported device subclasses. Port 2
    * intentionally omits the analog Cyberstick, which is P1-only. */
   static const struct retro_controller_description port_controllers[2][8] = {
      {
         { "2-button joystick", RETRO_DEVICE_JOYPAD },
         { "CPSF-MD (8 Buttons)", PX68K_DEVICE_CPSF_MD },
         { "CPSF-SFC (8 Buttons)", PX68K_DEVICE_CPSF_SFC },
         { "Cyberstick (Digital)", PX68K_DEVICE_CYBERSTICK_D },
         { "Cyberstick (Analog)", PX68K_DEVICE_CYBERSTICK_A },
         { "Joy2Key Remap", PX68K_DEVICE_JOY2KEY },
         { "RetroKeyboard", RETRO_DEVICE_KEYBOARD },
         { "None", RETRO_DEVICE_NONE }
      },
      {
         { "2-button joystick", RETRO_DEVICE_JOYPAD },
         { "CPSF-MD (8 Buttons)", PX68K_DEVICE_CPSF_MD },
         { "CPSF-SFC (8 Buttons)", PX68K_DEVICE_CPSF_SFC },
         { "Cyberstick (Digital)", PX68K_DEVICE_CYBERSTICK_D },
         { "Joy2Key Remap", PX68K_DEVICE_JOY2KEY },
         { "RetroKeyboard", RETRO_DEVICE_KEYBOARD },
         { "None", RETRO_DEVICE_NONE }
      }
   };

   static const struct retro_controller_info ports[] = {
      { port_controllers[0], 8 },
      { port_controllers[1], 7 },
      { NULL, 0 }
   };

   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &nocontent);

   libretro_supports_option_categories = 0;
   libretro_set_core_options(cb, &libretro_supports_option_categories);

#ifdef USE_LIBRETRO_VFS
   {
      vfs_iface_info.required_interface_version = 1;
      vfs_iface_info.iface                      = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
      {
         filestream_vfs_init(&vfs_iface_info);
	      path_vfs_init(&vfs_iface_info);
         dirent_vfs_init(&vfs_iface_info);
      }
   }
#endif
}

static void update_variables(int running)
{
   int i = 0, snd_opt = 0;
   char key[256] = {0};
   struct retro_variable var = {0};

   update_variable_midi_interface(running);

#ifdef HAVE_CPU_RUNTIME
   /* CPU implementation is selected at boot; changing it requires a core restart. */
   if (!running)
   {
      struct retro_variable cpu_var = { "px68k_cpu_emulation", NULL };
      int use_musashi = 0;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &cpu_var) && cpu_var.value &&
            !strcmp(cpu_var.value, "Musashi (Slower)"))
         use_musashi = 1;
      m68000_set_emulation(use_musashi);
   }
#endif

   /* Internal MIDI emulation is an alternative MIDI destination, independent
    * of RetroArch's external MIDI interface. */
   {
      struct retro_variable emulation_var;
      struct retro_variable volume_var;
      int emulation = PX68K_MIDIEMU_DISABLED;
      int volume = 100;
      emulation_var.key = "px68k_midi_emulation";
      emulation_var.value = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &emulation_var) && emulation_var.value)
      {
         if (!strcmp(emulation_var.value, "sc55")) emulation = PX68K_MIDIEMU_SC55;
         else if (!strcmp(emulation_var.value, "mt32")) emulation = PX68K_MIDIEMU_MT32;
         else if (!strcmp(emulation_var.value, "cm32l")) emulation = PX68K_MIDIEMU_CM32L;
      }
      volume_var.key = "px68k_midiemu_volume";
      volume_var.value = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &volume_var) && volume_var.value)
         volume = atoi(volume_var.value);
      midi_emulation_selected = emulation;
      px68k_midiemu_set_volume(volume);
      px68k_midiemu_configure(emulation, retro_system_directory, retro_content_directory);
      if (emulation != PX68K_MIDIEMU_DISABLED && px68k_midiemu_enabled())
         Config.MIDI_SW = 1;
   }

   {
      unsigned port, input;
      int descriptors_changed = 0;
      char joy2key_option[40];
      for (port = 0; port < 2; port++)
         for (input = 0; input < 16; input++)
         {
            const char *option_name = Joystick_GetJoy2KeyOptionName(input);
            if (!option_name)
               continue;
            snprintf(joy2key_option, sizeof(joy2key_option),
                  "px68k_j2k_p%u_%s", port + 1, option_name);
            var.key = joy2key_option;
            var.value = NULL;
            if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
               descriptors_changed |= Joystick_SetKeyMapping(port, input, var.value);
         }
      if (descriptors_changed)
         retro_set_controller_descriptors();
   }

   var.key = "px68k_cpuspeed";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "10Mhz") == 0)
         Config.clockmhz = 10;
      else if (strcmp(var.value, "16Mhz") == 0)
         Config.clockmhz = 16;
      else if (strcmp(var.value, "25Mhz") == 0)
         Config.clockmhz = 25;
      else if (strcmp(var.value, "33Mhz (OC)") == 0)
         Config.clockmhz = 33;
      else if (strcmp(var.value, "66Mhz (OC)") == 0)
         Config.clockmhz = 66;
      else if (strcmp(var.value, "100Mhz (OC)") == 0)
         Config.clockmhz = 100;
   }

   var.key = "px68k_ramsize";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int value = 0;
      if (strcmp(var.value, "1MB") == 0)
         value = 1;
      else if (strcmp(var.value, "2MB") == 0)
         value = 2;
      else if (strcmp(var.value, "3MB") == 0)
         value = 3;
      else if (strcmp(var.value, "4MB") == 0)
         value = 4;
      else if (strcmp(var.value, "5MB") == 0)
         value = 5;
      else if (strcmp(var.value, "6MB") == 0)
         value = 6;
      else if (strcmp(var.value, "7MB") == 0)
         value = 7;
      else if (strcmp(var.value, "8MB") == 0)
         value = 8;
      else if (strcmp(var.value, "9MB") == 0)
         value = 9;
      else if (strcmp(var.value, "10MB") == 0)
         value = 10;
      else if (strcmp(var.value, "11MB") == 0)
         value = 11;
      else if (strcmp(var.value, "12MB") == 0)
         value = 12;

      Config.ram_size = (value * 1024 * 1024);
   }

   /* Read on every variable update: ADPCM_Update checks this each audio block. */
   var.key   = "px68k_adpcm_lpf";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      Config.Sound_LPF = strcmp(var.value, "disabled") != 0;

   var.key    = "px68k_adpcm_vol";
   var.value  = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      snd_opt = atoi(var.value);
      if (snd_opt != Config.PCM_VOL)
      {
         Config.PCM_VOL = snd_opt;
         ADPCM_SetVolume((uint8_t)Config.PCM_VOL);
      }
   }

   var.key    = "px68k_opm_vol";
   var.value  = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      snd_opt = atoi(var.value);
      if (snd_opt != Config.OPM_VOL)
      {
         Config.OPM_VOL = snd_opt;
         OPM_SetVolume((uint8_t)Config.OPM_VOL);
      }
   }

#ifndef NO_MERCURY
   var.key    = "px68k_mercury_vol";
   var.value  = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      snd_opt = atoi(var.value);
      if (snd_opt != Config.MCR_VOL)
      {
         Config.MCR_VOL = snd_opt;
         Mcry_SetVolume((uint8_t)Config.MCR_VOL);
      }
   }
#endif

   update_variable_disk_drive_swap();

   var.key    = "px68k_menufontsize";
   var.value  = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "normal") == 0)
         Config.MenuFontSize = 0;
      else
         Config.MenuFontSize = 1;
   }

   if (Config.MenuLinesJump < 5 || Config.MenuLinesJump > 50)
      Config.MenuLinesJump = 10;

   var.key    = "px68k_menu_lines_jump";
   var.value  = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int jump = atoi(var.value);
      if (jump >= 5 && jump <= 50 && jump % 5 == 0)
         Config.MenuLinesJump = jump;
   }

   {
      /* Keyboard repeat: follow the X68000's own setting unless overridden. */
      int repeat_delay = -1, repeat_interval = -1;

      var.key   = "px68k_key_repeat_delay";
      var.value = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         if (!strcmp(var.value, "off"))
            repeat_delay = 0;
         else if (strcmp(var.value, "machine"))
            repeat_delay = atoi(var.value);
      }
      var.key   = "px68k_key_repeat_interval";
      var.value = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         if (strcmp(var.value, "machine"))
            repeat_interval = atoi(var.value);
      }
      Keyboard_SetRepeatOptions(repeat_delay, repeat_interval);
   }

   var.key    = "px68k_save_fdd_path";
   var.value  = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         Config.save_fdd_path = 0;
      if (!strcmp(var.value, "enabled"))
         Config.save_fdd_path = 1;
   }

   var.key    = "px68k_save_hdd_path";
   var.value  = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         Config.save_hdd_path = 0;
      if (!strcmp(var.value, "enabled"))
         Config.save_hdd_path = 1;
   }

   var.key    = "px68k_rumble_on_disk_read";
   var.value  = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         opt_rumble_enabled = false;
      if (!strcmp(var.value, "enabled"))
         opt_rumble_enabled = true;
   }

   /* PX68K Menu */

   var.key      = "px68k_joy_mouse";
   var.value    = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      Config.Joy2Mouse = !strcmp(var.value, "enabled");

   var.key   = "px68k_joy2mouse_analog_speed";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      joy_mouse_analog_speed = (float)atoi(var.value);

   var.key   = "px68k_joy2mouse_analog_deadzone";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      joy_mouse_deadzone = (float)atof(var.value);

   var.key   = "px68k_joy2mouse_digital_speed";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      joy_mouse_digital_speed = (float)atoi(var.value);

   Joystick_SetMouseOptions(joy_mouse_analog_speed, joy_mouse_deadzone,
         joy_mouse_digital_speed);
   Mouse_StartCapture(1);

   var.key    = "px68k_frameskip";
   var.value  = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "Auto Frame Skip"))
         Config.FrameRate = 7;
      else if (!strcmp(var.value, "1/2 Frame"))
         Config.FrameRate = 2;
      else if (!strcmp(var.value, "1/3 Frame"))
         Config.FrameRate = 3;
      else if (!strcmp(var.value, "1/4 Frame"))
         Config.FrameRate = 4;
      else if (!strcmp(var.value, "1/5 Frame"))
         Config.FrameRate = 5;
      else if (!strcmp(var.value, "1/6 Frame"))
         Config.FrameRate = 6;
      else if (!strcmp(var.value, "1/8 Frame"))
         Config.FrameRate = 8;
      else if (!strcmp(var.value, "1/16 Frame"))
         Config.FrameRate = 16;
      else if (!strcmp(var.value, "1/32 Frame"))
         Config.FrameRate = 32;
      else if (!strcmp(var.value, "1/60 Frame"))
         Config.FrameRate = 60;
      else if (!strcmp(var.value, "Full Frame"))
         Config.FrameRate = 1;
   }

   var.key     = "px68k_aspect_ratio";
   var.value   = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "x68000"))
         px68k_aspect_mode = 0;
      else if (!strcmp(var.value, "1:1"))
         px68k_aspect_mode = 2;
      else
         px68k_aspect_mode = 1; /* "4:3" */
      /* WinDraw_Draw notices the new aspect and updates the geometry */
   }

   var.key     = "px68k_adjust_frame_rates";
   var.value   = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int temp = Config.AdjustFrameRates;
      if (!strcmp(var.value, "disabled"))
         Config.AdjustFrameRates = 0;
      else if (!strcmp(var.value, "enabled"))
         Config.AdjustFrameRates = 1;

      if (running) /* minimize the chance of resetting av_info during startup */
         CHANGEAV_TIMING = CHANGEAV_TIMING || Config.AdjustFrameRates != temp;
   }


   var.key   = "px68k_text_off";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         Debug_Text = 1;
      else if (!strcmp(var.value, "enabled"))
         Debug_Text = 0;
   }

   var.key   = "px68k_grp_off";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         Debug_Grp = 1;
      else if (!strcmp(var.value, "enabled"))
         Debug_Grp = 0;
   }

   var.key   = "px68k_sp_off";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         Debug_Sp = 1;
      else if (!strcmp(var.value, "enabled"))
         Debug_Sp = 0;
   }
}

/************************************
 * libretro implementation
 ************************************/

void retro_get_system_info(struct retro_system_info *info)
{
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
#ifndef PX68K_VERSION
#define PX68K_VERSION "0.15+"
#endif
   info->library_name     = "PX68K";
   info->library_version  = PX68K_VERSION GIT_VERSION;
   info->need_fullpath    = true;
   info->valid_extensions = "dim|img|d88|88d|hdm|dup|2hd|xdf|hdf|cmd|m3u|zip";
   /* The core reads ZIPs itself: left to the frontend, an archive is
    * unpacked and only its first disk image is passed on. */
   info->block_extract    = true;
}


void retro_get_system_av_info(struct retro_system_av_info *info)
{
   /* FIXME handle PAL/NTSC */
   struct retro_game_geometry geom;
   struct retro_system_timing timing = { FRAMERATE, SOUNDRATE };
   px68k_fill_geometry(&geom);
   info->geometry                    = geom;
   info->timing                      = timing;
}

/* Length of main RAM up to and including its last non-zero 4 KiB page.
 *
 * Fast (Run-Ahead) savestates copy 12 MiB of RAM twice per frame, yet most
 * of it is normally untouched zeros. Everything above this length is zero
 * by construction (it is verified here, not assumed from the RAM size
 * option or from which code paths write memory), so it can be skipped on
 * save and cleared on load. Reading is cheaper than copying, and the load
 * side is a plain memset. */
#define RAM_SCAN_PAGE 4096u

static uint32_t ram_used_length(const uint8_t *ram)
{
   uint32_t page = MEM_SIZE / RAM_SCAN_PAGE;

   while (page)
   {
      const uint64_t *w = (const uint64_t*)(ram + (size_t)(page - 1) * RAM_SCAN_PAGE);
      uint64_t acc      = 0;
      unsigned i;

      for (i = 0; i < RAM_SCAN_PAGE / sizeof(uint64_t); i++)
         acc |= w[i];

      if (acc)
         break;
      page--;
   }

   /* Never report 0: a zero-size entry is skipped by the writer, and a
    * label-less (fast) reader would then expect it and read the next
    * variable into RAM. One page keeps both sides in step. */
   return page ? page * RAM_SCAN_PAGE : RAM_SCAN_PAGE;
}

int StateAction(StateMem *sm, int load, int data_only)
{
   /* Normal states always store the whole array (same layout as before,
    * so they stay interchangeable with older builds). Fast states store
    * only the used part; on load VARLEN accepts either. */
   uint32_t ram_len = (!load && sm->fastsavestates && !sm->count_only)
      ? ram_used_length(MEM) : MEM_SIZE;

   SFORMAT StateRegs[] =
   {
      { MEM, load ? (uint32_t)MEM_SIZE : ram_len, PX68KSTATE_VARLEN, "RAM" },
      SFARRAYN(SRAM, 16384, "SRAM"),
      SFVAR(ICount),
      SFVAR(ClkUsed),
      SFVAR(VLINE),
      SFVAR(VLINE_TOTAL),

      SFVAR(tick),
      SFVAR(timercnt),

      SFEND
   };

   int ret = 0, count = 0;

   ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "MAIN", false);
   ret &= m68000_StateAction(sm, load, data_only);
   ret &= GVRAM_StateAction(sm, load, data_only);
   ret &= TVRAM_StateAction(sm, load, data_only);
   ret &= CRTC_StateAction(sm, load, data_only);
   ret &= Pal_StateAction(sm, load, data_only);
   ret &= BG_StateAction(sm, load, data_only);
   ret &= DMAC_StateAction(sm, load, data_only);
   ret &= MFP_StateAction(sm, load, data_only);
   ret &= IRQH_StateAction(sm, load, data_only);
   ret &= SCC_StateAction(sm, load, data_only);
   ret &= FDC_StateAction(sm, load, data_only);
   ret &= FDD_StateAction(sm, load, data_only);
   ret &= SASI_StateAction(sm, load, data_only);

   ret &= RTC_StateAction(sm, load, data_only);
   ret &= PIA_StateAction(sm, load, data_only);
   ret &= SysPort_StateAction(sm, load, data_only);
   ret &= IOC_StateAction(sm, load, data_only);
   ret &= SRAM_StateAction(sm, load, data_only);

   /* sound-related states */
   ret &= dswin_StateAction(sm, load, data_only);
   ret &= ADPCM_StateAction(sm, load, data_only);
   ret &= MIDI_StateAction(sm, load, data_only);

   /* What the synth was told is not part of the machine, but a real load needs
    * it to put the synth back (see midi_restore_after_load). Run-Ahead
    * states don't carry it. */
   if (!sm->fastsavestates)
   {
      if (!load)
         MIDIShadow_SetSource(midi_current_source());
      ret &= MIDIShadow_StateAction(sm, load, data_only);
   }
   ret &= OPM_StateAction(sm, load, data_only);

   return ret;
}

static int SavestateContext(void)
{
   int flags = RETRO_SAVESTATE_CONTEXT_NORMAL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT, &flags))
      return flags;
   return RETRO_SAVESTATE_CONTEXT_NORMAL;
}

static bool UsingFastSavestates(void)
{
   int flags = SavestateContext();
   return ((flags == RETRO_SAVESTATE_CONTEXT_RUNAHEAD_SAME_INSTANCE) ||
         ((flags == RETRO_SAVESTATE_CONTEXT_RUNAHEAD_SAME_BINARY)));
}

/* Sized in a dry run: no allocation and no copying. The result is the size
 * of a normal (labelled, full-RAM) state, which is an upper bound for a fast
 * state, so one buffer size serves every context. */
size_t retro_serialize_size(void)
{
   StateMem st;

   PX68KSS_InitCount(&st);

   if (!PX68KSS_SaveSM(&st, 0, 0, NULL, NULL, NULL))
      return 0;

   return st.len;
}

/* Writes straight into the frontend's buffer (it used to build the state in
 * a temporary buffer and copy it over, i.e. an extra malloc plus a full
 * memcpy of the whole state on every Run-Ahead frame). */
bool retro_serialize(void *data, size_t size)
{
   StateMem st;
   bool fast, ret;

   if (!data || size > 0xffffffffU)
      return false;

   fast = UsingFastSavestates();
   PX68KSS_InitWrite(&st, data, (uint32_t)size, fast);

   ret = PX68KSS_SaveSM(&st, 0, 0, NULL, NULL, NULL) != 0;

   /* Keep saved files deterministic. Fast states are never stored, so
    * skip clearing what can be megabytes of unused tail. */
   if (ret && !fast && st.len < size)
      memset((uint8_t*)data + st.len, 0, size - st.len);

   return ret;
}

bool retro_unserialize(const void *data, size_t size)
{
   StateMem st;
   bool fast;

   if (!data || size > 0xffffffffU)
      return false;

   /* The state's own header says whether this is a Run-Ahead rewind (fast) or
    * a state loaded for real. That is more reliable than the frontend's
    * context flag at the moment of the call. */
   fast = PX68KSS_IsFastState(data, size);

   if (!fast)
      midi_output_panic();

   PX68KSS_InitRead(&st, data, (uint32_t)size);

   if (!PX68KSS_LoadSM(&st, 0, 0))
      return false;

   /* A real load: the MIDI destination still holds whatever it was last told,
    * so put it back to what the state was using. */
   if (!fast)
      midi_restore_after_load();

   return true;
}

/* TODO/FIXME - implement cheats */
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned index, bool enabled, const char *code) { }

/* Mounts a ZIP's floppies (or, without any, its first HDF) through the
 * same px68k command line an M3U or HDF launch builds. */
static int load_zip_content(const char *path)
{
   char cmdline[MAX_PATH * 2 + 32];
   const char *name;
   int slot = px68k_zip_register_archive(path);
   int count;

   if (slot < 0)
      return 0;

   if ((count = zip_fill_disk_table(slot, 0)) > 0)
   {
      int archive_mode = get_archive_floppy_mode();
      int pair         = count > 1 && archive_mode != 2 && archive_mode != 3;

      disk.index       = 0;
      disk.inserted[0] = true;
      disk.inserted[1] = pair ? true : false;
      snprintf(RPATH, sizeof(RPATH), "%s", disk.path[0]);
      if (pair)
         snprintf(cmdline, sizeof(cmdline), "px68k \"%s\" \"%s\"",
               disk.path[0], disk.path[1]);
      else
         snprintf(cmdline, sizeof(cmdline), "px68k \"%s\"", disk.path[0]);
      parse_cmdline(cmdline);
      return 1;
   }

   if (!(name = px68k_zip_name(slot, 1, 0))
         || !px68k_zip_make_virtual_path(slot, name, RPATH, sizeof(RPATH))
         || !px68k_zip_preload(RPATH))
   {
      RPATH[0] = '\0';
      return 0;
   }

   snprintf(cmdline, sizeof(cmdline), "px68k -h \"%s\"", RPATH);
   parse_cmdline(cmdline);
   return 1;
}

static int is_zip_path(const char *path)
{
   size_t len = strlen(path);
   return len >= 4 && !strcasecmp(path + len - 4, ".zip");
}

bool retro_load_game(const struct retro_game_info *info)
{
   ResetConfigSaveState();
   px68k_zip_reset();
   no_content = 1;
   RPATH[0] = '\0';

   if (info && info->path)
   {
      const char *full_path = info->path;
      no_content            = 0;
      extract_directory(base_dir, info->path, sizeof(base_dir));
      if (is_zip_path(full_path))
      {
         if (!load_zip_content(full_path))
         {
            if (log_cb)
               log_cb(RETRO_LOG_ERROR, "[PX68K] ZIP contains no supported floppy or HDF images, or could not be read.\n");
            return false;
         }
      }
      else
         snprintf(RPATH, sizeof(RPATH), "%s", full_path);

      if (!retro_load_game_internal(RPATH))
         return false;
   }

   /* alloc memory pointers */
   if (!WinX68k_Init())
      return false;
   
   /* alloc OPM-related pointers */
   if (!OPM_Init(4000000/*3579545*/))
      return false;

   return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info
*info, size_t num_info) { return false; }

void retro_unload_game(void)
{
   int _d;

   RPATH[0]    = '\0';

   /* Persist the mounted paths before cleanup clears the live drive state.
    * retro_deinit() may run after this function, so SaveConfigBeforeUnload()
    * also prevents that later save from replacing the paths with empty ones. */
   SaveConfigBeforeUnload();

   /* Release the current content's floppy media before anything else.
    * retro_unload_game must return all 4 drives to the same clean state as
    * a fresh core boot; leaving it only to retro_deinit meant that when the
    * frontend reloads content (especially a multi-disk m3u) the disk image
    * buffers leaked and stale disks stayed "inserted" in FDD0..3.
    *   - FDD_Cleanup() ejects+frees every drive/format (DIM/D88/XDF), with
    *     write-back for modified images (idempotent: X_Eject no-ops on an
    *     already-empty drive, so a later retro_deinit->FDD_Cleanup is safe).
    *   - FDD_Init() zeroes the FDD hardware struct and the image pointers.
    *   - Config FDD/HD paths are cleared so nothing is re-mounted from a
    *     stale slot. */
   audio_buffer_reset();
   FDD_Cleanup();
   FDD_Init();
   for (_d = 0; _d < 2;  _d++) Config.FDDImage[_d][0] = '\0';
   for (_d = 0; _d < 16; _d++) Config.HDImage[_d][0]  = '\0';
   px68k_zip_reset();

   /* Re-arm the one-shot machine boot so the NEXT content load runs
    * pre_main() again (m68000_init + WinX68k_LoadROMs + WinX68k_Reset)
    * against freshly allocated buffers.  Salvia does
    * retro_unload_game() + retro_deinit() + retro_init() + retro_load_game()
    * between games; retro_deinit() frees MEM/IPL/FONT (WinX68k_Cleanup)
    * and retro_load_game() malloc's them again at new addresses.
    * Leaving firstcall at 0 skipped the re-boot, so the C68K fetch table
    * kept pointing at the previous (now freed) MEM -> access violation in
    * C68k_Exec on the first retro_run() of the second game.
    * ARGUC/PARAMCOUNT are one-shot accumulators as well: reset them so the
    * next command line is parsed and rebuilt from scratch instead of being
    * appended after the previous game's arguments. */
   firstcall   = 1;
   ARGUC       = 0;
   PARAMCOUNT  = 0;
   /* pre_main() decides whether the content came from a cmd/m3u (use the
    * parsed ARGUV) or is a single file (use RPATH) with
    *   Only1Arg = strcmp(ARGUV[0], "px68k") ? 1 : 0;
    * A cmd/m3u load leaves ARGUV[0] == "px68k".  A following single .dim/.hdf
    * never runs parse_cmdline, so without clearing it the stale "px68k" makes
    * Only1Arg 0 while ARGUC is 0 -> no options are built -> pmain() gets no
    * disk and the second game boots with an empty drive.  Clear ARGUV[0] so a
    * single-file load is detected correctly (a real cmd/m3u rewrites it). */
   ARGUV[0][0] = '\0';
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void *retro_get_memory_data(unsigned id)
{
   if ( id == RETRO_MEMORY_SYSTEM_RAM )
      return MEM;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   if ( id == RETRO_MEMORY_SYSTEM_RAM )
      return 0xc00000;
    return 0;
}

void retro_init(void)
{
   struct retro_log_callback log;
   struct retro_rumble_interface rumble;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   const char *system_dir      = NULL;
   const char *content_dir     = NULL;
   const char *save_dir        = NULL;
   const char *browse_dir      = NULL;

   retro_system_conf[0] = 0;
   retro_browse_conf[0] = 0;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;
   px68k_midiemu_set_log_callback(log_cb);

   /* if defined, use the system directory */
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
      retro_system_directory = system_dir;

   /* if defined, use the system directory */
   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
      retro_content_directory = content_dir;

   /* If save directory is defined use it, otherwise use system directory */
   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;
   else
      /* make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend */
      retro_save_directory = retro_system_directory;
   px68k_zip_set_save_directory(retro_save_directory);
   
   /* If browse directory is defined use it for StartDir config */
   if (environ_cb(RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY, &browse_dir) && browse_dir)
      retro_browse_directory = browse_dir;

   if (!retro_system_directory)
      strcpy(RETRO_DIR, ".");
   else
      strcpy(RETRO_DIR, retro_system_directory);

   sprintf(retro_system_conf, "%s%ckeropi", RETRO_DIR, SLASH);

   if (retro_browse_directory)
      strcpy(retro_browse_conf, retro_browse_directory);

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
      return;

   if (environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble) && rumble.set_rumble_state)
      rumble_cb = rumble.set_rumble_state;

   libretro_supports_input_bitmasks = 0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_input_bitmasks = 1;

   disk_swap_interface_init();
#if 0
    struct retro_keyboard_callback cbk = { keyboard_cb };
    environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &cbk);
#endif

   midi_interface_init();

   /* set sane defaults */
   Config.save_fdd_path = 1;
   Config.clockmhz      = 10;
   Config.ram_size      = 2 * 1024 *1024;

   update_variables(0);

   /* A frontend may send the controller selection before retro_init().
    * Reapply such selections after the core defaults are initialized.
    * Later changes are applied immediately by retro_set_controller_port_device(). */
   {
      unsigned port;
      for (port = 0; port < 2; port++)
         if (controller_device_set[port])
            retro_set_controller_port_device(port, controller_device[port]);
   }

   memset(Core_Key_State, 0, 512);
   memset(Core_old_Key_State, 0, sizeof(Core_old_Key_State));

   FRAMERATE = px68k_current_framerate();
   applied_frame_clk = CRTC_FrameClocks();

}

void retro_deinit(void)
{
   audio_buffer_reset();
   SRAM_UpdateBoot();

   OPM_Cleanup();
#ifndef	NO_MERCURY
   Mcry_Cleanup();
#endif

   Joystick_Cleanup();
   SRAM_Cleanup();
   FDD_Cleanup();
#if 0
   CDROM_Cleanup();
#endif
   MIDI_Cleanup();
   px68k_midiemu_shutdown();
   WinX68k_Cleanup();
   WinDraw_Cleanup();

   SaveConfig();
   px68k_zip_reset();
   libretro_supports_input_bitmasks    = 0;
   libretro_supports_midi_output       = 0;
   libretro_supports_option_categories = 0;
}

void retro_reset(void)
{
   audio_buffer_reset();
   WinX68k_Reset();
   if (Config.MIDI_SW && Config.MIDI_Reset)
	   MIDI_Reset();
}

static void rumble_frames(void)
{
   static int last_read_state;

   if (!rumble_cb)
      return;

   if (last_read_state != FDD_IsReading)
   {
      if (opt_rumble_enabled && FDD_IsReading)
      {
         rumble_cb(0, RETRO_RUMBLE_STRONG, 0x8000);
         rumble_cb(0, RETRO_RUMBLE_WEAK, 0x800);
      }
      else
      {
         rumble_cb(0, RETRO_RUMBLE_STRONG, 0);
         rumble_cb(0, RETRO_RUMBLE_WEAK, 0);
      }
   }

   last_read_state = FDD_IsReading;
}

typedef struct {
   uint16_t lrkey;
   uint8_t keycode;
} LRKCNV;

static const LRKCNV KeyTable[] = {
   { RETROK_ESCAPE,        0x01 }, /* ESC */
   { RETROK_1,             0x02 }, /* 1 ! */
   { RETROK_2,             0x03 }, /* 2 " */
   { RETROK_3,             0x04 }, /* 3 # */
   { RETROK_4,             0x05 }, /* 4 $ */
   { RETROK_5,             0x06 }, /* 5 % */
   { RETROK_6,             0x07 }, /* 6 & */
   { RETROK_7,             0x08 }, /* 7 ' */
   { RETROK_8,             0x09 }, /* 8 ( */
   { RETROK_9,             0x0a }, /* 9 ) */
   { RETROK_0,             0x0b }, /* 0 _ */
   { RETROK_MINUS,         0x0c }, /* - = */
   { RETROK_EQUALS,        0x0d }, /* ^ ~ */
   { RETROK_BACKSLASH,     0x0e }, /* ¥ | */ /* yen symbol */
   { RETROK_BACKSPACE,     0x0f }, /* BS */

   { RETROK_TAB,           0x10 }, /* TAB */
   { RETROK_q,             0x11 }, /* q Q */
   { RETROK_w,             0x12 }, /* e Q */
   { RETROK_e,             0x13 }, /* e E */
   { RETROK_r,             0x14 }, /* r R */
   { RETROK_t,             0x15 }, /* t T */
   { RETROK_y,             0x16 }, /* y Y */
   { RETROK_u,             0x17 }, /* u U */
   { RETROK_i,             0x18 }, /* i I */
   { RETROK_o,             0x19 }, /* o O */
   { RETROK_p,             0x1a }, /* p P */
   { RETROK_BACKQUOTE,     0x1b }, /* @ ` */
   { RETROK_LEFTBRACKET,   0x1c }, /*[] ] } */
   { RETROK_RETURN,        0x1d }, /* RETURN */

   { RETROK_a,             0x1e }, /* a A */
   { RETROK_s,             0x1f }, /* s S */
   { RETROK_d,             0x20 }, /* d D */
   { RETROK_f,             0x21 }, /* f F */
   { RETROK_g,             0x22 }, /* g G */
   { RETROK_h,             0x23 }, /* h H */
   { RETROK_j,             0x24 }, /* j J */
   { RETROK_k,             0x25 }, /* k K */
   { RETROK_l,             0x26 }, /* l L */
   { RETROK_SEMICOLON,     0x27 }, /* ; + */
   { RETROK_QUOTE,         0x28 }, /* : * */
   { RETROK_RIGHTBRACKET,  0x29 }, /* [ { */

   { RETROK_z,             0x2a }, /* z Z */
   { RETROK_x,             0x2b }, /* x X */
   { RETROK_c,             0x2c }, /* c C */
   { RETROK_v,             0x2d }, /* v V */
   { RETROK_b,             0x2e }, /* b B */
   { RETROK_n,             0x2f }, /* n N */
   { RETROK_m,             0x30 }, /* m M */
   { RETROK_COMMA,         0x31 }, /* , < */
   { RETROK_PERIOD,        0x32 }, /* . > */
   { RETROK_SLASH,         0x33 }, /* / ? */
   { RETROK_0,             0x34 }, /* underquote _ as shift+0 which was empty, Japanese
                                    chars can't overlap as we're not using them */

   { RETROK_SPACE,         0x35 }, /* SPACE */
   { RETROK_HOME,          0x36 }, /* HOME */
   { RETROK_DELETE,        0x37 }, /* DEL */
   { RETROK_PAGEDOWN,      0x38 }, /* ROLL UP */
   { RETROK_PAGEUP,        0x39 }, /* ROLL DOWN */
   { RETROK_END,           0x3a }, /* UNDO */
   { RETROK_LEFT,          0x3b }, /* ← */
   { RETROK_UP,            0x3c }, /* ↑ */
   { RETROK_RIGHT,         0x3d }, /* → */
   { RETROK_DOWN,          0x3e }, /* ↓ */

   { RETROK_CLEAR,         0x3f }, /* CLR */
   { RETROK_KP_DIVIDE,     0x40 }, /* / */
   { RETROK_KP_MULTIPLY,   0x41 }, /* * */
   { RETROK_KP_MINUS,      0x42 }, /* - */
   { RETROK_KP7,           0x43 }, /* 7 */
   { RETROK_KP8,           0x44 }, /* 8 */
   { RETROK_KP9,           0x45 }, /* 9 */
   { RETROK_KP_PLUS,       0x46 }, /* + */
   { RETROK_KP4,           0x47 }, /* 4 */
   { RETROK_KP5,           0x48 }, /* 5 */
   { RETROK_KP6,           0x49 }, /* 6 */
   { RETROK_KP_EQUALS,     0x4a }, /* = */
   { RETROK_KP1,           0x4b }, /* 1 */
   { RETROK_KP2,           0x4c }, /* 2 */
   { RETROK_KP3,           0x4d }, /* 3 */
   { RETROK_KP_ENTER,      0x4e }, /* ENTER */
   { RETROK_KP0,           0x4f }, /*  */
#if 0
   { RETROK_COMMA,0x50 },  /* . > */
#endif
   { RETROK_KP_PERIOD,     0x51 }, /* . */

   { RETROK_PRINT,         0x52 }, /* symbol input (kigou) */
   { RETROK_SCROLLOCK,     0x53 }, /* registration (touroku) */
   { RETROK_F11,           0x54 }, /* HELP */
#if 0
   { NC,                   0x55 }, /* XF1 */
   { NC,                   0x56 }, /* XF2 */
   { NC,                   0x57 }, /* XF3 */
   { NC,                   0x58 }, /* XF4 */
   { NC,                   0x59 }, /* XF5 */

   { NC,                   0x5a }, /* KANA */
   { NC,                   0x5b }, /* ROMAN Alphabet */
   { NC,                   0x5c }, /* Enter code */
#endif
   { RETROK_CAPSLOCK,      0x5d }, /* CAPSLOCK */

   { RETROK_INSERT,        0x5e }, /* INSERT */
#if 0
   { NC,                   0x5f }, /* Hiragana */
   { NC,                   0x60 }, /* Full-width */
#endif
   { RETROK_BREAK,         0x61 },  /* BREAK */
   { RETROK_PAUSE,         0x61 },  /* BREAK (allow shift+break combo) */
#if 0
   { NC,                   0x62 }, /* COPY */
#endif
   { RETROK_F1,            0x63},  /* F1 */
   { RETROK_F2,            0x64},  /* F2 */
   { RETROK_F3,            0x65},  /* F3 */
   { RETROK_F4,            0x66},  /* F4 */
   { RETROK_F5,            0x67},  /* F5 */
   { RETROK_F6,            0x68},  /* F6 */
   { RETROK_F7,            0x69},  /* F7 */
   { RETROK_F8,            0x6a},  /* F8 */
   { RETROK_F9,            0x6b},  /* F9 */
   { RETROK_F10,           0x6c},  /* F10 */

#if 0
   { NC,                   0x6d }, /* unused */
   { NC,                   0x6e }, /* unused */
   { NC,                   0x6f }, /* unused */
#endif

   { RETROK_LSHIFT,        0x70 }, /* SHIFT */
   { RETROK_RSHIFT,        0x70 }, /* SHIFT */
   { RETROK_LCTRL,         0x71 }, /* CTRL */
   { RETROK_RCTRL,         0x71 }, /* CTRL */
   { RETROK_LSUPER,        0x72 }, /* OPT.1 */
   { RETROK_RSUPER,        0x73 }, /* OPT.2 */
   { RETROK_LALT,          0x72 }, /* OPT.1 */
   { RETROK_RALT,          0x73 }, /* OPT.2 */
};

static void handle_retrok(void)
{
   int i;

#define KEYP(a, b)                                                            \
   {                                                                          \
      if (Core_Key_State[a] && Core_Key_State[a] != Core_old_Key_State[a])    \
      {                                                                       \
         log_cb(RETRO_LOG_DEBUG, "KeyDown: lrkey = %3d code = %02x\n", a, b); \
         send_keycode(b, 2);                                                  \
      }                                                                       \
      else if (!Core_Key_State[a] && Core_Key_State[a] != Core_old_Key_State[a]) \
      {                                                                       \
         log_cb(RETRO_LOG_DEBUG, "KeyUp: lrkey = %3d code = %02x\n", a, b);   \
         send_keycode(b, 1);                                                  \
      }                                                                       \
   }

   if(Core_Key_State[RETROK_F12] && Core_Key_State[RETROK_F12]!=Core_old_Key_State[RETROK_F12]  )
   {
      if (menu_mode == menu_out)
      {
         oldrw     = retrow;
         oldrh     = retroh;
         retrow    = 800;
         retroh    = 600;
         CHANGEAV  = 1;
         menu_mode = menu_enter;
         midi_output_panic();
         audio_buffer_reset();
         DSound_Stop();
      }
      else
      {
         CHANGEAV  = 1;
         retrow    = oldrw;
         retroh    = oldrh;
         audio_buffer_reset();
         DSound_Play();
         menu_mode = menu_out;
      }
   }

   if (Core_Key_State[RETROK_COMPOSE])
   {
      static const LRKCNV KeyTable1[] = {
         { RETROK_F1,      0x55 }, /* XF1 */
         { RETROK_F2,      0x56 }, /* XF2 */
         { RETROK_F3,      0x57 }, /* XF3 */
         { RETROK_F4,      0x58 }, /* XF4 */
         { RETROK_F5,      0x59 }, /* XF5 */
         { RETROK_F6,      0x5a }, /* KANA */
         { RETROK_F7,      0x5b }, /* ROMAN Alphabet */
         { RETROK_F8,      0x5c }, /* Enter code */
         { RETROK_F9,      0x5f }, /* Hiragana */
         { RETROK_F10,     0x60 }, /* Full-width */
      };
      int i;
      for (i = 0; i < NELEMENTS(KeyTable1); i++)
      {
         KEYP(KeyTable1[i].lrkey, KeyTable1[i].keycode);
      }
   }
   else
   {
      for (i = 0; i < NELEMENTS(KeyTable); i++)
         KEYP(KeyTable[i].lrkey, KeyTable[i].keycode);
   }
   
#undef KEYP
}

#define CLOCK_SLICE 200

/*  Core Main Loop */
static void WinX68k_Exec(void)
{
   int clk_total, clkdiv, usedclk, hsync, clk_next, clk_count, clk_line=0;
   int KeyIntCnt = 0, MouseIntCnt = 0;
   uint32_t t_start = timeGetTime(), t_end;

   if(!(cpu_readmem24_dword(0xed0008) == Config.ram_size))
   {
      cpu_writemem24(0xe8e00d, 0x31); /* SRAM write permission */
      cpu_writemem24_dword(0xed0008, Config.ram_size); /* Define RAM amount */
   }

   if (Config.FrameRate != 7)
      DispFrame = (DispFrame + 1) % Config.FrameRate;
   else
   {				/* Auto Frame Skip */
      if (FrameSkipQueue)
      {
         if (FrameSkipCount > 15)
         {
            FrameSkipCount = 0;
            FrameSkipQueue++;
            DispFrame      = 0;
         }
         else
         {
            FrameSkipCount++;
            FrameSkipQueue--;
            DispFrame      = 1;
         }
      }
      else
      {
         FrameSkipCount    = 0;
         DispFrame         = 0;
      }
   }

   vline     = 0;
   clk_count = -ICount;
   clk_total = (int)CRTC_FrameClocks();

   clk_total = (clk_total*Config.clockmhz)/10;
   clkdiv    = Config.clockmhz;

#if 0
   if (Config.XVIMode == 1)
   {
      clk_total = (clk_total * 16) / 10;
      clkdiv    = 16;
   }
   else if (Config.XVIMode == 2)
   {
      clk_total = (clk_total * 24) / 10;
      clkdiv    = 24;
   }
   else
      clkdiv    = 10;
#endif

   if(clkdiv != old_clkdiv || Config.ram_size != old_ram_size)
   {
      old_clkdiv = clkdiv;
      old_ram_size = Config.ram_size;
   }

   ICount  += clk_total;
   clk_next = (clk_total/VLINE_TOTAL);
   hsync    = 1;

   do
   {
      int m, n = (ICount > CLOCK_SLICE) ? CLOCK_SLICE : ICount;

      if ( hsync )
      {
         hsync    = 0;
         clk_line = 0;
         MFP_Int(0);
         if ((vline >= CRTC_VSTART) && (vline < CRTC_VEND))
            VLINE = ((vline - CRTC_VSTART) * CRTC_VStep) / 2;
         else
            VLINE = (uint32_t)-1;
         if ((!(MFP[MFP_AER] & 0x40)) && (vline == CRTC_IntLine))
            MFP_Int(1);
         if (MFP[MFP_AER] & 0x10)
         {
            if (vline == CRTC_VSTART)
               MFP_Int(9);
         }
         else
         {
            if (CRTC_VEND >= VLINE_TOTAL)
            {
               if ((long)vline == (CRTC_VEND - VLINE_TOTAL))
                  MFP_Int(9);		/* Is it Exciting Hour? （TOTAL<VEND） */
            }
            else
            {
               /* V-DISP (GPIP4) falling edge = end of the display period.
                * MFP_TimerA() (event-count mode) counts the very same edge
                * at vline == CRTC_VEND, so the GPIP4 interrupt must fire there
                * too.  Firing it at VLINE_TOTAL-1 delays it by the whole
                * vertical blank and lets a game's main loop (which is woken
                * by the Timer-A event) start modifying its sprite buffer/PCG
                * before the V-DISP handler has copied the buffer to sprite
                * RAM (The New Zealand Story). */
               if ((long)vline == (long)CRTC_VEND)
                  MFP_Int(9);
            }
         }
      }

      {
         m68000_execute(n);
         m          = (n-m68000_ICountBk);
         ClkUsed   += m*10;
         usedclk    = ClkUsed/clkdiv;
         clk_line  += usedclk;
         ClkUsed   -= usedclk*clkdiv;
         ICount    -= m;
         clk_count += m;
      }

      MFP_Timer(usedclk);
      RTC_Timer(usedclk);
      DMA_Exec(0);
      DMA_Exec(1);
      DMA_Exec(2);

      if (clk_count >= clk_next)
      {
         /* MIDI output is now paced by the emulated 31.25-kbit/s UART in MIDI_Timer. */
         MFP_TimerA();
         if ((MFP[MFP_AER] & 0x40) && (vline == CRTC_IntLine))
            MFP_Int(1);
         if ( (!DispFrame) && (vline >= CRTC_VSTART) && (vline < CRTC_VEND))
         {
            if ( CRTC_VStep==1 )
            {
               /* HighReso 256dot (read twice) */
               if ( vline%2 )
                  WinDraw_DrawLine();
            }
            else if (CRTC_VStep == 4)
            {
               /* LowReso 512dot
                * draw twice per scanline (interlace) */
               WinDraw_DrawLine();
               VLINE++;
               WinDraw_DrawLine();
            }
            else /* High 512dot / Low 256dot */
               WinDraw_DrawLine();
         }

         ADPCM_PreUpdate(clk_line);
         OPM_Timer(clk_line);
         MIDI_Timer(clk_line);
#ifndef	NO_MERCURY
         Mcry_PreUpdate(clk_line);
#endif

         KeyIntCnt++;
         if (KeyIntCnt > (VLINE_TOTAL/4))
         {
            KeyIntCnt = 0;
            Keyboard_Int();
         }
         MouseIntCnt++;
         if (MouseIntCnt > (VLINE_TOTAL / 8))
         {
            MouseIntCnt = 0;
            SCC_IntCheck();
         }
         DSound_Send0(fb_audio_clock(clk_line));

         vline++;
         clk_next  = (clk_total*(vline+1))/VLINE_TOTAL;
         hsync     = 1;
      }
   } while (vline < VLINE_TOTAL);

   if (CRTC_Mode & 2)
   {
      /* FastClr byte adjustment (PITAPAT) */
      if (CRTC_FastClr)
      {
         /* if FastClr=1 and CRTC_Mode&2 then end */
         CRTC_FastClr--;
         if (!CRTC_FastClr)
            CRTC_Mode &= 0xfd;
      }
      else
      {
         /* FastClr start */
         if (CRTC_Regs[0x29] & 0x10)
            CRTC_FastClr = 1;
         else
            CRTC_FastClr = 2;
         TVRAM_SetAllDirty();
         GVRAM_FastClear();
      }
   }

   FDD_SetFDInt();
   if (!DispFrame)
      WinDraw_Draw();

   t_end = timeGetTime();
   if ((int)(t_end - t_start) > (CRTC_HighFreq() ? 14 : 16))
   {
      FrameSkipQueue += ((t_end - t_start) / (CRTC_HighFreq() ? 14 : 16)) + 1;
      if (FrameSkipQueue > 100)
         FrameSkipQueue = 100;
   }
}

void retro_run(void)
{
   int i;
   int mouse_x, mouse_y, mouse_l, mouse_r;
   bool updated    = false;
   static bool mbL = false, mbR = false;
   static uint32_t midi_last_run_time = 0;
   uint32_t midi_now = timeGetTime();

   /* RetroArch can stop calling retro_run while paused. On resume, silence
    * voices left active by the external MIDI device. The threshold is much
    * longer than a normal frame and does not affect rapid Run-Ahead replays. */
   if (midi_last_run_time && (uint32_t)(midi_now - midi_last_run_time) > 250)
      midi_output_panic();
   midi_last_run_time = midi_now;

   if (firstcall)
   {
      pre_main();
      firstcall     = 0;
      /* Initialization done */
      update_variables(0);
      audio_frame_fraction = 0.0;
      soundbuf_size = next_audio_frame_size();
      audio_buffer_reset();
      return;
   }

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables(1);

   /* Process frontend AV changes after emulation, immediately before
    * video_cb(), so dimensions correspond to the frame being submitted. */
   input_poll_cb();
   rumble_frames();

   FDD_IsReading = 0;

   if (menu_mode == menu_out)
   {
      Joystick_Update(0, -1, 0);
      Joystick_Update(0, -1, 1);
      Keyboard_OSDPointerUpdate();
      if (FRAMERATE > 0.0)
         Keyboard_RepeatTick((unsigned)(1000000.0 / FRAMERATE));

      WinX68k_Exec();
   }
   else
      Keyboard_OSDReleaseAll(); /* PX68K menu: no OSD key may stay held */

   /* Physical mouse input remains available. Joy2Mouse adds P1 controller
    * movement as a second source without consuming native joystick buttons. */
   mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
   mouse_y = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
   /* While the OSD keyboard is open, touch/mouse input belongs to it:
    * the emulated mouse stays idle (Android maps touches to the mouse). */
   if (menu_mode == menu_out && Keyboard_IsSwKeyboard())
      mouse_x = mouse_y = 0;
   Mouse_Event(0, mouse_x, mouse_y);

   if (Config.Joy2Mouse && menu_mode == menu_out && !Keyboard_IsSwKeyboard())
   {
      float joy_mouse_x, joy_mouse_y;
      Joystick_GetMouseDelta(0, &joy_mouse_x, &joy_mouse_y);
      Mouse_Event(0, joy_mouse_x, joy_mouse_y);
   }

   {
      mouse_l = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
      mouse_r = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
      if (menu_mode == menu_out && Keyboard_IsSwKeyboard())
         mouse_l = mouse_r = 0;

      if (!mbL && mouse_l)
      {
         mbL = true;
         Mouse_Event(1, 1.0f, 0.0f);
      }
      else if (mbL && !mouse_l)
      {
         mbL = false;
         Mouse_Event(1, 0.0f, 0.0f);
      }
      if (!mbR && mouse_r)
      {
         mbR = true;
         Mouse_Event(2, 1.0f, 0.0f);
      }
      else if (mbR && !mouse_r)
      {
         mbR = false;
         Mouse_Event(2, 0.0f, 0.0f);
      }
   }

   for(i = 0; i < 320; i++)
      Core_Key_State[i] = input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, i) ? 0x80: 0;


   sync_controller_types();

   /* The PX68K menu is a Joy2Key target (default: RetroPad L2) and works
    * from either logical port. */
   if (Joystick_IsMenuPressed())
      Core_Key_State[RETROK_F12] = 0x80;


   if(memcmp( Core_Key_State,Core_old_Key_State , sizeof(Core_Key_State) ) )
      handle_retrok();

   memcpy(Core_old_Key_State,Core_Key_State , sizeof(Core_Key_State) );

   if (menu_mode != menu_out)
   {
      int ret = 0;
      int key = 0;
      if (Core_Key_State[RETROK_RIGHT] || Core_Key_State[RETROK_PAGEDOWN])
         key |= JOY_RIGHT;
      if (Core_Key_State[RETROK_LEFT] || Core_Key_State[RETROK_PAGEUP])
         key |= JOY_LEFT;
      if (Core_Key_State[RETROK_UP])
         key |= JOY_UP;
      if (Core_Key_State[RETROK_DOWN])
         key |= JOY_DOWN;
      if (Core_Key_State[RETROK_z] || Core_Key_State[RETROK_RETURN])
         key |= JOY_TRG1;
      if (Core_Key_State[RETROK_x] || Core_Key_State[RETROK_BACKSPACE])
         key |= JOY_TRG2;

      Joystick_Update(1, key, 0);

      ret       = WinUI_Menu(menu_mode == menu_enter);
      menu_mode = menu_in;
      if (ret == WUM_MENU_END)
      {
         audio_buffer_reset();
         DSound_Play();
         menu_mode = menu_out;
      }
   }

   /* The paused internal menu must not advance any synthesizer. */
   if (soundbuf_size < 0) soundbuf_size = 0;
   if (soundbuf_size > (int)(sizeof(soundbuf) / (2 * sizeof(soundbuf[0]))))
      soundbuf_size = (int)(sizeof(soundbuf) / (2 * sizeof(soundbuf[0])));
   if (menu_mode != menu_out)
      memset(soundbuf, 0, (size_t)soundbuf_size * 4);
   else
   {
      raudio_callback(soundbuf, NULL, soundbuf_size << 2);

      px68k_midiemu_mix(soundbuf, (size_t)soundbuf_size);
   }

   if (libretro_supports_midi_output && midi_cb.output_enabled())
      midi_cb.flush();

   audio_batch_cb((const int16_t*)soundbuf, soundbuf_size);
   /* TODO/FIXME - hardcoded pitch here */
   {
      /* Refresh-rate change detection: the new rate must persist for 3
       * frames (mode switches write several registers, sometimes across
       * frames) and differ by more than 0.1% before the frontend is told. */
      static uint32_t pending_clk = 0;
      static int stable_frames = 0;
      uint32_t c = CRTC_FrameClocks();
      uint32_t d = c > applied_frame_clk ? c - applied_frame_clk : applied_frame_clk - c;
      if (applied_frame_clk && d * 1000u > applied_frame_clk)
      {
         if (c == pending_clk)
            stable_frames++;
         else
         {
            pending_clk = c;
            stable_frames = 1;
         }
         if (stable_frames >= 3)
         {
            CHANGEAV_TIMING = 1;
            stable_frames = 0;
         }
      }
      else
         stable_frames = 0;
   }
   if (CHANGEAV || CHANGEAV_TIMING)
   {
      struct retro_system_av_info av;
      if (CHANGEAV_TIMING)
      {
         FRAMERATE = px68k_current_framerate();
         applied_frame_clk = CRTC_FrameClocks();
         audio_frame_fraction = 0.0;
         retro_get_system_av_info(&av);
         environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);

         CHANGEAV_TIMING = 0;
         CHANGEAV = 0;
      }
      else
      {
         struct retro_game_geometry geom;
         /* While the keyboard is up its own geometry stays in force; the
          * latest game size/aspect is applied when it closes. */
         if (!osd_video_geometry_active)
         {
            px68k_fill_geometry(&geom);
            environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geom);
         }
         CHANGEAV = 0;
      }
   }
   soundbuf_size = next_audio_frame_size();
   {
      bool osd_visible = (menu_mode == menu_out) && Keyboard_IsSwKeyboard();
      if (osd_visible)
      {
         struct retro_variable osd_layout_var = {
            "px68k_osd_keyboard_layout", NULL
         };
         int fullscreen_layout = 1;
         if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &osd_layout_var) &&
               osd_layout_var.value &&
               !strcmp(osd_layout_var.value, "game_above"))
            fullscreen_layout = 0;
         {
            struct retro_variable opacity_var = {
               "px68k_osd_keyboard_opacity", NULL
            };
            struct retro_variable theme_var = {
               "px68k_osd_keyboard_theme", NULL
            };
            int opacity = 100, dark = 1, look3d = 1;
            if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &opacity_var) &&
                  opacity_var.value)
               opacity = atoi(opacity_var.value);
            if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &theme_var) &&
                  theme_var.value && strstr(theme_var.value, "light"))
               dark = 0;
            if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &theme_var) &&
                  theme_var.value && strstr(theme_var.value, "flat"))
               look3d = 0;
            WinDraw_SetOSDKeyboardStyle(opacity, dark, look3d);
            {
               struct retro_variable tint_var = {
                  "px68k_osd_keyboard_cursor_tint", NULL
               };
               struct retro_variable frame_var = {
                  "px68k_osd_keyboard_cursor_frame", NULL
               };
               const char *tint = "fff199", *frame = "ffffff";
               if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &tint_var) &&
                     tint_var.value)
                  tint = tint_var.value;
               if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &frame_var) &&
                     frame_var.value)
                  frame = frame_var.value;
               WinDraw_SetOSDKeyboardCursor(tint, frame);
            }
            {
               struct retro_variable hover_var = {
                  "px68k_osd_keyboard_hover", NULL
               };
               struct retro_variable fb_var = {
                  "px68k_osd_keyboard_feedback", NULL
               };
               int hover = 1, feedback = 2;
               if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &hover_var) &&
                     hover_var.value && !strcmp(hover_var.value, "disabled"))
                  hover = 0;
               if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &fb_var) &&
                     fb_var.value)
               {
                  if (!strcmp(fb_var.value, "off"))
                     feedback = 0;
                  else if (!strcmp(fb_var.value, "flash"))
                     feedback = 1;
               }
               {
                  struct retro_variable lbl_var = {
                     "px68k_osd_keyboard_labels", NULL
                  };
                  int japanese = 0;
                  if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &lbl_var) &&
                        lbl_var.value && !strcmp(lbl_var.value, "japanese"))
                     japanese = 1;
                  WinDraw_SetOSDKeyboardLabels(japanese);
               }
               Keyboard_SetOSDHover(hover);
               WinDraw_SetOSDKeyboardFeedback(feedback);
            }
         }
         WinDraw_DrawOSDKeyboard(osd_video_buffer, videoBuffer,
               retrow, retroh, fullscreen_layout);
      }

      /* Keep the frontend viewport at the keyboard's 768x512 reference
       * resolution while it is visible. retrow/retroh continue tracking the
       * emulated mode, so closing the keyboard restores the latest game size. */
      if (osd_visible != osd_video_geometry_active)
      {
         struct retro_game_geometry geom;
         osd_video_geometry_active = osd_visible;
         /* closing the keyboard restores the current game size AND aspect */
         px68k_fill_geometry(&geom);
         environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geom);
      }

      if (osd_visible)
         video_cb(osd_video_buffer, OSD_VIDEO_WIDTH, OSD_VIDEO_HEIGHT, 800 << 1);
      else
         video_cb(videoBuffer, retrow, retroh, 800 << 1);
   }
}
