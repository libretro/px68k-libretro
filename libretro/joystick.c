/* JOYSTICK.C - joystick support for WinX68k */

#include "common.h"
#include "prop.h"
#include "joystick.h"
#include "winui.h"
#include "keyboard.h"
#include "mouse.h"
#include <string.h>
#include <ctype.h>

#include "libretro.h"
extern retro_input_state_t input_state_cb;
extern uint32_t libretro_supports_input_bitmasks;

uint8_t JoyKeyState;
static uint8_t JoyKeyState0;
static uint8_t JoyKeyState1;

static uint8_t AnalogState[3];
static uint8_t JoyState[2][2];
static uint8_t JoyPortData[2];

/* Convert the left RetroPad stick into relative mouse velocity. A 12.5%
 * deadzone suppresses controller drift; the remaining range is scaled so a
 * full deflection moves the pointer by 12 emulated mouse units per frame. */
#define JOY_MOUSE_DEFAULT_DEADZONE 4096
#define JOY2KEY_MOUSE_BTN1 0xfe
#define JOY2KEY_MOUSE_BTN2 0xfd
#define JOY2KEY_MENU       0xff

static float JoyMouseAnalogSpeed = 12.0f;
static float JoyMouseDigitalSpeed = 6.0f;
static int JoyMouseDeadzone = JOY_MOUSE_DEFAULT_DEADZONE;

void Joystick_SetMouseOptions(float analog_speed, float deadzone_percent, float digital_speed)
{
   if (analog_speed < 1.0f)
      analog_speed = 1.0f;
   if (analog_speed > 24.0f)
      analog_speed = 24.0f;
   if (deadzone_percent < 0.0f)
      deadzone_percent = 0.0f;
   if (deadzone_percent > 50.0f)
      deadzone_percent = 50.0f;
   if (digital_speed < 1.0f)
      digital_speed = 1.0f;
   if (digital_speed > 12.0f)
      digital_speed = 12.0f;

   JoyMouseAnalogSpeed = analog_speed;
   JoyMouseDigitalSpeed = digital_speed;
   JoyMouseDeadzone = (int)(32768.0f * deadzone_percent / 100.0f + 0.5f);
}

static float joystick_mouse_axis(int value)
{
   if (value > JoyMouseDeadzone)
      return (float)(value - JoyMouseDeadzone) *
         (JoyMouseAnalogSpeed / (32767 - JoyMouseDeadzone));
   if (value < -JoyMouseDeadzone)
      return (float)(value + JoyMouseDeadzone) *
         (JoyMouseAnalogSpeed / (32768 - JoyMouseDeadzone));
   return 0.0f;
}

/* Player 1's D-pad and left stick provide an additional mouse movement
 * source. This deliberately does not alter JoyState or consume any buttons,
 * so the selected native joystick and Joy2Key mappings continue to work. */
void Joystick_GetMouseDelta(unsigned port, float *dx, float *dy)
{
   int analog_x, analog_y;
   int digital_x, digital_y;

   if (!dx || !dy)
      return;

   *dx = 0.0f;
   *dy = 0.0f;

   if (port >= 2 || !input_state_cb || Config.JOY_TYPE[port] == PAD_NONE)
      return;

   analog_x = input_state_cb(port, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
   analog_y = input_state_cb(port, RETRO_DEVICE_ANALOG,
         RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

   digital_x = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_RIGHT) -
      input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_LEFT);
   digital_y = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_DOWN) -
      input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
         RETRO_DEVICE_ID_JOYPAD_UP);

   if (digital_x && digital_y)
   {
      *dx = (float)digital_x * (JoyMouseDigitalSpeed * 0.70710678f);
      *dy = (float)digital_y * (JoyMouseDigitalSpeed * 0.70710678f);
   }
   else
   {
      *dx = (float)digital_x * JoyMouseDigitalSpeed;
      *dy = (float)digital_y * JoyMouseDigitalSpeed;
   }

   *dx += joystick_mouse_axis(analog_x);
   *dy += joystick_mouse_axis(analog_y);
}

/* A RetroPad button drives one native PX68K signal in a specific data bank.
 * Keeping the button ID, bank, active-low signal and display label together
 * makes the CPSF layouts explicit, avoids duplicating bit tests in
 * Joystick_Update() and lets the input descriptors be generated from the
 * very same table that drives the emulated hardware.
 *
 * Both layouts below are the ones used by master before the controller
 * rework (same RetroPad button -> same PX68K signal). */
typedef struct
{
   uint8_t retropad_button;
   uint8_t bank;
   uint8_t signal;
   const char *label;
} CPSFButtonMap;

