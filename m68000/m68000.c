/******************************************************************************

	m68000.c

	M68000 CPUインタフェース関数

******************************************************************************/

#include "m68000.h"

#if defined (HAVE_CYCLONE)
struct Cyclone m68k;
#endif
#if defined (HAVE_C68K)
#include "c68k/c68k.h"
#endif
#if defined (HAVE_M68000)
#include "m68k/c68k.h"
#endif
#if defined (HAVE_MUSASHI)
#include "musashi/m68k.h"
#include "musashi/m68kcpu.h"
#endif
#ifdef HAVE_CPU_RUNTIME
static int m68000_use_musashi = 0;
void m68000_set_emulation(int use_musashi) { m68000_use_musashi = use_musashi ? 1 : 0; }
int m68000_get_emulation(void) { return m68000_use_musashi; }
#else
void m68000_set_emulation(int use_musashi) { (void)use_musashi; }
int m68000_get_emulation(void) { return 0; }
#endif

#include "../x68k/x68kmemory.h"

int m68000_ICountBk;
int ICount;

#ifdef HAVE_CPU_RUNTIME
/* Runtime-selectable CPU state format.
 *
 * The common register set makes save states portable between C68K and
 * Musashi. Backend-specific transient state is also stored so same-backend
 * restores (including RetroArch Run-Ahead) retain the existing precision.
 */
