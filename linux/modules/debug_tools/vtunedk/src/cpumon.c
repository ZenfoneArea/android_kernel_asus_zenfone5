/*COPYRIGHT**
    Copyright (C) 2005-2012 Intel Corporation.  All Rights Reserved.
 
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

/*
 *  CVS_Id="$Id$"
 */

#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/interrupt.h>
#if defined(DRV_EM64T)
#include <asm/desc.h>
#endif

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#if defined(DRV_IA32) || defined(DRV_EM64T)
#include "apic.h"
#endif
#include "lwpmudrv.h"
#include "control.h"
#include "utility.h"
#include "cpumon.h"

#if defined(DRV_IA64)
#if defined(PERFMON_V1) || defined(PERFMON_V2) || defined(PERFMON_V2_ALT)
#include <asm/hw_irq.h>
#include <asm/perfmon.h>
#endif
#include <linux/slab.h>
#include "pmi.h"

#if defined(PERFMON_V1) || defined(PERFMON_V2_ALT)
#define SEP_PERFMON_IRQ            IA64_PERFMON_VECTOR
#else
#define SEP_PERFMON_IRQ            0xED
#endif

static U32 ebs_irq = 0;
#if defined(PERFMON_V1) || defined(PERFMON_V2_ALT)
static pfm_intr_handler_desc_t desc;
#endif

#if defined(PERFMON_V1)
#define CPUMON_INSTALL_INTERRUPT(desc)    pfm_install_alternate_syswide_subsystem((desc))
#define CPUMON_REMOVE_INTERRUPT(desc)     pfm_remove_alternate_syswide_subsystem((desc))
#endif
#if defined(PERFMON_V2_ALT)
#define CPUMON_INSTALL_INTERRUPT(desc)    pfm_install_alt_pmu_interrupt((desc));
#define CPUMON_REMOVE_INTERRUPT(desc)     pfm_remove_alt_pmu_interrupt((desc))
#endif

#endif


/*
 * CPU Monitoring Functionality
 */


/*
 * General per-processor initialization
 */

#if defined(DRV_IA32)

typedef union {
    unsigned long long    u64[1];
    unsigned short int    u16[4];
} local_handler_t;

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Save_Cpu(param)
 *
 * @param    param    unused parameter
 *
 * @return   None     No return needed
 *
 * @brief  Save the old handler for restoration when done
 *
 */