static const CPSFButtonMap CPSF_MD_BUTTONS[] =
{
   { RETRO_DEVICE_ID_JOYPAD_A,      0, JOY_TRG1, "MD A" },
   { RETRO_DEVICE_ID_JOYPAD_B,      0, JOY_TRG2, "MD B" },
   { RETRO_DEVICE_ID_JOYPAD_X,      1, JOY_TRG4, "MD Y" },
   { RETRO_DEVICE_ID_JOYPAD_Y,      1, JOY_TRG3, "MD X" },
   { RETRO_DEVICE_ID_JOYPAD_L,      1, JOY_TRG5, "MD Z" },
   { RETRO_DEVICE_ID_JOYPAD_R,      1, JOY_TRG8, "MD C" },
   { RETRO_DEVICE_ID_JOYPAD_START,  1, JOY_TRG6, "Start" },
   { RETRO_DEVICE_ID_JOYPAD_SELECT, 1, JOY_TRG7, "Mode" }
};

static const CPSFButtonMap CPSF_SFC_BUTTONS[] =
{
   { RETRO_DEVICE_ID_JOYPAD_A,      0, JOY_TRG1, "SFC B" },
   { RETRO_DEVICE_ID_JOYPAD_B,      1, JOY_TRG4, "SFC Y" },
   { RETRO_DEVICE_ID_JOYPAD_X,      1, JOY_TRG3, "SFC X" },
   { RETRO_DEVICE_ID_JOYPAD_Y,      1, JOY_TRG8, "SFC L" },
   { RETRO_DEVICE_ID_JOYPAD_L,      1, JOY_TRG5, "SFC R" },
   { RETRO_DEVICE_ID_JOYPAD_R,      0, JOY_TRG2, "SFC A" },
   { RETRO_DEVICE_ID_JOYPAD_START,  1, JOY_TRG6, "Start" },
   { RETRO_DEVICE_ID_JOYPAD_SELECT, 1, JOY_TRG7, "Select" }
};

#define CPSF_COUNT(t) ((unsigned)(sizeof(t) / sizeof((t)[0])))
const char *Joystick_NativeLabel(int type, unsigned retropad_button)
{
   const CPSFButtonMap *map = NULL;
   unsigned count = 0, i;

   if (type == PAD_CPSF_MD)
   {
      map   = CPSF_MD_BUTTONS;
      count = CPSF_COUNT(CPSF_MD_BUTTONS);
   }
   else if (type == PAD_CPSF_SFC)
   {
      map   = CPSF_SFC_BUTTONS;
      count = CPSF_COUNT(CPSF_SFC_BUTTONS);
   }
   for (i = 0; i < count; i++)
      if (map[i].retropad_button == retropad_button)
         return map[i].label;
   return NULL;
}

static void apply_cpsf_button_map(uint32_t retropad_state,
      uint8_t *joy_state, const CPSFButtonMap *map, unsigned count)
{
   unsigned i;
   for (i = 0; i < count; i++)
   {
      if (retropad_state & (1u << map[i].retropad_button))
         joy_state[map[i].bank] &= (uint8_t)~map[i].signal;
   }
}

/* This stores whether the buttons were down. This avoids key repeats. */
static uint8_t JoyDownState0;

/* Native X68000 keyboard matrix codes used by Joy2Key Remap. */
typedef struct
{
   const char *name;
   uint8_t code;
} Joy2KeyName;

