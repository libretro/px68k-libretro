/*
* SCSI.C - External SCSI board (CZ-6BS1)
* Supported by taking over SCSI IOCS (SPC is not emulated)
* Built-in SCSI (dummy) IPL is defined in winx68k.c
*/

#include "common.h"
#include "../libretro/dosio.h"
#include "winx68k.h"
#include "m68000.h"
#include "scsi.h"

uint8_t	SCSIIPL[0x2000];

void SCSI_Init(void)
{
	/* Original SCSI ROM
	 * Operation: When SCSI IOCS is called, the SCSI IOCS number is output to $e9f800.
	 * Booting from a SCSI device is not possible, the initialization routine only sets the vector for SCSI IOCS ($F5).
	 */
	static	uint8_t	SCSIIMG[] = {
		0x00, 0xea, 0x00, 0x34,				/* $ea0020 Entry address for SCSI startup */
		0x00, 0xea, 0x00, 0x36,				/* $ea0024 Entry address of IOCS vector setting (must be 8 bytes before "Human") */
		0x00, 0xea, 0x00, 0x4a,				/* $ea0028 SCSI IOCS entry address */
		0x48, 0x75, 0x6d, 0x61,				/* $ea002c ↓ */
		0x6e, 0x36, 0x38, 0x6b,				/* $ea0030 ID "Human68k" (always right before the startup entry point) */
		0x4e, 0x75,							/* $ea0034 "rts" (startup entry point, does nothing) */
		0x23, 0xfc, 0x00, 0xea, 0x00, 0x4a,	/* $ea0036 ↓ (IOCS vector setting entry point) */
		0x00, 0x00, 0x07, 0xd4,				/* $ea003c "move.l #$ea004a, $7d4.l" (IOCS $F5 vector setting) */
		0x74, 0xff,							/* $ea0040 "moveq #-1, d2" */
		0x4e, 0x75,							/* $ea0042 "rts" */
		0x53, 0x43, 0x53, 0x49, 0x45, 0x58,	/* $ea0044 ID "SCSIEX" (SCSI card ID) */
		0x13, 0xc1, 0x00, 0xe9, 0xf8, 0x00,	/* $ea004a "move.b d1, $e9f800" (SCSI IOCS call entry point) */
		0x4e, 0x75,							/* $ea0050 "rts" */
	};
	int i;
	uint8_t tmp;
	memset(SCSIIPL, 0, 0x2000);
	memcpy(&SCSIIPL[0x20], SCSIIMG, sizeof(SCSIIMG));
	for (i=0; i<0x2000; i+=2)
	{
		tmp = SCSIIPL[i];
		SCSIIPL[i] = SCSIIPL[i+1];
		SCSIIPL[i+1] = tmp;
	}
}

void SCSI_Cleanup(void) { }
void FASTCALL SCSI_Write(uint32_t adr, uint8_t data) { }

uint8_t FASTCALL SCSI_Read(uint32_t adr)
{
	return SCSIIPL[(adr^1)&0x1fff];
}
