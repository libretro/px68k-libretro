
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include <libretro.h>
#include <libretro_core_options.h>
#include <string/stdstring.h>

#include "libretro/winx68k.h"
#include "libretro/dswin.h"
#include "libretro/keyboard.h"
#include "libretro/prop.h"
#include "libretro/mouse.h"
#include "fmgen/fmg_wrap.h"
#include "x68k/adpcm.h"
#include "x68k/fdd.h"
#include "x68k/x68kmemory.h"
#ifndef NO_MERCURY
#include "x68k/mercury.h"
#endif

#ifdef _WIN32
char slash = '\\';
#else
char slash = '/';
#endif

#define MODE_HIGH 55.5  /* 31.50 kHz - commonly used  */
#define MODE_NORM 59.94 /* 15.98 kHz - actual value should be ~61.46 fps. this is lowered to
                     * reduced the chances of audio stutters due to mismatch
                     * fps when vsync is used since most monitors are only capable
                     * of upto 60Hz refresh rate. */

#define SOUNDRATE 44100.0
#define SNDSZ round(SOUNDRATE / FRAMERATE)

static char RPATH[512];
static char RETRO_DIR[512];
static const char *retro_save_directory;
static const char *retro_system_directory;
const char *retro_content_directory;
char retro_system_conf[512];
char base_dir[MAX_PATH];

char Core_Key_State[512];
char Core_old_Key_State[512];

static bool joypad1, joypad2;

static bool opt_analog;

int retrow = 800;
int retroh = 600;
int CHANGEAV = 0;
int CHANGEAV_TIMING = 0; /* Separate change of geometry from change of refresh rate */
int VID_MODE = 1;
float FRAMERATE = MODE_NORM;
DWORD libretro_supports_input_bitmasks = 0;

static signed short soundbuf[1024 * 2];
static int soundbuf_size;

uint16_t *videoBuffer;

static retro_video_refresh_t video_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_set_rumble_state_t rumble_cb;
static unsigned no_content;

static int opt_rumble_enabled = 1;

/* .dsk swap support */
struct disk_control_interface_t
{
   unsigned dci_version;  /* disk control interface version, 0 = use old interface */
   unsigned total_images; /* total number if disk images */
   unsigned index;        /* currect disk index */
   unsigned drive;        /* current active drive */
   bool inserted[2];      /* tray state for FDD0/FDD1, 0 = disk ejected, 1 = disk inserted */

   unsigned char path[10][MAX_PATH];   /* disk image paths */
   unsigned char label[10][MAX_PATH];  /* disk image base name w/o extension */

   unsigned g_initial_disc;                     /* initial disk index */
   unsigned char g_initial_disc_path[MAX_PATH]; /* initial disk path */
};

static struct disk_control_interface_t disk;
static struct retro_disk_control_callback dskcb;
static struct retro_disk_control_ext_callback dskcb_ext;

static void update_variables(void);

static bool is_path_absolute(const char* path)
{
   if (path[0] == slash)
      return true;

#ifdef _WIN32
   if ((path[0] >= 'a' && path[0] <= 'z') ||
      (path[0] >= 'A' && path[0] <= 'Z'))
   {
      if (path[1] == ':')
         return true;
   }
#endif
   return false;
}

static void extract_basename(char *buf, const char *path, size_t size)
{
   const char *base = strrchr(path, '/');
   if (!base)
      base = strrchr(path, '\\');
   if (!base)
      base = path;

   if (*base == '\\' || *base == '/')
      base++;

   strncpy(buf, base, size - 1);
   buf[size - 1] = '\0';

   char *ext = strrchr(buf, '.');
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

static void update_disk_drive_swap(void)
{
   struct retro_variable var =
   {
      "px68k_disk_drive",
      NULL
   };

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "FDD0") == 0)
         disk.drive = 0;
      else
         disk.drive = 1;
   }
}

