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

#ifndef __IA_CSS_PIPELINE_H__
#define __IA_CSS_PIPELINE_H__

#include "sh_css_internal.h"
#include "ia_css_pipeline_common.h"

#define IA_CSS_PIPELINE_NUM_MAX		(20)

/* Pipeline stage to be executed on SP/ISP */
struct ia_css_pipeline_stage {
	unsigned int stage_num;
	struct ia_css_binary *binary;	/* built-in binary */
	struct ia_css_binary_info *binary_info;
	const struct ia_css_fw_info *firmware;	/* acceleration binary */
	/* SP function for SP stage */
	enum ia_css_pipeline_stage_sp_func sp_func;
	unsigned max_input_width;	/* For SP raw copy */
	struct sh_css_binary_args args;
	int mode;
	bool out_frame_allocated;
	bool vf_frame_allocated;
	struct ia_css_pipeline_stage *next;
};

/* Pipeline of n stages to be executed on SP/ISP per stage */
struct ia_css_pipeline {
	enum ia_css_pipe_id pipe_id;
	uint8_t pipe_num;
	bool stop_requested;
	struct ia_css_pipeline_stage *stages;
	struct ia_css_pipeline_stage *current_stage;
	unsigned num_stages;
	struct ia_css_frame in_frame;
	struct ia_css_frame out_frame;
	struct ia_css_frame vf_frame;
	enum ia_css_frame_delay dvs_frame_delay;
	unsigned inout_port_config;
	int num_execs;
	bool acquire_isp_each_stage;
};

/* Stage descriptor used to create a new stage in the pipeline */
struct ia_css_pipeline_stage_desc {
	struct ia_css_binary *binary;
	const struct ia_css_fw_info *firmware;
	enum ia_css_pipeline_stage_sp_func sp_func;
	unsigned max_input_width;
	unsigned int mode;
	struct ia_css_frame *cc_frame;
	struct ia_css_frame *in_frame;
	struct ia_css_frame *out_frame;
	struct ia_css_frame *vf_frame;
};

/** @brief initialize the pipeline module
 *
 * @return    None
 *
 * Initializes the pipeline module. This API has to be called
 * before any operation on the pipeline module is done
 */
void ia_css_pipeline_init(void);

/** @brief initialize the pipeline structure with default values
 *
 * @param[out] pipeline  structure to be initialized with defaults
 * @param[in] pipe_id
 * @param[in] pipe_num Number that uniquely identifies a pipeline.
 * @return                     IA_CSS_SUCCESS or error code upon error.
 *
 * Initializes the pipeline structure with a set of default values.
 * This API is expected to be used when a pipeline structure is allocated
 * externally and needs sane defaults
 */
enum ia_css_err ia_css_pipeline_create(
	struct ia_css_pipeline *pipeline,
	enum ia_css_pipe_id pipe_id,
	unsigned int pipe_num);

/** @brief destroy a pipeline
 *
 * @param[in] pipeline
 * @return    None
 *
 */
void ia_css_pipeline_destroy(struct ia_css_pipeline *pipeline);

/** @brief Starts a pipeline
 *
 * @param[in] pipe_id
 * @param[in] pipeline
 * @return    None
 *
 */
void ia_css_pipeline_start(enum ia_css_pipe_id pipe_id,
			   struct ia_css_pipeline *pipeline);

/** @brief Request to stop a pipeline
 *
 * @param[in] pipeline
 * @return                     IA_CSS_SUCCESS or error code upon error.
 *
 */
enum ia_css_err ia_css_pipeline_request_stop(struct ia_css_pipeline *pipeline);

/** @brief Check whether pipeline has stopped
 *
 * @param[in] pipeline
 * @return    true if the pipeline has stopped
 *
 */
bool ia_css_pipeline_has_stopped(struct ia_css_pipeline *pipe);

/** @brief clean all the stages pipeline and make it as new
 *
 * @param[in] pipeline
 * @return    None
 *
 */
void ia_css_pipeline_clean(struct ia_css_pipeline *pipeline);

/** @brief Add a stage to pipeline.
 *
 * @param     pipeline               Pointer to the pipeline to be added to.
 * @param[in] stage_desc       The description of the stage
 * @param[out] stage            The successor of the stage.
 * @return                     IA_CSS_SUCCESS or error code upon error.
 *
 * Add a new stage to a non-NULL pipeline.
 * The stage consists of an ISP binary or firmware and input and output
 * arguments.
*/
enum ia_css_err ia_css_pipeline_create_and_add_stage(
			struct ia_css_pipeline *pipeline,
			struct ia_css_pipeline_stage_desc *stage_desc,
			struct ia_css_pipeline_stage **stage);

/** @brief Finalize the stages in a pipeline
 *
 * @param     pipeline               Pointer to the pipeline to be added to.
 * @return                     None
 *
 * This API is expected to be called after adding all stages
*/
void ia_css_pipeline_finalize_stages(struct ia_css_pipeline *pipeline);

/** @brief gets a stage from the pipeline
 *
 * @param[in] pipeline
 * @return                     IA_CSS_SUCCESS or error code upon error.
 *
 */
enum ia_css_err ia_css_pipeline_get_stage(struct ia_css_pipeline *pipeline,
			  int mode,
			  struct ia_css_pipeline_stage **stage);

/** @brief gets the output stage from the pipeline
 *
 * @param[in] pipeline
 * @return                     IA_CSS_SUCCESS or error code upon error.
 *
 */
enum ia_css_err ia_css_pipeline_get_output_stage(
			struct ia_css_pipeline *pipeline,
			int mode,
			struct ia_css_pipeline_stage **stage);

/** @brief Checks whether the pipeline uses params
 *
 * @param[in] pipeline
 * @return    true if the pipeline uses params
 *
 */
bool ia_css_pipeline_uses_params(struct ia_css_pipeline *pipeline);

/**
 * @brief get the SP thread ID.
 *
 * @param[in]	key	The query key, typical use is pipe_num.
 * @param[out]	val	The query value.
 *
 * @return
 *	true, if the query succeeds;
 *	false, if the query fails.
 */
bool ia_css_pipeline_get_sp_thread_id(unsigned int key, unsigned int *val);

#if defined(USE_INPUT_SYSTEM_VERSION_2401)
/**
 * @brief Get the pipeline io status
 *
 * @param[in] None
 * @return
 *	Pointer to pipe_io_status
 */
struct sh_css_sp_pipeline_io_status *ia_css_pipeline_get_pipe_io_status(void);
#endif

/**
 * @brief Map an SP thread to this pipeline
 *
 * @param[in]	pipeline
 * @param[in]	map true for mapping and false for unmapping sp threads.
 *
 */
void ia_css_pipeline_map(struct ia_css_pipeline *pipeline, bool map);

#endif /*__IA_CSS_PIPELINE_H__*/
