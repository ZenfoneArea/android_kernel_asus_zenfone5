/*COPYRIGHT**
    Copyright (C) 2005-2013 Intel Corporation.  All Rights Reserved.

    This file is part of SEP Development Kit

    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
**COPYRIGHT*/

#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/fs.h>
#if defined(DRV_IA32) || defined(DRV_EM64T)
#include <asm/msr.h>
#endif
#include <linux/ptrace.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv.h"
#if defined(DRV_IA32) || defined(DRV_EM64T)
#include "core2.h"
#include "silvermont.h"
#if !defined (DRV_ATOM_ONLY)
#include "core.h"
#include "corei7_unc.h"
#include "snbunc_cbo.h"
#include "snbunc_imc.h"
#if !defined (DRV_BIGCORE)
#include "wsmexunc_imc.h"
#include "wsmexunc_qpi.h"
#include "wsmexunc_wbox.h"
#include "jktunc_imc.h"
#include "jktunc_qpill.h"
#include "jaketown_ubox.h"
#endif
#include "haswellunc_ncu.h"
#if !defined (DRV_BIGCORE)
#include "ivtunc_cbo.h"
#include "ivtunc_imc.h"
#include "ivytown_pcu.h"
#include "ivytown_ha.h"
#include "ivytown_qpill.h"
#include "ivytown_r3qpi.h"
#include "ivytown_ubox.h"
#include "ivytown_r2pcie.h"
#endif
#endif
#endif
#if defined(DRV_IA64)
#include "montecito.h"
#include "poulson.h"
#endif
#include "utility.h"
#if defined(BUILD_CHIPSET)
#include "lwpmudrv_chipset.h"
#include "chap.h"
#include "gmch.h"
#endif

volatile int config_done;

#if defined(BUILD_CHIPSET)
extern CHIPSET_CONFIG pma;
#endif

#if defined(DRV_IA64)

#define PMV_MASK_BIT            0x10000    // pmv.m  0 = unmask counter overflow interrrupts

extern U64
UTILITY_Read_PMV (
    void
)
{
    U64 r;

    __asm__("mov %0=cr.pmv":"=r"(r));

    return r;
}

extern VOID
UTILITY_Set_PMV_Mask (
    VOID
)
{
    U64 pmv;

    pmv = SYS_Read_PMV();
    pmv |= PMV_MASK_BIT;
    SYS_Write_PMV(pmv);

    return;
}

extern VOID
UTILITY_Clear_PMV_Mask (
    VOID
)
{
    U64 pmv;

    pmv = SYS_Read_PMV();
    pmv &= ~PMV_MASK_BIT;
    SYS_Write_PMV(pmv);

    return;
}

/*
 *  History:
 *     Inheritied from sampling 1, which "adapted from Perfmon2 routines in
 *     /usr/src/linux/arch/ia64/kernel/perfmon.c".
 */
extern void
UTILITY_Set_PSR_PP (
    void
)
{
    ia64_ssm(IA64_PSR_PP);
    ia64_srlz_i();

    return;
}

extern void
UTILITY_Clear_PSR_PP (
    void
)
{
    ia64_rsm(IA64_PSR_PP);
    ia64_srlz_i();

    return;
}

extern void
UTILITY_Set_PSR_UP (
    void
)
{
    ia64_ssm(IA64_PSR_UP);
    ia64_srlz_i();

    return;
}

extern void
UTILITY_Clear_PSR_UP (
    void
)
{
    ia64_rsm(IA64_PSR_UP);
    ia64_srlz_i();

    return;
}

extern void
UTILITY_Set_DCR_PP (
    void
)
{
    ia64_setreg(_IA64_REG_CR_DCR, ia64_getreg(_IA64_REG_CR_DCR) | IA64_DCR_PP);
    ia64_srlz_i();

    return;
}

extern void
UTILITY_Clear_DCR_PP (
    void
)
{
    ia64_setreg(_IA64_REG_CR_DCR, ia64_getreg(_IA64_REG_CR_DCR) & ~IA64_DCR_PP);
    ia64_srlz_i();

    return;
}

#endif

extern DRV_BOOL
UTILITY_down_read_mm (
    struct task_struct *p
)
{
#ifdef SUPPORTS_MMAP_READ
    mmap_down_read(p->mm);
#else
    down_read((struct rw_semaphore *) &p->mm->mmap_sem);
#endif
    return TRUE;
}

extern VOID
UTILITY_up_read_mm (
    struct task_struct *p
)
{
#ifdef SUPPORTS_MMAP_READ
    mmap_up_read(p->mm);
#else
    up_read((struct rw_semaphore *) &p->mm->mmap_sem);
#endif

    return;
}