static bool set_eject_state(bool ejected)
{
   if(disk.index == disk.total_images)
   {
      //retroarch is trying to set "no disk in tray"
      return true;
   }

   if (ejected)
   {
      FDD_EjectFD(disk.drive);
      Config.FDDImage[disk.drive][0] = '\0';
   }
   else
   {
      strcpy(Config.FDDImage[disk.drive], disk.path[disk.index]);
      FDD_SetFD(disk.drive, Config.FDDImage[disk.drive], 0);
   }
   disk.inserted[disk.drive] = !ejected;
   return true;
}

static bool get_eject_state(void)
{
   update_disk_drive_swap();
   return !disk.inserted[disk.drive];
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

static bool add_image_index(void)
{
   if (disk.total_images >= 10)
      return false;

   disk.total_images++;
   return true;
}

static bool replace_image_index(unsigned index, const struct retro_game_info *info)
{
   unsigned char image[MAX_PATH];
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

void attach_disk_swap_interface(void)
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
   disk.drive        = 1;
   disk.inserted[0]  = false;
   disk.inserted[1]  = false;

   disk.g_initial_disc         = 0;
   disk.g_initial_disc_path[0] = '\0';

   for (i = 0; i < 10; i++)
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

retro_input_state_t input_state_cb;
retro_audio_sample_t audio_cb;
retro_audio_sample_batch_t audio_batch_cb;
retro_log_printf_t log_cb;

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

static char CMDFILE[512];

static int loadcmdfile(char *argv)
{
   int res = 0;

   FILE *fp = fopen(argv, "r");

   if (fp != NULL)
   {
      if (fgets(CMDFILE, 512, fp) != NULL)
         res = 1;
      fclose(fp);
   }

   return res;
}

static int HandleExtension(char *path, char *ext)
{
   int len = strlen(path);

   if (len >= 4 &&
         path[len - 4] == '.' &&
         path[len - 3] == ext[0] &&
         path[len - 2] == ext[1] &&
         path[len - 1] == ext[2])
   {
      return 1;
   }

   return 0;
}
//Args for experimental_cmdline
static char ARGUV[64][1024];
static unsigned char ARGUC = 0;

// Args for Core
static char XARGV[64][1024];
static const char* xargv_cmd[64];
static int PARAMCOUNT = 0;

extern int cmain(int argc, char *argv[]);

static void parse_cmdline(const char *argv);

static bool read_m3u(const char *file)
{
   unsigned index = 0;
   char line[MAX_PATH];
   char name[MAX_PATH];
   FILE *f = fopen(file, "r");

   if (!f)
      return false;

   while (fgets(line, sizeof(line), f) && index < sizeof(disk.path) / sizeof(disk.path[0]))
   {
      if (line[0] == '#')
         continue;

      char *carriage_return = strchr(line, '\r');
      if (carriage_return)
         *carriage_return = '\0';

      char *newline = strchr(line, '\n');
      if (newline)
         *newline = '\0';

      // Remove any beginning and ending quotes as these can cause issues when feeding the paths into command line later
      if (line[0] == '"')
          memmove(line, line + 1, strlen(line));

      if (line[strlen(line) - 1] == '"')
          line[strlen(line) - 1]  = '\0';

      if (line[0] != '\0')
      {
         char image_label[4096];
         char *custom_label;
         size_t len = 0;

         if (is_path_absolute(line))
            strncpy(name, line, sizeof(name));
         else
            snprintf(name, sizeof(name), "%s%c%s", base_dir, slash, line);

         custom_label = strchr(name, '|');
         if (custom_label)
         {
            /* get disk path */
            len = custom_label + 1 - name;
            strncpy(disk.path[index], name, len - 1);

            /* get custom label */
            custom_label++;
            strncpy(disk.label[index], custom_label, sizeof(disk.label[index]));
         }
         else
         {
            /* copy path */
            strncpy(disk.path[index], name, sizeof(disk.path[index]));

            /* extract base name from path for labels */
            extract_basename(image_label, name, sizeof(image_label));
            strncpy(disk.label[index], image_label, sizeof(disk.label[index]));
         }

         index++;
      }
   }

   disk.total_images = index;
   fclose(f);

   return (disk.total_images != 0);
}

static void Add_Option(const char* option)
{
   static int first = 0;

   if(first == 0)
   {
      PARAMCOUNT = 0;
      first++;
   }

   sprintf(XARGV[PARAMCOUNT++], "%s\0", option);
}

static int pre_main(const char *argv)
{
   int i = 0;
   int Only1Arg;
   int isM3U = 0;

   for (i = 0; i < 64; i++)
      xargv_cmd[i] = NULL;

   if (no_content) {
      p6logd("PARAMCOUNT = %d\n", PARAMCOUNT);
      PARAMCOUNT = 0;
      goto run_pmain;
   }

   if (strlen(argv) > strlen("cmd"))
   {
      if (HandleExtension((char*)argv, "cmd") || HandleExtension((char*)argv, "CMD"))
         i = loadcmdfile((char*)argv);
      else if (HandleExtension((char*)argv, "m3u") || HandleExtension((char*)argv, "M3U"))
      {
         if (!read_m3u((char*)argv))
         {
            if (log_cb)
               log_cb(RETRO_LOG_ERROR, "%s\n", "[libretro]: failed to read m3u file ...");
            return false;
         }

         sprintf((char*)argv, "px68k \"%s\"", disk.path[0]);
         if(disk.total_images > 1)
         {
            sprintf((char*)argv, "%s \"%s\"", argv, disk.path[1]);
            disk.inserted[1] = true;
         }

         disk.inserted[0] = true;
         isM3U = 1;
      }
   }

   if (i == 1)
      parse_cmdline(CMDFILE);
   else
      parse_cmdline(argv);

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

      if (cfgload == 0)
      {
         //Add_Option("-verbose");
         //Add_Option(retro_system_tos);
         //Add_Option("-8");
      }

      Add_Option(RPATH);
   }
   else
   { // Pass all cmdline args
      for (i = 0; i < ARGUC; i++)
         Add_Option(ARGUV[i]);
   }

   for (i = 0; i < PARAMCOUNT; i++)
   {
      xargv_cmd[i] = (char*)(XARGV[i]);
   }

   /* Log successfully loaded paths when loading from m3u */
   if (isM3U)
   {
      p6logd("%s\n", "Loading from an m3u file ...");
      for (i = 0; i < disk.total_images; i++)
         p6logd("index %d: %s\n", i + 1, disk.path[i]);
   }

   /* List arguments to be passed to core */
   p6logd("%s\n", "Parsing arguments ...");
   for (i = 0; i < PARAMCOUNT; i++)
      p6logd("%d : %s\n", i, xargv_cmd[i]);

run_pmain:
   pmain(PARAMCOUNT, (char **)xargv_cmd);

   if (PARAMCOUNT)
      xargv_cmd[PARAMCOUNT - 2] = NULL;

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
               /* word goes from start_of_word to p-1 */
               //... do something with the word ...
               for (c2 = 0, p2 = start_of_word; p2 < p; p2++, c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUC++;

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_STRING or we handled the end above */
         case IN_WORD:
            /* we're in a word, so keep going until we get to a space */
            if (isspace(c))
            {
               /* word goes from start_of_word to p-1 */
               //... do something with the word ...
               for (c2 = 0, p2 = start_of_word; p2 <p; p2++, c2++)
                  ARGUV[ARGUC][c2] = (unsigned char) *p2;
               ARGUC++;

               state = DULL; /* back to "not in word, not in string" state */
            }
            continue; /* either still IN_WORD or we handled the end above */
      }
   }
}