static const Joy2KeyName Joy2KeyNames[] =
{
   { "esc", 0x01 }, { "1", 0x02 }, { "2", 0x03 }, { "3", 0x04 },
   { "4", 0x05 }, { "5", 0x06 }, { "6", 0x07 }, { "7", 0x08 },
   { "8", 0x09 }, { "9", 0x0a }, { "0", 0x0b }, { "minus", 0x0c },
   { "equals", 0x0d }, { "yen", 0x0e }, { "backspace", 0x0f },
   { "tab", 0x10 }, { "q", 0x11 }, { "w", 0x12 }, { "e", 0x13 },
   { "r", 0x14 }, { "t", 0x15 }, { "y", 0x16 }, { "u", 0x17 },
   { "i", 0x18 }, { "o", 0x19 }, { "p", 0x1a }, { "at", 0x1b },
   { "lbracket", 0x1c }, { "return", 0x1d }, { "a", 0x1e },
   { "s", 0x1f }, { "d", 0x20 }, { "f", 0x21 }, { "g", 0x22 },
   { "h", 0x23 }, { "j", 0x24 }, { "k", 0x25 }, { "l", 0x26 },
   { "semicolon", 0x27 }, { "colon", 0x28 }, { "rbracket", 0x29 },
   { "z", 0x2a }, { "x", 0x2b }, { "c", 0x2c }, { "v", 0x2d },
   { "b", 0x2e }, { "n", 0x2f }, { "m", 0x30 }, { "comma", 0x31 },
   { "period", 0x32 }, { "slash", 0x33 }, { "underquote", 0x34 }, { "space", 0x35 },
   { "home", 0x36 }, { "delete", 0x37 }, { "roll_up", 0x38 },
   { "roll_down", 0x39 }, { "undo", 0x3a }, { "left", 0x3b },
   { "up", 0x3c }, { "right", 0x3d }, { "down", 0x3e },
   { "clear", 0x3f }, { "kp_divide", 0x40 }, { "kp_multiply", 0x41 },
   { "kp_minus", 0x42 }, { "kp7", 0x43 }, { "kp8", 0x44 },
   { "kp9", 0x45 }, { "kp_plus", 0x46 }, { "kp4", 0x47 },
   { "kp5", 0x48 }, { "kp6", 0x49 }, { "kp_equals", 0x4a },
   { "kp1", 0x4b }, { "kp2", 0x4c }, { "kp3", 0x4d },
   { "kp_enter", 0x4e }, { "kp0", 0x4f }, { "kp_comma", 0x50 },
   { "kp_period", 0x51 },
   { "symbol", 0x52 }, { "touroku", 0x53 }, { "help", 0x54 },
   { "xf1", KBD_XF1 }, { "xf2", KBD_XF2 }, { "xf3", KBD_XF3 },
   { "xf4", KBD_XF4 }, { "xf5", KBD_XF5 }, { "kana", 0x5a },
   { "roman", 0x5b }, { "enter_code", 0x5c }, { "capslock", 0x5d },
   { "insert", 0x5e }, { "hiragana", 0x5f }, { "fullwidth", 0x60 },
   { "break", 0x61 }, { "copy", 0x62 }, { "f1", KBD_F1 },
   { "f2", KBD_F2 }, { "f3", 0x65 }, { "f4", 0x66 },
   { "f5", 0x67 }, { "f6", 0x68 }, { "f7", 0x69 },
   { "f8", 0x6a }, { "f9", 0x6b }, { "f10", 0x6c },
   { "shift", 0x70 }, { "ctrl", 0x71 }, { "opt1", KBD_OPT1 },
   { "opt2", KBD_OPT2 }, { NULL, 0 }
};

/* One authoritative definition for logical Joy2Key actions. Keep the
 * action order stable: core-option keys, labels and RetroPad IDs all derive
 * from this table. */
typedef struct
{
   unsigned retropad_button;
   const char *name;
   const char *option;
} Joy2KeyAction;