static int m68000_RuntimeStateAction(StateMem *sm, int load, int data_only)
{
    uint32_t format_version = 0;
    uint32_t backend = 0;
    uint32_t reg_d[8] = {0}, reg_a[8] = {0};
    uint32_t reg_pc = 0, reg_sr = 0, reg_usp = 0, reg_ssp = 0;
    uint32_t reg_irq_level = 0, reg_stopped = 0;
    uint32_t raw_c68k_d[8] = {0}, raw_c68k_a[8] = {0};
    uint32_t raw_c68k_flag_c = 0, raw_c68k_flag_v = 0;
    uint32_t raw_c68k_flag_notz = 0, raw_c68k_flag_n = 0;
    uint32_t raw_c68k_flag_x = 0, raw_c68k_flag_i = 0, raw_c68k_flag_s = 0;
    uint32_t raw_c68k_usp = 0, raw_c68k_pc = 0, raw_c68k_halt = 0;
    int32_t raw_c68k_irq_line = 0, raw_c68k_irq_state = 0;
    int32_t raw_c68k_icount = 0, raw_c68k_icount_bk = 0;
    uint32_t raw_m_c_flag = 0, raw_m_v_flag = 0, raw_m_not_z_flag = 0;
    uint32_t raw_m_n_flag = 0, raw_m_x_flag = 0, raw_m_m_flag = 0;
    uint32_t raw_m_s_flag = 0, raw_m_int_level = 0, raw_m_stopped = 0;
    int32_t raw_m_remaining_cycles = 0;
    int ret;
    SFORMAT StateRegs[] = {
        SFVARN(format_version, "CPU_STATE_FORMAT"),
        SFVARN(backend, "CPU_STATE_BACKEND"),
        SFVARN(reg_d[0], "REG_D0"), SFVARN(reg_d[1], "REG_D1"),
        SFVARN(reg_d[2], "REG_D2"), SFVARN(reg_d[3], "REG_D3"),
        SFVARN(reg_d[4], "REG_D4"), SFVARN(reg_d[5], "REG_D5"),
        SFVARN(reg_d[6], "REG_D6"), SFVARN(reg_d[7], "REG_D7"),
        SFVARN(reg_a[0], "REG_A0"), SFVARN(reg_a[1], "REG_A1"),
        SFVARN(reg_a[2], "REG_A2"), SFVARN(reg_a[3], "REG_A3"),
        SFVARN(reg_a[4], "REG_A4"), SFVARN(reg_a[5], "REG_A5"),
        SFVARN(reg_a[6], "REG_A6"), SFVARN(reg_a[7], "REG_A7"),
        SFVARN(reg_pc, "REG_PC"), SFVARN(reg_sr, "REG_SR"),
        SFVARN(reg_usp, "REG_USP"), SFVARN(reg_ssp, "REG_SSP"),
        SFVARN(reg_irq_level, "REG_IRQ_LEVEL"),
        SFVARN(reg_stopped, "REG_STOPPED"),
        /* Keep the legacy C68K names for named-state backward compatibility. */
        SFVARN(raw_c68k_d[0], "D0"), SFVARN(raw_c68k_d[1], "D1"),
        SFVARN(raw_c68k_d[2], "D2"), SFVARN(raw_c68k_d[3], "D3"),
        SFVARN(raw_c68k_d[4], "D4"), SFVARN(raw_c68k_d[5], "D5"),
        SFVARN(raw_c68k_d[6], "D6"), SFVARN(raw_c68k_d[7], "D7"),
        SFVARN(raw_c68k_a[0], "A0"), SFVARN(raw_c68k_a[1], "A1"),
        SFVARN(raw_c68k_a[2], "A2"), SFVARN(raw_c68k_a[3], "A3"),
        SFVARN(raw_c68k_a[4], "A4"), SFVARN(raw_c68k_a[5], "A5"),
        SFVARN(raw_c68k_a[6], "A6"), SFVARN(raw_c68k_a[7], "A7"),
        SFVARN(raw_c68k_flag_c, "flag_C"), SFVARN(raw_c68k_flag_v, "flag_V"),
        SFVARN(raw_c68k_flag_notz, "flag_notZ"), SFVARN(raw_c68k_flag_n, "flag_N"),
        SFVARN(raw_c68k_flag_x, "flag_X"), SFVARN(raw_c68k_flag_i, "flag_I"),
        SFVARN(raw_c68k_flag_s, "flag_S"), SFVARN(raw_c68k_usp, "USP"),
        SFVARN(raw_c68k_pc, "PC"), SFVARN(raw_c68k_halt, "HaltState"),
        SFVARN(raw_c68k_irq_line, "IRQLine"), SFVARN(raw_c68k_irq_state, "IRQState"),
        SFVARN(raw_c68k_icount, "ICount"), SFVARN(raw_c68k_icount_bk, "ICountBk"),
        /* Musashi transient state: present in every state for a fixed layout. */
        SFVARN(raw_m_c_flag, "MUSASHI_c_flag"),
        SFVARN(raw_m_v_flag, "MUSASHI_v_flag"),
        SFVARN(raw_m_not_z_flag, "MUSASHI_not_z_flag"),
        SFVARN(raw_m_n_flag, "MUSASHI_n_flag"),
        SFVARN(raw_m_x_flag, "MUSASHI_x_flag"),
        SFVARN(raw_m_m_flag, "MUSASHI_m_flag"),
        SFVARN(raw_m_s_flag, "MUSASHI_s_flag"),
        SFVARN(raw_m_int_level, "MUSASHI_int_level"),
        SFVARN(raw_m_stopped, "MUSASHI_stopped"),
        SFVARN(raw_m_remaining_cycles, "MUSASHI_remaining_cycles"),
        SFEND
    };

    if (!load)
    {
        int i;
        format_version = 1;
        backend = m68000_use_musashi ? 1u : 0u;
        for (i = 0; i < 8; i++)
        {
            if (backend)
            {
                reg_d[i] = m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_D0 + i));
                reg_a[i] = m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_A0 + i));
            }
            else
            {
                reg_d[i] = C68k_Get_Reg(&C68K, C68K_D0 + i);
                reg_a[i] = C68k_Get_Reg(&C68K, C68K_A0 + i);
                raw_c68k_d[i] = C68K.D[i];
                raw_c68k_a[i] = C68K.A[i];
            }
        }
        if (backend)
        {
            reg_pc = m68k_get_reg(NULL, M68K_REG_PC);
            reg_sr = m68k_get_reg(NULL, M68K_REG_SR);
            reg_usp = m68k_get_reg(NULL, M68K_REG_USP);
            reg_ssp = m68k_get_reg(NULL, M68K_REG_ISP);
            reg_irq_level = m68ki_cpu.int_level;
            reg_stopped = m68ki_cpu.stopped != 0;
            raw_m_c_flag = m68ki_cpu.c_flag;
            raw_m_v_flag = m68ki_cpu.v_flag;
            raw_m_not_z_flag = m68ki_cpu.not_z_flag;
            raw_m_n_flag = m68ki_cpu.n_flag;
            raw_m_x_flag = m68ki_cpu.x_flag;
            raw_m_m_flag = m68ki_cpu.m_flag;
            raw_m_s_flag = m68ki_cpu.s_flag;
            raw_m_int_level = m68ki_cpu.int_level;
            raw_m_stopped = m68ki_cpu.stopped;
            raw_m_remaining_cycles = m68ki_remaining_cycles;
        }
        else
        {
            reg_pc = C68k_Get_Reg(&C68K, C68K_PC);
            reg_sr = C68k_Get_Reg(&C68K, C68K_SR);
            reg_usp = C68k_Get_Reg(&C68K, C68K_USP);
            reg_ssp = C68k_Get_Reg(&C68K, C68K_MSP);
            reg_irq_level = (C68K.IRQState == CLEAR_LINE) ? 0u : (uint32_t)C68K.IRQLine;
            reg_stopped = C68K.HaltState != 0;
            raw_c68k_flag_c = C68K.flag_C; raw_c68k_flag_v = C68K.flag_V;
            raw_c68k_flag_notz = C68K.flag_notZ; raw_c68k_flag_n = C68K.flag_N;
            raw_c68k_flag_x = C68K.flag_X; raw_c68k_flag_i = C68K.flag_I;
            raw_c68k_flag_s = C68K.flag_S; raw_c68k_usp = C68K.USP;
            raw_c68k_pc = reg_pc; raw_c68k_halt = C68K.HaltState;
            raw_c68k_irq_line = C68K.IRQLine; raw_c68k_irq_state = C68K.IRQState;
            raw_c68k_icount = C68K.ICount; raw_c68k_icount_bk = C68K.ICountBk;
        }
        return PX68KSS_StateAction(sm, 0, data_only, StateRegs, "X68K_CPU", false);
    }

    ret = PX68KSS_StateAction(sm, 1, data_only, StateRegs, "X68K_CPU", false);
    if (ret && format_version == 1)
    {
        int i;
        if (backend == (uint32_t)(m68000_use_musashi ? 1 : 0))
        {
            if (m68000_use_musashi)
            {
                for (i = 0; i < 8; i++)
                {
                    m68k_set_reg((m68k_register_t)(M68K_REG_D0 + i), reg_d[i]);
                    m68k_set_reg((m68k_register_t)(M68K_REG_A0 + i), reg_a[i]);
                }
                m68k_set_reg(M68K_REG_SR, reg_sr);
                m68k_set_reg(M68K_REG_USP, reg_usp);
                m68k_set_reg(M68K_REG_ISP, reg_ssp);
                m68k_set_reg(M68K_REG_A7, reg_a[7]);
                m68k_set_reg(M68K_REG_PC, reg_pc);
                m68ki_cpu.c_flag = raw_m_c_flag; m68ki_cpu.v_flag = raw_m_v_flag;
                m68ki_cpu.not_z_flag = raw_m_not_z_flag; m68ki_cpu.n_flag = raw_m_n_flag;
                m68ki_cpu.x_flag = raw_m_x_flag; m68ki_cpu.m_flag = raw_m_m_flag;
                m68ki_cpu.s_flag = raw_m_s_flag; m68ki_cpu.int_level = raw_m_int_level;
                m68ki_cpu.stopped = raw_m_stopped;
                m68ki_remaining_cycles = raw_m_remaining_cycles;
            }
            else
            {
                for (i = 0; i < 8; i++)
                {
                    C68K.D[i] = raw_c68k_d[i]; C68K.A[i] = raw_c68k_a[i];
                }
                C68K.flag_C = raw_c68k_flag_c; C68K.flag_V = raw_c68k_flag_v;
                C68K.flag_notZ = raw_c68k_flag_notz; C68K.flag_N = raw_c68k_flag_n;
                C68K.flag_X = raw_c68k_flag_x; C68K.flag_I = raw_c68k_flag_i;
                C68K.flag_S = raw_c68k_flag_s; C68K.USP = raw_c68k_usp;
                C68k_Set_Reg(&C68K, C68K_PC, raw_c68k_pc);
                C68K.HaltState = raw_c68k_halt; C68K.IRQLine = raw_c68k_irq_line;
                C68K.IRQState = raw_c68k_irq_state; C68K.ICount = raw_c68k_icount;
                C68K.ICountBk = raw_c68k_icount_bk;
            }
        }
        else if (m68000_use_musashi)
        {
            /* Cross-load C68K state into Musashi using architectural state. */
            for (i = 0; i < 8; i++)
            {
                m68k_set_reg((m68k_register_t)(M68K_REG_D0 + i), reg_d[i]);
                m68k_set_reg((m68k_register_t)(M68K_REG_A0 + i), reg_a[i]);
            }
            m68k_set_reg(M68K_REG_SR, reg_sr); m68k_set_reg(M68K_REG_USP, reg_usp);
            m68k_set_reg(M68K_REG_ISP, reg_ssp); m68k_set_reg(M68K_REG_A7, reg_a[7]);
            m68k_set_reg(M68K_REG_PC, reg_pc);
            m68ki_cpu.int_level = reg_irq_level;
            m68ki_cpu.stopped = reg_stopped ? STOP_LEVEL_STOP : 0;
            m68ki_remaining_cycles = 0;
        }
        else
        {
            /* Cross-load Musashi state into C68K using architectural state. */
            for (i = 0; i < 8; i++)
            {
                C68k_Set_Reg(&C68K, C68K_D0 + i, reg_d[i]);
                C68k_Set_Reg(&C68K, C68K_A0 + i, reg_a[i]);
            }
            C68k_Set_Reg(&C68K, C68K_SR, reg_sr);
            C68k_Set_Reg(&C68K, C68K_USP, reg_usp);
            C68k_Set_Reg(&C68K, C68K_MSP, reg_ssp);
            C68k_Set_Reg(&C68K, C68K_A7, reg_a[7]);
            C68k_Set_Reg(&C68K, C68K_PC, reg_pc);
            C68K.IRQLine = (int32_t)reg_irq_level;
            C68K.IRQState = reg_irq_level ? HOLD_LINE : CLEAR_LINE;
            C68K.HaltState = reg_stopped ? 1u : 0u;
            C68K.ICount = 0; C68K.ICountBk = 0;
        }
        return 1;
    }

    if (ret)
    {
        /* Older named C68K states lack the format marker; restore their fields. */
        int i;
        for (i = 0; i < 8; i++) { C68K.D[i] = raw_c68k_d[i]; C68K.A[i] = raw_c68k_a[i]; }
        C68K.flag_C = raw_c68k_flag_c; C68K.flag_V = raw_c68k_flag_v;
        C68K.flag_notZ = raw_c68k_flag_notz; C68K.flag_N = raw_c68k_flag_n;
        C68K.flag_X = raw_c68k_flag_x; C68K.flag_I = raw_c68k_flag_i;
        C68K.flag_S = raw_c68k_flag_s; C68K.USP = raw_c68k_usp;
        C68k_Set_Reg(&C68K, C68K_PC, raw_c68k_pc); C68K.HaltState = raw_c68k_halt;
        C68K.IRQLine = raw_c68k_irq_line; C68K.IRQState = raw_c68k_irq_state;
        C68K.ICount = raw_c68k_icount; C68K.ICountBk = raw_c68k_icount_bk;
        if (m68000_use_musashi)
        {
            /* Convert an older C68K-only save into the currently selected Musashi backend. */
            for (i = 0; i < 8; i++)
            {
                m68k_set_reg((m68k_register_t)(M68K_REG_D0 + i), C68K.D[i]);
                m68k_set_reg((m68k_register_t)(M68K_REG_A0 + i), C68K.A[i]);
            }
            m68k_set_reg(M68K_REG_SR, C68k_Get_Reg(&C68K, C68K_SR));
            m68k_set_reg(M68K_REG_USP, C68k_Get_Reg(&C68K, C68K_USP));
            m68k_set_reg(M68K_REG_ISP, C68k_Get_Reg(&C68K, C68K_MSP));
            m68k_set_reg(M68K_REG_A7, C68K.A[7]);
            m68k_set_reg(M68K_REG_PC, C68k_Get_Reg(&C68K, C68K_PC));
            m68ki_cpu.int_level = (C68K.IRQState == CLEAR_LINE) ? 0u : (uint32_t)C68K.IRQLine;
            m68ki_cpu.stopped = C68K.HaltState ? STOP_LEVEL_STOP : 0;
            m68ki_remaining_cycles = 0;
        }
        return 1;
    }

    /* Backward compatibility for old Musashi-only save states. */
    {
        uint32_t tmp32[20] = {0};
        uint32_t c_flag = 0, v_flag = 0, not_z_flag = 0, n_flag = 0;
        uint32_t x_flag = 0, m_flag = 0, s_flag = 0, int_level = 0, stopped = 0;
        int32_t remaining_cycles = 0;
        SFORMAT LegacyMusashiRegs[] = {
            SFVARN(tmp32[0], "M68K_REG_D0"), SFVARN(tmp32[1], "M68K_REG_D1"),
            SFVARN(tmp32[2], "M68K_REG_D2"), SFVARN(tmp32[3], "M68K_REG_D3"),
            SFVARN(tmp32[4], "M68K_REG_D4"), SFVARN(tmp32[5], "M68K_REG_D5"),
            SFVARN(tmp32[6], "M68K_REG_D6"), SFVARN(tmp32[7], "M68K_REG_D7"),
            SFVARN(tmp32[8], "M68K_REG_A0"), SFVARN(tmp32[9], "M68K_REG_A1"),
            SFVARN(tmp32[10], "M68K_REG_A2"), SFVARN(tmp32[11], "M68K_REG_A3"),
            SFVARN(tmp32[12], "M68K_REG_A4"), SFVARN(tmp32[13], "M68K_REG_A5"),
            SFVARN(tmp32[14], "M68K_REG_A6"), SFVARN(tmp32[15], "M68K_REG_A7"),
            SFVARN(tmp32[16], "M68K_REG_PC"), SFVARN(tmp32[17], "M68K_REG_SR"),
            SFVARN(tmp32[18], "M68K_REG_USP"), SFVARN(tmp32[19], "M68K_REG_ISP"),
            SFVARN(c_flag, "m68ki_cpu.c_flag"), SFVARN(v_flag, "m68ki_cpu.v_flag"),
            SFVARN(not_z_flag, "m68ki_cpu.not_z_flag"), SFVARN(n_flag, "m68ki_cpu.n_flag"),
            SFVARN(x_flag, "m68ki_cpu.x_flag"), SFVARN(m_flag, "m68ki_cpu.m_flag"),
            SFVARN(s_flag, "m68ki_cpu.s_flag"), SFVARN(int_level, "m68ki_cpu.int_level"),
            SFVARN(stopped, "m68ki_cpu.stopped"),
            SFVARN(remaining_cycles, "m68ki_remaining_cycles"), SFEND
        };
        ret = PX68KSS_StateAction(sm, 1, data_only, LegacyMusashiRegs, "MUSASHI_CPU", false);
        if (!ret) return 0;
        for (int i = 0; i < 8; i++)
        {
            m68k_set_reg((m68k_register_t)(M68K_REG_D0 + i), tmp32[i]);
            m68k_set_reg((m68k_register_t)(M68K_REG_A0 + i), tmp32[8 + i]);
        }
        m68k_set_reg(M68K_REG_SR, tmp32[17]); m68k_set_reg(M68K_REG_USP, tmp32[18]);
        m68k_set_reg(M68K_REG_ISP, tmp32[19]); m68k_set_reg(M68K_REG_A7, tmp32[15]);
        m68k_set_reg(M68K_REG_PC, tmp32[16]);
        m68ki_cpu.c_flag = c_flag; m68ki_cpu.v_flag = v_flag; m68ki_cpu.not_z_flag = not_z_flag;
        m68ki_cpu.n_flag = n_flag; m68ki_cpu.x_flag = x_flag; m68ki_cpu.m_flag = m_flag;
        m68ki_cpu.s_flag = s_flag; m68ki_cpu.int_level = int_level; m68ki_cpu.stopped = stopped;
        m68ki_remaining_cycles = remaining_cycles;
        if (!m68000_use_musashi)
        {
            for (int i = 0; i < 8; i++)
            {
                C68k_Set_Reg(&C68K, C68K_D0 + i, tmp32[i]);
                C68k_Set_Reg(&C68K, C68K_A0 + i, tmp32[8 + i]);
            }
            C68k_Set_Reg(&C68K, C68K_SR, tmp32[17]);
            C68k_Set_Reg(&C68K, C68K_USP, tmp32[18]);
            C68k_Set_Reg(&C68K, C68K_MSP, tmp32[19]);
            C68k_Set_Reg(&C68K, C68K_A7, tmp32[15]); C68k_Set_Reg(&C68K, C68K_PC, tmp32[16]);
            C68K.IRQLine = (int32_t)int_level; C68K.IRQState = int_level ? HOLD_LINE : CLEAR_LINE;
            C68K.HaltState = stopped ? 1u : 0u; C68K.ICount = 0; C68K.ICountBk = 0;
        }
        return 1;
    }
}
#endif