#if defined(DRV_IA64)
static __inline__ unsigned long
itp_get_itc (
    void
)
{
    unsigned long result;

    __asm__ __volatile__("mov %0=ar.itc":"=r"(result)::"memory");

    return result;
}
#endif

extern VOID
UTILITY_Read_TSC (
    U64* pTsc
)
{
#if defined(DRV_IA32) || defined(DRV_EM64T)
    rdtscll(*(pTsc));
#else
    *(pTsc) = itp_get_itc();
#endif

    return;
}

#if defined(DRV_IA32) || defined(DRV_EM64T)
/* ------------------------------------------------------------------------- */
/*!
 * @fn       VOID UTILITY_Read_Cpuid
 *
 * @brief    executes the cpuid_function of cpuid and returns values
 *
 * @param  IN   cpuid_function
 *         OUT  rax  - results of the cpuid instruction in the
 *         OUT  rbx  - corresponding registers
 *         OUT  rcx
 *         OUT  rdx
 *
 * @return   none
 *
 * <I>Special Notes:</I>
 *              <NONE>
 *
 */
extern VOID
UTILITY_Read_Cpuid (
    U64   cpuid_function,
    U64  *rax_value,
    U64  *rbx_value,
    U64  *rcx_value,
    U64  *rdx_value
)
{
    U32 function = (U32) cpuid_function;
    U32 *eax     = (U32 *) rax_value;
    U32 *ebx     = (U32 *) rbx_value;
    U32 *ecx     = (U32 *) rcx_value;
    U32 *edx     = (U32 *) rdx_value;

    *eax = function;

    __asm__("cpuid"
            : "=a" (*eax),
              "=b" (*ebx),
              "=c" (*ecx),
              "=d" (*edx)
            : "a"  (function),
              "b"  (*ebx),
              "c"  (*ecx),
              "d"  (*edx));

    return;
}
#endif