static struct retro_input_descriptor inputDescriptors[64];

static struct retro_input_descriptor inputDescriptorsP1[] = {
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2 - Menu" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
   { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },
};
static struct retro_input_descriptor inputDescriptorsP2[] = {
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
   { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },

};

static struct retro_input_descriptor inputDescriptorsNull[] = {
   { 0, 0, 0, 0, NULL }
};


static void retro_set_controller_descriptors()
{
   unsigned i;
   unsigned size = 16;

   for (i = 0; i < (2 * size); i++)
      inputDescriptors[i] = inputDescriptorsNull[0];

   if (joypad1 && joypad2)
   {
      for (i = 0; i < (2 * size); i++)
      {
         if (i < size)
            inputDescriptors[i] = inputDescriptorsP1[i];
         else
            inputDescriptors[i] = inputDescriptorsP2[i - size];
      }
   }
   else if (joypad1 || joypad2)
   {
      for (i = 0; i < size; i++)
      {
         if (joypad1)
            inputDescriptors[i] = inputDescriptorsP1[i];
         else
            inputDescriptors[i] = inputDescriptorsP2[i];
      }
   }
   else
      inputDescriptors[0] = inputDescriptorsNull[0];
   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   if (port >= 2)
      return;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (port == 0)
            joypad1 = true;
         if (port == 1)
            joypad2 = true;
         break;
      case RETRO_DEVICE_KEYBOARD:
         if (port == 0)
            joypad1 = false;
         if (port == 1)
            joypad2 = false;
         break;
      case RETRO_DEVICE_NONE:
         if (port == 0)
            joypad1 = false;
         if (port == 1)
            joypad2 = false;
         break;
      default:
         if (log_cb)
            log_cb(RETRO_LOG_ERROR, "[libretro]: Invalid device, setting type to RETRO_DEVICE_JOYPAD ...\n");
   }
   log_cb(RETRO_LOG_INFO, "Set Controller Device: %d, Port: %d %d %d\n", device, port, joypad1, joypad2);
   retro_set_controller_descriptors();
}