int m68000_StateAction(StateMem *sm, int load, int data_only)
{
#ifdef HAVE_CPU_RUNTIME
    return m68000_RuntimeStateAction(sm, load, data_only);
#endif
#ifdef HAVE_C68K
	int ret = 0;
	uint32_t pc = 0;
	SFORMAT StateRegs[] = 
	{
		SFVARN(C68K.D[0], "D0"),
		SFVARN(C68K.D[1], "D1"),
		SFVARN(C68K.D[2], "D2"),
		SFVARN(C68K.D[3], "D3"),
		SFVARN(C68K.D[4], "D4"),
		SFVARN(C68K.D[5], "D5"),
		SFVARN(C68K.D[6], "D6"),
		SFVARN(C68K.D[7], "D7"),

		SFVARN(C68K.A[0], "A0"),
		SFVARN(C68K.A[1], "A1"),
		SFVARN(C68K.A[2], "A2"),
		SFVARN(C68K.A[3], "A3"),
		SFVARN(C68K.A[4], "A4"),
		SFVARN(C68K.A[5], "A5"),
		SFVARN(C68K.A[6], "A6"),
		SFVARN(C68K.A[7], "A7"),

		SFVARN(C68K.flag_C, "flag_C"),
		SFVARN(C68K.flag_V, "flag_V"),
		SFVARN(C68K.flag_notZ, "flag_notZ"),
		SFVARN(C68K.flag_X, "flag_X"),
		SFVARN(C68K.flag_I, "flag_I"),
		SFVARN(C68K.flag_S, "flag_S"),

		SFVARN(C68K.USP, "USP"),

		SFVARN(pc, "PC"),

		SFVARN(C68K.Status, "status"),
		SFVARN(C68K.IRQLine, "IRQLine"),

		SFVARN(C68K.CycleToDo, "CycleToDo"),
		SFVARN(C68K.CycleIO, "CycleIO"),
		SFVARN(C68K.CycleSup, "CycleSup"),
		SFVARN(C68K.dirty1, "dirtyflag"),

		SFEND
	};

	if (!load)
		pc = m68000_get_reg(M68K_PC);

	ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_CPU", false);

	if (load)
		m68000_set_reg(M68K_PC, pc);

	return ret;
#endif
#ifdef HAVE_MUSASHI
	int ret = 0;
	uint32_t tmp32[20];

	SFORMAT StateRegs[] =
	{
		SFVARN(tmp32[0], "M68K_REG_D0"),
		SFVARN(tmp32[1], "M68K_REG_D1"),
		SFVARN(tmp32[2], "M68K_REG_D2"),
		SFVARN(tmp32[3], "M68K_REG_D3"),
		SFVARN(tmp32[4], "M68K_REG_D4"),
		SFVARN(tmp32[5], "M68K_REG_D5"),
		SFVARN(tmp32[6], "M68K_REG_D6"),
		SFVARN(tmp32[7], "M68K_REG_D7"),
		SFVARN(tmp32[8], "M68K_REG_A0"),
		SFVARN(tmp32[9], "M68K_REG_A1"),
		SFVARN(tmp32[10], "M68K_REG_A2"),
		SFVARN(tmp32[11], "M68K_REG_A3"),
		SFVARN(tmp32[12], "M68K_REG_A4"),
		SFVARN(tmp32[13], "M68K_REG_A5"),
		SFVARN(tmp32[14], "M68K_REG_A6"),
		SFVARN(tmp32[15], "M68K_REG_A7"),
		SFVARN(tmp32[16], "M68K_REG_PC"),
		SFVARN(tmp32[17], "M68K_REG_SR"),
		SFVARN(tmp32[18], "M68K_REG_USP"),
		SFVARN(tmp32[19], "M68K_REG_ISP"),

		SFVAR(m68ki_cpu.c_flag),
		SFVAR(m68ki_cpu.v_flag),
		SFVAR(m68ki_cpu.not_z_flag),
		SFVAR(m68ki_cpu.n_flag),
		SFVAR(m68ki_cpu.x_flag),
		SFVAR(m68ki_cpu.m_flag),
		SFVAR(m68ki_cpu.s_flag),
		
		SFVAR(m68ki_cpu.int_level),
		SFVAR(m68ki_cpu.stopped),
		SFVAR(m68ki_remaining_cycles),

		SFEND
	};

	if (!load)
	{
		tmp32[0] = m68k_get_reg(NULL, M68K_REG_D0);
		tmp32[1] = m68k_get_reg(NULL, M68K_REG_D1);
		tmp32[2] = m68k_get_reg(NULL, M68K_REG_D2);
		tmp32[3] = m68k_get_reg(NULL, M68K_REG_D3);
		tmp32[4] = m68k_get_reg(NULL, M68K_REG_D4);
		tmp32[5] = m68k_get_reg(NULL, M68K_REG_D5);
		tmp32[6] = m68k_get_reg(NULL, M68K_REG_D6);
		tmp32[7] = m68k_get_reg(NULL, M68K_REG_D7);
		tmp32[8] = m68k_get_reg(NULL, M68K_REG_A0);
		tmp32[9] = m68k_get_reg(NULL, M68K_REG_A1);
		tmp32[10] = m68k_get_reg(NULL, M68K_REG_A2);
		tmp32[11] = m68k_get_reg(NULL, M68K_REG_A3);
		tmp32[12] = m68k_get_reg(NULL, M68K_REG_A4);
		tmp32[13] = m68k_get_reg(NULL, M68K_REG_A5);
		tmp32[14] = m68k_get_reg(NULL, M68K_REG_A6);
		tmp32[15] = m68k_get_reg(NULL, M68K_REG_A7);
		tmp32[16] = m68k_get_reg(NULL, M68K_REG_PC);
		tmp32[17] = m68k_get_reg(NULL, M68K_REG_SR);
		tmp32[18] = m68k_get_reg(NULL, M68K_REG_USP);
		tmp32[19] = m68k_get_reg(NULL, M68K_REG_ISP);
	}

	ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "MUSASHI_CPU", false);

	if (load)
	{
		m68k_set_reg(M68K_REG_D0, tmp32[0]);
		m68k_set_reg(M68K_REG_D1, tmp32[1]);
		m68k_set_reg(M68K_REG_D2, tmp32[2]);
		m68k_set_reg(M68K_REG_D3, tmp32[3]);
		m68k_set_reg(M68K_REG_D4, tmp32[4]);
		m68k_set_reg(M68K_REG_D5, tmp32[5]);
		m68k_set_reg(M68K_REG_D6, tmp32[6]);
		m68k_set_reg(M68K_REG_D7, tmp32[7]);
		m68k_set_reg(M68K_REG_A0, tmp32[8]);
		m68k_set_reg(M68K_REG_A1, tmp32[9]);
		m68k_set_reg(M68K_REG_A2, tmp32[10]);
		m68k_set_reg(M68K_REG_A3, tmp32[11]);
		m68k_set_reg(M68K_REG_A4, tmp32[12]);
		m68k_set_reg(M68K_REG_A5, tmp32[13]);
		m68k_set_reg(M68K_REG_A6, tmp32[14]);
		m68k_set_reg(M68K_REG_A7, tmp32[15]);
		m68k_set_reg(M68K_REG_PC, tmp32[16]);
		m68k_set_reg(M68K_REG_SR, tmp32[17]);
		m68k_set_reg(M68K_REG_USP, tmp32[18]);
		m68k_set_reg(M68K_REG_ISP, tmp32[19]);
	};

	return ret;
