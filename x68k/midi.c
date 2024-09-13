/*
 *  MIDI.C - MIDI Board (CZ-6BM1) emulator
 */

#include "common.h"
#include "prop.h"
#include "winx68k.h"
#include "../libretro/dosio.h"
#include "../libretro/mmsystem.h"
#include "x68kmemory.h"
#include "irqh.h"
#include "midi.h"
#include "m68000.h"

#define MIDIBUFFERS 1024			/* 1024 (should never needed more than this) */
#define MIDIBUFTIMER 3200			/* 10MHz / (31.25K / 10bit) = 3200 */
#define MIDIFIFOSIZE 256
#define MIDIDELAYBUF 4096			/* is it ok to have 31250/10 = 3125 byts (1sec)?  */

enum {
	MIDI_NOTUSED,
	MIDI_DEFAULT,
	MIDI_MT32,
	MIDI_CM32L,
	MIDI_CM64,
	MIDI_CM300,
	MIDI_CM500,
	MIDI_SC55,
	MIDI_SC88,
	MIDI_LA,
	MIDI_GM,
	MIDI_GS,
	MIDI_XG
};

static void *hOut = NULL;
static int		MIDI_CTRL;
static int		MIDI_POS;
static int		MIDI_SYSCOUNT;
static uint8_t		MIDI_LAST;
static uint8_t		MIDI_BUF[MIDIBUFFERS];
static uint8_t		MIDI_EXCVBUF[MIDIBUFFERS];

static uint8_t		MIDI_RegHigh = 0;
static uint8_t		MIDI_Vector = 0;
static uint8_t		MIDI_IntEnable = 0;
static uint8_t		MIDI_IntVect = 0;
static uint8_t		MIDI_IntFlag = 0;
static uint32_t		MIDI_Buffered = 0;
static int32_t		MIDI_BufTimer = 3333;
static uint8_t		MIDI_R05 = 0;
static uint32_t		MIDI_GTimerMax = 0;
static uint32_t		MIDI_MTimerMax = 0;
static int32_t		MIDI_GTimerVal = 0;
static int32_t		MIDI_MTimerVal = 0;
static uint8_t		MIDI_MODULE = MIDI_NOTUSED;

static uint8_t MIDI_ResetType[5] = {
	MIDI_LA, MIDI_GM, MIDI_GS, MIDI_XG
};

typedef struct {
	uint32_t time;
	uint8_t msg;
} DELAYBUFITEM;

static DELAYBUFITEM DelayBuf[MIDIDELAYBUF];
static int DBufPtrW = 0;
static int DBufPtrR = 0;

/* Nekomichi 6, MIMPI tone map compatibility */

enum {
	MIMPI_LA = 0,
	MIMPI_PCM,
	MIMPI_GS,
	MIMPI_RHYTHM
};

static	uint8_t		LOADED_TONEMAP = 0;
static	uint8_t		ENABLE_TONEMAP = 0;
static	uint8_t		TONE_CH[16];
static	uint8_t		TONEBANK[3][128];
static	uint8_t		TONEMAP[3][128];

