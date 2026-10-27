/******************************************************************************
 *
 * C68K (68000 CPU emulator) version 0.80
 * Compiled with Dev-C++
 * Copyright 2003-2004 Stephane Dallongeville
 *
 * (Modified by NJ)
 *
 *****************************************************************************/

#include <string.h>
#include "c68k.h"

/******************************************************************************
	マクロ
******************************************************************************/

#include "c68kmacro.h"

/******************************************************************************
	グローバル構造体
******************************************************************************/

c68k_struc C68K;

/******************************************************************************
	ローカル変数
******************************************************************************/

static uint8_t c68k_bad_address[1 << C68K_FETCH_SFT];

/******************************************************************************
	ローカル関数
******************************************************************************/

/*--------------------------------------------------------
	割り込みコールバック
--------------------------------------------------------*/

static int32_t C68k_InterruptCallback(int32_t line)
{
	return C68K_INTERRUPT_AUTOVECTOR_EX + line;
}

/*--------------------------------------------------------
	リセットコールバック
--------------------------------------------------------*/

static void C68k_ResetCallback(void)
{
}

/******************************************************************************
	C68Kインタフェース関数
******************************************************************************/

/*--------------------------------------------------------
	CPU初期化
--------------------------------------------------------*/

void C68k_Init(c68k_struc *CPU, int32_t (*irq_cb)(int32_t level))
{
	int i;

	memset(CPU, 0, sizeof(c68k_struc));

	if (irq_cb) CPU->Interrupt_CallBack = irq_cb;
	else CPU->Interrupt_CallBack = C68k_InterruptCallback;
	CPU->Reset_CallBack = C68k_ResetCallback;

	memset(c68k_bad_address, 0xff, sizeof(c68k_bad_address));

	for (i = 0; i < C68K_FETCH_BANK; i++)
		CPU->Fetch[i] = (uintptr_t)c68k_bad_address;

	C68k_Exec(NULL, 0);
}

/*--------------------------------------------------------
	CPUリセット
--------------------------------------------------------*/

void C68k_Reset(c68k_struc *CPU)
{
	memset(CPU, 0, (uint8_t*)&CPU->BasePC - (uint8_t*)&CPU->D[0]);

	CPU->flag_I = 7;
	CPU->flag_S = C68K_SR_S;

	/* SP, PCの初期化はWinX68k_Reset()側で実行する */
}

/*--------------------------------------------------------
	割り込み処理
--------------------------------------------------------*/

void C68k_Set_IRQ(c68k_struc *CPU, int32_t line, int32_t state)
{
	CPU->IRQState = state;
	if (state == CLEAR_LINE)
	{
		CPU->IRQLine = 0;
	}
	else
	{
		CPU->IRQLine = line;
		if (C68K.ICount)
		{                                   /* 多重割り込み時（CARAT）*/
			C68K.ICountBk += C68K.ICount;   /* 強制的に割り込みチェックをさせる */
			C68K.ICount = 0;                /* 苦肉の策 ^^; */
		}
		CPU->HaltState = 0;
	}
}

/*--------------------------------------------------------
	レジスタ取得
--------------------------------------------------------*/

uint32_t C68k_Get_Reg(c68k_struc *CPU, int32_t regnum)
{
	switch (regnum)
	{
	case C68K_PC:  return (uint32_t)(CPU->PC - CPU->BasePC);
	case C68K_USP: return (CPU->flag_S ? CPU->USP : CPU->A[7]);
	case C68K_MSP: return (CPU->flag_S ? CPU->A[7] : CPU->USP);
	case C68K_SR:  return GET_SR();
	case C68K_D0:  return CPU->D[0];
	case C68K_D1:  return CPU->D[1];
	case C68K_D2:  return CPU->D[2];
	case C68K_D3:  return CPU->D[3];
	case C68K_D4:  return CPU->D[4];
	case C68K_D5:  return CPU->D[5];
	case C68K_D6:  return CPU->D[6];
	case C68K_D7:  return CPU->D[7];
	case C68K_A0:  return CPU->A[0];
	case C68K_A1:  return CPU->A[1];
	case C68K_A2:  return CPU->A[2];
	case C68K_A3:  return CPU->A[3];
	case C68K_A4:  return CPU->A[4];
	case C68K_A5:  return CPU->A[5];
	case C68K_A6:  return CPU->A[6];
	case C68K_A7:  return CPU->A[7];
	default: return 0;
	}
}