#endif
}

#if defined (HAVE_CYCLONE)

unsigned int read8(unsigned int a) {
	return (unsigned int) cpu_readmem24(a);
}

unsigned int read16(unsigned int a) {
	return (unsigned int) cpu_readmem24_word(a);
}

unsigned int MyCheckPc(unsigned int pc)
{
  pc-= m68k.membase; /* Get the real program counter */
  if (pc <= 0xbfffff) 			       					{ m68k.membase=(int) MEM; return m68k.membase+pc; }
  if ((pc >= 0xfc0000) && (pc <= 0xffffff))	{ m68k.membase=(int) IPL - 0xfc0000; return m68k.membase+pc; }
  if ((pc >= 0xc00000) && (pc <= 0xc7ffff)) m68k.membase=(int) GVRAM - 0xc00000;
  if ((pc >= 0xe00000) && (pc <= 0xe7ffff))	m68k.membase=(int) TVRAM - 0xe00000;
  if ((pc >= 0xea0000) && (pc <= 0xea1fff))	m68k.membase=(int) SCSIIPL - 0xea0000;
  if ((pc >= 0xed0000) && (pc <= 0xed3fff))	m68k.membase=(int) SRAM - 0xed0000;
  if ((pc >= 0xf00000) && (pc <= 0xfbffff))	m68k.membase=(int) FONT - 0xf00000;
  return m68k.membase+pc; /* New program counter */
}