void retro_set_environment(retro_environment_t cb)
{
   int nocontent = 1;

   static const struct retro_controller_description port[] = {
      { "RetroPad",              RETRO_DEVICE_JOYPAD },
      { "RetroKeyboard",         RETRO_DEVICE_KEYBOARD },
      { 0 },
   };

   static const struct retro_controller_info ports[] = {
      { port, 2 },
      { port, 2 },
      { NULL, 0 },
   };

   environ_cb = cb;
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &nocontent);
   libretro_set_core_options(environ_cb);
}

static void update_variables(void)
{
   int i = 0, snd_opt = 0;
   char key[256] = {0};
   struct retro_variable var = {0};

   strcpy(key, "px68k_joytype");
   var.key = key;
   for (i = 0; i < 2; i++)
   {
      key[strlen("px68k_joytype")] = '1' + i;
      var.value = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
         if (!(strcmp(var.value, "Default (2 Buttons)"))) {
            Config.JOY_TYPE[i] = 0;
         } else if (!(strcmp(var.value, "CPSF-MD (8 Buttons)"))) {
            Config.JOY_TYPE[i] = 1;
         } else if (!(strcmp(var.value, "CPSF-SFC (8 Buttons)"))) {
            Config.JOY_TYPE[i] = 2;
         }
      }
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
      else if (strcmp(var.value, "150Mhz (OC)") == 0)
         Config.clockmhz = 150;
      else if (strcmp(var.value, "200Mhz (OC)") == 0)
         Config.clockmhz = 200;
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

   var.key = "px68k_analog";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      //fprintf(stderr, "value: %s\n", var.value);
      if (!strcmp(var.value, "disabled"))
         opt_analog = false;
      if (!strcmp(var.value, "enabled"))
         opt_analog = true;

      //fprintf(stderr, "[libretro-test]: Analog: %s.\n",opt_analog?"ON":"OFF");
   }

   var.key = "px68k_adpcm_vol";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      snd_opt = atoi(var.value);
      if (snd_opt != Config.PCM_VOL)
      {
         Config.PCM_VOL = snd_opt;
         ADPCM_SetVolume((BYTE)Config.PCM_VOL);
      }
   }

   var.key = "px68k_opm_vol";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      snd_opt = atoi(var.value);
      if (snd_opt != Config.OPM_VOL)
      {
         Config.OPM_VOL = snd_opt;
         OPM_SetVolume((BYTE)Config.OPM_VOL);
      }
   }

