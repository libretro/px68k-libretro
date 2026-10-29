/*
 *  FDD.C - FDD Unit
 */

#include <string.h>

#include "common.h"
#include "../libretro/dosio.h"
#include "status.h"
#include "irqh.h"
#include "ioc.h"
#include "fdc.h"
#include "fdd.h"
#include "disk_d88.h"
#include "disk_xdf.h"
#include "disk_dim.h"
#include "../libretro/zipdisk.h"

typedef struct {
	int SetDelay[4];
	int Types[4];
	int ROnly[4];
	int EMask[4];
	int Blink[4];
	int Access;
} FDDINFO;

static FDDINFO fdd;
static int (*SetFD[4])(int, char*)                             = { 0, XDF_SetFD,        D88_SetFD,        DIM_SetFD };
static int (*Eject[4])(int)                                    = { 0, XDF_Eject,        D88_Eject,        DIM_Eject };
static int (*Seek[4])(int, int, FDCID*)                        = { 0, XDF_Seek,         D88_Seek,         DIM_Seek };
static int (*ReadID[4])(int, FDCID*)                           = { 0, XDF_ReadID,       D88_ReadID,       DIM_ReadID };
static int (*WriteID[4])(int, int, uint8_t*, int)        = { 0, XDF_WriteID,      D88_WriteID,      DIM_WriteID };
static int (*Read[4])(int, FDCID*, uint8_t*)             = { 0, XDF_Read,         D88_Read,         DIM_Read };
static int (*ReadDiag[4])(int, FDCID*, FDCID*, uint8_t*) = { 0, XDF_ReadDiag,     D88_ReadDiag,     DIM_ReadDiag };
static int (*Write[4])(int, FDCID*, uint8_t*, int)       = { 0, XDF_Write,        D88_Write,        DIM_Write };
static int (*GetCurrentID[4])(int, FDCID*)                     = { 0, XDF_GetCurrentID, D88_GetCurrentID, DIM_GetCurrentID };

int FDD_IsReading                                              = 0;

int FDD_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFARRAY32(fdd.SetDelay, 4),
		SFARRAY32(fdd.Types, 4),
		SFARRAY32(fdd.ROnly, 4),
		SFARRAY32(fdd.EMask, 4),
		SFARRAY32(fdd.Blink, 4),

		SFVAR(fdd.Access),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_FDD", false);

	return ret;
}

static void ConvertCapital(char* buf)
{
	for ( ; *buf; buf++)
   {
		if ( ((*buf>=0x80)&&(*buf<=0x9f))||(*buf>=0xe0) )
			buf++;
		else if ( (*buf>='a')&&(*buf<='z') )
         *buf -= 0x20;
	}
}

/* Identify floppy containers by structure before considering their names.
 * No DIM signature is required: older DIM files can have an empty headerinfo.
 * A size/track-map match is required to avoid mistaking raw sectors for DIM.
 * Detection reads only the first 256 bytes; no extra disk buffer is needed. */