#elif defined (HAVE_MUSASHI) || defined (HAVE_CPU_RUNTIME)
uint32_t m68k_read_memory_8(uint32_t address)
{
	return (uint32_t) cpu_readmem24(address);
}

void m68k_write_memory_8(uint32_t address, uint32_t data)
{
	cpu_writemem24(address, data);
}

uint32_t m68k_read_memory_16(uint32_t address)
{
	return (uint32_t) cpu_readmem24_word(address);
}

void m68k_write_memory_16(uint32_t address, uint32_t data)
{
	cpu_writemem24_word(address, data);
}

uint32_t m68k_read_memory_32(uint32_t address)
{
	return (uint32_t) cpu_readmem24_dword(address);
}

void m68k_write_memory_32(uint32_t address, uint32_t data)
{
	cpu_writemem24_dword(address, data);
}

#endif /* HAVE_CYCLONE */ /* HAVE_MUSASHI */


/******************************************************************************
	M68000インタフェース関数
******************************************************************************/

/*--------------------------------------------------------
	CPU初期化
--------------------------------------------------------*/
#if !defined (HAVE_MUSASHI) || defined (HAVE_CPU_RUNTIME)
int32_t my_irqh_callback(int32_t level);
#endif

void m68000_init(void)
{
#ifdef HAVE_CPU_RUNTIME
    C68k_Init(&C68K, my_irqh_callback);
    C68k_Set_ReadB(&C68K, cpu_readmem24);
    C68k_Set_ReadW(&C68K, cpu_readmem24_word);
    C68k_Set_WriteB(&C68K, cpu_writemem24);
    C68k_Set_WriteW(&C68K, cpu_writemem24_word);
    C68k_Set_Fetch(&C68K, 0x000000, 0xbfffff, (uintptr_t)MEM);
    C68k_Set_Fetch(&C68K, 0xc00000, 0xc7ffff, (uintptr_t)GVRAM);
    C68k_Set_Fetch(&C68K, 0xe00000, 0xe7ffff, (uintptr_t)TVRAM);
    C68k_Set_Fetch(&C68K, 0xea0000, 0xea1fff, (uintptr_t)SCSIIPL);
    C68k_Set_Fetch(&C68K, 0xed0000, 0xed3fff, (uintptr_t)SRAM);
    C68k_Set_Fetch(&C68K, 0xf00000, 0xfbffff, (uintptr_t)FONT);
    C68k_Set_Fetch(&C68K, 0xfc0000, 0xffffff, (uintptr_t)IPL);
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_init();
    return;
#elif defined (HAVE_CYCLONE)

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

#elif defined (HAVE_C68K)
    C68k_Init(&C68K, my_irqh_callback);
    C68k_Set_ReadB(&C68K, cpu_readmem24);
    C68k_Set_ReadW(&C68K, cpu_readmem24_word);
    C68k_Set_WriteB(&C68K, cpu_writemem24);
    C68k_Set_WriteW(&C68K, cpu_writemem24_word);
	C68k_Set_Fetch(&C68K, 0x000000, 0xbfffff, (uintptr_t)MEM);
    C68k_Set_Fetch(&C68K, 0xc00000, 0xc7ffff, (uintptr_t)GVRAM);
    C68k_Set_Fetch(&C68K, 0xe00000, 0xe7ffff, (uintptr_t)TVRAM);
    C68k_Set_Fetch(&C68K, 0xea0000, 0xea1fff, (uintptr_t)SCSIIPL);
    C68k_Set_Fetch(&C68K, 0xed0000, 0xed3fff, (uintptr_t)SRAM);
    C68k_Set_Fetch(&C68K, 0xf00000, 0xfbffff, (uintptr_t)FONT);
    C68k_Set_Fetch(&C68K, 0xfc0000, 0xffffff, (uintptr_t)IPL);
#elif defined (HAVE_MUSASHI)
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_init();
#endif /* HAVE_C68K */ /* HAVE_MUSASHI */
}