#ifndef NO_MERCURY
   var.key = "px68k_mercury_vol";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      snd_opt = atoi(var.value);
      if (snd_opt != Config.MCR_VOL)
      {
         Config.MCR_VOL = snd_opt;
         Mcry_SetVolume((BYTE)Config.MCR_VOL);
      }
   }
#endif

   update_disk_drive_swap();

   var.key = "px68k_menufontsize";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "normal") == 0)
         Config.MenuFontSize = 0;
      else
         Config.MenuFontSize = 1;
   }

   var.key = "px68k_joy1_select";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "XF1"))
         Config.joy1_select_mapping = KBD_XF1;
      else if (!strcmp(var.value, "XF2"))
         Config.joy1_select_mapping = KBD_XF2;
      else if (!strcmp(var.value, "XF3"))
         Config.joy1_select_mapping = KBD_XF3;
      else if (!strcmp(var.value, "XF4"))
         Config.joy1_select_mapping = KBD_XF4;
      else if (!strcmp(var.value, "XF5"))
         Config.joy1_select_mapping = KBD_XF5;
      else if (!strcmp(var.value, "F1"))
         Config.joy1_select_mapping = KBD_F1;
      else if (!strcmp(var.value, "F2"))
         Config.joy1_select_mapping = KBD_F2;
      else if (!strcmp(var.value, "OPT1"))
         Config.joy1_select_mapping = KBD_OPT1;
      else if (!strcmp(var.value, "OPT2"))
         Config.joy1_select_mapping = KBD_OPT2;
      else
         Config.joy1_select_mapping = 0;
   }

   var.key = "px68k_disk_path";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         Config.disk_path = 0;
      if (!strcmp(var.value, "enabled"))
         Config.disk_path = 1;
   }

   var.key = "px68k_rumble_on_disk_read";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         opt_rumble_enabled = 0;
      if (!strcmp(var.value, "enabled"))
         opt_rumble_enabled = 1;
   }

   /* PX68K Menu */

   var.key = "px68k_joy_mouse";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      int value = 0;
      if (!strcmp(var.value, "Joystick"))
         value = 0;
      else if (!strcmp(var.value, "Mouse"))
         value = 1;

      Config.JoyOrMouse = value;
      Mouse_StartCapture(value == 1);
   }

   var.key = "px68k_vbtn_swap";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "TRIG1 TRIG2"))
         Config.VbtnSwap = 0;
      else if (!strcmp(var.value, "TRIG2 TRIG1"))
         Config.VbtnSwap = 1;
   }

   var.key = "px68k_no_wait_mode";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         Config.NoWaitMode = 0;
      else if (!strcmp(var.value, "enabled"))
         Config.NoWaitMode = 1;
   }

   var.key = "px68k_frameskip";
   var.value = NULL;

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
}

/************************************
 * libretro implementation
 ************************************/

//static struct retro_system_av_info g_av_info;

void retro_get_system_info(struct retro_system_info *info)
{
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
#ifndef PX68K_VERSION
#define PX68K_VERSION "0.15+"
#endif
   memset(info, 0, sizeof(*info));
   info->library_name = "PX68K";
   info->library_version = PX68K_VERSION GIT_VERSION;
   info->need_fullpath = true;
   info->valid_extensions = "dim|zip|img|d88|88d|hdm|dup|2hd|xdf|hdf|cmd|m3u";
}


void retro_get_system_av_info(struct retro_system_av_info *info)
{
   /* FIXME handle PAL/NTSC */
   struct retro_game_geometry geom = { retrow, retroh,800, 600 ,4.0 / 3.0 };
   struct retro_system_timing timing = { FRAMERATE, SOUNDRATE };

   info->geometry = geom;
   info->timing   = timing;
}

void update_geometry(void)
{
   struct retro_system_av_info system_av_info;
   system_av_info.geometry.base_width = retrow;
   system_av_info.geometry.base_height = retroh;
   system_av_info.geometry.aspect_ratio = (float)4.0/3.0;// retro_aspect;
   environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &system_av_info);
}