static unsigned long FDD_LE32(const uint8_t *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static int FDD_DetectDiskType(char *filename)
{
    static const unsigned int dim_track_size[10] =
        {8192, 9216, 7680, 9216, 0, 0, 0, 0, 0, 9216};
    uint8_t header[256];
    void *fp;
    int64_t length;
    size_t got;
    unsigned long expected, disk_size, offset, previous;
    unsigned int i, tracks;
    int d88_valid;
    int fallback;
    char tmp[8], *p;

    fallback = FD_XDF;
    p = strrchr(filename, '.');
    if (p) {
        memset(tmp, 0, sizeof(tmp));
        strncpy(tmp, p + 1, 3);
        ConvertCapital(tmp);
        if (!strncmp(tmp, "D88", 3) || !strncmp(tmp, "88D", 3))
            fallback = FD_D88;
        else if (!strncmp(tmp, "DIM", 3))
            fallback = FD_DIM;
    }

    fp = file_open(filename);
    if (!fp) return fallback;
    file_seek(fp, 0, FSEEK_END);
    length = file_tell(fp);
    file_seek(fp, 0, FSEEK_SET);
    got = file_lread(fp, header, sizeof(header));
    file_close(fp);
    if (got != sizeof(header) || length <= 0) return fallback;

    /* D88: validate size and ordered, in-range track offsets. */
    disk_size = FDD_LE32(header + 28);
    d88_valid = (length >= 688 && disk_size == (unsigned long)length &&
                 header[26] <= 1 && (header[27] == 0 ||
                 header[27] == 0x10 || header[27] == 0x20 ||
                 header[27] == 0x30));
    /* The full D88 track table is 164 * 4 bytes; examine it only
     * after the fixed header passes basic checks. */
    if (d88_valid) {
        uint8_t table[656];
        fp = file_open(filename);
        if (fp) {
            file_seek(fp, 32, FSEEK_SET);
            got = file_lread(fp, table, sizeof(table));
            file_close(fp);
        } else got = 0;
        if (got != sizeof(table)) d88_valid = 0;
        previous = 0;
        tracks = 0;
        if (d88_valid) {
            for (i = 0; i < 164; ++i) {
                offset = FDD_LE32(table + i * 4);
                if (!offset) continue;
                if (offset < 688 || offset >= disk_size ||
                    (previous && offset <= previous)) {
                    d88_valid = 0;
                    break;
                }
                previous = offset;
                ++tracks;
            }
            if (!tracks) d88_valid = 0;
        }
        if (d88_valid) return FD_D88;
    }

    /* DIM: 256-byte header + precisely the tracks marked present.
     * Track flags are conventionally 0 or 1, but accept any nonzero
     * value as the existing DIM loader does. */
    if (header[0] < 10 && dim_track_size[header[0]]) {
        expected = 256;
        tracks = 0;
        for (i = 1; i <= 170; ++i) {
            if (header[i]) {
                expected += dim_track_size[header[0]];
                ++tracks;
            }
        }
        if (tracks && expected == (unsigned long)length)
            return FD_DIM;
    }

    /* The existing XDF handler supports 77 cylinders, two heads,
     * eight 1024-byte sectors. Do not misroute other raw geometries. */
    if (length == 1261568) return FD_XDF;
    return fallback;
}

static uint32_t FASTCALL FDD_Int(uint8_t irq)
{
	IRQH_IRQCallBack(irq);
	if ( irq==1 )
		return ((uint32_t)IOC_IntVect+1);
	return -1;
}

void FDD_SetFD(int drive, char* filename, int readonly)
{
	int type;
	if ( (drive<0)||(drive>3) ) return;
	type = FDD_DetectDiskType(filename);
	if ( (drive<0)||(drive>3) ) return;
	FDD_EjectFD(drive);
	if ( SetFD[type] ) {
		if ( SetFD[type](drive, filename) ) {
			fdd.Types[drive]  = type;
			fdd.ROnly[drive] |= readonly;
			if (px68k_is_zip_virtual_path(filename)) fdd.ROnly[drive] = 1;
			fdd.SetDelay[drive] = 3;
			fdd.EMask[drive] = 0;
			fdd.Blink[drive] = 0;
			StatBar_SetFDD(drive, filename);
			StatBar_ParamFDD(drive, (fdd.Types[drive]!=FD_Non)?((fdd.Access==drive)?2:1):0, ((fdd.Types[drive]!=FD_Non)&&(!fdd.EMask[drive]))?1:0, (fdd.Blink[drive])?1:0);
		}
	}
}

void FDD_EjectFD(int drive)
{
	int type;
	if ( (drive<0)||(drive>3) ) return;
	type = fdd.Types[drive];
	if ( Eject[type] ) {
		Eject[type](drive);
		if ( IOC_IntStat&2 ) IRQH_Int(1, &FDD_Int);
	}
	fdd.Types[drive] = FD_Non;
	fdd.ROnly[drive] = 0;
	fdd.EMask[drive] = 0;
	fdd.Blink[drive] = 0;
	StatBar_SetFDD(drive, "");
	StatBar_ParamFDD(drive, (fdd.Types[drive]!=FD_Non)?((fdd.Access==drive)?2:1):0, ((fdd.Types[drive]!=FD_Non)&&(!fdd.EMask[drive]))?1:0, (fdd.Blink[drive])?1:0);
}


/*
 *   Eject Mask / Blink / AccessDrive
 */
void FDD_SetEMask(int drive, int emask)
{
	if ( (drive<0)||(drive>3) ) return;
	if ( fdd.EMask[drive]==emask ) return;
	fdd.EMask[drive] = emask;
	StatBar_ParamFDD(drive, (fdd.Types[drive]!=FD_Non)?((fdd.Access==drive)?2:1):0, ((fdd.Types[drive]!=FD_Non)&&(!fdd.EMask[drive]))?1:0, (fdd.Blink[drive])?1:0);
}

void FDD_SetAccess(int drive)
{
	if ( fdd.Access==drive ) return;
	fdd.Access = drive;
	StatBar_ParamFDD(0, (fdd.Types[0]!=FD_Non)?((fdd.Access==0)?2:1):0, ((fdd.Types[0]!=FD_Non)&&(!fdd.EMask[0]))?1:0, (fdd.Blink[0])?1:0);
	StatBar_ParamFDD(1, (fdd.Types[1]!=FD_Non)?((fdd.Access==1)?2:1):0, ((fdd.Types[1]!=FD_Non)&&(!fdd.EMask[1]))?1:0, (fdd.Blink[1])?1:0);
}

void FDD_SetBlink(int drive, int blink)
{
	if ( (drive<0)||(drive>3) ) return;
	if ( fdd.Blink[drive]==blink ) return;
	fdd.Blink[drive] = blink;
	StatBar_ParamFDD(drive, (fdd.Types[drive]!=FD_Non)?((fdd.Access==drive)?2:1):0, ((fdd.Types[drive]!=FD_Non)&&(!fdd.EMask[drive]))?1:0, (fdd.Blink[drive])?1:0);
}

void FDD_Init(void)
{
	memset(&fdd,0 , sizeof(FDDINFO));
	fdd.Access = -1;
	D88_Init();
	XDF_Init();
	DIM_Init();
}


void FDD_Cleanup(void)
{
	D88_Cleanup();
	XDF_Cleanup();
	DIM_Cleanup();
}


void FDD_Reset(void)
{
	int i;
	FDD_SetAccess(-1);
	for (i=0; i<4; i++) {
		FDD_SetEMask(i, 0);
		FDD_SetBlink(i, 0);
	}
}


void FDD_SetFDInt(void)
{
	int i;
	for (i=0; i<4; i++) {
		if ( fdd.SetDelay[i] ) {
			fdd.SetDelay[i]--;
			if ( fdd.SetDelay[i]<=0 ) {
				if ( IOC_IntStat&2 ) IRQH_Int(1, &FDD_Int);
				fdd.SetDelay[i] = 0;
			}
		}
	}
}


int FDD_Seek(int drv, int trk, FDCID* id)
{
	int type;
	if ( (drv<0)||(drv>3) ) return 0;
	type = fdd.Types[drv];
	if ( Seek[type] )
		return Seek[type](drv, trk, id);
	return 0;
}

int FDD_ReadID(int drv, FDCID* id)
{
	int type;
	if ( (drv<0)||(drv>3) ) return 0;
	type = fdd.Types[drv];
	if ( ReadID[type] )
		return ReadID[type](drv, id);
	return 0;
}

int FDD_WriteID(int drv, int trk, uint8_t* buf, int num)
{
	int type;
	if ( (drv<0)||(drv>3) ) return 0;
	type = fdd.Types[drv];
	if ( WriteID[type] )
		return WriteID[type](drv, trk, buf, num);
	return 0;
}


int FDD_Read(int drv, FDCID* id, uint8_t* buf)
{
	int type;
	if ( (drv<0)||(drv>3) ) return 0;
	type = fdd.Types[drv];
	if ( Read[type] )
	{
		FDD_IsReading = 1;
		return Read[type](drv, id, buf);
	}
	return 0;
}


int FDD_ReadDiag(int drv, FDCID* id, FDCID* retid, uint8_t* buf)
{
	int type;
	if ( (drv<0)||(drv>3) ) return 0;
	type = fdd.Types[drv];
	if ( ReadDiag[type] )
		return ReadDiag[type](drv, id, retid, buf);
	return 0;
}


int FDD_Write(int drv, FDCID* id, uint8_t* buf, int del)
{
	int type;
	if ( (drv<0)||(drv>3) ) return 0;
	type = fdd.Types[drv];
	if ( Write[type] )
		return Write[type](drv, id, buf, del);
	return 0;
}


int FDD_GetCurrentID(int drv, FDCID* id)
{
	int type;
	if ( (drv<0)||(drv>3) ) return 0;
	type = fdd.Types[drv];
	if ( GetCurrentID[type] )
		return GetCurrentID[type](drv, id);
	return 0;
}


int FDD_IsReady(int drv)
{
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (fdd.Types[drv]!=FD_Non)&&(!fdd.SetDelay[drv]) )
		return 1;
	return 0;
}


int FDD_IsReadOnly(int drv)
{
	if ( (drv<0)||(drv>3) ) return 0;
	return fdd.ROnly[drv];
}


void FDD_SetReadOnly(int drv)
{
	fdd.ROnly[drv] |= 1;
}