static const Joy2KeyAction Joy2KeyActions[16] = {
   { RETRO_DEVICE_ID_JOYPAD_A,      "A",      "a" },
   { RETRO_DEVICE_ID_JOYPAD_B,      "B",      "b" },
   { RETRO_DEVICE_ID_JOYPAD_X,      "X",      "x" },
   { RETRO_DEVICE_ID_JOYPAD_Y,      "Y",      "y" },
   { RETRO_DEVICE_ID_JOYPAD_SELECT, "Select", "select" },
   { RETRO_DEVICE_ID_JOYPAD_START,  "Start",  "start" },
   { RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right",  "right" },
   { RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left",   "left" },
   { RETRO_DEVICE_ID_JOYPAD_UP,     "Up",     "up" },
   { RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down",   "down" },
   { RETRO_DEVICE_ID_JOYPAD_R,      "R",      "r" },
   { RETRO_DEVICE_ID_JOYPAD_L,      "L",      "l" },
   { RETRO_DEVICE_ID_JOYPAD_R2,     "R2",     "r2" },
   { RETRO_DEVICE_ID_JOYPAD_L2,     "L2",     "l2" },
   { RETRO_DEVICE_ID_JOYPAD_R3,     "R3",     "r3" },
   { RETRO_DEVICE_ID_JOYPAD_L3,     "L3",     "l3" }
};

unsigned Joystick_GetJoy2KeyRetropadId(unsigned action)
{
   return action < 16 ? Joy2KeyActions[action].retropad_button : 0xff;
}

const char *Joystick_GetJoy2KeyActionName(unsigned action)
{
   return action < 16 ? Joy2KeyActions[action].name : "";
}

const char *Joystick_GetJoy2KeyOptionName(unsigned action)
{
   return action < 16 ? Joy2KeyActions[action].option : NULL;
}

/* Resolve each logical Joy2Key action to the RetroPad input slot exposed by
 * the selected controller's input descriptors. The action index is stable
 * across devices; only the RetroPad slot that activates it changes.
 * 0xff means that the action is occupied by a native control or unavailable. */
static uint8_t joy2key_action_button(int type, unsigned action)
{
   if (action >= 16)
      return 0xff;

   switch (type)
   {
      case PAD_2BUTTON:
         /* D-pad and A/B are native; all other labelled slots are Joy2Key. */
         if (action == 2 || action == 3 || action == 4 || action == 5 ||
             action == 10 || action == 11 || action == 12 || action == 13 ||
             action == 14 || action == 15)
            return Joy2KeyActions[action].retropad_button;
         break;
      case PAD_CPSF_MD:
      case PAD_CPSF_SFC:
         /* The eight-button layouts leave R2, L2, R3 and L3 for Joy2Key. */
         if (action >= 12)
            return Joy2KeyActions[action].retropad_button;
         break;
      case PAD_CYBERSTICK_D:
         /* R2/L2/R3/L3 are the spare digital inputs. */
         if (action >= 12)
            return Joy2KeyActions[action].retropad_button;
         break;
      case PAD_CYBERSTICK_A:
         /* The analog stick axes are native. The digital D-pad is otherwise
          * unused, so expose it as Joy2Key; R2 is the logical Start slot.
          * Physical CyberStick Start/Select remain native. */
         if (action >= 6 && action <= 9)
            return Joy2KeyActions[action].retropad_button;
         if (action == 5)
            return RETRO_DEVICE_ID_JOYPAD_R2;
         if (action >= 13)
            return Joy2KeyActions[action].retropad_button;
         break;
      case PAD_JOY2KEY:
         return Joy2KeyActions[action].retropad_button;
      default:
         break;
   }
   return 0xff;
}

/* Return the logical Joy2Key action assigned to a RetroPad button, or -1
 * when that button is reserved for a native control or is unavailable. */
int Joystick_GetJoy2KeyAction(int type, unsigned retropad_button)
{
   unsigned action;
   for (action = 0; action < 16; action++)
      if (joy2key_action_button(type, action) == retropad_button)
         return (int)action;
   return -1;
}


/* 0xff = PX68K menu. Index 13 is L2: its Joy2Key option defaults to "menu"
 * on both ports (same default as the core options), so the menu stays
 * reachable even if the frontend supplies no core options at all. */
static uint8_t Joy2KeyMap[2][16] = {
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0, 0 },
   { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0, 0 }
};
static uint8_t Joy2KeyPrevious[2][16];
static uint8_t Joy2KeyRefs[256];

static void joy2key_release(uint8_t code)
{
   if (code && code != JOY2KEY_MENU && Joy2KeyRefs[code])
   {
      Joy2KeyRefs[code]--;
      if (!Joy2KeyRefs[code])
      {
         if (code < 128)
            send_keycode(code, P6K_UP);
         else if (code == JOY2KEY_MOUSE_BTN1)
            Mouse_Event(1, 0.0f, 0.0f);
         else if (code == JOY2KEY_MOUSE_BTN2)
            Mouse_Event(2, 0.0f, 0.0f);
      }
   }
}

static void joy2key_press(uint8_t code)
{
   if (code && code != JOY2KEY_MENU)
   {
      if (!Joy2KeyRefs[code])
      {
         if (code < 128)
            send_keycode(code, P6K_DOWN);
         else if (code == JOY2KEY_MOUSE_BTN1)
            Mouse_Event(1, 1.0f, 0.0f);
         else if (code == JOY2KEY_MOUSE_BTN2)
            Mouse_Event(2, 1.0f, 0.0f);
      }
      if (Joy2KeyRefs[code] < 255)
         Joy2KeyRefs[code]++;
   }
}

/* "state" is the already-polled RetroPad bitmask. Each logical action is
 * resolved to the slot advertised by the active controller's descriptors.
 * Native controls have no Joy2Key slot. With send_keys == 0 (menu open or
 * software keyboard), held keys are released and no new key is pressed. */
static void joy2key_update(unsigned port, uint32_t state, int send_keys)
{
   unsigned action;
   for (action = 0; action < 16; action++)
   {
      uint8_t button = joy2key_action_button(Config.JOY_TYPE[port], action);
      uint8_t down = (send_keys && button != 0xff &&
            (state & (1u << button))) ? 1 : 0;
      uint8_t code = Joy2KeyMap[port][action];
      uint8_t old_code = Joy2KeyPrevious[port][action];
      if (old_code && old_code != 0xff && (!down || old_code != code))
         joy2key_release(old_code);
      if (down && code && code != 0xff && (!old_code || old_code != code))
         joy2key_press(code);
      Joy2KeyPrevious[port][action] = down ? code : 0;
   }
}

int Joystick_SetKeyMapping(unsigned port, unsigned input, const char *value)
{
   unsigned i;
   uint8_t code = 0;
   if (port >= 2 || input >= 16 || !value)
      return 0;
   if (!strcmp(value, "menu"))
      code = JOY2KEY_MENU;
   else if (!strcmp(value, "mouse_btn1"))
      code = JOY2KEY_MOUSE_BTN1;
   else if (!strcmp(value, "mouse_btn2"))
      code = JOY2KEY_MOUSE_BTN2;
   for (i = 0; !code && Joy2KeyNames[i].name; i++)
   {
      if (!strcmp(value, Joy2KeyNames[i].name))
      {
         code = Joy2KeyNames[i].code;
         break;
      }
   }
   if (Joy2KeyMap[port][input] == code)
      return 0;
   if (Joy2KeyPrevious[port][input])
   {
      joy2key_release(Joy2KeyPrevious[port][input]);
      Joy2KeyPrevious[port][input] = 0;
   }
   Joy2KeyMap[port][input] = code;
   return 1;
}

/* Human-readable label for the currently selected output of a logical
 * Joy2Key action. Single-character keys stay lowercase (e.g. "z"). */
const char *Joystick_GetKeyMappingLabel(unsigned port, unsigned input)
{
   static char label[32];
   unsigned i;
   const char *name = NULL;
   uint8_t code;
   size_t out = 0;
   int capitalize = 1;

   if (port >= 2 || input >= 16)
      return "None";

   code = Joy2KeyMap[port][input];
   if (!code)
      return "None";
   if (code == JOY2KEY_MENU)
      return "Menu";
   if (code == JOY2KEY_MOUSE_BTN1)
      return "Mouse 1";
   if (code == JOY2KEY_MOUSE_BTN2)
      return "Mouse 2";

   for (i = 0; Joy2KeyNames[i].name; i++)
      if (Joy2KeyNames[i].code == code)
      {
         name = Joy2KeyNames[i].name;
         break;
      }
   if (!name)
      return "Unknown";

   if (strlen(name) == 1)
      return name;

   while (*name && out + 1 < sizeof(label))
   {
      if (*name == '_')
      {
         label[out++] = ' ';
         capitalize = 1;
      }
      else
      {
         label[out++] = capitalize ? (char)toupper((unsigned char)*name) : *name;
         capitalize = 0;
      }
      name++;
   }
   label[out] = '\0';
   return label;
}

int Joystick_HasKeyMapping(unsigned port, unsigned input)
{
   if (port >= 2 || input >= 16)
      return 0;
   if (Config.JOY_TYPE[port] == PAD_NONE)
      return 0;
   return Joy2KeyMap[port][input] != 0;
}

static void joy2key_reset(void)
{
   unsigned p, i;
   for (p = 0; p < 2; p++)
      for (i = 0; i < 16; i++)
      {
         if (Joy2KeyPrevious[p][i])
            joy2key_release(Joy2KeyPrevious[p][i]);
         Joy2KeyPrevious[p][i] = 0;
      }
}

#define CYBER_LATENCY 3
static int CyberCount = 0;
static int CyberLatency = CYBER_LATENCY;

static uint8_t JoyCyberA_Read(void)
{
   uint8_t ret = 0xff;

   switch (CyberCount)
   {
   case 0:
      /* data start - waits a bit after REQ */
      /* Suppose to take about 50us to respond, using counters here instead */
      ret = 0xff;
      break;
   case 1:
      /* data0: button A, B, C, D */
      ret = JoyState[0][0];
      break;
   case 2:
      /* data1: button E1, E2, F (Start), G (Select) */
      ret = JoyState[0][1];
      break;
   case 3:
      /* data2: Y axis, b4-b7 */
      ret = 0x90;
      ret |= (AnalogState[1] >> 4) & 0x0f;
      break;
   case 4:
      /* data3: X axis, b4-b7 */
      ret = 0xb0;
      ret |= (AnalogState[0] >> 4) & 0x0f;
      break;
   case 5:
      /* data4: Throttle Up/Down, b4-b7 */
      ret = 0x90;
      ret |= (AnalogState[2] >> 4) & 0x0f;
      break;
   case 6:
      /* data5: reserved */
      ret = 0xb0;
      break;
   case 7:
      /* data6: Y axis, b0-b3 */
      ret = 0x90;
      ret |= AnalogState[1] & 0x0f;
      break;
   case 8:
      /* data7: X axis, b0-b3 */
      ret = 0xb0;
      ret |= AnalogState[0] & 0x0f;
      break;
   case 9:
      /* data8: Throttle Up/Down, b0-b3 */
      ret = 0x90;
      ret |= AnalogState[2] & 0x0f;
      break;
   case 10:
      /* data9: reserved */
      ret = 0xb0;
      break;
   case 11:
      /* data10: mini buttons A/B on lever */
      ret = 0x9f & ~((~JoyState[0][0] & 0x0c));
      break;
   case 12:
      /* data11: */
      ret = 0xbf;
      break;
   default:
      ret = 0xff;
      break;
   }

   /* decrement internal counter */
   CyberLatency--;

/*   log_cb(RETRO_LOG_DEBUG, "lat = %2d count = %d ret = %02x\n", CyberLatency, CyberCount, ret); */

   if (CyberLatency <= 0)
   {
      if (CyberCount >= 13)
      {
         ret = 0xff;
      }
      else
      {
         /* next data cycle */
         CyberCount++;

         /* reset internal counter */
         CyberLatency = CYBER_LATENCY;
      }
   }

   return ret;
}

static void JoyCyberA_Write(uint8_t data)
{
   if ((data == 0) && (JoyPortData[0] == 0xff))
   {
      /* reset data acquisition cycle */
      CyberCount = 0;

      /* initialize internal latenct */
      CyberLatency = CYBER_LATENCY;
   }
   JoyPortData[0] = data;
}

void Joystick_Init(void)
{
   joy2key_reset();
   JoyKeyState = 0;
   JoyKeyState0 = 0;
   JoyKeyState1 = 0;
   JoyState[0][0] = 0xff;
   JoyState[0][1] = 0xff;
   JoyState[1][0] = 0xff;
   JoyState[1][1] = 0xff;
   JoyPortData[0] = 0;
   JoyPortData[1] = 0;
   AnalogState[0] = 0x97;
   AnalogState[1] = 0xb7;
   AnalogState[2] = 0x97;
   CyberCount     = 0;
   CyberLatency   = CYBER_LATENCY;
}

void Joystick_Cleanup(void)
{
   joy2key_reset();
}

uint8_t FASTCALL Joystick_Read(uint8_t num)
{
   if (num >= 2)
      return 0xff;

   switch (Config.JOY_TYPE[num])
   {
   case PAD_NONE:
      return 0xff;

   case PAD_CYBERSTICK_A:
      if (num == 0)
         return JoyCyberA_Read();
      return 0xff;

   case PAD_CYBERSTICK_D:
   case PAD_CPSF_MD:
   case PAD_CPSF_SFC:
      /* The PPI strobe selects the second button bank when high. */
      return JoyState[num][JoyPortData[num] == 0xff ? 1 : 0];

   case PAD_2BUTTON:
      if (JoyPortData[num] == 0xff)
         return 0xff;
      return JoyState[num][0];
   }
   return 0xff;
}

void FASTCALL Joystick_Write(uint8_t num, uint8_t data)
{
   if (num >= 2)
      return;

   /* The PIA select-line latch belongs to the hardware port, not to the
    * currently selected controller type. Always track it so changing a
    * device while Port C is high cannot leave the button bank out of sync. */
   if (Config.JOY_TYPE[num] == PAD_CYBERSTICK_A && num == 0)
      JoyCyberA_Write(data);
   else
      JoyPortData[num] = data;
}

int Joystick_IsMenuPressed(void)
{
   unsigned port, action;

   /* The menu is a logical Joy2Key action. Resolve it through the selected
    * device's descriptor mapping so it follows RetroArch Controls remaps. */
   if (!input_state_cb)
      return 0;
   for (port = 0; port < 2; port++)
   {
      if (Config.JOY_TYPE[port] == PAD_NONE)
         continue;
      for (action = 0; action < 16; action++)
      {
         uint8_t button;
         if (Joy2KeyMap[port][action] != JOY2KEY_MENU)
            continue;
         button = joy2key_action_button(Config.JOY_TYPE[port], action);
         if (button != 0xff && input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, button))
            return 1;
      }
   }
   return 0;
}

/* Menu navigation related vars */
#define RATE   3      /* repeat rate */
#define JOYDELAY 30   /* delay before 1st repeat */

static uint32_t get_px68k_input_bitmasks(int port)
{
   return input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
}

static uint16_t get_px68k_input(int port)
{
   uint32_t i   = 0;
   uint32_t res = 0;
   for (i = 0; i <= RETRO_DEVICE_ID_JOYPAD_L3; i++)
      res |= input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
   return res;
}

void FASTCALL Joystick_Update(int is_menu, int key, int port)
{
   static uint8_t pre_ret0 = 0xff;
   uint8_t ret0            = 0xff, ret1 = 0xff;
   uint16_t temp           = 0;
   uint32_t res            = 0;
   uint8_t *p_JoyState;

   /* PX68K exposes exactly two libretro joystick ports. Never index the
    * per-port state arrays until the requested port has been validated. */
   if (port < 0 || port >= 2)
      return;

   p_JoyState = JoyState[port];

   if (libretro_supports_input_bitmasks)
      res                  = get_px68k_input_bitmasks(port);
   else
      res                  = get_px68k_input(port);

   if (Config.JOY_TYPE[port] == PAD_NONE)
      res = 0;

   /* Joy2Key options are logical actions. Resolve each action to the
    * controller-specific RetroPad slot exposed by the input descriptors;
    * native controls have no Joy2Key slot and therefore cannot be stolen.
    * This reuses the already-polled input bitmask and adds no polling pass. */
   joy2key_update((unsigned)port, res,
         !is_menu && !Keyboard_IsSwKeyboard());

   /* Build the legacy joystick/menu state only from controls belonging
    * to the selected emulated device. In particular, unused RetroPad
    * controls on a 2-button joystick are Joy2Key-only and must never
    * leak into the joystick signal bits when their key mapping is None. */
   if (Config.JOY_TYPE[port] == PAD_2BUTTON)
   {
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
         temp |= JOY_RIGHT;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
         temp |= JOY_LEFT;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
         temp |= JOY_UP;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
         temp |= JOY_DOWN;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_A))
         temp |= JOY_TRG2;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_B))
         temp |= JOY_TRG1;
   }
   else if (Config.JOY_TYPE[port] != PAD_NONE &&
            Config.JOY_TYPE[port] != PAD_JOY2KEY)
   {
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
         temp |= JOY_RIGHT;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
         temp |= JOY_LEFT;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
         temp |= JOY_UP;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
         temp |= JOY_DOWN;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_A))
         temp |= JOY_TRG2;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_B))
         temp |= JOY_TRG1;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_X))
         temp |= JOY_TRG1;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
         temp |= JOY_TRG2;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_START))
         temp |= ~(JOY_UP | JOY_DOWN);
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
         temp |= ~(JOY_LEFT | JOY_RIGHT);
   }

   /* Ignore opposite directions. */
   if ((temp & (JOY_LEFT | JOY_RIGHT)) == (JOY_LEFT | JOY_RIGHT))
      temp &= ~(JOY_LEFT | JOY_RIGHT);
   if ((temp & (JOY_UP | JOY_DOWN)) == (JOY_UP | JOY_DOWN))
      temp &= ~(JOY_UP | JOY_DOWN);

   /* ret0 is for menu handling */
   ret0 ^= temp;

   p_JoyState[0] = 0xff;
   p_JoyState[1] = 0xff;

   /* Buttons */
   switch (Config.JOY_TYPE[port])
   {
      case PAD_2BUTTON:
         p_JoyState[0] = ret0;
         break;

      case PAD_CPSF_MD:
         /* data[0]: dpad, A, B */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
            p_JoyState[0] &= ~JOY_RIGHT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
            p_JoyState[0] &= ~JOY_LEFT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
            p_JoyState[0] &= ~JOY_UP;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
            p_JoyState[0] &= ~JOY_DOWN;
         apply_cpsf_button_map(res, p_JoyState, CPSF_MD_BUTTONS,
               sizeof(CPSF_MD_BUTTONS) / sizeof(CPSF_MD_BUTTONS[0]));
         break;

      case PAD_CPSF_SFC:
         /* data[0]: dpad, A, B */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
            p_JoyState[0] &= ~JOY_RIGHT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
            p_JoyState[0] &= ~JOY_LEFT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
            p_JoyState[0] &= ~JOY_UP;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
            p_JoyState[0] &= ~JOY_DOWN;
         apply_cpsf_button_map(res, p_JoyState, CPSF_SFC_BUTTONS,
               sizeof(CPSF_SFC_BUTTONS) / sizeof(CPSF_SFC_BUTTONS[0]));
         break;

      case PAD_CYBERSTICK_D:
         /* data[0] Right, Left, Up, Down, A, B */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT))
            p_JoyState[0] &= ~JOY_RIGHT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT))
            p_JoyState[0] &= ~JOY_LEFT;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_UP))
            p_JoyState[0] &= ~JOY_UP;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN))
            p_JoyState[0] &= ~JOY_DOWN;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_A))
            p_JoyState[0] &= ~0x20;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_B))
            p_JoyState[0] &= ~0x40;
         /* data[1]: buttons C, D, E1, E2, F (Start), G (Select) */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_X))
            p_JoyState[1] &= ~0x04;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
            p_JoyState[1] &= ~0x08;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_L))
            p_JoyState[1] &= ~0x20;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_R))
            p_JoyState[1] &= ~0x40;
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_START))
            p_JoyState[1] &= ~0x02; /* F: Start */
         if (res & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
            p_JoyState[1] &= ~0x01; /* G: Select */
         break;

      case PAD_CYBERSTICK_A:
#if 0
		analog_z = input_state_cb(port, RETRO_DEVICE_ANALOG,
			RETRO_DEVICE_INDEX_ANALOG_BUTTON,
		    RETRO_DEVICE_ID_JOYPAD_L2);

		if (analog_z == 0)
		{
			if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0,
			                   RETRO_DEVICE_ID_JOYPAD_L2))
			{
				analog_z = 0x8000;
			}
			else
			{
				analog_z = 0;
			}
		}
