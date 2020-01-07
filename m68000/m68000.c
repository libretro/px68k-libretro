/******************************************************************************

	m68000.c

	M68000 CPUインタフェース関数

******************************************************************************/

#include "m68000.h"

#ifndef CYCLONE
#include "c68k/c68k.h"
#else
struct Cyclone m68k;
typedef signed int s32;
#endif

#include "../x68k/x68kmemory.h"

int m68000_ICountBk;
int ICount;

#ifdef CYCLONE

unsigned int read8(unsigned int a) {
	return (unsigned int) cpu_readmem24(a);
}

unsigned int read16(unsigned int a) {
	return (unsigned int) cpu_readmem24_word(a);
}

unsigned int MyCheckPc(unsigned int pc)
{


  pc-= m68k.membase; // Get the real program counter
/*
  if (!tracing) {
  	//p6logd("CheckPC 0x%08X\n", pc);
  }
*/
  if (pc <= 0xbfffff) 			       					{ m68k.membase=(int) MEM; return m68k.membase+pc; }
  if ((pc >= 0xfc0000) && (pc <= 0xffffff))	{ m68k.membase=(int) IPL - 0xfc0000; return m68k.membase+pc; }
  if ((pc >= 0xc00000) && (pc <= 0xc7ffff)) m68k.membase=(int) GVRAM - 0xc00000;
  if ((pc >= 0xe00000) && (pc <= 0xe7ffff))	m68k.membase=(int) TVRAM - 0xe00000;
  if ((pc >= 0xea0000) && (pc <= 0xea1fff))	m68k.membase=(int) SCSIIPL - 0xea0000;
  if ((pc >= 0xed0000) && (pc <= 0xed3fff))	m68k.membase=(int) SRAM - 0xed0000;
  if ((pc >= 0xf00000) && (pc <= 0xfbffff))	m68k.membase=(int) FONT - 0xf00000;

  return m68k.membase+pc; // New program counter
}

#endif


/******************************************************************************
	M68000インタフェース関数
******************************************************************************/

/*--------------------------------------------------------
	CPU初期化
--------------------------------------------------------*/
s32 my_irqh_callback(s32 level);

void m68000_init(void)
{
#ifdef CYCLONE

	m68k.read8  = read8;
	m68k.read16 = read16;
	m68k.read32 = cpu_readmem24_dword;

	m68k.fetch8  = read8;
	m68k.fetch16 = read16;
	m68k.fetch32 = cpu_readmem24_dword;

	m68k.write8  = cpu_writemem24;
	m68k.write16 = cpu_writemem24_word;
	m68k.write32 = cpu_writemem24_dword;

	m68k.checkpc = MyCheckPc;

	m68k.IrqCallback = my_irqh_callback;

	CycloneInit();

#else


    C68k_Init(&C68K, my_irqh_callback);
#if 0
    C68k_Set_ReadB(&C68K, Memory_ReadB);
    C68k_Set_ReadW(&C68K, Memory_ReadW);
    C68k_Set_WriteB(&C68K, Memory_WriteB);
    C68k_Set_WriteW(&C68K, Memory_WriteW);
#else
    C68k_Set_ReadB(&C68K, Memory_ReadB);
    C68k_Set_ReadW(&C68K, Memory_ReadW);
    C68k_Set_WriteB(&C68K, Memory_WriteB);
    C68k_Set_WriteW(&C68K, Memory_WriteW);
#endif
        C68k_Set_Fetch(&C68K, 0x000000, 0xbfffff, (pointer)MEM);
        C68k_Set_Fetch(&C68K, 0xc00000, 0xc7ffff, (pointer)GVRAM);
        C68k_Set_Fetch(&C68K, 0xe00000, 0xe7ffff, (pointer)TVRAM);
        C68k_Set_Fetch(&C68K, 0xea0000, 0xea1fff, (pointer)SCSIIPL);
        C68k_Set_Fetch(&C68K, 0xed0000, 0xed3fff, (pointer)SRAM);
        C68k_Set_Fetch(&C68K, 0xf00000, 0xfbffff, (pointer)FONT);
        C68k_Set_Fetch(&C68K, 0xfc0000, 0xffffff, (pointer)IPL);

#endif
}


/*--------------------------------------------------------
	CPUリセット
--------------------------------------------------------*/

