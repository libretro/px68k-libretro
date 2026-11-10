#include <string.h>

#include "midi_shadow.h"
#include "state_inline.h"

#define MS_UNSET8      0xFF
#define MS_UNSET16     0xFFFF
#define MS_SYSEX_CAP   16384
#define MS_DE_MAX      128
#define MS_DE_SIZE     6
#define MS_MAGIC       0x3153444DU /* "MDS1" */

enum { SEL_NONE = 0, SEL_RPN = 1, SEL_NRPN = 2 };

typedef struct
{
   uint32_t magic;       /* set when written; tells a loaded state carries this section */
   uint32_t source;
   uint32_t sysex_len;
   uint32_t de_count;
   uint32_t flags;       /* bit 0: SysEx log overflowed, replay is incomplete */
   uint16_t bend[16];
   uint8_t  prog[16];
   uint8_t  sel_kind[16];
   uint8_t  sel_msb[16];
   uint8_t  sel_lsb[16];
   uint8_t  cc[16][128];
   uint8_t  de[MS_DE_MAX * MS_DE_SIZE]; /* ch, kind, msb, lsb, data msb, data lsb */
   uint8_t  sysex[MS_SYSEX_CAP];        /* complete F0..F7 messages, back to back */
} MSState;

static MSState ms;

static int loaded_source = -1;
static MSState before;      /* the tracker as it was before the last load */
static int have_before;
static int replay_needed  = 0;

/* What a load must change on the synth: the setup, not bookkeeping fields. */
static int same_setup(const void *pa, const void *pb)
{
   const MSState *a = (const MSState*)pa, *b = (const MSState*)pb;
   return a->sysex_len == b->sysex_len &&
      a->de_count == b->de_count &&
      !memcmp(a->bend, b->bend, sizeof(a->bend)) &&
      !memcmp(a->prog, b->prog, sizeof(a->prog)) &&
      !memcmp(a->sel_kind, b->sel_kind, sizeof(a->sel_kind)) &&
      !memcmp(a->sel_msb, b->sel_msb, sizeof(a->sel_msb)) &&
      !memcmp(a->sel_lsb, b->sel_lsb, sizeof(a->sel_lsb)) &&
      !memcmp(a->cc, b->cc, sizeof(a->cc)) &&
      !memcmp(a->de, b->de, a->de_count * MS_DE_SIZE) &&
      !memcmp(a->sysex, b->sysex, a->sysex_len);
}

static void clear_setup(void)
{
   unsigned ch;
   memset(ms.cc, MS_UNSET8, sizeof(ms.cc));
   memset(ms.prog, MS_UNSET8, sizeof(ms.prog));
   memset(ms.sel_kind, 0, sizeof(ms.sel_kind));
   memset(ms.sel_msb, 0, sizeof(ms.sel_msb));
   memset(ms.sel_lsb, 0, sizeof(ms.sel_lsb));
   memset(ms.de, 0, sizeof(ms.de));
   for (ch = 0; ch < 16; ch++)
      ms.bend[ch] = MS_UNSET16;
   ms.de_count  = 0;
   ms.sysex_len = 0;
   ms.flags     = 0;
}

void MIDIShadow_Reset(void)
{
   uint32_t source = ms.source;
   memset(&ms, 0, sizeof(ms));
   ms.source = source;
   clear_setup();
   loaded_source = -1;
}

void MIDIShadow_SetSource(int source)
{
   ms.source = (uint32_t)source;
}

int MIDIShadow_GetLoadedSource(void)
{
   return loaded_source;
}

int MIDIShadow_NeedsReplay(void)
{
   return replay_needed;
}

/* Undo what the last load did to the tracker. It follows the synth, not the
 * machine: after a Run-Ahead rewind the synth has still been told everything
 * up to the furthest frame, so that is what it must keep. */
void MIDIShadow_RestoreLive(void)
{
   if (!have_before)
      return;
   memcpy(&ms, &before, sizeof(ms));
   loaded_source = -1;
   replay_needed = 0;
}