/*--------------------------------------------------------
	CPUリセット
--------------------------------------------------------*/

void m68000_reset(void)
{
#ifdef HAVE_CPU_RUNTIME
    if (m68000_use_musashi) m68k_pulse_reset(); else C68k_Reset(&C68K);
    return;
#elif defined (HAVE_CYCLONE)
	CycloneReset(&m68k);
	m68k.state_flags = 0; /* Go to default state (not stopped, halted, etc.) */
	m68k.srh = 0x27; /* Set supervisor mode */
#elif defined (HAVE_C68K)
	C68k_Reset(&C68K);
#elif defined (HAVE_MUSASHI)
	m68k_pulse_reset();
#endif /* HAVE_C68K */ /* HAVE_MUSASHI */
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
#ifdef HAVE_CPU_RUNTIME
    return m68000_use_musashi ? m68k_execute(cycles) : C68k_Exec(&C68K, cycles);
#elif defined (HAVE_CYCLONE)
	m68k.cycles = cycles;
	CycloneRun(&m68k);
	return m68k.cycles ;
#elif defined (HAVE_C68K)
	return C68k_Exec(&C68K, cycles);
#elif defined (HAVE_MUSASHI)
        return m68k_execute(cycles);
#endif /* HAVE_C68K */ /* HAVE_MUSASHI */
}



