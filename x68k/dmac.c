#include "common.h"
#include "winx68k.h"
#include "m68000.h"
#include "x68kmemory.h"
#include "irqh.h"
#include "fdc.h"
#include "sasi.h"
#include "adpcm.h"
#include "mercury.h"
#include "dmac.h"

dmac_ch	DMA[4];

static int DMA_IntCH = 0;
static int DMA_LastInt = 0;
static int (*IsReady[4])(void) = { 0, 0, 0, 0 };

static uint32_t FASTCALL DMA_Int(uint8_t irq);

#define DMAINT(ch)     if ( DMA[ch].CCR&0x08 )	{ DMA_IntCH |= (1<<ch); IRQH_Int(3, &DMA_Int); }
#define DMAERR(ch,err) DMA[ch].CER  = err; \
                       DMA[ch].CSR |= 0x10; \
                       DMA[ch].CSR &= 0xf7; \
                       DMA[ch].CCR &= 0x7f; \
                       DMAINT(ch)

int DMAC_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
      SFVARN(DMA[0].CSR, "CSR0"),
	   SFVARN(DMA[0].CER, "CER0"),
	   SFVARN(DMA[0].DCR, "DCR0"),
	   SFVARN(DMA[0].OCR, "OCR0"),
	   SFVARN(DMA[0].SCR, "SCR0"),
	   SFVARN(DMA[0].CCR, "CCR0"),
	   SFVARN(DMA[0].MTC, "MTC0"),
	   SFVARN(DMA[0].MAR, "MAR0"),
	   SFVARN(DMA[0].DAR, "DAR0"),
	   SFVARN(DMA[0].BTC, "BTC0"),
	   SFVARN(DMA[0].BAR, "BAR0"),
	   SFVARN(DMA[0].NIV, "NIV0"),
	   SFVARN(DMA[0].EIV, "EIV0"),
	   SFVARN(DMA[0].MFC, "MFC0"),
	   SFVARN(DMA[0].CPR, "CPR0"),
	   SFVARN(DMA[0].DFC, "DFC0"),
	   SFVARN(DMA[0].BFC, "BFC0"),
	   SFVARN(DMA[0].GCR, "GCR0"),

      SFVARN(DMA[1].CSR, "CSR1"),
	   SFVARN(DMA[1].CER, "CER1"),
	   SFVARN(DMA[1].DCR, "DCR1"),
	   SFVARN(DMA[1].OCR, "OCR1"),
	   SFVARN(DMA[1].SCR, "SCR1"),
	   SFVARN(DMA[1].CCR, "CCR1"),
	   SFVARN(DMA[1].MTC, "MTC1"),
	   SFVARN(DMA[1].MAR, "MAR1"),
	   SFVARN(DMA[1].DAR, "DAR1"),
	   SFVARN(DMA[1].BTC, "BTC1"),
	   SFVARN(DMA[1].BAR, "BAR1"),
	   SFVARN(DMA[1].NIV, "NIV1"),
	   SFVARN(DMA[1].EIV, "EIV1"),
	   SFVARN(DMA[1].MFC, "MFC1"),
	   SFVARN(DMA[1].CPR, "CPR1"),
	   SFVARN(DMA[1].DFC, "DFC1"),
	   SFVARN(DMA[1].BFC, "BFC1"),
	   SFVARN(DMA[1].GCR, "GCR1"),

      SFVARN(DMA[2].CSR, "CSR2"),
	   SFVARN(DMA[2].CER, "CER2"),
	   SFVARN(DMA[2].DCR, "DCR2"),
	   SFVARN(DMA[2].OCR, "OCR2"),
	   SFVARN(DMA[2].SCR, "SCR2"),
	   SFVARN(DMA[2].CCR, "CCR2"),
	   SFVARN(DMA[2].MTC, "MTC2"),
	   SFVARN(DMA[2].MAR, "MAR2"),
	   SFVARN(DMA[2].DAR, "DAR2"),
	   SFVARN(DMA[2].BTC, "BTC2"),
	   SFVARN(DMA[2].BAR, "BAR2"),
	   SFVARN(DMA[2].NIV, "NIV2"),
	   SFVARN(DMA[2].EIV, "EIV2"),
	   SFVARN(DMA[2].MFC, "MFC2"),
	   SFVARN(DMA[2].CPR, "CPR2"),
	   SFVARN(DMA[2].DFC, "DFC2"),
	   SFVARN(DMA[2].BFC, "BFC2"),
	   SFVARN(DMA[2].GCR, "GCR2"),

      SFVARN(DMA[3].CSR, "CSR3"),
	   SFVARN(DMA[3].CER, "CER3"),
	   SFVARN(DMA[3].DCR, "DCR3"),
	   SFVARN(DMA[3].OCR, "OCR3"),
	   SFVARN(DMA[3].SCR, "SCR3"),
	   SFVARN(DMA[3].CCR, "CCR3"),
	   SFVARN(DMA[3].MTC, "MTC3"),
	   SFVARN(DMA[3].MAR, "MAR3"),
	   SFVARN(DMA[3].DAR, "DAR3"),
	   SFVARN(DMA[3].BTC, "BTC3"),
	   SFVARN(DMA[3].BAR, "BAR3"),
	   SFVARN(DMA[3].NIV, "NIV3"),
	   SFVARN(DMA[3].EIV, "EIV3"),
	   SFVARN(DMA[3].MFC, "MFC3"),
	   SFVARN(DMA[3].CPR, "CPR3"),
	   SFVARN(DMA[3].DFC, "DFC3"),
	   SFVARN(DMA[3].BFC, "BFC3"),
	   SFVARN(DMA[3].GCR, "GCR3"),

	   SFVAR(DMA_IntCH),
	   SFVAR(DMA_LastInt),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_DMAC", false);

	return ret;
}