static uint8_t EXCV_MTRESET[] = { 0xf0, 0x41, 0x10, 0x16, 0x12, 0x7f, 0x00, 0x00, 0x00, 0x01, 0xf7 };
static uint8_t EXCV_GMRESET[] = { 0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
static uint8_t EXCV_GSRESET[] = { 0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7};
static uint8_t EXCV_XGRESET[] = { 0xf0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xf7};

#define	MIDICTRL_READY		0
#define	MIDICTRL_2BYTES		1
#define	MIDICTRL_3BYTES		2
#define	MIDICTRL_EXCLUSIVE	3
#define	MIDICTRL_TIMECODE	4
#define MIDICTRL_SYSTEM		5

#define	MIDI_EXCLUSIVE		0xf0
#define MIDI_TIMECODE		0xf1
#define MIDI_SONGPOS		0xf2
#define MIDI_SONGSELECT		0xf3
#define	MIDI_TUNEREQUEST	0xf6
#define	MIDI_EOX			0xf7
#define	MIDI_TIMING			0xf8
#define MIDI_START			0xfa
#define MIDI_CONTINUE		0xfb
#define	MIDI_STOP			0xfc
#define	MIDI_ACTIVESENSE	0xfe
#define	MIDI_SYSTEMRESET	0xff

#define MIDIOUTS(a,b,c) (((size_t)c << 16) | ((size_t)b << 8) | (size_t)a)

int MIDI_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFVAR(MIDI_CTRL),
		SFVAR(MIDI_POS),
		SFVAR(MIDI_SYSCOUNT),

		SFVAR(MIDI_LAST),
		SFARRAY(MIDI_BUF, 1024),
		SFARRAY(MIDI_EXCVBUF, 1024),

		SFVAR(MIDI_RegHigh),
		SFVAR(MIDI_Vector),
		SFVAR(MIDI_IntEnable),
		SFVAR(MIDI_IntVect),
		SFVAR(MIDI_IntFlag),
		SFVAR(MIDI_Buffered),
		SFVAR(MIDI_BufTimer),
		SFVAR(MIDI_R05),
		SFVAR(MIDI_GTimerMax),
		SFVAR(MIDI_MTimerMax),
		SFVAR(MIDI_GTimerVal),
		SFVAR(MIDI_MTimerVal),
		SFVAR(MIDI_MODULE),

		SFARRAY(DelayBuf, ((4 + 1) * MIDIDELAYBUF)),
		SFVAR(DBufPtrW),
		SFVAR(DBufPtrR),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_MIDI", false);

	return ret;
}


static uint32_t FASTCALL MIDI_Int(uint8_t irq)
{
	IRQH_IRQCallBack(irq);
	if ( irq==4 )
		return (uint32_t)(MIDI_Vector|MIDI_IntVect);
	return (uint32_t)(-1);
}

void FASTCALL MIDI_Timer(uint32_t clk)
{
	if ( !Config.MIDI_SW ) return;	/* return when MIDI is OFF */

	MIDI_BufTimer -= clk;
	if (MIDI_BufTimer<0)
	{
		MIDI_BufTimer += MIDIBUFTIMER;
		if (MIDI_Buffered)
		{
			MIDI_Buffered--;
			if ( (MIDI_Buffered<MIDIFIFOSIZE)&&(MIDI_IntEnable&0x40) )		/* Tx FIFO Empty Interrupt（エトプリ） */
			{
				MIDI_IntFlag |= 0x40;
				MIDI_IntVect = 0x0c;
				IRQH_Int(4, &MIDI_Int);
			}
		}
	}

	if (MIDI_MTimerMax)
	{
		 MIDI_MTimerVal -= clk;
		if (MIDI_MTimerVal<0) /* Midi timer interrup (Magic Bottle) */
		{
			while (MIDI_MTimerVal<0) MIDI_MTimerVal += MIDI_MTimerMax*80;
			if ( (!(MIDI_R05&0x80))&&(MIDI_IntEnable&0x02) )
			{
				MIDI_IntFlag |= 0x02;
				MIDI_IntVect = 0x02;
				IRQH_Int(4, &MIDI_Int);
			}
		}
	}

	if (MIDI_GTimerMax)
	{
		MIDI_GTimerVal -= clk;
		if (MIDI_GTimerVal<0) /* General timer interrupt (RCD.X) */
		{
			while (MIDI_GTimerVal<0) MIDI_GTimerVal += MIDI_GTimerMax*80;
			if ( MIDI_IntEnable&0x80 )
			{
				MIDI_IntFlag |= 0x80;
				MIDI_IntVect = 0x0e;
				IRQH_Int(4, &MIDI_Int);
			}
		}
	}
}

static void MIDI_SetModule(void)
{
	if (Config.MIDI_SW)
		MIDI_MODULE = MIDI_ResetType[Config.MIDI_Type];
	else
		MIDI_MODULE = MIDI_NOTUSED;
}

static void MIDI_Sendexclusive(uint8_t *excv, size_t length)
{
	memcpy(MIDI_EXCVBUF, excv, length);
	midi_out_long_msg(MIDI_EXCVBUF, length);
}

