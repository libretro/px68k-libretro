
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "libretro.h"
#include "libretro/winx68k.h"
#include "libretro/dswin.h"
#include "libretro/prop.h"
#include "fmgen/fmg_wrap.h"
#include "x68k/adpcm.h"
#include "x68k/fdd.h"
#include "x68k/x68kmemory.h"

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

char RPATH[512];
char RETRO_DIR[512];
const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;
char retro_system_conf[512];

char Core_Key_Sate[512];
char Core_old_Key_Sate[512];

bool joypad1, joypad2;

bool opt_analog;

int joy1_select_mapping = 0;

int retrow = 800;
int retroh = 600;
int CHANGEAV = 0;
int CHANGEAV_TIMING = 0; /* Separate change of geometry from change of refresh rate */
int VID_MODE = 1;
float FRAMERATE = MODE_HIGH;
int JOY_TYPE[2] = {0}; /* Set controller type for each player to use */
int clockmhz = 10;
DWORD ram_size;
int pcm_vol, opm_vol;

int pauseg = 0;

signed short soundbuf[1024 * 2];

uint16_t *videoBuffer;

static retro_video_refresh_t video_cb;
static retro_environment_t environ_cb;

static  retro_input_poll_t input_poll_cb;

static char base_dir[MAX_PATH];

static void update_variables(void);

/* .dsk swap support */
struct retro_disk_control_callback dskcb;
unsigned disk_index = 0;
unsigned disk_images = 0;
char disk_paths[10][MAX_PATH];
bool disk_inserted = false;
unsigned disk_drive = 1;

bool set_eject_state(bool ejected)
{
   disk_inserted = !ejected;
   return true;
}

bool get_eject_state(void)
{
   return !disk_inserted;
}

unsigned get_image_index(void)
{
   return disk_index;
}

bool set_image_index(unsigned index)
{
   disk_index = index;
   if(disk_index == disk_images)
   {
      //retroarch is trying to set "no disk in tray"
      return true;
   }

   update_variables();
   FDD_SetFD(disk_drive, disk_paths[disk_index], 0);
   strcpy(Config.FDDImage[disk_drive], disk_paths[disk_index]);
   return true;
}

unsigned get_num_images(void)
{
   return disk_images;
}

bool add_image_index(void)
{
   if (disk_images >= 10)
      return false;

   disk_images++;
   return true;
}

bool replace_image_index(unsigned index, const struct retro_game_info *info)
{
   strcpy(disk_paths[index], info->path);
   return true;
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

int loadcmdfile(char *argv)
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

int HandleExtension(char *path, char *ext)
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
int PARAMCOUNT = 0;

extern int cmain(int argc, char *argv[]);

void parse_cmdline(const char *argv);

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

static bool read_m3u(const char *file)
{
   char line[MAX_PATH];
   char name[MAX_PATH];
   FILE *f = fopen(file, "r");

   if (!f)
      return false;

   while (fgets(line, sizeof(line), f) && disk_images < sizeof(disk_paths) / sizeof(disk_paths[0]))
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
         snprintf(name, sizeof(name), "%s%c%s", base_dir, slash, line);
         strcpy(disk_paths[disk_images], name);
         disk_images++;
      }
   }

   fclose(f);
   return (disk_images != 0);
}

void Add_Option(const char* option)
{
   static int first = 0;

   if(first == 0)
   {
      PARAMCOUNT = 0;
      first++;
   }

   sprintf(XARGV[PARAMCOUNT++], "%s\0", option);
}

int pre_main(const char *argv)
{
   int i = 0;
   int Only1Arg;

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

         sprintf((char*)argv, "px68k \"%s\"", disk_paths[0]);
         if(disk_images > 1)
         {
            sprintf((char*)argv, "%s \"%s\"", argv, disk_paths[1]);
         }
         disk_inserted = true;
         attach_disk_swap_interface();
      }
   }

   if (i == 1)
      parse_cmdline(CMDFILE);
   else
      parse_cmdline(argv);

   Only1Arg = (strcmp(ARGUV[0], "px68k") == 0) ? 0 : 1;

   for (i = 0; i < 64; i++)
      xargv_cmd[i] = NULL;

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

   pmain(PARAMCOUNT, (char **)xargv_cmd);

   xargv_cmd[PARAMCOUNT - 2] = NULL;

   return 0;
}