#endif
         {
            int stick_x = input_state_cb(port, RETRO_DEVICE_ANALOG,
                  RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
            int stick_y = input_state_cb(port, RETRO_DEVICE_ANALOG,
                  RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
            int throttle_y = input_state_cb(port, RETRO_DEVICE_ANALOG,
                  RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

            AnalogState[0] = (uint8_t)((stick_x + 0x8000) >> 8);
            AnalogState[1] = (uint8_t)((stick_y + 0x8000) >> 8);
            /* The throttle is sourced only from the right stick Y axis.
             * Frontends can remap any physical control to this axis. */
            AnalogState[2] = (uint8_t)((-throttle_y + 0x8000) >> 8);
         }
		if (AnalogState[1] < 30)
			ret0 &= ~JOY_UP;
		else if (AnalogState[1] > (255 - 30))
			ret0 &= ~JOY_DOWN;
		if (AnalogState[0] < 30)
			ret0 &= ~JOY_LEFT;
		else if (AnalogState[0] > (255 - 30))
			ret0 &= ~JOY_RIGHT;
		/* data[0]: buttons A, B, C, D */
		/* data[10]: mini buttons A, B on lever */
		p_JoyState[0]   = 0x9f;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_A))
			p_JoyState[0] &= ~0x08;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_B))
			p_JoyState[0] &= ~0x04;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_X))
			p_JoyState[0] &= ~0x02;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_Y))
			p_JoyState[0] &= ~0x01;
		/* data[1]: buttons E1, E2, F (Start), G (Select) */
		p_JoyState[1] = 0xbf;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_L))
			p_JoyState[1] &= ~0x08;
		if (res & (1 << RETRO_DEVICE_ID_JOYPAD_R))
			p_JoyState[1] &= ~0x04;
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_START))
         p_JoyState[1] &= ~0x02; /* F: Start */
      if (res & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT))
         p_JoyState[1] &= ~0x01; /* G: Select */
      break;
   }

   JoyDownState0   = ~(ret0 ^ pre_ret0) | ret0;
   pre_ret0        = ret0;

   /* input overrides section during Menu mode for faster menu browsing
    * by pressing and holding key or button aka turbo mode */
   if (is_menu)
   {
      int i;
      static int repeat_rate, repeat_delay;
      static uint8_t last_inbuf;
      uint8_t joy_in = (ret0 ^ 0xff);
      uint8_t inbuf  = (joy_in | key);

      if ((inbuf & (JOY_LEFT | JOY_RIGHT)) == (JOY_LEFT | JOY_RIGHT))
         inbuf &= ~(JOY_LEFT | JOY_RIGHT);
      if ((inbuf & (JOY_UP | JOY_DOWN)) == (JOY_UP | JOY_DOWN))
         inbuf &= ~(JOY_UP | JOY_DOWN);

      if (last_inbuf != inbuf)
      {
         last_inbuf    = inbuf;
         repeat_delay  = JOYDELAY;
         repeat_rate   = 0;
         JoyDownState0 = (inbuf ^ 0xff);
      }
      else
      {
         if (repeat_delay)
            repeat_delay--;
         if (repeat_delay == 0)
         {
            if (repeat_rate)
               repeat_rate--;
            if (repeat_rate == 0)
            {
               repeat_rate = RATE;
               for (i = 0; i < 4; i++)
               {
                  uint8_t tmp = (1 << i); /* which direction? UP/DOWN/LEFT/RIGHT */
                  if ((inbuf & tmp) == tmp)
                     JoyDownState0 &= ~tmp;
               }
            }
         }
      }
   }
   else
   {
      /* disable Joystick when software keyboard is active */
      if (!Keyboard_IsSwKeyboard())
      {
#if 0
         JoyState[0][port] = ret0;
         JoyState[1][port] = ret1;
#endif
      }
   }
}

uint8_t get_joy_downstate(void)   { return JoyDownState0;  }
void    reset_joy_downstate(void) { JoyDownState0 = 0xff;  }
