#include "common.h"
#include "../libretro/dosio.h"
#include "fdc.h"
#include "fdd.h"
#include "disk_dim.h"

/*
	22-10-24
		https://pc98.org/main.html
		- respect header trackflag instead of overriding it,
		prevents unnecessary resizing upon eject or saving of image file
*/

/* DIM Image Header */
typedef struct {
	uint8_t	type;
	uint8_t	trkflag[170];
	uint8_t	headerinfo[15];
	uint8_t	date[4];
	uint8_t	time[4];
	uint8_t	comment[61];
	uint8_t	overtrack;
} DIM_HEADER;

/* DIM Disk Type */
enum
{
	DIM_2HD = 0,
	DIM_2HS,
	DIM_2HC,
	DIM_2HDE,
	DIM_2HQ = 9
};

static const int SctLength[10] = {
	1024*8, 1024*9, 512*15, 1024*9, 0, 0, 0, 0, 0, 512*18
};

static char           DIMFile[4][MAX_PATH];
static int            DIMCur[4] = {0, 0, 0, 0};
static int            DIMTrk[4] = {0, 0, 0, 0};
static uint8_t       *DIMImg[4] = {0, 0, 0, 0};

void DIM_Init(void)
{
	int drv;

	for (drv=0; drv<4; drv++) {
		DIMCur[drv] = 0;
		DIMTrk[drv] = 0;
		/* Free any image still held so re-initialising never leaks the
		 * ~1.5MB buffer (DIMImg is always 0 or a valid malloc). */
		if (DIMImg[drv]) free(DIMImg[drv]);
		DIMImg[drv] = 0;
		memset(DIMFile[drv], 0, MAX_PATH);
	}
}


void DIM_Cleanup(void)
{
	int drv;
	for (drv=0; drv<4; drv++) DIM_Eject(drv);
}


int DIM_SetFD(int drv, char* filename)
{
	void *fp;
	DIM_HEADER* dh;
	unsigned int i, len;
	uint8_t * p;

	strncpy(DIMFile[drv], filename, MAX_PATH);
	DIMFile[drv][MAX_PATH-1] = 0;

	DIMImg[drv] = (uint8_t*)malloc(1024*9*170+sizeof(DIM_HEADER));		/* Maximum size */
	if ( !DIMImg[drv] ) return 0;
	memset(DIMImg[drv], 0xe5, 1024*9*170+sizeof(DIM_HEADER));
	if (!(fp = file_open(DIMFile[drv])))
	{
		memset(DIMFile[drv], 0, MAX_PATH);
		FDD_SetReadOnly(drv);
		return 0;
	}

	file_seek(fp, 0, FSEEK_SET);
	if ( file_lread(fp, DIMImg[drv], sizeof(DIM_HEADER))!=sizeof(DIM_HEADER) ) goto dim_set_error;
	dh = (DIM_HEADER*)DIMImg[drv];
	if ( dh->type>9 ) goto dim_set_error;
	len = SctLength[dh->type];
	if ( !len ) goto dim_set_error;
	p = DIMImg[drv]+sizeof(DIM_HEADER);
	for (i=0; i<170; i++) {
		if ( dh->trkflag[i] ) {
			if ( file_lread(fp, p, len)!=len )
				goto dim_set_error;
		}
		p += len;
	}
	file_close(fp);
	return 1;

dim_set_error:
	file_close(fp);
	FDD_SetReadOnly(drv);
	return 0;
}

int DIM_Eject(int drv)
{
	void *fp;
	DIM_HEADER* dh;
	unsigned int i, len;
	uint8_t* p;

	if ( !DIMImg[drv] )
   {
		memset(DIMFile[drv], 0, MAX_PATH);
		return 0;
	}
	dh = (DIM_HEADER*)DIMImg[drv];
	len = SctLength[dh->type];
	p = DIMImg[drv]+sizeof(DIM_HEADER);
	if ( !FDD_IsReadOnly(drv) ) {
		fp = file_open(DIMFile[drv]);
		if ( !fp ) goto dim_eject_error;
		file_seek(fp, 0, FSEEK_SET);
		if ( file_lwrite(fp, DIMImg[drv], sizeof(DIM_HEADER))!=sizeof(DIM_HEADER) ) goto dim_eject_error;
		for (i=0; i<170; i++) {
			if ( dh->trkflag[i] ) {
				if ( file_lwrite(fp, p, len)!=len ) goto dim_eject_error;
			}
			p += len;
		}
		file_close(fp);
	}
	free(DIMImg[drv]);
	DIMImg[drv] = 0;
	memset(DIMFile[drv], 0, MAX_PATH);
	return 1;

dim_eject_error:
	free(DIMImg[drv]);
	DIMImg[drv] = 0;
	memset(DIMFile[drv], 0, MAX_PATH);
	return 0;
}