static void 
cpumon_Save_Cpu (
    PVOID parm
)
{
    unsigned long        eflags;
    U64                 *idt_base;
    CPU_STATE            pcpu;

    preempt_disable();
    pcpu = &pcb[CONTROL_THIS_CPU()];
    preempt_enable();

    SYS_Local_Irq_Save(eflags);
    CPU_STATE_idt_base(pcpu) = idt_base = SYS_Get_IDT_Base();
    // save original perf. vector
    CPU_STATE_saved_ih(pcpu) = idt_base[CPU_PERF_VECTOR];
    SEP_PRINT_DEBUG("saved_ih is 0x%llx\n", CPU_STATE_saved_ih(pcpu));
    SYS_Local_Irq_Restore(eflags);
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Init_Cpu(param)
 *
 * @param    param    unused parameter
 *
 * @return   None     No return needed
 *
 * @brief  Set up the interrupt handler.  
 *
 */
static VOID 
cpumon_Init_Cpu (
    PVOID parm
)
{
    unsigned long        eflags;
    U64                 *idt_base;
    CPU_STATE            pcpu;
    local_handler_t      lhandler;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    unsigned long        cr0_value;
#endif

    preempt_disable();
    pcpu = &pcb[CONTROL_THIS_CPU()];
    preempt_enable();
    SYS_Local_Irq_Save(eflags);
    
    idt_base = CPU_STATE_idt_base(pcpu);
    // install perf. handler
    // These are the necessary steps to have an ISR entry
    // Note the changes in the data written
    lhandler.u64[0] = (unsigned long)SYS_Perfvec_Handler;
    lhandler.u16[3] = lhandler.u16[1];
    lhandler.u16[1] = SYS_Get_cs();
    lhandler.u16[2] = 0xee00;

    // From 3.10 kernel, the IDT memory has been moved to a read-only location
    // which is controlled by the bit 16 in the CR0 register.
    // The write protection should be temporarily released to update the IDT.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    cr0_value = read_cr0();
    write_cr0(cr0_value & ~X86_CR0_WP);
#endif
    idt_base[CPU_PERF_VECTOR] = lhandler.u64[0];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    write_cr0(cr0_value);
#endif

    SYS_Local_Irq_Restore(eflags);
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Destroy_Cpu(param)
 *
 * @param    param    unused parameter
 *
 * @return   None     No return needed
 *
 * @brief  Restore the old handler
 * @brief  Finish clean up of the apic
 *
 */
static VOID 
cpumon_Destroy_Cpu (
    PVOID ctx
)
{
    unsigned long        eflags;
    unsigned long long  *idt_base;
    CPU_STATE            pcpu;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    unsigned long        cr0_value;
#endif

    preempt_disable();
    pcpu = &pcb[CONTROL_THIS_CPU()];
    preempt_enable();

    SYS_Local_Irq_Save(eflags);
    // restore perf. vector (to a safe stub pointer)
    idt_base = SYS_Get_IDT_Base();
    APIC_Disable_PMI();

    // From 3.10 kernel, the IDT memory has been moved to a read-only location
    // which is controlled by the bit 16 in the CR0 register.
    // The write protection should be temporarily released to update the IDT.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    cr0_value = read_cr0();
    write_cr0(cr0_value & ~X86_CR0_WP);
#endif
    idt_base[CPU_PERF_VECTOR] = CPU_STATE_saved_ih(pcpu);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    write_cr0(cr0_value);
#endif

    SYS_Local_Irq_Restore(eflags);

    return;
}
#endif

#if defined(DRV_EM64T)
/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Set_IDT_Func(idt, func)
 *
 * @param  GATE_STRUCT*  - address of the idt vector
 * @param  PVOID         - function to set in IDT 
 *
 * @return None     No return needed
 *
 * @brief  Set up the interrupt handler.  
 * @brief  Save the old handler for restoration when done
 *
 */
static VOID
cpumon_Set_IDT_Func (
    GATE_STRUCT   *idt,
    PVOID          func
)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
    _set_gate(&idt[CPU_PERF_VECTOR], GATE_INTERRUPT, (unsigned long) func, 3, 0);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    unsigned long cr0_value;
#endif
    GATE_STRUCT  local;
    // _set_gate() cannot be used because the IDT table is not exported.

    pack_gate(&local, GATE_INTERRUPT, (unsigned long)func, 3, 0, __KERNEL_CS);

    // From 3.10 kernel, the IDT memory has been moved to a read-only location
    // which is controlled by the bit 16 in the CR0 register.
    // The write protection should be temporarily released to update the IDT.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    cr0_value = read_cr0();
    write_cr0(cr0_value & ~X86_CR0_WP);
#endif
    write_idt_entry((idt), CPU_PERF_VECTOR, &local);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    write_cr0(cr0_value);
#endif
#endif
    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Save_Cpu(param)
 *
 * @param  param - Unused, set up to enable parallel calls
 *
 * @return None     No return needed
 *
 * @brief  Set up the interrupt handler.  
 * @brief  Save the old handler for restoration when done
 *
 */
static VOID 
cpumon_Save_Cpu (
    PVOID parm
)
{
    unsigned long        eflags;
    IDTGDT_DESC          idt_base;
    CPU_STATE            pcpu = &pcb[CONTROL_THIS_CPU()];
    GATE_STRUCT          old_gate;
    GATE_STRUCT         *idt;

    SYS_Local_Irq_Save(eflags);
    SYS_Get_IDT_Base((PVOID*)&idt_base);
    idt  = idt_base.idtgdt_base;

    CPU_STATE_idt_base(pcpu) = idt;
    memcpy (&old_gate, &idt[CPU_PERF_VECTOR], 16);

    CPU_STATE_saved_ih(pcpu)  = (PVOID) ((((U64) old_gate.offset_high) << 32)   | 
                                         (((U64) old_gate.offset_middle) << 16) | 
                                          ((U64) old_gate.offset_low));
 
    SEP_PRINT_DEBUG("saved_ih is 0x%llx\n", CPU_STATE_saved_ih(pcpu));
    SYS_Local_Irq_Restore(eflags);

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Init_Cpu(param)
 *
 * @param    param    unused parameter
 *
 * @return   None     No return needed
 *
 * @brief  Set up the interrupt handler.  
 *
 */
static VOID 
cpumon_Init_Cpu (
    PVOID parm
)
{
    unsigned long        eflags;
    CPU_STATE            pcpu = &pcb[CONTROL_THIS_CPU()];
    GATE_STRUCT         *idt;

    SYS_Local_Irq_Save(eflags);
    idt = CPU_STATE_idt_base(pcpu);
    cpumon_Set_IDT_Func(idt, SYS_Perfvec_Handler);
    SYS_Local_Irq_Restore(eflags);

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void cpumon_Destroy_Cpu(param)
 *
 * @param    param    unused parameter
 *
 * @return   None     No return needed
 *
 * @brief  Restore the old handler
 * @brief  Finish clean up of the apic
 *
 */
static VOID 
cpumon_Destroy_Cpu (
    PVOID ctx
)
{
    unsigned long        eflags;
    CPU_STATE            pcpu = &pcb[CONTROL_THIS_CPU()];
    GATE_STRUCT         *idt;

    SYS_Local_Irq_Save(eflags);
    APIC_Disable_PMI();
    idt = CPU_STATE_idt_base(pcpu);
    cpumon_Set_IDT_Func(idt, CPU_STATE_saved_ih(pcpu));
    SYS_Local_Irq_Restore(eflags);

    return;
}
#endif

#if defined(DRV_IA32) || defined(DRV_EM64T)
/* ------------------------------------------------------------------------- */
/*!
 * @fn extern void CPUMON_Install_Cpuhools(void)
 *
 * @param    None
 *
 * @return   None     No return needed
 *
 * @brief  set up the interrupt handler (on a per-processor basis)
 * @brief  Initialize the APIC in two phases (current CPU, then others)
 *
 */
extern VOID 
CPUMON_Install_Cpuhooks (
    void
)
{
    S32  me = 0;
    PVOID linear;

    CONTROL_Invoke_Parallel(cpumon_Save_Cpu, (PVOID)(size_t)me);
    CONTROL_Invoke_Parallel(cpumon_Init_Cpu, (PVOID)(size_t)me);

    linear = NULL;
    APIC_Init(&linear);
    CONTROL_Invoke_Parallel(APIC_Init, &linear);
    CONTROL_Invoke_Parallel(APIC_Install_Interrupt_Handler, (PVOID)(size_t)me);

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn extern void CPUMON_Remove_Cpuhools(void)
 *
 * @param    None
 *
 * @return   None     No return needed
 *
 * @brief  De-Initialize the APIC in phases
 * @brief  clean up the interrupt handler (on a per-processor basis)
 *
 */
extern VOID 
CPUMON_Remove_Cpuhooks (
    void
)
{
    int            i;
    unsigned long  eflags;

    SYS_Local_Irq_Save(eflags);
    cpumon_Destroy_Cpu((PVOID)(size_t)0);
    SYS_Local_Irq_Restore(eflags);
    CONTROL_Invoke_Parallel_XS(cpumon_Destroy_Cpu, 
                               (PVOID)(size_t)0);
    
    // de-initialize APIC
    APIC_Unmap(CPU_STATE_apic_linear_addr(&pcb[0]));
    for (i = 0; i < GLOBAL_STATE_num_cpus(driver_state); i++) {
        APIC_Deinit_Phase1(i);
    }

    return;
}
#endif /* defined(DRV_IA32) || defined(DRV_EM64T) */

#if defined(DRV_IA64)

/* ------------------------------------------------------------------------- */
/*!
 * @fn          int CPUMON_Install_Cpuhooks(VOID)
 * @brief       Assign the PMU interrupt to the driver
 *
 * @return      zero if successful, non-zero error value if something failed
 *
 * Install the driver ebs handler onto the PMU interrupt. If perfmon is
 * compiled in then we ask perfmon for the interrupt, otherwise we ask the
 * kernel...
 *
 * <I>Special Notes:</I>
 *
 * @Note This routine is for Itanium(R)-based systems only!
 *
 *      For IA32, the LBRs are not frozen when a PMU interrupt is taken.
 * Since the LBRs capture information on every branch, for the LBR
 * registers to be useful, we need to freeze them as quickly as
 * possible after the interrupt. This means hooking the IDT directly
 * to call a driver specific interrupt handler. That happens in the
 * vtxsys.S file via samp_get_set_idt_entry. The real routine being
 * called first upon PMU interrupt is t_ebs (in vtxsys.S) and that
 * routine calls PMI_Interrupt_Handler()...
 *
 */
extern void
CPUMON_Install_Cpuhooks (
    void
)
{
    int status = -1;

    SEP_PRINT_DEBUG("CPUMON_Install_Cpuhooks: entered... pmv 0x%p \n", SYS_Read_PMV());

#if defined(PERFMON_V1) || defined(PERFMON_V2_ALT)
    /*
     * if Perfmon1 or Perfmon2_alt is set, we can use the perfmon.c
     * interface to steal perfmon.c's interrupt handler for our use
     * perfmon.c has already done register_percpu_irq()
     */

     ebs_irq      = SEP_PERFMON_IRQ;
     desc.handler = &PMI_Interrupt_Handler;
     status       = CPUMON_INSTALL_INTERRUPT(&desc);
     if (status) {
         SEP_PRINT_ERROR("CPUMON_Install_Cpuhooks: CPUMON_INSTALL_INTERRUPT returned %d\n",status);
     }
#elif !defined(PERFMON_V2)
    if (pebs_irqaction) {
        return status;
    }

#ifdef SA_PERCPU_IRQ_SUPPORTED
    ebs_irq        = SEP_PERFMON_IRQ;
    pebs_irqaction = (struct irqaction *) 1;
    status         = request_irq(SEP_PERFMON_IRQ,
                                 PMI_Interrupt_Handler,
                                 SA_INTERRUPT | SA_PERCPU_IRQ,
                                 "SEP Sampling",
                                 NULL);

#else
    {
        pebs_irqaction = kmalloc(sizeof (struct irqaction), GFP_ATOMIC);
        if (pebs_irqaction) {
            memset(pebs_irqaction, 0, sizeof (struct irqaction));
            ebs_irq                 = SEP_PERFMON_IRQ;
            pebs_irqaction->handler = (void *)PMI_Interrupt_Handler;
            pebs_irqaction->flags   = SA_INTERRUPT;
            pebs_irqaction->name    = SEP_DRIVER_NAME;
            pebs_irqaction->dev_id  = NULL;

            register_percpu_irq(ebs_irq, pebs_irqaction);
            status = 0;
        }
        else {
            SEP_PRINT_WARNING("couldn't kmalloc pebs_irqaction (%d bytes)\n",
                              (int)sizeof(struct irqaction));
        }
    }
#endif
#endif
    SEP_PRINT("IRQ vector 0x%x will be used for handling PMU interrupts\n", SEP_PERFMON_IRQ);

    SEP_PRINT_DEBUG("CPUMON_Install_Cpuhooks: exit...... rc=0x%x pmv=0x%p \n",
                    status, SYS_Read_PMV());

    return;
}

extern VOID
CPUMON_Remove_Cpuhooks (
    void
)
{
#if defined(PERFMON_V1) || defined(PERFMON_V2_ALT)
    int status;

    SEP_PRINT_DEBUG("CPUMON_Remove_Cpuhooks: entered... pmv=0x%p \n", SYS_Read_PMV());

    /*
     * if Perfmon1 or Perfmon2_alt is set, we used the perfmon.c
     * interface to steal perfmon.c's interrupt handler for our use.
     * Now we must release it back.
     * Don't free_irq() because perfmon.c still wants to use it
     */
     status = CPUMON_REMOVE_INTERRUPT(&desc);
     if (status) {
         SEP_PRINT_WARNING("CPUMON_Remove_Cpuhooks: CPUMON_REMOVE_INTERRUPT returned: %d\n",status);
     }

#elif !defined(PERFMON_V2)
    SEP_PRINT_DEBUG("CPUMON_Remove_Cpuhooks: entered... pmv=0x%p \n", SYS_Read_PMV());
    if (xchg(&pebs_irqaction, 0)) {
        free_irq(ebs_irq, NULL);
    }
#endif

    SEP_PRINT_DEBUG("CPUMON_Remove_Cpuhooks: exit... pmv=0x%p \n", SYS_Read_PMV());

    return;
}
#endif