void parse_cmdline(const char *argv)
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

void texture_init(void)
{
   memset(videoBuffer, 0, sizeof(*videoBuffer));
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


void retro_set_controller_descriptors()
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
   environ_cb = cb;

   struct retro_variable variables[] = {
      { "px68k_cpuspeed" , "CPU Speed; 10Mhz|16Mhz|25Mhz|33Mhz (OC)|66Mhz (OC)|100Mhz (OC)|150Mhz (OC)|200Mhz (OC)" },
      { "px68k_ramsize" , "RAM Size (Restart); 2MB|3MB|4MB|5MB|6MB|7MB|8MB|9MB|10MB|11MB|12MB|1MB" },
      { "px68k_analog" , "Use Analog; OFF|ON" },
      { "px68k_joytype1" , "P1 Joypad Type; Default (2 Buttons)|CPSF-MD (8 Buttons)|CPSF-SFC (8 Buttons)" },
      { "px68k_joytype2" , "P2 Joypad Type; Default (2 Buttons)|CPSF-MD (8 Buttons)|CPSF-SFC (8 Buttons)" },
      { "px68k_adpcm_vol" , "ADPCM Volume; 15|0|1|2|3|4|5|6|7|8|9|10|11|12|13|14" },
      { "px68k_opm_vol" , "OPM Volume; 12|13|14|15|0|1|2|3|4|5|6|7|8|9|10|11" },
      { "px68k_disk_drive" , "Swap Disks on Drive; FDD1|FDD0" },
      { "px68k_joy1_select" , "P1 Joystick Select Mapping; XF1|XF2|XF3|XF4|XF5|F1|F2" },
#ifndef NO_MERCURY
      { "px68k_mercury_vol" , "OPM Volume; 13|14|15|0|1|2|3|4|5|6|7|8|9|10|11|12" },
#endif
      { NULL, NULL },
   };

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
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

static void update_variables(void)
{
   int i;
   char key[256];
   struct retro_variable var = {0};

   strcpy(key, "px68k_joytype");
   var.key = key;
   for (i = 0; i < 2; i++)
   {
      key[strlen("px68k_joytype")] = '1' + i;
      var.value = NULL;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
         if (!(strcmp(var.value, "Default (2 Buttons)"))) {
            JOY_TYPE[i] = 0;
         } else if (!(strcmp(var.value, "CPSF-MD (8 Buttons)"))) {
            JOY_TYPE[i] = 1;
         } else if (!(strcmp(var.value, "CPSF-SFC (8 Buttons)"))) {
            JOY_TYPE[i] = 2;
         }
      }
   }

   var.key = "px68k_cpuspeed";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "10Mhz") == 0)
         clockmhz = 10;
      else if (strcmp(var.value, "16Mhz") == 0)
         clockmhz = 16;
      else if (strcmp(var.value, "25Mhz") == 0)
         clockmhz = 25;
      else if (strcmp(var.value, "33Mhz (OC)") == 0)
         clockmhz = 33;
      else if (strcmp(var.value, "66Mhz (OC)") == 0)
         clockmhz = 66;
      else if (strcmp(var.value, "100Mhz (OC)") == 0)
         clockmhz = 100;
      else if (strcmp(var.value, "150Mhz (OC)") == 0)
         clockmhz = 150;
      else if (strcmp(var.value, "200Mhz (OC)") == 0)
         clockmhz = 200;
   }

   var.key = "px68k_ramsize";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "1MB") == 0)
         ram_size = 1;
      else if (strcmp(var.value, "2MB") == 0)
         ram_size = 2;
      else if (strcmp(var.value, "3MB") == 0)
         ram_size = 3;
      else if (strcmp(var.value, "4MB") == 0)
         ram_size = 4;
      else if (strcmp(var.value, "5MB") == 0)
         ram_size = 5;
      else if (strcmp(var.value, "6MB") == 0)
         ram_size = 6;
      else if (strcmp(var.value, "7MB") == 0)
         ram_size = 7;
      else if (strcmp(var.value, "8MB") == 0)
         ram_size = 8;
      else if (strcmp(var.value, "9MB") == 0)
         ram_size = 9;
      else if (strcmp(var.value, "10MB") == 0)
         ram_size = 10;
      else if (strcmp(var.value, "11MB") == 0)
         ram_size = 11;
      else if (strcmp(var.value, "12MB") == 0)
         ram_size = 12;

      ram_size <<= 20; /* convert to bytes */
   }

   var.key = "px68k_analog";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      //fprintf(stderr, "value: %s\n", var.value);
      if (!strcmp(var.value, "OFF"))
         opt_analog = false;
      if (!strcmp(var.value, "ON"))
         opt_analog = true;

      //fprintf(stderr, "[libretro-test]: Analog: %s.\n",opt_analog?"ON":"OFF");
   }

   var.key = "px68k_adpcm_vol";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      pcm_vol = atoi(var.value);
      if (pcm_vol != Config.PCM_VOL)
      {
         Config.PCM_VOL = pcm_vol;
         ADPCM_SetVolume((BYTE)Config.PCM_VOL);
      }
   }

   var.key = "px68k_opm_vol";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      opm_vol = atoi(var.value);
      if (opm_vol != Config.OPM_VOL)
      {
         Config.OPM_VOL = opm_vol;
         OPM_SetVolume((BYTE)Config.OPM_VOL);
      }
   }

   var.key = "px68k_disk_drive";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "FDD0") == 0)
         disk_drive = 0;
      else
         disk_drive = 1;
   }

   var.key = "px68k_joy1_select";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "XF1") == 0)
         joy1_select_mapping = 0;
      else if (strcmp(var.value, "XF2") == 0)
         joy1_select_mapping = 1;
      else if (strcmp(var.value, "XF3") == 0)
         joy1_select_mapping = 2;
      else if (strcmp(var.value, "XF4") == 0)
         joy1_select_mapping = 3;
      else if (strcmp(var.value, "XF5") == 0)
         joy1_select_mapping = 4;
      else if (strcmp(var.value, "F1") == 0)
         joy1_select_mapping = 5;
      else if (strcmp(var.value, "F2") == 0)
         joy1_select_mapping = 6;
   }


}