static void SetID(int drv, FDCID* id, int c, int h, int r)
{
	int type = DIMImg[drv][0];
	switch (type) {
		case DIM_2HD:				/* 1024byte/sct, 8sct/trk */
			id->n = 3; break;
		case DIM_2HS:				/* 1024byte/sct, 9sct/trk */
			if ( (c)||(h)||(r!=1) ) r += 9;
			id->n = 3; break;
		case DIM_2HDE:
			if ( (c)||(h)||(r!=1) ) h += 0x80;
			id->n = 3; break;
		case DIM_2HC: /* 512byte/sct, 15sct/trk */
		case DIM_2HQ: /* 512byte/sct, 18sct/trk */
			id->n = 2; break;
	}
	id->c = c;
	id->h = h;
	id->r = r;
}


static int IncTrk(int drv, int r)
{
	int type = DIMImg[drv][0];
	switch (type) {
		case DIM_2HD: /* 1024byte/sct, 8sct/trk */
			r = (r+1)&7;
			break;
		case DIM_2HS: /* 1024byte/sct, 9sct/trk */
			if ( r>8 ) r -= 9; /* 9SCDRV?? */
		case DIM_2HDE:
			r = (r+1)%9;
			break;
		case DIM_2HC: /* 512byte/sct, 15sct/trk */
			r = (r+1)%15;
			break;
		case DIM_2HQ: /* 512byte/sct, 18sct/trk */
			r = (r+1)%18;
			break;
	}
	return r;
}


static int GetPos(int drv, FDCID* id)
{
	int ret, c = id->c, h = id->h, r = id->r, n = id->n;
	int type = DIMImg[drv][0];
	switch (type) {
		case DIM_2HD: /* 1024byte/sct, 8sct/trk */
			if ( (c<0)||(c>84)||(h<0)||(h>1)||(r<1)||(r>8)||(n!=3) ) return 0;
			ret = SctLength[type]*(c*2+h)+((r-1)<<10);
			ret += sizeof(DIM_HEADER);
			break;
		case DIM_2HS: /* 1024byte/sct, 9sct/trk */
			if ( r>9 ) r -= 9; /* 9SCDRV?? */
			if ( (c<0)||(c>84)||(h<0)||(h>1)||(r<1)||(r>9)||(n!=3) ) return 0;
			ret = SctLength[type]*(c*2+h)+((r-1)<<10);
			ret += sizeof(DIM_HEADER);
			break;
		case DIM_2HDE:
			h &= 1; /* 9SCDRV?? */
			if ( (c<0)||(c>84)||(h<0)||(h>1)||(r<1)||(r>9)||(n!=3) ) return 0;
			ret = SctLength[type]*(c*2+h)+((r-1)<<10);
			ret += sizeof(DIM_HEADER);
			break;
		case DIM_2HC: /* 512byte/sct, 15sct/trk */
			if ( (c<0)||(c>84)||(h<0)||(h>1)||(r<1)||(r>15)||(n!=2) ) return 0;
			ret = SctLength[type]*(c*2+h)+((r-1)<<9);
			ret += sizeof(DIM_HEADER);
			break;
		case DIM_2HQ: /* 512byte/sct, 18sct/trk */
			if ( (c<0)||(c>84)||(h<0)||(h>1)||(r<1)||(r>18)||(n!=2) ) return 0;
			ret = SctLength[type]*(c*2+h)+((r-1)<<9);
			ret += sizeof(DIM_HEADER);
			break;
		default:
			ret = 0;
			break;
	}
	return ret;
}