static int ADPCM_IsReady(void)    { return 1; }
static int DMA_DummyIsReady(void) { return 0; }

static void DMA_SetReadyCB(int ch, int (*func)(void))
{
	if ( (ch>=0)&&(ch<=3) ) IsReady[ch] = func;
}

static uint32_t FASTCALL DMA_Int(uint8_t irq)
{
	uint32_t ret = 0xffffffff;
	int bit = 0;
	int i = DMA_LastInt;
	IRQH_IRQCallBack(irq);
	if ( irq==3 ) {
		do {
			bit = 1<<i;
			if ( DMA_IntCH&bit ) {
				if ( (DMA[i].CSR)&0x10 )
					ret = ((uint32_t)(DMA[i].EIV));
				else
					ret = ((uint32_t)(DMA[i].NIV));
				DMA_IntCH &= ~bit;
				break;
			}
			i = (i+1)&3;
		} while ( i!=DMA_LastInt );
	}
	DMA_LastInt = i;
	if ( DMA_IntCH ) IRQH_Int(3, &DMA_Int);
	return ret;
}

uint8_t FASTCALL DMA_Read(uint32_t adr)
{
   int off = adr & 0x3f;
   int ch = (adr >> 6) & 0x03;

   if (adr >= 0xe84100)
      return 0;

   switch (off)
   {
      case 0x00:
         if ((ch == 2) && (off == 0))
         {
#ifndef NO_MERCURY
            DMA[ch].CSR = (DMA[ch].CSR & 0xfe) | (Mcry_LRTiming & 1);
#else
            DMA[ch].CSR = (DMA[ch].CSR & 0xfe);
            Mcry_LRTiming ^= 1;
#endif
         }
         return DMA[ch].CSR;
      case 0x01:
         return DMA[ch].CER;
      case 0x04:
         return DMA[ch].DCR;
      case 0x05:
         return DMA[ch].OCR;
      case 0x06:
         return DMA[ch].SCR;
      case 0x07:
         return DMA[ch].CCR;
      case 0x0a:
         return (DMA[ch].MTC >> 8) & 0xff;
      case 0x0b:
         return DMA[ch].MTC & 0xff;
      case 0x0c:
         return (DMA[ch].MAR >> 24) & 0xff;
      case 0x0d:
         return (DMA[ch].MAR >> 16) & 0xff;
      case 0x0e:
         return (DMA[ch].MAR >> 8) & 0xff;
      case 0x0f:
         return DMA[ch].MAR & 0xff;
      case 0x14:
         return (DMA[ch].DAR >> 24) & 0xff;
      case 0x15:
         return (DMA[ch].DAR >> 16) & 0xff;
      case 0x16:
         return (DMA[ch].DAR >> 8) & 0xff;
      case 0x17:
         return DMA[ch].DAR & 0xff;
      case 0x1a:
         return (DMA[ch].BTC >> 8) & 0xff;
      case 0x1b:
         return DMA[ch].BTC & 0xff;
      case 0x1c:
         return (DMA[ch].BAR >> 24) & 0xff;
      case 0x1d:
         return (DMA[ch].BAR >> 16) & 0xff;
      case 0x1e:
         return (DMA[ch].BAR >> 8) & 0xff;
      case 0x1f:
         return DMA[ch].BAR & 0xff;
      case 0x25:
         return DMA[ch].NIV;
      case 0x27:
         return DMA[ch].EIV;
      case 0x29:
         return DMA[ch].MFC;
      case 0x2d:
         return DMA[ch].CPR;
      case 0x31:
         return DMA[ch].DFC;
      case 0x39:
         return DMA[ch].BFC;
      case 0x3f:
         return DMA[ch].GCR;
   }

   return 0;
}