void MIDI_Reset(void)
{
	memset(DelayBuf, 0, sizeof(DelayBuf));
	DBufPtrW = DBufPtrR = 0;

	if (hOut)
	{
		size_t msg;
		switch(MIDI_MODULE)
		{
			case MIDI_NOTUSED:
				return;
			case MIDI_MT32:
			case MIDI_CM32L:
			case MIDI_CM64:
			case MIDI_LA:
				MIDI_Sendexclusive(EXCV_MTRESET, sizeof(EXCV_MTRESET));
				break;
			case MIDI_SC55:
			case MIDI_SC88:
			case MIDI_GS:
				MIDI_Sendexclusive(EXCV_GSRESET, sizeof(EXCV_GSRESET));
				break;
			case MIDI_XG:
				MIDI_Sendexclusive(EXCV_XGRESET, sizeof(EXCV_XGRESET));
				break;
			default:
				MIDI_Sendexclusive(EXCV_GMRESET, sizeof(EXCV_GMRESET));
				break;
		}
		for (msg=0x7bb0; msg<0x7bc0; msg++)
			midi_out_short_msg(msg);
	}
}

void MIDI_Init(void)
{
	memset(DelayBuf, 0, sizeof(DelayBuf));
	DBufPtrW = DBufPtrR = 0;

	MIDI_SetModule();
	MIDI_RegHigh = 0;
	MIDI_Vector = 0;
	MIDI_IntEnable = 0;
	MIDI_IntVect = 0;
	MIDI_IntFlag = 0;
	MIDI_R05 = 0;

	MIDI_CTRL = MIDICTRL_READY;
	MIDI_LAST = 0x80;

	if (!hOut)
	{
		if (midi_out_open(&hOut) != 0)
			hOut = NULL;
	}
}

void MIDI_Cleanup(void)
{
	if (hOut)
	{
		MIDI_Reset();
		hOut = NULL;
	}
}