/* ---- recording ---- */

static void data_entry(unsigned ch, int is_lsb, uint8_t value)
{
   uint8_t *e = NULL;
   uint32_t i;

   if (ms.sel_kind[ch] == SEL_NONE)
      return;

   for (i = 0; i < ms.de_count; i++)
   {
      uint8_t *c = &ms.de[i * MS_DE_SIZE];
      if (c[0] == ch && c[1] == ms.sel_kind[ch] &&
            c[2] == ms.sel_msb[ch] && c[3] == ms.sel_lsb[ch])
      {
         e = c;
         break;
      }
   }

   if (!e)
   {
      if (ms.de_count >= MS_DE_MAX)
      {
         ms.flags |= 1;
         return;
      }
      e    = &ms.de[ms.de_count++ * MS_DE_SIZE];
      e[0] = (uint8_t)ch;
      e[1] = ms.sel_kind[ch];
      e[2] = ms.sel_msb[ch];
      e[3] = ms.sel_lsb[ch];
      e[4] = MS_UNSET8;
      e[5] = MS_UNSET8;
   }

   e[is_lsb ? 5 : 4] = value;
}

static void control_change(unsigned ch, unsigned cc, uint8_t value)
{
   switch (cc)
   {
      case 6:   data_entry(ch, 0, value); return;
      case 38:  data_entry(ch, 1, value); return;
      case 98:  ms.sel_kind[ch] = SEL_NRPN; ms.sel_lsb[ch] = value; return;
      case 99:  ms.sel_kind[ch] = SEL_NRPN; ms.sel_msb[ch] = value; return;
      case 100: ms.sel_kind[ch] = SEL_RPN;  ms.sel_lsb[ch] = value; goto rpn_null;
      case 101: ms.sel_kind[ch] = SEL_RPN;  ms.sel_msb[ch] = value;
rpn_null:
         /* RPN null (127/127) deselects: later data entry goes nowhere. */
         if (ms.sel_msb[ch] == 127 && ms.sel_lsb[ch] == 127)
            ms.sel_kind[ch] = SEL_NONE;
         return;
      case 121: /* reset all controllers */
         ms.cc[ch][1] = ms.cc[ch][11] = MS_UNSET8;
         ms.cc[ch][64] = ms.cc[ch][65] = ms.cc[ch][66] = ms.cc[ch][67] = MS_UNSET8;
         ms.bend[ch] = MS_UNSET16;
         ms.sel_kind[ch] = SEL_NONE;
         return;
      default:
         break;
   }

   /* 96/97 (data inc/dec) and 120+ (channel mode: notes off etc.) are
    * actions, not state. */
   if (cc >= 120 || cc == 96 || cc == 97)
      return;

   ms.cc[ch][cc] = value;
}

void MIDIShadow_Short(size_t packed)
{
   uint8_t status = (uint8_t)(packed & 0xFF);
   uint8_t d1     = (uint8_t)((packed >> 8) & 0x7F);
   uint8_t d2     = (uint8_t)((packed >> 16) & 0x7F);
   unsigned ch    = status & 0x0F;

   switch (status & 0xF0)
   {
      case 0xB0: control_change(ch, d1, d2);                 break;
      case 0xC0: ms.prog[ch] = d1;                           break;
      case 0xE0: ms.bend[ch] = (uint16_t)(d1 | (d2 << 7));   break;
      default: break;
   }
}

/* Messages that put the synth back into a known mode. Everything recorded
 * before one of them is moot. The device ID (byte 2) is ignored. */
