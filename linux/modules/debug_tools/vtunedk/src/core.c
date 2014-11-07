/*COPYRIGHT**
    Copyright (C) 2002-2012 Intel Corporation.  All Rights Reserved.
 
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
#include <linux/wait.h>
#include <linux/fs.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"

#include "lwpmudrv.h"
#include "utility.h"
#include "control.h"
#include "output.h"
#include "core.h"
#include "ecb_iterators.h"
#include "pebs.h"
#include "apic.h"

#define  CORE_ENABLE_BIT  0x0400000

extern EVENT_CONFIG   global_ec;
extern U64           *read_counter_info;
extern DRV_CONFIG     pcfg;

/* ------------------------------------------------------------------------- */
/*!
 * @fn void core_Write_PMU(param)
 *
 * @param    param    dummy parameter which is not used
 *
 * @return   None     No return needed
 *
 * @brief    Initial set up of the PMU registers
 *
 * <I>Special Notes</I>
 *         Initial write of PMU registers.
 *         Walk through the enties and write the value of the register accordingly.
 *         Assumption:  For CCCR registers the enable bit is set to value 0.
 *         When current_group = 0, then this is the first time this routine is called,
 *         initialize the locks and set up EM tables.
 */
static VOID
core_Write_PMU (
    VOID  *param
)
{
    U32            this_cpu = CONTROL_THIS_CPU();
    CPU_STATE      pcpu = &pcb[this_cpu];

    if (CPU_STATE_current_group(pcpu) == 0) {
        if (EVENT_CONFIG_mode(global_ec) != EM_DISABLED) {
            U32            index;
            U32            st_index; 
            U32            j;

            /* Save all the initialization values away into an array for Event Multiplexing. */
            for (j = 0; j < EVENT_CONFIG_num_groups(global_ec); j++) {
                CPU_STATE_current_group(pcpu) = j;
                st_index   = CPU_STATE_current_group(pcpu) * EVENT_CONFIG_max_gp_events(global_ec);
                FOR_EACH_DATA_GP_REG(pecb, i) {
                    index = st_index + (ECB_entries_reg_id(pecb,i) - IA32_PMC0);
                    CPU_STATE_em_tables(pcpu)[index] = ECB_entries_reg_value(pecb,i);
                } END_FOR_EACH_DATA_REG;
            }
            /* Reset the current group to the very first one. */
            CPU_STATE_current_group(pcpu) = 0;
        }
    }

    FOR_EACH_REG_ENTRY(pecb, i) {
        SYS_Write_MSR(ECB_entries_reg_id(pecb,i), ECB_entries_reg_value(pecb,i));
    } END_FOR_EACH_REG_ENTRY;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void core_Disable_PMU(param)
 *
 * @param    param    dummy parameter which is not used
 *
 * @return   None     No return needed
 *
 * @brief    Reset the enable bit for all the Control registers
 *
 */
static VOID
core_Disable_PMU (
    PVOID  param
)
{
    U32        this_cpu = CONTROL_THIS_CPU();
    CPU_STATE  pcpu     = &pcb[this_cpu];
    ECB        pecb     = PMU_register_data[CPU_STATE_current_group(pcpu)];
    SYS_Write_MSR(IA32_PERFEVTSEL0, ECB_entries_reg_value(pecb,0));
    if (GLOBAL_STATE_current_phase(driver_state) != DRV_STATE_RUNNING) {
        APIC_Disable_PMI();
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void core_Enable_PMU(param)
 *
 * @param    param    dummy parameter which is not used
 *
 * @return   None     No return needed
 *
 * @brief    Set the enable bit for all the Control registers
 *
 */
static VOID
core_Enable_PMU (
    PVOID   param
)
{
    U32        this_cpu = CONTROL_THIS_CPU();
    CPU_STATE  pcpu     = &pcb[this_cpu];
    ECB        pecb     = PMU_register_data[CPU_STATE_current_group(pcpu)];
    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RUNNING) {
        APIC_Enable_Pmi();
        SYS_Write_MSR(IA32_PERFEVTSEL0, (ECB_entries_reg_value(pecb,0) | CORE_ENABLE_BIT));
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn void core_Check_Overflow(masks)
 *
 * @param    masks    the mask structure to populate
 *
 * @return   None     No return needed
 *
 * @brief  Called by the data processing method to figure out which registers have overflowed.
 *
 */
static void
core_Check_Overflow (
    DRV_MASKS masks
)
{
    U64              val;
    U32              event_sel_reg;
    U32              this_cpu    = CONTROL_THIS_CPU();
    BUFFER_DESC      bd          = &cpu_buf[this_cpu];
    DRV_EVENT_MASK_NODE event_flag;

    BUFFER_DESC_sample_count(bd) = 0;

    SEP_PRINT_DEBUG("core_Check_Overflow\n");

    // initialize masks 
    DRV_MASKS_masks_num(masks) = 0;

    FOR_EACH_DATA_REG(pecb,i) {
        event_sel_reg = IA32_PERFEVTSEL0 + (ECB_entries_reg_id(pecb, i) - IA32_PMC0);
        val           = SYS_Read_MSR(event_sel_reg);

        /*
         * Have I overflowed?  If so, add to the list of items to read
         * Question: There are 2 read msr's here?  Hmmm...
         */
        if (val & EM_INT_MASK) {
            val  = SYS_Read_MSR(ECB_entries_reg_id(pecb, i));
            val &= ECB_entries_max_bits(pecb,i);
            if (val < ECB_entries_reg_value(pecb,i)) {
                SYS_Write_MSR(ECB_entries_reg_id(pecb,i), ECB_entries_reg_value(pecb,i));

                DRV_EVENT_MASK_bitFields1(&event_flag) = (U8) 0;
                if (DRV_MASKS_masks_num(masks) < MAX_OVERFLOW_EVENTS) {
                    DRV_EVENT_MASK_bitFields1(DRV_MASKS_eventmasks(masks) + DRV_MASKS_masks_num(masks)) = DRV_EVENT_MASK_bitFields1(&event_flag);
                    DRV_EVENT_MASK_event_idx(DRV_MASKS_eventmasks(masks) + DRV_MASKS_masks_num(masks)) = ECB_entries_event_id_index(pecb, i);
                    DRV_MASKS_masks_num(masks)++;
                } 
                else {
                    SEP_PRINT_ERROR("The array for event masks is full.\n");
                }

            }
        }
    } END_FOR_EACH_DATA_REG;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn core_Read_PMU_Data(param)
 *
 * @param    param    dummy parameter which is not used
 *
 * @return   None     No return needed
 *
 * @brief    Read all the data MSR's into a buffer.  Called by the interrupt handler.
 *
 */
static void
core_Read_PMU_Data (
    PVOID   param
)
{
    S32         start_index, j;
    U64        *buffer    = read_counter_info;
    U32         this_cpu;

    preempt_disable();
    this_cpu  = CONTROL_THIS_CPU();
    preempt_enable();
    start_index = DRV_CONFIG_num_events(pcfg) * this_cpu;
    SEP_PRINT_DEBUG("PMU control_data 0x%p, buffer 0x%p, j = %d\n", PMU_register_data, buffer, j);
    FOR_EACH_DATA_REG(pecb,i) {
        j = start_index + ECB_entries_event_id_index(pecb,i);
        buffer[j] = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
        SEP_PRINT_DEBUG("this_cpu %d, event_id %d, value 0x%I64x\n", this_cpu, i, buffer[j]);
    } END_FOR_EACH_DATA_REG;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn core_Swap_Group(restart)
 *
 * @param    restart    dummy parameter which is not used
 *
 * @return   None     No return needed
 *
 * @brief    Perform the mechanics of swapping the event groups for event mux operations
 *
 * <I>Special Notes</I>
 *         Swap function for event multiplexing.
 *         Freeze the counting.
 *         Swap the groups.
 *         Enable the counting.
 *
 */
static VOID
core_Swap_Group (
    DRV_BOOL  restart
)
{
    U32            index;
    U32            next_group;
    U32            st_index; 
    U32            this_cpu = CONTROL_THIS_CPU();
    CPU_STATE      pcpu     = &pcb[this_cpu];
    
    
    st_index   = CPU_STATE_current_group(pcpu) * EVENT_CONFIG_max_gp_events(global_ec);
    next_group = (CPU_STATE_current_group(pcpu) + 1) % EVENT_CONFIG_num_groups(global_ec);

    SEP_PRINT_DEBUG("current group : 0x%x\n", CPU_STATE_current_group(pcpu));
    SEP_PRINT_DEBUG("next group : 0x%x\n", next_group);

    // Save the counters for the current group
    if (!DRV_CONFIG_event_based_counts(pcfg)) {
        FOR_EACH_DATA_GP_REG(pecb, i) {
            index = st_index + (ECB_entries_reg_id(pecb, i) - IA32_PMC0);
            CPU_STATE_em_tables(pcpu)[index] = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
            SEP_PRINT_DEBUG("Saved value for reg 0x%x : 0x%I64x ",
                            ECB_entries_reg_id(pecb,i),
                            CPU_STATE_em_tables(pcpu)[index]);
        } END_FOR_EACH_DATA_GP_REG;
    }

    CPU_STATE_current_group(pcpu)  = next_group;

    // First write the GP control registers (eventsel)
    FOR_EACH_CCCR_GP_REG(pecb, i) {
        SYS_Write_MSR(ECB_entries_reg_id(pecb,i), ECB_entries_reg_value(pecb,i));
    } END_FOR_EACH_CCCR_GP_REG;

    if (DRV_CONFIG_event_based_counts(pcfg)) {
        // In EBC mode, reset the counts for all events except for trigger event
        FOR_EACH_DATA_REG(pecb, i) {
            if (ECB_entries_event_id_index(pecb, i) != CPU_STATE_trigger_event_num(pcpu)) {
                SYS_Write_MSR(ECB_entries_reg_id(pecb,i), 0LL);
            }
        } END_FOR_EACH_DATA_REG;
    }
    else {
        // Then write the gp count registers
        st_index = CPU_STATE_current_group(pcpu) * EVENT_CONFIG_max_gp_events(global_ec);
        FOR_EACH_DATA_GP_REG(pecb, i) {
            index = st_index + (ECB_entries_reg_id(pecb, i) - IA32_PMC0);
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), CPU_STATE_em_tables(pcpu)[index]);
            SEP_PRINT_DEBUG("Restore value for reg 0x%x : 0x%I64x ",
                            ECB_entries_reg_id(pecb,i),
                            CPU_STATE_em_tables(pcpu)[index]);
        } END_FOR_EACH_DATA_GP_REG;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn core_Clean_Up(param)
 *
 * @param    param    dummy parameter which is not used
 *
 * @return   None     No return needed
 *
 * @brief    Set all the registers with the cleanup bit to 0
 *
 */
static VOID
core_Clean_Up (
    VOID   *param
)
{
    FOR_EACH_REG_ENTRY(pecb, i) {
        if (ECB_entries_clean_up_get(pecb,i)) {
            SEP_PRINT_DEBUG("clean up set --- RegId --- %x\n", ECB_entries_reg_id(pecb,i));
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), 0LL);
        }
    } END_FOR_EACH_REG_ENTRY;

    return;
}

/* ------------------------------------------------------------------------- */
/*!
 * @fn core_Read_Counts(param, id)
 *
 * @param    param    The read thread node to process
 * @param    id       The event id for the which the sample is generated
 *
 * @return   None     No return needed
 *
 * @brief    Read CPU event based counts data and store into the buffer param;
 *           For the case of the trigger event, store the SAV value. 
 */
static VOID
core_Read_Counts (
    PVOID  param,
    U32    id
)
{
    U64            *data;
    int             data_index;
    U32             this_cpu            = CONTROL_THIS_CPU();
    CPU_STATE       pcpu                = &pcb[this_cpu];
    U32             event_id            = 0;

    data       = (U64 *)param;
    data_index = 0;

    // Write GroupID
    data[data_index] = CPU_STATE_current_group(pcpu) + 1;
    // Increment the data index as the event id starts from zero
    data_index++;

    FOR_EACH_DATA_REG(pecb,i) {
        event_id = ECB_entries_event_id_index(pecb,i);
        if (event_id == id) {
            data[data_index + event_id] = ~(ECB_entries_reg_value(pecb,i) - 1) & 
                                           ECB_entries_max_bits(pecb,i);;
        }
        else {
            data[data_index + event_id] = SYS_Read_MSR(ECB_entries_reg_id(pecb,i));
            SYS_Write_MSR(ECB_entries_reg_id(pecb,i), 0LL);
        }
    } END_FOR_EACH_DATA_REG;

    return;
}

static VOID
core_Initialize (
    VOID  *param
)
{
    return;
}


static VOID
core_Destroy (
    VOID  *param
)
{
    return;
}

static VOID
core_Dummy1 (
    VOID *arg
)
{
    return;
}


//
// Initialize the dispatch table
//
DISPATCH_NODE  core_dispatch = 
{ 
    core_Initialize,       // init
    core_Destroy,          // fini
    core_Write_PMU,        // write
    core_Disable_PMU,      // freeze
    core_Enable_PMU,       // restart
    core_Read_PMU_Data,    // read
    core_Check_Overflow,   // check for overflow
    core_Swap_Group,
    core_Dummy1,
    core_Clean_Up,
    NULL,
    NULL,
    NULL,
    core_Read_Counts,
    NULL,                  // check_overflow_gp_errata
    NULL,                  // read_ro
    NULL                   // platform_info
};