void MIDI_Message(uint8_t mes)
{
	if (!hOut)
		return;

	switch(mes)
	{
		case MIDI_TIMING:
		case MIDI_START:
		case MIDI_CONTINUE:
		case MIDI_STOP:
		case MIDI_ACTIVESENSE:
		case MIDI_SYSTEMRESET:
			return;
	}

	if (MIDI_CTRL == MIDICTRL_READY) {
		if (mes & 0x80) {
			/* status */
			MIDI_POS = 0;
			switch(mes & 0xf0) {
				case 0xc0:
				case 0xd0:
					MIDI_LAST = mes;
					MIDI_CTRL = MIDICTRL_2BYTES;
					break;
				case 0x80:
				case 0x90:
				case 0xa0:
				case 0xb0:
				case 0xe0:
					MIDI_LAST = mes;
					MIDI_CTRL = MIDICTRL_3BYTES;
					break;
				default:
					switch(mes) {
						case MIDI_EXCLUSIVE:
							MIDI_CTRL = MIDICTRL_EXCLUSIVE;
							break;
						case MIDI_TIMECODE:
							MIDI_CTRL = MIDICTRL_TIMECODE;
							break;
						case MIDI_SONGPOS:
							MIDI_CTRL = MIDICTRL_SYSTEM;
							MIDI_SYSCOUNT = 3;
							break;
						case MIDI_SONGSELECT:
							MIDI_CTRL = MIDICTRL_SYSTEM;
							MIDI_SYSCOUNT = 2;
							break;
						case MIDI_TUNEREQUEST:
							MIDI_CTRL = MIDICTRL_SYSTEM;
							MIDI_SYSCOUNT = 1;
							break;
						default:
							return;
					}
					break;
			}
		}
		else {
			/* running status */
			MIDI_BUF[0] = MIDI_LAST;
			MIDI_POS    = 1;
			switch (MIDI_LAST & 0xf0)
			{
			case 0xc0:
			case 0xd0:
				MIDI_CTRL = MIDICTRL_2BYTES;
				break;
			case 0x80:
			case 0x90:
			case 0xa0:
			case 0xb0:
			case 0xe0:
				MIDI_CTRL   = MIDICTRL_3BYTES;
				break;
			default:
				return;
			}
		}
	}
	else if ( (mes&0x80) && ((MIDI_CTRL!=MIDICTRL_EXCLUSIVE)||(mes!=MIDI_EOX)) ) /* When a control byte appears in the data section of a message... (GENOCIDE2) */
	{
		/* status */
		MIDI_POS = 0;
		switch(mes & 0xf0)
		{
			case 0xc0:
			case 0xd0:
				MIDI_LAST = mes;
				MIDI_CTRL = MIDICTRL_2BYTES;
				break;
			case 0x80:
			case 0x90:
			case 0xa0:
			case 0xb0:
			case 0xe0:
				MIDI_LAST = mes;
				MIDI_CTRL = MIDICTRL_3BYTES;
				break;
			default:
				switch(mes) {
					case MIDI_EXCLUSIVE:
						MIDI_CTRL = MIDICTRL_EXCLUSIVE;
						break;
					case MIDI_TIMECODE:
						MIDI_CTRL = MIDICTRL_TIMECODE;
						break;
					case MIDI_SONGPOS:
						MIDI_CTRL = MIDICTRL_SYSTEM;
						MIDI_SYSCOUNT = 3;
						break;
					case MIDI_SONGSELECT:
						MIDI_CTRL = MIDICTRL_SYSTEM;
						MIDI_SYSCOUNT = 2;
						break;
					case MIDI_TUNEREQUEST:
						MIDI_CTRL = MIDICTRL_SYSTEM;
						MIDI_SYSCOUNT = 1;
						break;
					default:
						return;
				}
				break;
		}
	}

	MIDI_BUF[MIDI_POS++] = mes;

	switch(MIDI_CTRL)
	{
		case MIDICTRL_2BYTES:
			if (MIDI_POS >= 2)
			{
				if (ENABLE_TONEMAP)
				{
					if (((MIDI_BUF[0] & 0xf0) == 0xc0) &&
						(TONE_CH[MIDI_BUF[0] & 0x0f] < MIMPI_RHYTHM))
						MIDI_BUF[1] = TONEMAP[ TONE_CH[MIDI_BUF[0] & 0x0f] ][ MIDI_BUF[1] & 0x7f ];
				}
				midi_out_short_msg(MIDIOUTS(MIDI_BUF[0], MIDI_BUF[1], 0));
				MIDI_CTRL = MIDICTRL_READY;
			}
			break;
		case MIDICTRL_3BYTES:
			if (MIDI_POS >= 3)
			{
				midi_out_short_msg( 
				MIDIOUTS(MIDI_BUF[0],
					MIDI_BUF[1], MIDI_BUF[2]));
				MIDI_CTRL = MIDICTRL_READY;
			}
			break;
		case MIDICTRL_EXCLUSIVE:
			if (mes == MIDI_EOX)
			{
				MIDI_Sendexclusive(MIDI_BUF, MIDI_POS);
				MIDI_CTRL = MIDICTRL_READY;
			}
			else if (MIDI_POS >= MIDIBUFFERS) /* overflow */
				MIDI_CTRL = MIDICTRL_READY;
			break;
		case MIDICTRL_TIMECODE:
			if (MIDI_POS >= 2)
			{
				/* it should be the same as exclusive */
				if ((mes == 0x7e) || (mes == 0x7f))
					MIDI_CTRL = MIDICTRL_EXCLUSIVE;
				else
					MIDI_CTRL = MIDICTRL_READY;
			}
			break;
		case MIDICTRL_SYSTEM:
			if (MIDI_POS >= MIDI_SYSCOUNT)
				MIDI_CTRL = MIDICTRL_READY;
			break;
	}
}