static int is_mode_reset(const uint8_t *s, size_t len)
{
   static const uint8_t gs[]   = { 0xF0, 0x41, 0, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7 };
   static const uint8_t mt32[] = { 0xF0, 0x41, 0, 0x16, 0x12, 0x7F, 0x00, 0x00, 0x00, 0x01, 0xF7 };
   static const uint8_t xg[]   = { 0xF0, 0x43, 0, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7 };
   size_t i;

   if (len == 6 && s[1] == 0x7E && s[3] == 0x09 && s[4] <= 0x03) /* GM on/off, GM2 on */
      return 1;
   if (len == sizeof(gs) || len == sizeof(mt32) || len == sizeof(xg))
   {
      const uint8_t *ref = (len == sizeof(gs)) ? ((s[3] == 0x42) ? gs : mt32) : xg;
      if (len == sizeof(xg) && s[1] != 0x43)
         return 0;
      for (i = 0; i < len; i++)
         if (i != 2 && s[i] != ref[i])
            return 0;
      return 1;
   }
   return 0;
}

void MIDIShadow_Long(const uint8_t *s, size_t len)
{
   size_t pos = 0;

   if (len < 4 || s[0] != 0xF0 || s[len - 1] != 0xF7)
      return;

   if (is_mode_reset(s, len))
      clear_setup();

   /* Parameter-style messages (Roland/Yamaha: same target, new value) are
    * sent over and over by some games, e.g. master-volume fades. Keep only
    * the latest per target so the log doesn't fill up. The last three bytes
    * (data, checksum, EOX) are the value; everything before is the target. */
   if (len >= 8 && len <= 32 && (s[1] == 0x41 || s[1] == 0x43))
   {
      while (pos < ms.sysex_len)
      {
         size_t rec = pos;
         while (rec < ms.sysex_len && ms.sysex[rec] != 0xF7)
            rec++;
         rec++; /* include EOX */
         if (rec - pos == len && !memcmp(ms.sysex + pos, s, len - 3))
         {
            memcpy(ms.sysex + pos, s, len);
            return;
         }
         pos = rec;
      }
   }

   if (ms.sysex_len + len > MS_SYSEX_CAP)
   {
      ms.flags |= 1;
      return;
   }
   memcpy(ms.sysex + ms.sysex_len, s, len);
   ms.sysex_len += (uint32_t)len;
}

/* ---- replay ---- */

#define CC_MSG(ch, cc, v) ((size_t)(0xB0 | (ch)) | ((size_t)(cc) << 8) | ((size_t)(v) << 16))

static void send_select(MIDIShadow_ShortFn out, unsigned ch, int kind, uint8_t msb, uint8_t lsb)
{
   if (kind == SEL_NRPN)
   {
      out(CC_MSG(ch, 99, msb));
      out(CC_MSG(ch, 98, lsb));
   }
   else
   {
      out(CC_MSG(ch, 101, msb));
      out(CC_MSG(ch, 100, lsb));
   }
}

void MIDIShadow_Replay(MIDIShadow_ShortFn out, MIDIShadow_LongFn out_long)
{
   size_t pos = 0;
   unsigned ch, cc;
   uint32_t i;

   /* Global setup first, in the order it was originally sent. */
   while (pos < ms.sysex_len)
   {
      size_t rec = pos;
      while (rec < ms.sysex_len && ms.sysex[rec] != 0xF7)
         rec++;
      rec++;
      if (rec > ms.sysex_len)
         break;
      out_long(ms.sysex + pos, rec - pos);
      pos = rec;
   }

   for (ch = 0; ch < 16; ch++)
   {
      /* Bank select must precede the program change it applies to. */
      if (ms.cc[ch][0]  != MS_UNSET8) out(CC_MSG(ch, 0,  ms.cc[ch][0]));
      if (ms.cc[ch][32] != MS_UNSET8) out(CC_MSG(ch, 32, ms.cc[ch][32]));
      if (ms.prog[ch]   != MS_UNSET8) out((size_t)(0xC0 | ch) | ((size_t)ms.prog[ch] << 8));

      for (cc = 1; cc < 120; cc++)
      {
         if (cc == 32 || cc == 6 || cc == 38 || (cc >= 96 && cc <= 101))
            continue;
         if (ms.cc[ch][cc] != MS_UNSET8)
            out(CC_MSG(ch, cc, ms.cc[ch][cc]));
      }

      for (i = 0; i < ms.de_count && i < MS_DE_MAX; i++)
      {
         const uint8_t *e = &ms.de[i * MS_DE_SIZE];
         if (e[0] != ch)
            continue;
         send_select(out, ch, e[1], e[2], e[3]);
         if (e[4] != MS_UNSET8) out(CC_MSG(ch, 6,  e[4]));
         if (e[5] != MS_UNSET8) out(CC_MSG(ch, 38, e[5]));
         send_select(out, ch, e[1], 127, 127); /* leave nothing selected */
      }
      /* Put back whichever RPN/NRPN the game had selected. */
      if (ms.sel_kind[ch] != SEL_NONE)
         send_select(out, ch, ms.sel_kind[ch], ms.sel_msb[ch], ms.sel_lsb[ch]);

      if (ms.bend[ch] != MS_UNSET16)
         out((size_t)(0xE0 | ch) | ((size_t)(ms.bend[ch] & 0x7F) << 8) |
               ((size_t)((ms.bend[ch] >> 7) & 0x7F) << 16));
   }
}

