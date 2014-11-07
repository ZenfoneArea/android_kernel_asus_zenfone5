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

#ifndef _SH_CSS_SP_H_
#define _SH_CSS_SP_H_

#include <system_global.h>
#if !defined(HAS_NO_INPUT_FORMATTER)
#include "input_formatter.h"
#endif

#include "ia_css_binary.h"
#include "sh_css_internal.h"
#include "ia_css_types.h"
#include "ia_css_pipeline.h"
#include "ia_css_queue.h"

/* Function to initialize the data and bss section descr of the binary */
void
sh_css_sp_store_init_dmem(const struct ia_css_fw_info *fw);

void
store_sp_stage_data(enum ia_css_pipe_id id, unsigned int pipe_num, unsigned stage);

void
sh_css_stage_write_binary_info(struct ia_css_binary_info *info);

void
sh_css_sp_init_group(bool two_ppc, enum ia_css_stream_format input_format,
		     bool no_isp_sync, uint8_t if_config_index);

void
store_sp_group_data(void);

/* Start binary (jpeg) copy on the SP */
void
sh_css_sp_start_binary_copy(unsigned int pipe_num, struct ia_css_frame *out_frame,
			    unsigned two_ppc);

unsigned int
sh_css_sp_get_binary_copy_size(void);

/* Return the value of a SW interrupt */
unsigned int
sh_css_sp_get_sw_interrupt_value(unsigned int irq);

void
sh_css_sp_init_pipeline(struct ia_css_pipeline *me,
			enum ia_css_pipe_id id,
			uint8_t pipe_num,
			bool xnr,
			bool two_ppc,
			bool continuous,
			bool offline,
			unsigned int required_bds_factor,
			enum sh_css_pipe_config_override copy_ovrd,
			enum ia_css_input_mode input_mode,
			const struct ia_css_metadata_config *md_config
#if !defined(IS_ISP_2500_SYSTEM)
			, const mipi_port_ID_t port_id
#endif
			);

void
sh_css_sp_uninit_pipeline(unsigned int pipe_num);

void
sh_css_write_host2sp_command(enum host2sp_commands host2sp_command);

enum host2sp_commands
sh_css_read_host2sp_command(void);

void
sh_css_init_host2sp_frame_data(void);

/**
 * @brief Update the offline frame information in host_sp_communication.
 *
 * @param[in] frame_num The offline frame number.
 * @param[in] frame The pointer to the offline frame.
 */
void
sh_css_update_host2sp_offline_frame(
				unsigned frame_num,
				struct ia_css_frame *frame);

/**
 * @brief Get the right queue to operate on
 *
 * @param[in] type
 * @param[in] id
 * @param[in] thread
 */
ia_css_queue_t*
sh_css_get_queue(enum sh_css_queue_type type, enum sh_css_buffer_queue_id id,
		 int thread);
/**
 * @brief Update the mipi frame information in host_sp_communication.
 *
 * @param[in] frame_num The mipi frame number.
 * @param[in] frame The pointer to the mipi frame.
 */
void
sh_css_update_host2sp_mipi_frame(
				unsigned frame_num,
				struct ia_css_frame *frame);

/**
 * @brief Update the nr of offline frames to use in host_sp_communication.
 *
 * @param[in] num_frames The number of raw frames to use.
 */
void
sh_css_update_host2sp_cont_num_raw_frames(unsigned num_frames, bool set_avail);

/**
 * @brief Update the nr of mipi frames to use in host_sp_communication.
 *
 * @param[in] num_frames The number of mipi frames to use.
 */
void
sh_css_update_host2sp_cont_num_mipi_frames(unsigned num_frames);

void
sh_css_event_init_irq_mask(void);

void
sh_css_sp_start_isp(void);

void
sh_css_sp_set_sp_running(bool flag);

bool
sh_css_sp_is_running(void);

#if SP_DEBUG !=SP_DEBUG_NONE

void
sh_css_sp_get_debug_state(struct sh_css_sp_debug_state *state);

#endif

#if !defined(HAS_NO_INPUT_FORMATTER)
extern void sh_css_sp_set_if_configs(
	const input_formatter_cfg_t	*config_a,
	const input_formatter_cfg_t	*config_b,
	const uint8_t		if_config_index);
#endif

void
sh_css_sp_program_input_circuit(int fmt_type,
				int ch_id,
				enum ia_css_input_mode input_mode);

void
sh_css_sp_configure_sync_gen(int width,
			     int height,
			     int hblank_cycles,
			     int vblank_cycles);

void
sh_css_sp_configure_tpg(int x_mask,
			int y_mask,
			int x_delta,
			int y_delta,
			int xy_mask);

void
sh_css_sp_configure_prbs(int seed);

void
sh_css_sp_reset_global_vars(void);

enum ia_css_err
sh_css_sp_write_frame_pointers(const struct sh_css_binary_args *args,
				unsigned pipe_num, unsigned stage_num);

/**
 * @brief Initialize the DMA software-mask in the debug mode.
 * This API should be ONLY called in the debugging mode.
 * And it should be always called before the first call of
 * "sh_css_set_dma_sw_reg(...)".
 *
 * @param[in]	dma_id		The ID of the target DMA.
 *
 * @return
 *	- true, if it is successful.
 *	- false, otherwise.
 */
extern bool sh_css_sp_init_dma_sw_reg(int dma_id);

/**
 * @brief Set the DMA software-mask in the debug mode.
 * This API should be ONLYL called in the debugging mode. Must
 * call "sh_css_set_dma_sw_reg(...)" before this
 * API is called for the first time.
 *
 * @param[in]	dma_id		The ID of the target DMA.
 * @param[in]	channel_id	The ID of the target DMA channel.
 * @param[in]	request_type	The type of the DMA request.
 *				For example:
 *				- "0" indicates the writing request.
 *				- "1" indicates the reading request.
 *
 * @param[in]	enable		If it is "true", the target DMA
 *				channel is enabled in the software.
 *				Otherwise, the target DMA channel
 *				is disabled in the software.
 *
 * @return
 *	- true, if it is successful.
 *	- false, otherwise.
 */
extern bool sh_css_sp_set_dma_sw_reg(int dma_id,
		int channel_id,
		int request_type,
		bool enable);


extern struct sh_css_sp_group sh_css_sp_group;
extern struct sh_css_sp_stage sh_css_sp_stage;
extern struct sh_css_isp_stage sh_css_isp_stage;

#endif /* _SH_CSS_SP_H_ */