/*--------------------------------------------------------
	割り込み処理
--------------------------------------------------------*/

void m68000_set_irq_line(int irqline, int state)
{
#ifdef HAVE_CPU_RUNTIME
    (void)state;
    if (m68000_use_musashi) m68k_set_irq((unsigned int)irqline);
    else C68k_Set_IRQ(&C68K, (int32_t)irqline, irqline ? HOLD_LINE : CLEAR_LINE);
    return;
#elif defined (HAVE_CYCLONE)
	m68k.irq = irqline;
#elif defined (HAVE_C68K)
	C68k_Set_IRQ(&C68K, irqline);
#elif defined (HAVE_MUSASHI)
	m68k_set_irq(irqline);
#endif /* HAVE_C68K */ /* HAVE_MUSASHI */
}

/*--------------------------------------------------------
	レジスタ取得
--------------------------------------------------------*/

uint32_t m68000_get_reg(int regnum)
{
#ifdef HAVE_CPU_RUNTIME
    if (m68000_use_musashi) {
        switch (regnum) {
        case M68K_PC: return m68k_get_reg(NULL, M68K_REG_PC);
        case M68K_USP: return m68k_get_reg(NULL, M68K_REG_USP);
        case M68K_MSP: return m68k_get_reg(NULL, M68K_REG_MSP);
        case M68K_SR: return m68k_get_reg(NULL, M68K_REG_SR);
        default:
            if (regnum >= M68K_D0 && regnum <= M68K_D7) return m68k_get_reg(NULL, M68K_REG_D0 + regnum - M68K_D0);
            if (regnum >= M68K_A0 && regnum <= M68K_A7) return m68k_get_reg(NULL, M68K_REG_A0 + regnum - M68K_A0);
            return 0;
        }
    }
#endif
#if defined (HAVE_CYCLONE)
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
#elif defined (HAVE_C68K) || defined (HAVE_CPU_RUNTIME)
	switch (regnum)
	{
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

	default: return 0;
	}
#elif defined (HAVE_MUSASHI)
	switch (regnum)
	{
	case M68K_PC:  return m68k_get_reg(NULL, M68K_REG_PC);
	case M68K_USP: return m68k_get_reg(NULL, M68K_REG_USP);
	case M68K_MSP: return m68k_get_reg(NULL, M68K_REG_MSP);
	case M68K_SR:  return m68k_get_reg(NULL, M68K_REG_SR);
	case M68K_D0:  return m68k_get_reg(NULL, M68K_REG_D0);
	case M68K_D1:  return m68k_get_reg(NULL, M68K_REG_D1);
	case M68K_D2:  return m68k_get_reg(NULL, M68K_REG_D2);
	case M68K_D3:  return m68k_get_reg(NULL, M68K_REG_D3);
	case M68K_D4:  return m68k_get_reg(NULL, M68K_REG_D4);
	case M68K_D5:  return m68k_get_reg(NULL, M68K_REG_D5);
	case M68K_D6:  return m68k_get_reg(NULL, M68K_REG_D6);
	case M68K_D7:  return m68k_get_reg(NULL, M68K_REG_D7);
	case M68K_A0:  return m68k_get_reg(NULL, M68K_REG_A0);
	case M68K_A1:  return m68k_get_reg(NULL, M68K_REG_A1);
	case M68K_A2:  return m68k_get_reg(NULL, M68K_REG_A2);
	case M68K_A3:  return m68k_get_reg(NULL, M68K_REG_A3);
	case M68K_A4:  return m68k_get_reg(NULL, M68K_REG_A4);
	case M68K_A5:  return m68k_get_reg(NULL, M68K_REG_A5);
	case M68K_A6:  return m68k_get_reg(NULL, M68K_REG_A6);
	case M68K_A7:  return m68k_get_reg(NULL, M68K_REG_A7);

	default: return 0;
	}
#endif /* HAVE_C68K */ /* HAVE_MUSASHI */
}