uint8_t FASTCALL MIDI_Read(uint32_t adr)
{
	uint8_t ret = 0;

	if ( (adr<0xeafa01)||(adr>=0xeafa10)||(!Config.MIDI_SW) ) /* 変なアドレスか、 */
	{
		/* When MIDI is OFF, a bus error occurs */
		BusErrFlag = 1;
		return 0;
	}

	switch(adr&15)
	{
		case 0x01:
			ret = (MIDI_Vector | MIDI_IntVect);
			MIDI_IntVect=0x10;
			break;
		case 0x09:			/* R04, 14, ... 94 */
			if (MIDI_RegHigh == 5)
			{
				if (MIDI_Buffered>=MIDIFIFOSIZE)
					return 0x01;	/* Tx full/not ready */
				return 0xc0;	/* Tx empty/ready */
			}
			break;
		case 0x03:
		case 0x05:
		case 0x07:
		case 0x0b:			/* R05, 15, ... 95 */
		case 0x0d:			/* R06, 16, ... 96 */
		case 0x0f:			/* R07, 17, ... 97 */
			break;
	}
	return ret;
}


static void AddDelayBuf(uint8_t msg)
{
	int newptr = (DBufPtrW+1)%MIDIDELAYBUF;
	if ( newptr!=DBufPtrR )
	{
		DelayBuf[DBufPtrW].time = timeGetTime();
		DelayBuf[DBufPtrW].msg  = msg;
		DBufPtrW = newptr;
	}
}

void MIDI_DelayOut(unsigned int delay)
{
	while ( DBufPtrW!=DBufPtrR )
	{
		unsigned int t = timeGetTime();
		if ( (t-DelayBuf[DBufPtrR].time)>=delay )
		{
			MIDI_Message(DelayBuf[DBufPtrR].msg);
			DBufPtrR = (DBufPtrR+1)%MIDIDELAYBUF;
		}
		else
			break;
	}
}

void FASTCALL MIDI_Write(uint32_t adr, uint8_t data)
{
	if ( (adr<0xeafa01)||(adr>=0xeafa10)||(!Config.MIDI_SW) )
	{
		/* When MIDI is OFF, a bus error occurs */
		BusErrFlag = 1;
		return;
	}

	switch(adr&15)
	{
		case 0x03:
			MIDI_RegHigh = data&0x0f;
			if (data&0x80)
				MIDI_Init();
			break;
		case 0x01:
		case 0x05:
		case 0x07:
			break;
		case 0x09:			/* R04, 14, ... 94 */
			switch(MIDI_RegHigh)
			{
				case 0:
					MIDI_Vector = (data&0xe0);
					break;
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7:
				case 9:
					break;
				case 8:
					MIDI_GTimerMax = (MIDI_GTimerMax&0xff00)|(uint32_t)data;
					break;
			}
			break;
		case 0x0b:			/* R05, 15, ... 95 */
			switch(MIDI_RegHigh)
			{
				case 0:
					MIDI_R05 = data;
					break;
				case 1:
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7:
				case 9:
					break;
				case 8:
					MIDI_GTimerMax = (MIDI_GTimerMax&0xff)|(((uint32_t)(data&0x3f))*256);
					if (data&0x80)
						MIDI_GTimerVal = MIDI_GTimerMax*80;
					break;
			}
			break;
		case 0x0d:			/* R06, 16, ... 96 */
			switch(MIDI_RegHigh)
			{
				case 0:
					MIDI_IntEnable = data;
					break;
				case 5:			/* Out Data Byte */
					if (!MIDI_Buffered)
						MIDI_BufTimer = MIDIBUFTIMER;
					MIDI_Buffered ++;
					AddDelayBuf(data);
					break;
				case 8:
					MIDI_MTimerMax = (MIDI_MTimerMax&0xff00)|(uint32_t)data;
					break;
				case 1:
				case 2:
				case 3:
				case 4:
				case 6:
				case 7:
				case 9:
					break;
			}
			break;
		case 0x0f:			/* R07, 17, ... 97 */
			if (MIDI_RegHigh == 8)
			{
				MIDI_MTimerMax = (MIDI_MTimerMax&0xff)|(((uint32_t)(data&0x3f))*256);
				if (data&0x80)
					MIDI_MTimerVal = MIDI_MTimerMax*80;
			}
			break;
	}
}

static int exstrcmp(char *str, char *cmp)
{
	uint8_t	c;

	while(*cmp)
	{
		c = *str++;
		if ((c >= 'a') && (c <= 'z'))
			c -= 0x20;
		if (c != *cmp++)
			return 1;
	}
	return 0;
}