void update_input(void)
{
  input_poll_cb();
}


#if 0
static void keyboard_cb(bool down, unsigned keycode, uint32_t character, uint16_t mod)
{
}
#endif

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
   const char *full_path;

   full_path = info->path;

   strcpy(RPATH, full_path);

   extract_directory(base_dir, info->path, sizeof(base_dir));

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
   pauseg = 0;
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

   attach_disk_swap_interface();
/*
    struct retro_keyboard_callback cbk = { keyboard_cb };
    environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &cbk);
*/
   update_variables();

   memset(Core_Key_Sate, 0, 512);
   memset(Core_old_Key_Sate, 0, sizeof(Core_old_Key_Sate));
}

void retro_deinit(void)
{
   end_loop_retro();
   p6logd("Retro DeInit\n");
}

void retro_reset(void)
{
   WinX68k_Reset();
}

static int firstcall = 1;

void retro_run(void)
{
   bool updated = false;

   if(firstcall)
   {
      pre_main(RPATH);
      firstcall = 0;
      p6logd("INIT done\n");
      update_variables();
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
      p6logd("w:%d h:%d a:%.3f\n", retrow, retroh, (float)(4.0/3.0));
      p6logd("fps:%.2f soundrate:%d\n", FRAMERATE, (int)SOUNDRATE);
   }

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
   {
      update_variables();
   }

   update_input();

   if(pauseg != -1)
   {
   }

   exec_app_retro();

   raudio_callback(NULL, NULL, SNDSZ * 4);

   video_cb(videoBuffer, retrow, retroh, /*retrow*/ 800 << 1/*2*/);
}