/*--------------------------------------------------------
	レジスタ設定
--------------------------------------------------------*/

void C68k_Set_Reg(c68k_struc *CPU, int32_t regnum, uint32_t val)
{
	switch (regnum)
	{
	case C68K_PC:
		CPU->BasePC = CPU->Fetch[(val >> C68K_FETCH_SFT) & C68K_FETCH_MASK];
		CPU->BasePC -= val & 0xff000000;
		CPU->PC = val + CPU->BasePC;
		break;

	case C68K_USP:
		if (CPU->flag_S) CPU->USP = val;
		else CPU->A[7] = val;
		break;

	case C68K_MSP:
		if (CPU->flag_S) CPU->A[7] = val;
		else CPU->USP = val;
		break;

	case C68K_SR: SET_SR(val); break;
	case C68K_D0: CPU->D[0] = val; break;
	case C68K_D1: CPU->D[1] = val; break;
	case C68K_D2: CPU->D[2] = val; break;
	case C68K_D3: CPU->D[3] = val; break;
	case C68K_D4: CPU->D[4] = val; break;
	case C68K_D5: CPU->D[5] = val; break;
	case C68K_D6: CPU->D[6] = val; break;
	case C68K_D7: CPU->D[7] = val; break;
	case C68K_A0: CPU->A[0] = val; break;
	case C68K_A1: CPU->A[1] = val; break;
	case C68K_A2: CPU->A[2] = val; break;
	case C68K_A3: CPU->A[3] = val; break;
	case C68K_A4: CPU->A[4] = val; break;
	case C68K_A5: CPU->A[5] = val; break;
	case C68K_A6: CPU->A[6] = val; break;
	case C68K_A7: CPU->A[7] = val; break;
	default: break;
	}

}

/*--------------------------------------------------------
	フェッチアドレス設定
--------------------------------------------------------*/

void C68k_Set_Fetch(c68k_struc *CPU, uint32_t low_adr, uint32_t high_adr, uintptr_t fetch_adr)
{
	uint32_t i, j;

	i = (low_adr >> C68K_FETCH_SFT) & C68K_FETCH_MASK;
	j = (high_adr >> C68K_FETCH_SFT) & C68K_FETCH_MASK;
	fetch_adr -= i << C68K_FETCH_SFT;
	while (i <= j) CPU->Fetch[i++] = fetch_adr;
}

/*--------------------------------------------------------
	メモリリード/ライト関数設定
--------------------------------------------------------*/

void C68k_Set_ReadB(c68k_struc *CPU, uint32_t (*Func)(uint32_t address))
{
	CPU->Read_Byte = Func;
	CPU->Read_Byte_PC_Relative = Func;
}

void C68k_Set_ReadW(c68k_struc *CPU, uint32_t (*Func)(uint32_t address))
{
	CPU->Read_Word = Func;
	CPU->Read_Word_PC_Relative = Func;
}

void C68k_Set_ReadB_PC_Relative(c68k_struc *CPU, uint32_t (*Func)(uint32_t address))
{
	CPU->Read_Byte_PC_Relative = Func;
}

void C68k_Set_ReadW_PC_Relative(c68k_struc *CPU, uint32_t (*Func)(uint32_t address))
{
	CPU->Read_Word_PC_Relative = Func;
}

void C68k_Set_WriteB(c68k_struc *CPU, void (*Func)(uint32_t address, uint32_t data))
{
	CPU->Write_Byte = Func;
}

void C68k_Set_WriteW(c68k_struc *CPU, void (*Func)(uint32_t address, uint32_t data))
{
	CPU->Write_Word = Func;
}

/*--------------------------------------------------------
	コールバック関数設定
--------------------------------------------------------*/

void C68k_Set_IRQ_Callback(c68k_struc *CPU, int32_t (*Func)(int32_t irqline))
{
	CPU->Interrupt_CallBack = Func;
}

void C68k_Set_Reset_Callback(c68k_struc *CPU, void (*Func)(void))
{
	CPU->Reset_CallBack = Func;
}
