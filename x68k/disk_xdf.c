#include "common.h"
#include "../libretro/dosio.h"
#include "fdc.h"
#include "fdd.h"
#include "disk_xdf.h"

static char           XDFFile[4][MAX_PATH];
static int            XDFCur[4] = {0, 0, 0, 0};
static int            XDFTrk[4] = {0, 0, 0, 0};
static uint8_t       *XDFImg[4] = {0, 0, 0, 0};

void XDF_Init(void)
{
	int drv;

	for (drv=0; drv<4; drv++) {
		XDFCur[drv] = 0;
		/* Free any image still held so re-init never leaks the buffer
		 * (XDFImg is always 0 or a valid malloc). */
		if (XDFImg[drv]) free(XDFImg[drv]);
		XDFImg[drv] = 0;
		memset(XDFFile[drv], 0, MAX_PATH);
	}
}


void XDF_Cleanup(void)
{
	int drv;
	for (drv=0; drv<4; drv++) XDF_Eject(drv);
}


int XDF_SetFD(int drv, char* filename)
{
	void *fp;

	strncpy(XDFFile[drv], filename, MAX_PATH);
	XDFFile[drv][MAX_PATH-1] = 0;

	XDFImg[drv] = (uint8_t*)malloc(1261568);
	if ( !XDFImg[drv] )
      return 0;
	memset(XDFImg[drv], 0xe5, 1261568);
	fp = file_open(XDFFile[drv]);
	if ( !fp )
   {
		memset(XDFFile[drv], 0, MAX_PATH);
		FDD_SetReadOnly(drv);
		return 0;
	}
	file_seek(fp, 0, FSEEK_SET);
	file_lread(fp, XDFImg[drv], 1261568);
	file_close(fp);
	return 1;
}


int XDF_Eject(int drv)
{
	void *fp;

	if ( !XDFImg[drv] ) {
		memset(XDFFile[drv], 0, MAX_PATH);
		return 0;
	}
	if ( !FDD_IsReadOnly(drv) ) {
		if (!(fp = file_open(XDFFile[drv]))) goto xdf_eject_error;
		file_seek(fp, 0, FSEEK_SET);
		if ( file_lwrite(fp, XDFImg[drv], 1261568)!=1261568 ) goto xdf_eject_error;
		file_close(fp);
	}
	free(XDFImg[drv]);
	XDFImg[drv] = 0;
	memset(XDFFile[drv], 0, MAX_PATH);
	return 1;

xdf_eject_error:
	free(XDFImg[drv]);
	XDFImg[drv] = 0;
	memset(XDFFile[drv], 0, MAX_PATH);
	return 0;
}


int XDF_Seek(int drv, int trk, FDCID* id)
{
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (trk<0)||(trk>153) ) return 0;
	if ( !XDFImg[drv] ) return 0;
	if ( XDFTrk[drv]!=trk ) XDFCur[drv] = 0;
	id->c = trk>>1;
	id->h = trk&1;
	id->r = XDFCur[drv]+1;
	id->n = 3;
	XDFTrk[drv] = trk;
	return 1;
}


int XDF_GetCurrentID(int drv, FDCID* id)
{
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (XDFTrk[drv]<0)||(XDFTrk[drv]>153) ) return 0;
	if ( !XDFImg[drv] ) return 0;
	id->c = XDFTrk[drv]>>1;
	id->h = XDFTrk[drv]&1;
	id->r = XDFCur[drv]+1;
	id->n = 3;
	return 1;
}


int XDF_ReadID(int drv, FDCID* id)
{
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (XDFTrk[drv]<0)||(XDFTrk[drv]>153) ) return 0;
	if ( !XDFImg[drv] ) return 0;
	id->c = XDFTrk[drv]>>1;
	id->h = XDFTrk[drv]&1;
	id->r = XDFCur[drv]+1;
	id->n = 3;
	XDFCur[drv] = (XDFCur[drv]+1)&7;
	return 1;
}


int XDF_WriteID(int drv, int trk, uint8_t *buf, int num)
{
	int i;
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (trk<0)||(trk>153) ) return 0;
	if ( !XDFImg[drv] ) return 0;
	if ( num!=8 ) return 0;
	for (i=0; i<8; i++, buf+=4) {
		if ( (((buf[0]<<1)+buf[1])!=trk)||(buf[2]<1)||(buf[2]>8)||(buf[3]!=3) ) return 0;
	}
	XDFTrk[drv] = trk;
	return 1;
}


int XDF_Read(int drv, FDCID* id, uint8_t *buf)
{
	int pos;
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (XDFTrk[drv]<0)||(XDFTrk[drv]>153) ) return 0;
	if ( !XDFImg[drv] ) return 0;
	if ( (((id->c<<1)+id->h)!=XDFTrk[drv]) ) return 0;
	if ( (id->r<1)||(id->r>8) ) return 0;
	if ( (id->h!=0)&&(id->h!=1) ) return 0;
	if ( id->n!=3 ) return 0;
	pos = ((((id->c<<1)+(id->h))*8)+(id->r-1))<<10;
	memcpy(buf, XDFImg[drv]+pos, 1024);
	XDFCur[drv] = (id->r)&7;
	return 1;
}


int XDF_ReadDiag(int drv, FDCID* id, FDCID* retid, uint8_t *buf)
{
	int pos;
	(void)id;
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (XDFTrk[drv]<0)||(XDFTrk[drv]>153) ) return 0;
	if ( (XDFCur[drv]<0)||(XDFCur[drv]>8) ) return 0;
	if ( !XDFImg[drv] ) return 0;
	pos = ((XDFTrk[drv]*8)+XDFCur[drv])<<10;
	memcpy(buf, XDFImg[drv]+pos, 1024);
	retid->c = XDFTrk[drv]>>1;
	retid->h = XDFTrk[drv]&1;
	retid->r = XDFCur[drv]+1;
	retid->n = 3;
	XDFCur[drv] = (XDFCur[drv]+1)&7;
	return 1;
}


int XDF_Write(int drv, FDCID* id, uint8_t *buf, int del)
{
	int pos;
	(void)del;
	if ( (drv<0)||(drv>3) ) return 0;
	if ( (XDFTrk[drv]<0)||(XDFTrk[drv]>153) ) return 0;
	if ( !XDFImg[drv] ) return 0;
	if ( (((id->c<<1)+id->h)!=XDFTrk[drv]) ) return 0;
	if ( (id->r<1)||(id->r>8) ) return 0;
	if ( (id->h!=0)&&(id->h!=1) ) return 0;
	if ( id->n!=3 ) return 0;
	pos = ((((id->c<<1)+(id->h))*8)+(id->r-1))<<10;
	memcpy(XDFImg[drv]+pos, buf, 1024);
	XDFCur[drv] = (id->r)&7;
	return 1;
}