void m68000_reset(void)
{
#ifdef CYCLONE

	CycloneReset(&m68k);
	m68k.state_flags = 0; // Go to default state (not stopped, halted, etc.)
	m68k.srh = 0x27; // Set supervisor mode

	#else
	C68k_Reset(&C68K);
#endif
}


/*--------------------------------------------------------
	CPU停止
--------------------------------------------------------*/

void m68000_exit(void)
{
}


/*--------------------------------------------------------
	CPU実行
--------------------------------------------------------*/

int m68000_execute(int cycles)
{	
#ifdef CYCLONE
	m68k.cycles = cycles;
	CycloneRun(&m68k);
	return m68k.cycles ;
#else
	return C68k_Exec(&C68K, cycles);

#endif
}



/*--------------------------------------------------------
	割り込み処理
--------------------------------------------------------*/

void m68000_set_irq_line(int irqline, int state)
{
	#ifdef CYCLONE

	m68k.irq = irqline;
	
	#else
//	if (irqline == IRQ_LINE_NMI)
//		irqline = 7;

//	C68k_Set_IRQ(&C68K, irqline, state);
	C68k_Set_IRQ(&C68K, irqline);
#endif
}


/*--------------------------------------------------------
	割り込みコールバック関数設定
--------------------------------------------------------*/

void m68000_set_irq_callback(int (*callback)(int line))
{
//	C68k_Set_IRQ_Callback(&C68K, callback);
}


/*--------------------------------------------------------
	レジスタ取得
--------------------------------------------------------*/

UINT32 m68000_get_reg(int regnum)
{
	#ifdef CYCLONE

	switch (regnum)
	{
		case M68K_PC: return m68k.pc - m68k.membase;
		case M68K_SR: return CycloneGetSr(&m68k);
		case M68K_D0: return m68k.d[0];
		case M68K_D1: return m68k.d[1];
		case M68K_D2: return m68k.d[2];
		case M68K_D3: return m68k.d[3];
		case M68K_D4: return m68k.d[4];
		case M68K_D5: return m68k.d[5];
		case M68K_D6: return m68k.d[6];
		case M68K_D7: return m68k.d[7];
		case M68K_A0: return m68k.a[0];
		case M68K_A1: return m68k.a[1];
		case M68K_A2: return m68k.a[2];
		case M68K_A3: return m68k.a[3];
		case M68K_A4: return m68k.a[4];
		case M68K_A5: return m68k.a[5];
		case M68K_A6: return m68k.a[6];
		case M68K_A7: return m68k.a[7];
		default:
			break;
	}
	return 0x0BADC0DE;
#else
	switch (regnum)
	{
#if 0
	case M68K_PC:  return C68k_Get_Reg(&C68K, C68K_PC);
	case M68K_USP: return C68k_Get_Reg(&C68K, C68K_USP);
	case M68K_MSP: return C68k_Get_Reg(&C68K, C68K_MSP);
	case M68K_SR:  return C68k_Get_Reg(&C68K, C68K_SR);
	case M68K_D0:  return C68k_Get_Reg(&C68K, C68K_D0);
	case M68K_D1:  return C68k_Get_Reg(&C68K, C68K_D1);
	case M68K_D2:  return C68k_Get_Reg(&C68K, C68K_D2);
	case M68K_D3:  return C68k_Get_Reg(&C68K, C68K_D3);
	case M68K_D4:  return C68k_Get_Reg(&C68K, C68K_D4);
	case M68K_D5:  return C68k_Get_Reg(&C68K, C68K_D5);
	case M68K_D6:  return C68k_Get_Reg(&C68K, C68K_D6);
	case M68K_D7:  return C68k_Get_Reg(&C68K, C68K_D7);
	case M68K_A0:  return C68k_Get_Reg(&C68K, C68K_A0);
	case M68K_A1:  return C68k_Get_Reg(&C68K, C68K_A1);
	case M68K_A2:  return C68k_Get_Reg(&C68K, C68K_A2);
	case M68K_A3:  return C68k_Get_Reg(&C68K, C68K_A3);
	case M68K_A4:  return C68k_Get_Reg(&C68K, C68K_A4);
	case M68K_A5:  return C68k_Get_Reg(&C68K, C68K_A5);
	case M68K_A6:  return C68k_Get_Reg(&C68K, C68K_A6);
	case M68K_A7:  return C68k_Get_Reg(&C68K, C68K_A7);
#else
	case M68K_PC:  return C68k_Get_PC(&C68K);
	case M68K_USP: return C68k_Get_USP(&C68K);
	case M68K_MSP: return C68k_Get_MSP(&C68K);
	case M68K_SR:  return C68k_Get_SR(&C68K);
	case M68K_D0:  return C68k_Get_DReg(&C68K, 0);
	case M68K_D1:  return C68k_Get_DReg(&C68K, 1);
	case M68K_D2:  return C68k_Get_DReg(&C68K, 2);
	case M68K_D3:  return C68k_Get_DReg(&C68K, 3);
	case M68K_D4:  return C68k_Get_DReg(&C68K, 4);
	case M68K_D5:  return C68k_Get_DReg(&C68K, 5);
	case M68K_D6:  return C68k_Get_DReg(&C68K, 6);
	case M68K_D7:  return C68k_Get_DReg(&C68K, 7);
	case M68K_A0:  return C68k_Get_AReg(&C68K, 0);
	case M68K_A1:  return C68k_Get_AReg(&C68K, 1);
	case M68K_A2:  return C68k_Get_AReg(&C68K, 2);
	case M68K_A3:  return C68k_Get_AReg(&C68K, 3);
	case M68K_A4:  return C68k_Get_AReg(&C68K, 4);
	case M68K_A5:  return C68k_Get_AReg(&C68K, 5);
	case M68K_A6:  return C68k_Get_AReg(&C68K, 6);
	case M68K_A7:  return C68k_Get_AReg(&C68K, 7);
#endif

	default: return 0;
	}
#endif
}


