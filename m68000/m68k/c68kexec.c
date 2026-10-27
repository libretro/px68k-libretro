/******************************************************************************
 *
 * C68K (68000 CPU emulator) version 0.80
 * Compiled with Dev-C++
 * Copyright 2003-2004 Stephane Dallongeville
 *
 * (Modified by NJ)
 *
 *****************************************************************************/

#include "c68k.h"
#include "c68kmacro.h"

static void *JumpTable[0x10000];

extern uint32_t BusErrHandling;
extern uint32_t BusErrAdr;

int32_t C68k_Exec(c68k_struc *cpu, int32_t cycles)
{
	if (cpu != 0)
	{
		c68k_struc *CPU;
		uintptr_t PC;
		uint32_t Opcode;
		uint32_t adr;
		uint32_t res;
		uintptr_t src;
		uintptr_t dst;
		int32_t cycles_used;

		CPU = cpu;
		PC = CPU->PC;

		CPU->ICount = cycles;

C68k_Check_Interrupt:
		CHECK_INT
		if (!CPU->HaltState)
		{

C68k_Exec_Next:
			if (CPU->ICount > 0)
			{

				if (BusErrHandling) {
					SWAP_SP();
					PUSH_32_F(GET_PC() - 2);
					PUSH_16_F(GET_SR());
					CPU->A[7] -= 2;
					PUSH_32_F(BusErrAdr);
					CPU->A[7] -= 2;
					CPU->flag_S = C68K_SR_S;
					PC = READ_MEM_32((C68K_BUS_ERROR_EX) << 2);
					SET_PC(PC);
					BusErrHandling = 0;
				}

				Opcode = READ_IMM_16();
				PC += 2;
				goto *JumpTable[Opcode];

				#include "c68k_op.c"
			}
		}

		CPU->PC = PC;

		cycles_used = cycles - CPU->ICount - CPU->ICountBk;

		CPU->ICount   = 0;
		CPU->ICountBk = 0;

		return cycles_used;
	}
	else
	{
		/* initializes jump table */
		#include "c68k_ini.c"
	}

	return 0;
}