static int CheckTrack(int drv, int trk)
{
	DIM_HEADER* dh = (DIM_HEADER*)DIMImg[drv];
	switch (dh->type) {
		case DIM_2HD: /* 1024byte/sct, 8sct/trk */
			if ( ((trk>153)&&(!dh->overtrack))||(!dh->trkflag[trk]) ) return 0;
			break;
		case DIM_2HS: /* 1024byte/sct, 9sct/trk */
			if ( ((trk>159)&&(!dh->overtrack))||(!dh->trkflag[trk]) ) return 0;
			break;
		case DIM_2HDE:
			if ( ((trk>159)&&(!dh->overtrack))||(!dh->trkflag[trk]) ) return 0;
			break;
		case DIM_2HC: /* 512byte/sct, 15sct/trk */
			if ( ((trk>159)&&(!dh->overtrack))||(!dh->trkflag[trk]) ) return 0;
			break;
		case DIM_2HQ: /* 512byte/sct, 18sct/trk */
			if ( ((trk>159)&&(!dh->overtrack))||(!dh->trkflag[trk]) ) return 0;
			break;
		default:
			return 0;
	}
	return 1;
}


int DIM_Seek(int drv, int trk, FDCID* id)
{
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (trk<0)||(trk>169) ) return 0;
	if ( !DIMImg[drv] ) return 0;
	if ( DIMTrk[drv]!=trk ) DIMCur[drv] = 0;
	SetID(drv, id, trk>>1, trk&1, DIMCur[drv]+1);
	DIMTrk[drv] = trk;
	return 1;
}


int DIM_GetCurrentID(int drv, FDCID* id)
{
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (DIMTrk[drv]<0)||(DIMTrk[drv]>169) ) return 0;
	if ( !DIMImg[drv] ) return 0;
	if ( !CheckTrack(drv, DIMTrk[drv]) ) return 0;
	SetID(drv, id, DIMTrk[drv]>>1,DIMTrk[drv]&1, DIMCur[drv]+1);
	return 1;
}


int DIM_ReadID(int drv, FDCID* id)
{
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (DIMTrk[drv]<0)||(DIMTrk[drv]>169) ) return 0;
	if ( !DIMImg[drv] ) return 0;
	if ( !CheckTrack(drv, DIMTrk[drv]) ) return 0;
	SetID(drv, id, DIMTrk[drv]>>1,DIMTrk[drv]&1, DIMCur[drv]+1);
	DIMCur[drv] = IncTrk(drv, DIMCur[drv]);
	return 1;
}

int DIM_WriteID(int drv, int trk, uint8_t *buf, int num)
{
	return 0;
}


int DIM_Read(int drv, FDCID* id, uint8_t *buf)
{
	int pos;
	if ( (drv<0)||(drv>3) ) return 0;
	if ( !DIMImg[drv] ) return 0;
	if ( (((id->c<<1)+(id->h&1))!=DIMTrk[drv]) ) return 0;
	if ( !CheckTrack(drv, (id->c<<1)+(id->h&1)) ) return 0;
	pos = GetPos(drv, id);
	if ( !pos ) return 0;
	memcpy(buf, DIMImg[drv]+pos, (id->n==2)?512:1024);
	DIMCur[drv] = IncTrk(drv, id->r-1);
	return 1;
}


int DIM_ReadDiag(int drv, FDCID* id, FDCID* retid, uint8_t* buf)
{
	int pos;
	if ( (drv<0)||(drv>3) ) return 0;
	if ( !DIMImg[drv] ) return 0;
	if ( !CheckTrack(drv, DIMTrk[drv]) ) return 0;
	SetID(drv, retid, DIMTrk[drv]>>1, DIMTrk[drv]&1, DIMCur[drv]+1);
	pos = GetPos(drv, retid);
	if ( !pos ) return 0;
	memcpy(buf, DIMImg[drv]+pos, (retid->n==2)?512:1024);
	DIMCur[drv] = IncTrk(drv, DIMCur[drv]);
	return 1;
}


int DIM_Write(int drv, FDCID* id, uint8_t* buf, int del)
{
	int pos;
	if ( (drv<0)||(drv>3) ) return 0;
	if ( !DIMImg[drv] ) return 0;
	if ( (((id->c<<1)+(id->h&1))!=DIMTrk[drv]) ) return 0;
	if ( !CheckTrack(drv, (id->c<<1)+(id->h&1)) ) return 0;
	pos = GetPos(drv, id);
	if ( !pos ) return 0;
	memcpy(DIMImg[drv]+pos, buf, (id->n==2)?512:1024);
	DIMCur[drv] = IncTrk(drv, id->r-1);
	return 1;
}