/*--------------------------------------------------------
	レジスタ設定
--------------------------------------------------------*/

void m68000_set_reg(int regnum, UINT32 val)
{
	#ifdef CYCLONE
	switch (regnum)
	{
		case M68K_PC:
		  	m68k.pc = m68k.checkpc(val+m68k.membase);
			break;
		case M68K_SR:
		  	CycloneSetSr(&m68k, val);
			break;
		case M68K_D0: m68k.d[0] = val; break;
		case M68K_D1: m68k.d[1] = val; break;
		case M68K_D2: m68k.d[2] = val; break;
		case M68K_D3: m68k.d[3] = val; break;
		case M68K_D4: m68k.d[4] = val; break;
		case M68K_D5: m68k.d[5] = val; break;
		case M68K_D6: m68k.d[6] = val; break;
		case M68K_D7: m68k.d[7] = val; break;
		case M68K_A0: m68k.a[0] = val; break;
		case M68K_A1: m68k.a[1] = val; break;
		case M68K_A2: m68k.a[2] = val; break;
		case M68K_A3: m68k.a[3] = val; break;
		case M68K_A4: m68k.a[4] = val; break;
		case M68K_A5: m68k.a[5] = val; break;
		case M68K_A6: m68k.a[6] = val; break;
		case M68K_A7: m68k.a[7] = val; break;

		
		default: break;
	}
	
	#else

	switch (regnum)
	{
#if 0
	case M68K_PC:  C68k_Set_Reg(&C68K, C68K_PC, val); break;
	case M68K_USP: C68k_Set_Reg(&C68K, C68K_USP, val); break;
	case M68K_MSP: C68k_Set_Reg(&C68K, C68K_MSP, val); break;
	case M68K_SR:  C68k_Set_Reg(&C68K, C68K_SR, val); break;
	case M68K_D0:  C68k_Set_Reg(&C68K, C68K_D0, val); break;
	case M68K_D1:  C68k_Set_Reg(&C68K, C68K_D1, val); break;
	case M68K_D2:  C68k_Set_Reg(&C68K, C68K_D2, val); break;
	case M68K_D3:  C68k_Set_Reg(&C68K, C68K_D3, val); break;
	case M68K_D4:  C68k_Set_Reg(&C68K, C68K_D4, val); break;
	case M68K_D5:  C68k_Set_Reg(&C68K, C68K_D5, val); break;
	case M68K_D6:  C68k_Set_Reg(&C68K, C68K_D6, val); break;
	case M68K_D7:  C68k_Set_Reg(&C68K, C68K_D7, val); break;
	case M68K_A0:  C68k_Set_Reg(&C68K, C68K_A0, val); break;
	case M68K_A1:  C68k_Set_Reg(&C68K, C68K_A1, val); break;
	case M68K_A2:  C68k_Set_Reg(&C68K, C68K_A2, val); break;
	case M68K_A3:  C68k_Set_Reg(&C68K, C68K_A3, val); break;
	case M68K_A4:  C68k_Set_Reg(&C68K, C68K_A4, val); break;
	case M68K_A5:  C68k_Set_Reg(&C68K, C68K_A5, val); break;
	case M68K_A6:  C68k_Set_Reg(&C68K, C68K_A6, val); break;
	case M68K_A7:  C68k_Set_Reg(&C68K, C68K_A7, val); break;
#else
	case M68K_PC:  C68k_Set_PC(&C68K, val); break;
	case M68K_USP: C68k_Set_USP(&C68K, val); break;
	case M68K_MSP: C68k_Set_MSP(&C68K, val); break;
	case M68K_SR:  C68k_Set_SR(&C68K, val); break;
	case M68K_D0:  C68k_Set_DReg(&C68K, 0, val); break;
	case M68K_D1:  C68k_Set_DReg(&C68K, 1, val); break;
	case M68K_D2:  C68k_Set_DReg(&C68K, 2, val); break;
	case M68K_D3:  C68k_Set_DReg(&C68K, 3, val); break;
	case M68K_D4:  C68k_Set_DReg(&C68K, 4, val); break;
	case M68K_D5:  C68k_Set_DReg(&C68K, 5, val); break;
	case M68K_D6:  C68k_Set_DReg(&C68K, 6, val); break;
	case M68K_D7:  C68k_Set_DReg(&C68K, 7, val); break;
	case M68K_A0:  C68k_Set_AReg(&C68K, 0, val); break;
	case M68K_A1:  C68k_Set_AReg(&C68K, 1, val); break;
	case M68K_A2:  C68k_Set_AReg(&C68K, 2, val); break;
	case M68K_A3:  C68k_Set_AReg(&C68K, 3, val); break;
	case M68K_A4:  C68k_Set_AReg(&C68K, 4, val); break;
	case M68K_A5:  C68k_Set_AReg(&C68K, 5, val); break;
	case M68K_A6:  C68k_Set_AReg(&C68K, 6, val); break;
	case M68K_A7:  C68k_Set_AReg(&C68K, 7, val); break;
#endif
	default: break;
	}
#endif
}