/*--------------------------------------------------------
	レジスタ設定
--------------------------------------------------------*/

void m68000_set_reg(int regnum, uint32_t val)
{
#ifdef HAVE_CPU_RUNTIME
    if (m68000_use_musashi) {
        switch (regnum) {
        case M68K_PC: m68k_set_reg(M68K_REG_PC, val); break;
        case M68K_USP: m68k_set_reg(M68K_REG_USP, val); break;
        case M68K_MSP: m68k_set_reg(M68K_REG_MSP, val); break;
        case M68K_SR: m68k_set_reg(M68K_REG_SR, val); break;
        default:
            if (regnum >= M68K_D0 && regnum <= M68K_D7) m68k_set_reg(M68K_REG_D0 + regnum - M68K_D0, val);
            else if (regnum >= M68K_A0 && regnum <= M68K_A7) m68k_set_reg(M68K_REG_A0 + regnum - M68K_A0, val);
            break;
        }
        return;
    }
#endif
#if defined (HAVE_CYCLONE)
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
#elif defined (HAVE_C68K) || defined (HAVE_CPU_RUNTIME)
	switch (regnum)
	{
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
	default: break;
	}
#elif defined (HAVE_MUSASHI)
	switch (regnum)
	{
	case M68K_PC:  m68k_set_reg(M68K_REG_PC, val); break;
	case M68K_USP: m68k_set_reg(M68K_REG_USP, val); break;
	case M68K_MSP: m68k_set_reg(M68K_REG_MSP, val); break;
	case M68K_SR:  m68k_set_reg(M68K_REG_SR, val); break;
	case M68K_D0:  m68k_set_reg(M68K_REG_D0, val); break;
	case M68K_D1:  m68k_set_reg(M68K_REG_D1, val); break;
	case M68K_D2:  m68k_set_reg(M68K_REG_D2, val); break;
	case M68K_D3:  m68k_set_reg(M68K_REG_D3, val); break;
	case M68K_D4:  m68k_set_reg(M68K_REG_D4, val); break;
	case M68K_D5:  m68k_set_reg(M68K_REG_D5, val); break;
	case M68K_D6:  m68k_set_reg(M68K_REG_D6, val); break;
	case M68K_D7:  m68k_set_reg(M68K_REG_D7, val); break;
	case M68K_A0:  m68k_set_reg(M68K_REG_A0, val); break;
	case M68K_A1:  m68k_set_reg(M68K_REG_A1, val); break;
	case M68K_A2:  m68k_set_reg(M68K_REG_A2, val); break;
	case M68K_A3:  m68k_set_reg(M68K_REG_A3, val); break;
	case M68K_A4:  m68k_set_reg(M68K_REG_A4, val); break;
	case M68K_A5:  m68k_set_reg(M68K_REG_A5, val); break;
	case M68K_A6:  m68k_set_reg(M68K_REG_A6, val); break;
	case M68K_A7:  m68k_set_reg(M68K_REG_A7, val); break;
	default: break;
	}

#endif /* HAVE_C68K */ /* HAVE_MUSASHI */
}


/*------------------------------------------------------
	セーブ/ロード ステート
------------------------------------------------------*/

#ifdef SAVE_STATE

STATE_SAVE( m68000 )
{
#if defined (HAVE_CYCLONE)
/* empty */
#elif defined (HAVE_C68K)
	int i;
	uint32_t pc = C68k_Get_Reg(&C68K, C68K_PC);

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
#endif /* HAVE_C68K */
}

STATE_LOAD( m68000 )
{
#if defined (HAVE_CYCLONE)
/* empty */
#elif defined (HAVE_C68K)
	int i;
	uint32_t pc;

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
#endif /* HAVE_C68K */
}

#endif /* SAVE_STATE */