void update_timing(void)
{
   struct retro_system_av_info system_av_info;
   retro_get_system_av_info(&system_av_info);
   FRAMERATE = (VID_MODE ? MODE_HIGH : MODE_NORM);
   system_av_info.timing.fps = FRAMERATE;
   environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &system_av_info);
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data, size_t size)
{
   return false;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

bool retro_load_game(const struct retro_game_info *info)
{
   const char *full_path = 0;

   no_content = 1;
   RPATH[0] = '\0';

   if (info && info->path) {
      no_content = 0;
      full_path = info->path;
      strcpy(RPATH, full_path);
      extract_directory(base_dir, info->path, sizeof(base_dir));
   }

   p6logd("LOAD EMU\n");

   return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
   (void)game_type;
   (void)info;
   (void)num_info;
   return false;
}

void retro_unload_game(void)
{
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
   const char *system_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
   {
      // if defined, use the system directory
      retro_system_directory = system_dir;
   }

   const char *content_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
   {
      // if defined, use the system directory
      retro_content_directory = content_dir;
   }

   const char *save_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
   {
      // If save directory is defined use it, otherwise use system directory
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;
   }
   else
   {
      // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
      retro_save_directory = retro_system_directory;
   }

   if(retro_system_directory == NULL) sprintf(RETRO_DIR, "%s\0",".");
   else sprintf(RETRO_DIR, "%s\0", retro_system_directory);

   sprintf(retro_system_conf, "%s%ckeropi\0", RETRO_DIR, slash);

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      fprintf(stderr, "RGB565 is not supported.\n");
      exit(0);
   }

   if (environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble) && rumble.set_rumble_state)
      rumble_cb = rumble.set_rumble_state;

   libretro_supports_input_bitmasks = 0;
   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_input_bitmasks = 1;

   disk_swap_interface_init();
/*
    struct retro_keyboard_callback cbk = { keyboard_cb };
    environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &cbk);
*/

   /* set sane defaults */
   Config.disk_path = 1;
   Config.clockmhz = 10;
   Config.ram_size = 2 * 1024 *1024;
   Config.JOY_TYPE[0] = 0;
   Config.JOY_TYPE[1] = 0;

   update_variables();

   memset(Core_Key_State, 0, 512);
   memset(Core_old_Key_State, 0, sizeof(Core_old_Key_State));
}

void retro_deinit(void)
{
   end_loop_retro();
   p6logd("Retro DeInit\n");
   libretro_supports_input_bitmasks = 0;
}

void retro_reset(void)
{
   WinX68k_Reset();
}

static int firstcall = 1;

static void rumbleFrames(void)
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

void retro_run(void)
{
   bool updated = false;

   if(firstcall)
   {
      pre_main(RPATH);
      firstcall = 0;
      p6logd("INIT done\n");
      update_variables();
      soundbuf_size = SNDSZ;
      return;
   }

   if (CHANGEAV || CHANGEAV_TIMING)
   {
      if (CHANGEAV_TIMING)
      {
         update_timing();
         CHANGEAV_TIMING = 0;
         CHANGEAV = 0;
      }
      if (CHANGEAV)
      {
         update_geometry();
         CHANGEAV = 0;
      }
      soundbuf_size = SNDSZ;
      p6logd("w:%d h:%d a:%.3f\n", retrow, retroh, (float)(4.0/3.0));
      p6logd("fps:%.2f soundrate:%d\n", FRAMERATE, (int)SOUNDRATE);
   }

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
   {
      update_variables();
   }

   input_poll_cb();

   rumbleFrames();

   FDD_IsReading = 0;

   exec_app_retro();

   raudio_callback(soundbuf, NULL, soundbuf_size << 2);
   audio_batch_cb((const int16_t*)soundbuf, soundbuf_size);

   video_cb(videoBuffer, retrow, retroh, /*retrow*/ 800 << 1/*2*/);
}