void FASTCALL DMA_Write(uint32_t adr, uint8_t data)
{
   int off = adr & 0x3f;
   int ch = (adr >> 6) & 0x03;
   uint8_t old = 0;

   if (adr >= 0xe84100)
      return;

   switch (off)
   {
      case 0x00:
         DMA[ch].CSR &= ((~data) | 0x09);
         break;

      case 0x01:
         DMA[ch].CER &= (~data);
         break;

      case 0x04:
         DMA[ch].DCR = data;
         break;

      case 0x05:
         DMA[ch].OCR = data;
         break;

      case 0x06:
         DMA[ch].SCR = data;
         break;

      case 0x07:
         old         = DMA[ch].CCR;
         DMA[ch].CCR = (data & 0xef) | (DMA[ch].CCR & 0x80); /* CCR STR cannot be dropped by writing */
         if ((data & 0x10) && (DMA[ch].CCR & 0x80))
         {
            /* Software Abort */
            DMAERR(ch, 0x11)
               break;
         }
         if (data & 0x20) /* Halt */
         {
            /* Should be correct, but Nemesis'90 will not work properly with it so... */
            /* DMA[ch].CSR &= 0xf7; */
            break;
         }
         if (data & 0x80)
         {
            /* Start DMA */
            if (old & 0x20)
            {
               /* Halt release */
               DMA[ch].CSR |= 0x08;
               DMA_Exec(ch);
            }
            else
            {
               if (DMA[ch].CSR & 0xf8)
               {
                  /* Timing errors */
                  DMAERR(ch, 0x02)
                     break;
               }
               DMA[ch].CSR |= 0x08;
               if ((DMA[ch].OCR & 8) /*&&(!DMA[ch].MTC)*/)
               {
                  /* Array/Link Array Chain */
                  DMA[ch].MAR = dma_readmem24_dword(DMA[ch].BAR) & 0xffffff;
                  DMA[ch].MTC = dma_readmem24_word(DMA[ch].BAR + 4);
                  if (DMA[ch].OCR & 4)
                  {
                     DMA[ch].BAR = dma_readmem24_dword(DMA[ch].BAR + 6);
                  }
                  else
                  {
                     DMA[ch].BAR += 6;
                     if (!DMA[ch].BTC)
                     {
                        /* This is also a counting error */
                        DMAERR(ch, 0x0f)
                           break;
                     }
                  }
               }
               if (!DMA[ch].MTC)
               {
                  /* Counting error */
                  DMAERR(ch, 0x0d)
                     break;
               }
               DMA[ch].CER = 0x00;
               DMA_Exec(ch); /* Since you may need to check the counter immediately after starting, run it for a little while. */
            }
         }
         if ((data & 0x40) && (!DMA[ch].MTC))
         {
            /* Continuous Op. */
            if (DMA[ch].CCR & 0x80)
            {
               if (DMA[ch].CCR & 0x40)
               {
                  DMAERR(ch, 0x02)
               }
               else if (DMA[ch].OCR & 8)
               {
                  /* Array/Link Array Chain */
                  DMAERR(ch, 0x01)
               }
               else
               {
                  DMA[ch].MAR = DMA[ch].BAR;
                  DMA[ch].MTC = DMA[ch].BTC;
                  DMA[ch].CSR |= 0x08;
                  DMA[ch].BAR = 0;
                  DMA[ch].BTC = 0;
                  if (!DMA[ch].MAR)
                  {
                     DMA[ch].CSR |= 0x40; /* Block transfer end bit/interrupt? */
                     DMAINT(ch)
                        break;
                  }
                  else if (!DMA[ch].MTC)
                  {
                     DMAERR(ch, 0x0d)
                        break;
                  }
                  DMA[ch].CCR &= 0xbf;
                  DMA_Exec(ch);
               }
            }
            else
            {
               /* The CNT bit in non-active mode indicates an operation timing error */
               DMAERR(ch, 0x02)
            }
         }
         break;

      case 0x0a:
         DMA[ch].MTC &= 0x00ff;
         DMA[ch].MTC |= (data << 8);
         break;
      case 0x0b:
         DMA[ch].MTC &= 0xff00;
         DMA[ch].MTC |= data;
         break;

      case 0x0c:
         break;
      case 0x0d:
         DMA[ch].MAR &= 0x0000ffff;
         DMA[ch].MAR |= (data << 16);
         break;
      case 0x0e:
         DMA[ch].MAR &= 0x00ff00ff;
         DMA[ch].MAR |= (data << 8);
         break;
      case 0x0f:
         DMA[ch].MAR &= 0x00ffff00;
         DMA[ch].MAR |= data;
         break;

      case 0x14:
         break;
      case 0x15:
         DMA[ch].DAR &= 0x0000ffff;
         DMA[ch].DAR |= (data << 16);
         break;
      case 0x16:
         DMA[ch].DAR &= 0x00ff00ff;
         DMA[ch].DAR |= (data << 8);
         break;
      case 0x17:
         DMA[ch].DAR &= 0x00ffff00;
         DMA[ch].DAR |= data;
         break;

      case 0x1a:
         DMA[ch].BTC &= 0x00ff;
         DMA[ch].BTC |= (data << 8);
         break;
      case 0x1b:
         DMA[ch].BTC &= 0xff00;
         DMA[ch].BTC |= data;
         break;

      case 0x1c:
         break;
      case 0x1d:
         DMA[ch].BAR &= 0x0000ffff;
         DMA[ch].BAR |= (data << 16);
         break;
      case 0x1e:
         DMA[ch].BAR &= 0x00ff00ff;
         DMA[ch].BAR |= (data << 8);
         break;
      case 0x1f:
         DMA[ch].BAR &= 0x00ffff00;
         DMA[ch].BAR |= data;
         break;

      case 0x25:
         DMA[ch].NIV = data;
         break;

      case 0x27:
         DMA[ch].EIV = data;
         break;

      case 0x29:
         DMA[ch].MFC = data;
         break;

      case 0x2d:
         DMA[ch].CPR = data;
         break;

      case 0x31:
         DMA[ch].DFC = data;
         break;

      case 0x39:
         DMA[ch].BFC = data;
         break;

      case 0x3f:
         if (ch == 3)
            DMA[ch].GCR = data;
         break;
   }
}