/* ---- savestate ---- */

int MIDIShadow_StateAction(StateMem *sm, int load, int data_only)
{
   SFORMAT regs[] =
   {
      SFVARN(ms.magic, "MAGIC"),
      SFVARN(ms.source, "SOURCE"),
      SFVARN(ms.sysex_len, "SYSEXLEN"),
      SFVARN(ms.de_count, "DECOUNT"),
      SFVARN(ms.flags, "FLAGS"),
      SFARRAY16N(ms.bend, 16, "BEND"),
      SFARRAYN(ms.prog, sizeof(ms.prog), "PROG"),
      SFARRAYN(ms.sel_kind, sizeof(ms.sel_kind), "SELKIND"),
      SFARRAYN(ms.sel_msb, sizeof(ms.sel_msb), "SELMSB"),
      SFARRAYN(ms.sel_lsb, sizeof(ms.sel_lsb), "SELLSB"),
      SFARRAYN(ms.cc, sizeof(ms.cc), "CC"),
      SFARRAYN(ms.de, sizeof(ms.de), "DE"),
      /* Always the full capacity when saving: the size of a state must not
       * depend on what has been played, because a frontend may ask for it
       * once (rewind) and reuse the buffer. VARLEN lets a load accept any
       * recorded length. */
      { ms.sysex, MS_SYSEX_CAP, PX68KSTATE_VARLEN, "SYSEX" },
      SFEND
   };
   int ret;

   if (!load)
   {
      ms.magic = MS_MAGIC;
      /* Keep saved files deterministic: nothing stale after the used part.
       * (Fast states are never stored, so skip it there.) */
      if (!sm->fastsavestates)
         memset(ms.sysex + ms.sysex_len, 0, MS_SYSEX_CAP - ms.sysex_len);
      return PX68KSS_StateAction(sm, 0, data_only, regs, "X68K_MIDISYN", false);
   }

   /* A state from an older build has no such section: optional, and then
    * it says nothing about the synth, so start from a clean slate. */
   memcpy(&before, &ms, sizeof(ms)); /* what the synth has been told so far */
   have_before = 1;
   ms.magic = 0;
   clear_setup();
   ret = PX68KSS_StateAction(sm, 1, data_only, regs, "X68K_MIDISYN", true);

   if (ms.magic != MS_MAGIC)
   {
      MIDIShadow_Reset();
      return ret;
   }

   /* Never trust counts read from a file. */
   if (ms.sysex_len > MS_SYSEX_CAP) ms.sysex_len = 0;
   if (ms.de_count  > MS_DE_MAX)    ms.de_count  = 0;
   {
      unsigned ch;
      for (ch = 0; ch < 16; ch++)
         if (ms.sel_kind[ch] > SEL_NRPN)
            ms.sel_kind[ch] = SEL_NONE;
   }
   loaded_source = (int)ms.source;
   /* If the synth already holds exactly this setup (rewinding, quick-loading
    * the same point), there is nothing to send. */
   replay_needed = !same_setup(&before, &ms);
   return ret;
}