/* ------------------------------------------------------------------------- */
/*!
 * @fn       VOID UTILITY_Configure_CPU
 *
 * @brief    Reads the CPU information from the hardware
 *
 * @param    param   dispatch_id -  The id of the dispatch table.
 *
 * @return   Pointer to the correct dispatch table for the CPU architecture
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
extern  DISPATCH
UTILITY_Configure_CPU (
    U32 dispatch_id
)
{
    DISPATCH     dispatch = NULL;
    switch (dispatch_id) {
#if defined(DRV_IA32) && !defined(DRV_ATOM_ONLY)
        case 0:
            SEP_PRINT_DEBUG("Set up the Core(TM) processor dispatch table\n");
            dispatch = &core_dispatch;
            break;
#endif
#if defined(DRV_IA32) || defined(DRV_EM64T)
        case 1:
            SEP_PRINT_DEBUG("Set up the Core(TM)2 processor dispatch table\n");
            dispatch = &core2_dispatch;
            break;
        case 6:
            SEP_PRINT_DEBUG("Set up the Silvermont dispatch table\n");
            dispatch = &silvermont_dispatch;
            break;

#if !defined(DRV_ATOM_ONLY)
        case 2:
            dispatch = &corei7_dispatch;
            SEP_PRINT_DEBUG("Set up the Core i7(TM) processor dispatch table\n");
            break;
        case 3:
            SEP_PRINT_DEBUG("Set up the Core i7(TM) dispatch table\n");
            dispatch = &corei7_dispatch_htoff_mode;
            break;
        case 4:
            dispatch = &corei7_dispatch_2;
            SEP_PRINT_DEBUG("Set up the Sandybridge processor dispatch table\n");
            break;
        case 5:
            SEP_PRINT_DEBUG("Set up the Sandybridge dispatch table\n");
            dispatch = &corei7_dispatch_htoff_mode_2;
            break;
        case 100:
            SEP_PRINT_DEBUG("Set up the Core i7 uncore dispatch table\n");
            dispatch = &corei7_unc_dispatch;
            break;
        case 200:
            SEP_PRINT_DEBUG("Set up the SNB iMC dispatch table\n");
            dispatch = &snbunc_imc_dispatch;
            break;
        case 201:
            SEP_PRINT_DEBUG("Set up the SNB Cbo dispatch table\n");
            dispatch = &snbunc_cbo_dispatch;
            break;
#if !defined (DRV_BIGCORE)
        case 210:
            SEP_PRINT_DEBUG("Set up the WSM-EX iMC dispatch table\n");
            dispatch = &wsmexunc_imc_dispatch;
            break;
        case 211:
            SEP_PRINT_DEBUG("Set up the WSM-EX QPI dispatch table\n");
            dispatch = &wsmexunc_qpi_dispatch;
            break;
        case 212:
            SEP_PRINT_DEBUG("Set up the WSM-EX WBOX dispatch table\n");
            dispatch = &wsmexunc_wbox_dispatch;
            break;
        case 220:
            SEP_PRINT_DEBUG("Set up the JKT IMC dispatch table\n");
            dispatch = &jktunc_imc_dispatch;
            break;
        case 221:
            SEP_PRINT_DEBUG("Set up the JKT QPILL dispatch table\n");
            dispatch = &jktunc_qpill_dispatch;
            break;
        case 222:
            SEP_PRINT_DEBUG("Set up the Jaketown UBOX dispatch table\n");
            dispatch = &jaketown_ubox_dispatch;
            break;
#endif            
        case 500:
            SEP_PRINT_DEBUG("Set up the Haswell UNC NCU dispatch table\n");
            dispatch = &haswellunc_ncu_dispatch;
            break;
#if !defined (DRV_BIGCORE)
        case 600:
            SEP_PRINT_DEBUG("Set up the IVT UNC CBO dispatch table\n");
            dispatch = &ivtunc_cbo_dispatch;
            break;
        case 610:
            SEP_PRINT_DEBUG("Set up the IVT UNC IMC dispatch table\n");
            dispatch = &ivtunc_imc_dispatch;
            break;
        case 620:
            SEP_PRINT("Set up the Ivytown UNC PCU dispatch table\n");
            dispatch = &ivytown_pcu_dispatch;
            break;
        case 630:
            SEP_PRINT("Set up the Ivytown UNC PCU dispatch table\n");
            dispatch = &ivytown_ha_dispatch;
            break;
        case 640:
            SEP_PRINT_DEBUG("Set up the Ivytown QPI dispatch table\n");
            dispatch = &ivytown_qpill_dispatch;
            break;
        case 650:
            SEP_PRINT_DEBUG("Set up the Ivytown R3QPI dispatch table\n");
            dispatch = &ivytown_r3qpi_dispatch;
            break;
        case 660:
            SEP_PRINT("Set up the Ivytown UNC UBOX dispatch table\n");
            dispatch = &ivytown_ubox_dispatch;
            break;
        case 670:
            SEP_PRINT("Set up the Ivytown UNC R2PCIe dispatch table\n");
            dispatch = &ivytown_r2pcie_dispatch;
            break;
#endif            
#endif
#endif
#if defined(DRV_IA64)
        case 4:
            dispatch = &montecito_dispatch;
            SEP_PRINT_DEBUG("Set up the Itanium(TM) Processor dispatch table\n");
            break;
        case 5:
            dispatch = &poulson_dispatch;
            SEP_PRINT_DEBUG("Set up the Itanium(TM) Processor dispatch table\n");
            break;
#endif
        default:
            dispatch = NULL;
            SEP_PRINT_ERROR("Architecture not supported (dispatch_id=%d)\n", dispatch_id);
            break;
    }

    return dispatch;
}

#if defined(DRV_IA32) || defined(DRV_EM64T)

extern U64
SYS_Read_MSR (
    U32   msr
)
{
    U64 val;

    rdmsrl(msr, val);

    return val;
}


#if defined(BUILD_CHIPSET)
/* ------------------------------------------------------------------------- */
/*!
 * @fn       VOID UTILITY_Configure_Chipset
 *
 * @brief    Configures the chipset information
 *
 * @param    none
 *
 * @return   none
 *
 * <I>Special Notes:</I>
 *              <NONE>
 */
extern  CS_DISPATCH
UTILITY_Configure_Chipset (
    void
)
{
    if (CHIPSET_CONFIG_gmch_chipset(pma)) {
#if !defined (DRV_BIGCORE)
        cs_dispatch = &gmch_dispatch;
        SEP_PRINT_DEBUG("UTLITY_Configure_Chipset: using GMCH dispatch table!\n");
#endif
    }
    else if (CHIPSET_CONFIG_mch_chipset(pma) || CHIPSET_CONFIG_ich_chipset(pma)) {
        cs_dispatch = &chap_dispatch;
        SEP_PRINT_DEBUG("UTLITY_Configure_Chipset: using CHAP dispatch table!\n");
    }
    else {
        SEP_PRINT_ERROR("UTLITY_Configure_Chipset: unable to map chipset dispatch table!\n");
    }

    SEP_PRINT_DEBUG("UTLITY_Configure_Chipset: exiting with cs_dispatch=0x%p\n", cs_dispatch);

    return cs_dispatch;
}

#endif

#endif