static void cutdelimita(char **buf)
{
	uint8_t	c;

	for(;;) {
		c = **buf;
		if (!c)
			break;
		if (c > ' ')
			break;
		(*buf)++;
	}
}

static int getvalue(char **buf, int cutspace)
{
	int	ret    = 0;
	int	valhit = 0;
	uint8_t	c;

	if (cutspace)
		cutdelimita(buf);
	for (;; valhit=1)
	{
		c = **buf;
		if (!c)
		{
			if (!valhit)
				return(-1);
			break;
		}
		if ((c < '0') || (c > '9'))
			break;
		ret = ret * 10 + (c - '0');
		(*buf)++;
	}
	return ret;
}

static int file_readline(void *fh, char *buf, int len)
{
	size_t	pos;
	size_t	readsize;
	int	i;

	if (len < 2)
		return -1;
	pos = file_seek(fh, 0, FSEEK_CUR);
	if (pos == -1)
		return -1;
	readsize = file_lread(fh, buf, len-1);
	if (readsize == -1)
		return -1;
	if (!readsize)
		return -1;
	for (i=0; i<readsize; i++)
	{
		pos++;
		if ((buf[i] == 0x0a) || (buf[i] == 0x0d))
			break;
	}
	buf[i] = '\0';
	if (file_seek(fh, pos, FSEEK_SET) != pos)
		return(-1);
	return(i);
}

static void mimpidefline_analyze(char *buf)
{
	cutdelimita(&buf);
	if (*buf == '@')
	{
		int ch;
		buf++;
		ch = getvalue(&buf, 0);
		if ((ch < 1) || (ch > 16))
			return;
		ch--;
		cutdelimita(&buf);
		if (!exstrcmp(buf, "LA"))
			TONE_CH[ch] = MIMPI_LA;
		else if (!exstrcmp(buf, "PCM"))
			TONE_CH[ch] = MIMPI_PCM;
		else if (!exstrcmp(buf, "GS"))
			TONE_CH[ch] = MIMPI_GS;
		else if (!exstrcmp(buf, "RHYTHM"))
			TONE_CH[ch] = MIMPI_RHYTHM;
	}
	else {
		int	mod, num, bank, tone;
		mod = getvalue(&buf, 0);
		if ((mod < 0) || (mod >= MIMPI_RHYTHM)) {
			return;
		}
		num = getvalue(&buf, 1);
		if ((num < 1) || (num > 128)) {
			return;
		}
		num--;
		tone = getvalue(&buf, 1);
		if ((tone < 1) || (tone > 128)) {
			return;
		}
		if (*buf == ':') {
			buf++;
			bank = tone - 1;
			tone = getvalue(&buf, 1);
			if ((tone < 1) || (tone > 128)) {
				return;
			}
			TONEBANK[mod][num] = bank;
		}
		TONEMAP[mod][num] = tone-1;
	}
}

int MIDI_SetMimpiMap(char *filename)
{
	uint8_t		b;
	void *		fh;
	char		buf[128];

	LOADED_TONEMAP = 0;
	memset(TONE_CH, 0, sizeof(TONE_CH));
	memset(TONEBANK[0], 0, sizeof(TONEBANK));
	for (b=0; b<128; b++)
   {
		TONEMAP[0][b] = b;
		TONEMAP[1][b] = b;
		TONEMAP[2][b] = b;
	}
	TONE_CH[9] = MIMPI_RHYTHM;

	if ((filename == NULL) || (!filename[0]))
	{
		ENABLE_TONEMAP = 0;
		return 0;
	}
	fh = file_open(filename);
	if (fh == (void*)-1)
	{
		ENABLE_TONEMAP = 0;
		return 0;
	}
	while(file_readline(fh, buf, sizeof(buf)) >= 0)
		mimpidefline_analyze(buf);
	file_close(fh);

	LOADED_TONEMAP = 1;
	return 1;
}

int MIDI_EnableMimpiDef(int enable)
{
	ENABLE_TONEMAP = 0;
	if ((enable) && (LOADED_TONEMAP))
	{
		ENABLE_TONEMAP = 1;
		return 1;
	}
	return 0;
}