int FASTCALL DMA_Exec(int ch)
{
	uint32_t *src, *dst;

	if ( DMA[ch].OCR&0x80 ) {		/* Device->Memory */
		src = &DMA[ch].DAR;
		dst = &DMA[ch].MAR;
	} else {				/* Memory->Device */
		src = &DMA[ch].MAR;
		dst = &DMA[ch].DAR;
	}

	while ( (DMA[ch].CSR&0x08) && (!(DMA[ch].CCR&0x20)) && (!(DMA[ch].CSR&0x80)) && (DMA[ch].MTC) && (((DMA[ch].OCR&3)!=2)||(IsReady[ch]())) ) {
		BusErrFlag = 0;
		switch ( ((DMA[ch].OCR>>4)&3)+((DMA[ch].DCR>>1)&4) ) {
			case 0:
			case 3:
				dma_writemem24(*dst, dma_readmem24(*src));
				if ( DMA[ch].SCR&4 ) DMA[ch].MAR += 1; else if ( DMA[ch].SCR&8 ) DMA[ch].MAR -= 1;
				if ( DMA[ch].SCR&1 ) DMA[ch].DAR += 2; else if ( DMA[ch].SCR&2 ) DMA[ch].DAR -= 2;
				break;
			case 1:
				dma_writemem24(*dst, dma_readmem24(*src));
				if ( DMA[ch].SCR&4 ) DMA[ch].MAR += 1; else if ( DMA[ch].SCR&8 ) DMA[ch].MAR -= 1;
				if ( DMA[ch].SCR&1 ) DMA[ch].DAR += 2; else if ( DMA[ch].SCR&2 ) DMA[ch].DAR -= 2;
				dma_writemem24(*dst, dma_readmem24(*src));
				if ( DMA[ch].SCR&4 ) DMA[ch].MAR += 1; else if ( DMA[ch].SCR&8 ) DMA[ch].MAR -= 1;
				if ( DMA[ch].SCR&1 ) DMA[ch].DAR += 2; else if ( DMA[ch].SCR&2 ) DMA[ch].DAR -= 2;
				break;
			case 2:
				dma_writemem24(*dst, dma_readmem24(*src));
				if ( DMA[ch].SCR&4 ) DMA[ch].MAR += 1; else if ( DMA[ch].SCR&8 ) DMA[ch].MAR -= 1;
				if ( DMA[ch].SCR&1 ) DMA[ch].DAR += 2; else if ( DMA[ch].SCR&2 ) DMA[ch].DAR -= 2;
				dma_writemem24(*dst, dma_readmem24(*src));
				if ( DMA[ch].SCR&4 ) DMA[ch].MAR += 1; else if ( DMA[ch].SCR&8 ) DMA[ch].MAR -= 1;
				if ( DMA[ch].SCR&1 ) DMA[ch].DAR += 2; else if ( DMA[ch].SCR&2 ) DMA[ch].DAR -= 2;
				dma_writemem24(*dst, dma_readmem24(*src));
				if ( DMA[ch].SCR&4 ) DMA[ch].MAR += 1; else if ( DMA[ch].SCR&8 ) DMA[ch].MAR -= 1;
				if ( DMA[ch].SCR&1 ) DMA[ch].DAR += 2; else if ( DMA[ch].SCR&2 ) DMA[ch].DAR -= 2;
				dma_writemem24(*dst, dma_readmem24(*src));
				if ( DMA[ch].SCR&4 ) DMA[ch].MAR += 1; else if ( DMA[ch].SCR&8 ) DMA[ch].MAR -= 1;
				if ( DMA[ch].SCR&1 ) DMA[ch].DAR += 2; else if ( DMA[ch].SCR&2 ) DMA[ch].DAR -= 2;
				break;
			case 4:
				dma_writemem24(*dst, dma_readmem24(*src));
				if ( DMA[ch].SCR&4 ) DMA[ch].MAR += 1; else if ( DMA[ch].SCR&8 ) DMA[ch].MAR -= 1;
				if ( DMA[ch].SCR&1 ) DMA[ch].DAR += 1; else if ( DMA[ch].SCR&2 ) DMA[ch].DAR -= 1;
				break;
			case 5:
				dma_writemem24_word(*dst, dma_readmem24_word(*src));
				if ( DMA[ch].SCR&4 ) DMA[ch].MAR += 2; else if ( DMA[ch].SCR&8 ) DMA[ch].MAR -= 2;
				if ( DMA[ch].SCR&1 ) DMA[ch].DAR += 2; else if ( DMA[ch].SCR&2 ) DMA[ch].DAR -= 2;
				break;
			case 6:
				dma_writemem24_dword(*dst, dma_readmem24_dword(*src));
				if ( DMA[ch].SCR&4 ) DMA[ch].MAR += 4; else if ( DMA[ch].SCR&8 ) DMA[ch].MAR -= 4;
				if ( DMA[ch].SCR&1 ) DMA[ch].DAR += 4; else if ( DMA[ch].SCR&2 ) DMA[ch].DAR -= 4;
				break;
		}

		if ( BusErrFlag ) {
			switch ( BusErrFlag ) {
			case 1:					/* BusErr/Read */
				if ( DMA[ch].OCR&0x80 )		/* Device->Memory */
					DMAERR(ch,0x0a)
				else
					DMAERR(ch,0x09)
				break;
			case 2:					/* BusErr/Write */
				if ( DMA[ch].OCR&0x80 )		/* Device->Memory */
					DMAERR(ch,0x09)
				else
					DMAERR(ch,0x0a)
				break;
			case 3:					/* AdrErr/Read */
				if ( DMA[ch].OCR&0x80 )		/* Device->Memory */
					DMAERR(ch,0x06)
				else
					DMAERR(ch,0x05)
				break;
			case 4:					/* BusErr/Write */
				if ( DMA[ch].OCR&0x80 )		/* Device->Memory */
					DMAERR(ch,0x05)
				else
					DMAERR(ch,0x06)
				break;
			}
			BusErrFlag = 0;
			break;
		}

		DMA[ch].MTC--;
		if ( !DMA[ch].MTC ) {					/* Transfer of specified number of bytes completed */
			if ( DMA[ch].OCR&8 ) {				/* If running in chained mode */
				if ( DMA[ch].OCR&4 ) {			/* If running in chained mode */
					if ( DMA[ch].BAR ) {
						DMA[ch].MAR = dma_readmem24_dword(DMA[ch].BAR);
						DMA[ch].MTC = dma_readmem24_word(DMA[ch].BAR+4);
						DMA[ch].BAR = dma_readmem24_dword(DMA[ch].BAR+6);
						if ( BusErrFlag ) {
							if ( BusErrFlag==1 )
								DMAERR(ch,0x0b)
							else
								DMAERR(ch,0x07)
							BusErrFlag = 0;
							break;
						} else if ( !DMA[ch].MTC ) {
							DMAERR(ch,0x0d)
							break;
						}
					}
				} else {						/* Array chain */
					DMA[ch].BTC--;
					if ( DMA[ch].BTC ) {		/* Next block exists */
						DMA[ch].MAR = dma_readmem24_dword(DMA[ch].BAR);
						DMA[ch].MTC = dma_readmem24_word(DMA[ch].BAR+4);
						DMA[ch].BAR += 6;
						if ( BusErrFlag ) {
							if ( BusErrFlag==1 )
								DMAERR(ch,0x0b)
							else
								DMAERR(ch,0x07)
							BusErrFlag = 0;
							break;
						} else if ( !DMA[ch].MTC ) {
							DMAERR(ch,0x0d)
							break;
						}
					}
				}
			} else {								      /* Normal mode (only one block) finished */
				if ( DMA[ch].CCR&0x40 ) {			/* Countinuous action */
					DMA[ch].CSR |= 0x40;			   /* Block transfer end bit/interrupt */
					DMAINT(ch)
					if ( DMA[ch].BAR ) {
						DMA[ch].MAR  = DMA[ch].BAR;
						DMA[ch].MTC  = DMA[ch].BTC;
						DMA[ch].CSR |= 0x08;
						DMA[ch].BAR  = 0x00;
						DMA[ch].BTC  = 0x00;
						if ( !DMA[ch].MTC ) {
							DMAERR(ch,0x0d)
							break;
						}
						DMA[ch].CCR &= 0xbf;
					}
				}
			}
			if ( !DMA[ch].MTC ) {
				DMA[ch].CSR |= 0x80;
				DMA[ch].CSR &= 0xf7;
				DMAINT(ch)
			}
		}
		if ( (DMA[ch].OCR&3)!=1 ) break;
	}
	return 0;
}

void DMA_Init(void)
{
	int i;
	DMA_IntCH = 0;
	DMA_LastInt = 0;
	for (i=0; i<4; i++) {
		memset(&DMA[i], 0, sizeof(dmac_ch));
		DMA[i].CSR = 0;
		DMA[i].CCR = 0;
		DMA_SetReadyCB(i, DMA_DummyIsReady);
	}
	DMA_SetReadyCB(0, FDC_IsDataReady);
	DMA_SetReadyCB(1, SASI_IsReady);
#ifndef	NO_MERCURY
	DMA_SetReadyCB(2, Mcry_IsReady);
#endif
	DMA_SetReadyCB(3, ADPCM_IsReady);
}