/*------------------------------------------------------
	セーブ/ロード ステート
------------------------------------------------------*/

#ifdef SAVE_STATE

STATE_SAVE( m68000 )
{
	#ifdef CYCLONE
	
	#else

	int i;
	UINT32 pc = C68k_Get_Reg(&C68K, C68K_PC);

	for (i = 0; i < 8; i++)
		state_save_long(&C68K.D[i], 1);
	for (i = 0; i < 8; i++)
		state_save_long(&C68K.A[i], 1);

	state_save_long(&C68K.flag_C, 1);
	state_save_long(&C68K.flag_V, 1);
	state_save_long(&C68K.flag_Z, 1);
	state_save_long(&C68K.flag_N, 1);
	state_save_long(&C68K.flag_X, 1);
	state_save_long(&C68K.flag_I, 1);
	state_save_long(&C68K.flag_S, 1);
	state_save_long(&C68K.USP, 1);
	state_save_long(&pc, 1);
	state_save_long(&C68K.HaltState, 1);
	state_save_long(&C68K.IRQLine, 1);
	state_save_long(&C68K.IRQState, 1);
	#endif
}

STATE_LOAD( m68000 )
{
	#ifdef CYCLONE
	
	#else

	int i;
	UINT32 pc;

	for (i = 0; i < 8; i++)
		state_load_long(&C68K.D[i], 1);
	for (i = 0; i < 8; i++)
		state_load_long(&C68K.A[i], 1);

	state_load_long(&C68K.flag_C, 1);
	state_load_long(&C68K.flag_V, 1);
	state_load_long(&C68K.flag_Z, 1);
	state_load_long(&C68K.flag_N, 1);
	state_load_long(&C68K.flag_X, 1);
	state_load_long(&C68K.flag_I, 1);
	state_load_long(&C68K.flag_S, 1);
	state_load_long(&C68K.USP, 1);
	state_load_long(&pc, 1);
	state_load_long(&C68K.HaltState, 1);
	state_load_long(&C68K.IRQLine, 1);
	state_load_long(&C68K.IRQState, 1);

	C68k_Set_Reg(&C68K, C68K_PC, pc);
	#endif
}

#endif /* SAVE_STATE */
