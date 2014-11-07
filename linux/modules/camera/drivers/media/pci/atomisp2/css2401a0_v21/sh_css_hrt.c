/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "platform_support.h"

#include "sh_css_hrt.h"

#include "device_access.h"
#include "assert_support.h"

#define __INLINE_EVENT__
#include "event_fifo.h"
#define __INLINE_SP__
#include "sp.h"
#define __INLINE_ISP__
#include "isp.h"
#define __INLINE_IRQ__
#include "irq.h"
#define __INLINE_FIFO_MONITOR__
#include "fifo_monitor.h"

#if !defined(HAS_NO_INPUT_SYSTEM)
#include "input_system.h"	/* MIPI_PREDICTOR_NONE,... */
#endif

/* System independent */
#include "sh_css_internal.h"
#if !defined(HAS_NO_INPUT_SYSTEM)
#include "ia_css_isys.h"
#endif


bool sh_css_hrt_system_is_idle(void)
{
	hrt_data	status;
	bool not_idle = false;

	not_idle |= !isp_ctrl_getbit(ISP0_ID, ISP_SC_REG, ISP_IDLE_BIT);

	status = fifo_monitor_reg_load(FIFO_MONITOR0_ID,
#if defined(IS_ISP_2500_SYSTEM)
		HIVE_GP_REGS_SP1_STRMON_STAT_IDX);
#else
		HIVE_GP_REGS_SP_STREAM_STAT_IDX);
#endif
	not_idle |= ((status & FIFO_CHANNEL_SP_VALID_MASK) != 0);

#if defined(IS_ISP_2500_SYSTEM)
	// checking status of 2nd SP
	status = fifo_monitor_reg_load(FIFO_MONITOR0_ID,
		HIVE_GP_REGS_SP2_STRMON_STAT_IDX);
	not_idle |= ((status & FIFO_CHANNEL_SP_VALID_MASK) != 0);
#endif

#if !defined(IS_ISP_2500_SYSTEM)
#if defined(HAS_FIFO_MONITORS_VERSION_2)
	status = fifo_monitor_reg_load(FIFO_MONITOR0_ID,
		HIVE_GP_REGS_SP_STREAM_STAT_B_IDX);
	not_idle |= ((status & FIFO_CHANNEL_SP_VALID_B_MASK) != 0);
#endif
#endif

	status = fifo_monitor_reg_load(FIFO_MONITOR0_ID,
#if defined(IS_ISP_2500_SYSTEM)
		HIVE_GP_REGS_ISP_STRMON_STAT_IDX);
#else
		HIVE_GP_REGS_ISP_STREAM_STAT_IDX);
#endif
	not_idle |= ((status & FIFO_CHANNEL_ISP_VALID_MASK) != 0);

	status = fifo_monitor_reg_load(FIFO_MONITOR0_ID,
#if defined(IS_ISP_2500_SYSTEM)
		HIVE_GP_REGS_MOD_STRMON_STAT_IDX);
#else
		HIVE_GP_REGS_MOD_STREAM_STAT_IDX);
#endif
	not_idle |= ((status & FIFO_CHANNEL_MOD_VALID_MASK) != 0);

return !not_idle;
}

enum ia_css_err sh_css_hrt_sp_wait(void)
{
#if defined(HAS_IRQ_MAP_VERSION_2)
	irq_sw_channel_id_t	irq_id = IRQ_SW_CHANNEL0_ID;
#else
	irq_sw_channel_id_t	irq_id = IRQ_SW_CHANNEL2_ID;
#endif
	/*
	 * Wait till SP is idle or till there is a SW2 interrupt
	 * The SW2 interrupt will be used when frameloop runs on SP
	 * and signals an event with similar meaning as SP idle
	 * (e.g. frame_done)
	 */
	while (!sp_ctrl_getbit(SP0_ID, SP_SC_REG, SP_IDLE_BIT) &&
		((irq_reg_load(IRQ0_ID,
			_HRT_IRQ_CONTROLLER_STATUS_REG_IDX) &
			(1U<<(irq_id + IRQ_SW_CHANNEL_OFFSET))) == 0)) {
		hrt_sleep();
	}

return IA_CSS_SUCCESS;
}
