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

#include "ia_css.h"
#include "ia_css_debug.h"
#include "sw_event_global.h"		/* encode_sw_event */
#include "sp.h"			/* cnd_sp_irq_enable() */
#include "assert_support.h"
#include "memory_access.h"
#include "sh_css_sp.h"
#include "ia_css_pipeline.h"
#include "ia_css_isp_param.h"
#include "ia_css_eventq.h"

#define PIPELINE_NUM_UNMAPPED                   (~0)
#define PIPELINE_SP_THREAD_EMPTY_TOKEN          (0x0)
#define PIPELINE_SP_THREAD_RESERVED_TOKEN       (0x1)

/*******************************************************
*** Static variables
********************************************************/
static unsigned int pipeline_num_to_sp_thread_map[IA_CSS_PIPELINE_NUM_MAX];
static unsigned int pipeline_sp_thread_list[SH_CSS_MAX_SP_THREADS];

/*******************************************************
*** Static functions
********************************************************/
static void pipeline_init_sp_thread_map(void);
static void pipeline_map_num_to_sp_thread(unsigned int pipe_num);
static void pipeline_unmap_num_to_sp_thread(unsigned int pipe_num);
static void pipeline_init_defaults(
	struct ia_css_pipeline *pipeline,
	enum ia_css_pipe_id pipe_id,
	unsigned int pipe_num);

static void pipeline_stage_destroy(struct ia_css_pipeline_stage *stage);
static enum ia_css_err pipeline_stage_create(
	struct ia_css_pipeline_stage_desc *stage_desc,
	struct ia_css_pipeline_stage **new_stage);

/*******************************************************
*** Public functions
********************************************************/
void ia_css_pipeline_init(void)
{
	pipeline_init_sp_thread_map();
}

enum ia_css_err ia_css_pipeline_create(
	struct ia_css_pipeline *pipeline,
	enum ia_css_pipe_id pipe_id,
	unsigned int pipe_num)
{
	assert(pipeline != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipeline_create() enter:\n");

	pipeline_init_defaults(pipeline, pipe_id, pipe_num);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipeline_create() exit: pipe_num=%d\n",
		pipeline->pipe_num);

	return IA_CSS_SUCCESS;
}

void ia_css_pipeline_map(struct ia_css_pipeline *pipeline, bool map)
{
	assert(pipeline != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipeline_map() enter: pipe_num=%d\n", pipeline->pipe_num);

	if(map) {
		pipeline_map_num_to_sp_thread(pipeline->pipe_num);
	} else {
		pipeline_unmap_num_to_sp_thread(pipeline->pipe_num);
	}
}

/** @brief destroy a pipeline
 *
 * @param[in] pipeline
 * @return    None
 *
 */
void ia_css_pipeline_destroy(struct ia_css_pipeline *pipeline)
{
	assert(pipeline != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipeline_destroy() enter: pipe_num=%d\n",
		pipeline->pipe_num);
	/* Free the pipeline number */

	ia_css_pipeline_clean(pipeline);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipeline_destroy() exit:");
}

/* Run a pipeline and wait till it completes. */
void ia_css_pipeline_start(enum ia_css_pipe_id pipe_id,
			   struct ia_css_pipeline *pipeline)
{
	uint8_t pipe_num = 0;
	unsigned int thread_id;
	ia_css_queue_t *eventq;

	assert(pipeline != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
	      "ia_css_pipeline_start() enter: pipe_id=%d, pipeline=%p\n",
	      pipe_id, pipeline);
	pipeline->pipe_id = pipe_id;
	sh_css_sp_init_pipeline(pipeline, pipe_id, pipe_num,
				false, false, false, true, SH_CSS_BDS_FACTOR_1_00,
				SH_CSS_PIPE_CONFIG_OVRD_NO_OVRD,
				IA_CSS_INPUT_MODE_MEMORY, NULL
#if !defined(HAS_NO_INPUT_SYSTEM)
				, (mipi_port_ID_t) 0
#endif
				);

	ia_css_pipeline_get_sp_thread_id(pipe_num, &thread_id);
	eventq = sh_css_get_queue(sh_css_host2sp_event_queue,
					-1, -1);
	if (NULL == eventq) {
		/* Error as the queue is not initialized */
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"ia_css_pipeline_start() leaving: host2sp_eventq"
			"not available\n");
		return;
	}
	ia_css_eventq_send(eventq,
			SP_SW_EVENT_ID_4, (uint8_t)thread_id, 0, 0);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
	      "ia_css_pipeline_start() leave: return_void\n");
}

/**
 * @brief Query the SP thread ID.
 * Refer to "sh_css_internal.h" for details.
 */
bool ia_css_pipeline_get_sp_thread_id(unsigned int key, unsigned int *val)
{
	assert(key < IA_CSS_PIPELINE_NUM_MAX);
	assert(key < IA_CSS_PIPE_ID_NUM);
	assert(val != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
	      "ia_css_pipeline_get_sp_thread_id() enter: key=%d\n",
	      key);
	*val = pipeline_num_to_sp_thread_map[key];

	assert(*val != (unsigned)PIPELINE_NUM_UNMAPPED);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
	      "ia_css_pipeline_get_sp_thread_id() leave: return_val=%d\n",
	      *val);
	return true;
}

enum ia_css_err ia_css_pipeline_request_stop(struct ia_css_pipeline *pipeline)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	unsigned int thread_id;
	ia_css_queue_t *eventq;

	assert(pipeline != NULL);

	if (pipeline == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipeline_request_stop() enter: pipeline=%p\n",
		pipeline);
	pipeline->stop_requested = true;

	/* Send stop event to the sp*/
	/* This needs improvement, stop on all the pipes available
	 * in the stream*/
	ia_css_pipeline_get_sp_thread_id(pipeline->pipe_num, &thread_id);
	eventq = sh_css_get_queue(sh_css_host2sp_event_queue,
					-1, -1);
	if (NULL == eventq) {
		/* Error as the queue is not initialized */
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"ia_css_pipeline_request_stop() leaving:"
			"host2sp_eventq not available\n");
		return IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;
	}
	ia_css_eventq_send(eventq,
			SP_SW_EVENT_ID_5, (uint8_t)thread_id, 0,  0);
	sh_css_sp_uninit_pipeline(pipeline->pipe_num);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		      "ia_css_pipeline_request_stop() leave: return_err=%d\n",
		      err);
	return err;
}

void ia_css_pipeline_clean(struct ia_css_pipeline *pipeline)
{
	struct ia_css_pipeline_stage *s;

	assert(pipeline != NULL);
	s = pipeline->stages;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipeline_clean() enter:\n");

	while (s) {
		struct ia_css_pipeline_stage *next = s->next;
		pipeline_stage_destroy(s);
		s = next;
	}
	pipeline_init_defaults(pipeline, pipeline->pipe_id, pipeline->pipe_num);
}

/** @brief Add a stage to pipeline.
 *
 * @param       pipeline      Pointer to the pipeline to be added to.
 * @param[in]   stage_desc    The description of the stage
 * @param[out]	stage         The successor of the stage.
 * @return      IA_CSS_SUCCESS or error code upon error.
 *
 * Add a new stage to a non-NULL pipeline.
 * The stage consists of an ISP binary or firmware and input and
 * output arguments.
*/
enum ia_css_err ia_css_pipeline_create_and_add_stage(
		struct ia_css_pipeline *pipeline,
		struct ia_css_pipeline_stage_desc *stage_desc,
		struct ia_css_pipeline_stage **stage)
{
	struct ia_css_pipeline_stage *last, *new_stage = NULL;
	enum ia_css_err err;

	/* other arguments can be NULL */
	assert(pipeline != NULL);
	assert(stage_desc != NULL);
	last = pipeline->stages;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		      "ia_css_pipeline_create_and_add_stage() enter:\n");
	if (!stage_desc->binary && !stage_desc->firmware
	    && (stage_desc->sp_func == IA_CSS_PIPELINE_NO_FUNC)) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			      "ia_css_pipeline_create_and_add_stage() done:"
			      " Invalid args\n");

		return IA_CSS_ERR_INTERNAL_ERROR;
	}

	/* Find the last stage */
	while (last && last->next)
		last = last->next;

	/* if in_frame is not set, we use the out_frame from the previous
	 * stage, if no previous stage, it's an error.
	 */
	if ((stage_desc->sp_func == IA_CSS_PIPELINE_NO_FUNC)
		&& (!stage_desc->in_frame)
		&& (!stage_desc->firmware)
		&& (!stage_desc->binary->online)) {

		/* Do this only for ISP stages*/
		if (last)
			stage_desc->in_frame = last->args.out_frame;

		if (!stage_desc->in_frame)
			return IA_CSS_ERR_INTERNAL_ERROR;
	}

	/* Create the new stage */
	err = pipeline_stage_create(stage_desc, &new_stage);
	if (err != IA_CSS_SUCCESS) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			      "ia_css_pipeline_create_and_add_stage() done:"
			      " stage_create_failed\n");
		return err;
	}

	if (last)
		last->next = new_stage;
	else
		pipeline->stages = new_stage;

	/* Output the new stage */
	if (stage)
		*stage = new_stage;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		      "ia_css_pipeline_create_and_add_stage() done:");
	return IA_CSS_SUCCESS;
}

void ia_css_pipeline_finalize_stages(struct ia_css_pipeline *pipeline)
{
	unsigned i = 0;
	struct ia_css_pipeline_stage *stage;

	assert(pipeline != NULL);
	for (stage = pipeline->stages; stage; stage = stage->next) {
		stage->stage_num = i;
		i++;
	}
	pipeline->num_stages = i;
}

enum ia_css_err ia_css_pipeline_get_stage(struct ia_css_pipeline *pipeline,
					  int mode,
					  struct ia_css_pipeline_stage **stage)
{
	struct ia_css_pipeline_stage *s;
	assert(pipeline != NULL);
	assert(stage != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		      "ia_css_pipeline_get_stage() enter:\n");
	for (s = pipeline->stages; s; s = s->next) {
		if (s->mode == mode) {
			*stage = s;
			return IA_CSS_SUCCESS;
		}
	}
	return IA_CSS_ERR_INTERNAL_ERROR;
}

enum ia_css_err ia_css_pipeline_get_output_stage(
		struct ia_css_pipeline *pipeline,
		int mode,
		struct ia_css_pipeline_stage **stage)
{
	struct ia_css_pipeline_stage *s;
	assert(pipeline != NULL);
	assert(stage != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		      "ia_css_pipeline_get_output_stage() enter:\n");

	*stage = NULL;
	/* First find acceleration firmware at end of pipe */
	for (s = pipeline->stages; s; s = s->next) {
		if (s->firmware && s->mode == mode &&
		    s->firmware->info.isp.sp.enable.output)
			*stage = s;
	}
	if (*stage)
		return IA_CSS_SUCCESS;
	/* If no firmware, find binary in pipe */
	return ia_css_pipeline_get_stage(pipeline, mode, stage);
}

bool ia_css_pipeline_has_stopped(struct ia_css_pipeline *pipeline)
{
	/* Android compilation files if made an local variable
	stack size on android is limited to 2k and this structure
	is around 2.5K, in place of static malloc can be done but
	if this call is made too often it will lead to fragment memory
	versus a fixed allocation */
	static struct sh_css_sp_group sp_group;
	unsigned int thread_id;
	const struct ia_css_fw_info *fw;
	unsigned int HIVE_ADDR_sp_group;

	fw = &sh_css_sp_fw;
	HIVE_ADDR_sp_group = fw->info.sp.group;

	ia_css_pipeline_get_sp_thread_id(pipeline->pipe_num, &thread_id);
	sp_dmem_load(SP0_ID,
		     (unsigned int)sp_address_of(sp_group),
		     &sp_group, sizeof(struct sh_css_sp_group));
	return sp_group.pipe[thread_id].num_stages == 0;
}

#if defined(USE_INPUT_SYSTEM_VERSION_2401)
struct sh_css_sp_pipeline_io_status *ia_css_pipeline_get_pipe_io_status(void)
{
	return(&sh_css_sp_group.pipe_io_status);
}
#endif

/*******************************************************
*** Static functions
********************************************************/

/* Pipeline:
 * To organize the several different binaries for each type of mode,
 * we use a pipeline. A pipeline contains a number of stages, each with
 * their own binary and frame pointers.
 * When stages are added to a pipeline, output frames that are not passed
 * from outside are automatically allocated.
 * When input frames are not passed from outside, each stage will use the
 * output frame of the previous stage as input (the full resolution output,
 * not the viewfinder output).
 * Pipelines must be cleaned and re-created when settings of the binaries
 * change.
 */
static void pipeline_stage_destroy(struct ia_css_pipeline_stage *stage)
{
	if (stage->out_frame_allocated) {
		ia_css_frame_free(stage->args.out_frame);
		stage->args.out_frame = NULL;
	}
	if (stage->vf_frame_allocated) {
		ia_css_frame_free(stage->args.out_vf_frame);
		stage->args.out_vf_frame = NULL;
	}
	if (stage->binary)
		ia_css_isp_param_destroy_isp_parameters(&stage->binary->mem_params, &stage->binary->css_params);
	sh_css_free(stage);
}

static void pipeline_init_sp_thread_map(void)
{
	unsigned int i;

	for (i = 1; i < SH_CSS_MAX_SP_THREADS; i++)
		pipeline_sp_thread_list[i] = PIPELINE_SP_THREAD_EMPTY_TOKEN;

	for (i = 0; i < IA_CSS_PIPELINE_NUM_MAX; i++)
		pipeline_num_to_sp_thread_map[i] = PIPELINE_NUM_UNMAPPED;
}

static void pipeline_map_num_to_sp_thread(unsigned int pipe_num)
{
	unsigned int i;
	bool found_sp_thread = false;

	/* pipe is not mapped to any thread */
	assert(pipeline_num_to_sp_thread_map[pipe_num]
		== (unsigned)PIPELINE_NUM_UNMAPPED);

	for (i = 0; i < SH_CSS_MAX_SP_THREADS; i++) {
		if (pipeline_sp_thread_list[i] ==
		    PIPELINE_SP_THREAD_EMPTY_TOKEN) {
			pipeline_sp_thread_list[i] =
			    PIPELINE_SP_THREAD_RESERVED_TOKEN;
			pipeline_num_to_sp_thread_map[pipe_num] = i;
			found_sp_thread = true;
			break;
		}
	}

	/* Make sure a mapping is found */
	/* I could do:
		assert(i < SH_CSS_MAX_SP_THREADS);

		But the below is more descriptive.
	*/
	assert(found_sp_thread != false);
}

static void pipeline_unmap_num_to_sp_thread(unsigned int pipe_num)
{
	unsigned int thread_id;
	assert(pipeline_num_to_sp_thread_map[pipe_num]
		!= (unsigned)PIPELINE_NUM_UNMAPPED);

	thread_id = pipeline_num_to_sp_thread_map[pipe_num];
	pipeline_num_to_sp_thread_map[pipe_num] = PIPELINE_NUM_UNMAPPED;
	pipeline_sp_thread_list[thread_id] = PIPELINE_SP_THREAD_EMPTY_TOKEN;
}

static enum ia_css_err pipeline_stage_create(
	struct ia_css_pipeline_stage_desc *stage_desc,
	struct ia_css_pipeline_stage **new_stage)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_pipeline_stage *stage = NULL;
	struct ia_css_binary *binary;
	struct ia_css_frame *vf_frame;
	struct ia_css_frame *out_frame;
	const struct ia_css_fw_info *firmware;

	/* Verify input parameters*/
	if (!(stage_desc->in_frame) && !(stage_desc->firmware)
	    && (stage_desc->binary) && !(stage_desc->binary->online)) {
	    err = IA_CSS_ERR_INTERNAL_ERROR;
		goto ERR;
	}

	binary = stage_desc->binary;
	firmware = stage_desc->firmware;
	vf_frame = stage_desc->vf_frame;
	out_frame = stage_desc->out_frame;

	stage = sh_css_malloc(sizeof(*stage));
	if (stage == NULL) {
		err = IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;
		goto ERR;
	}
	memset(stage, 0, sizeof(*stage));

	if (firmware) {
		stage->binary = NULL;
		stage->binary_info =
		    (struct ia_css_binary_info *)&firmware->info.isp;
	} else {
		stage->binary = binary;
		if (binary)
			stage->binary_info =
			    (struct ia_css_binary_info *)binary->info;
		else
			stage->binary_info = NULL;
	}

	stage->firmware = firmware;
	stage->sp_func = stage_desc->sp_func;
	stage->max_input_width = stage_desc->max_input_width;
	stage->mode = stage_desc->mode;
	stage->out_frame_allocated = false;
	stage->vf_frame_allocated = false;
	stage->next = NULL;
	sh_css_binary_args_reset(&stage->args);

	if (!(out_frame) && (binary)
	    && (binary->out_frame_info.res.width)) {
		err = ia_css_frame_allocate_from_info(&out_frame,
							&binary->out_frame_info);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
		stage->out_frame_allocated = true;
	}
	/* VF frame is not needed in case of need_pp
	   However, the capture binary needs a vf frame to write to.
	 */
	if (!vf_frame) {
		if ((binary && binary->vf_frame_info.res.width) ||
		    (firmware && firmware->info.isp.sp.enable.vf_veceven)
		    ) {
			err = ia_css_frame_allocate_from_info(&vf_frame,
							&binary->vf_frame_info);
			if (err != IA_CSS_SUCCESS)
				goto ERR;
			stage->vf_frame_allocated = true;
		}
	} else if (vf_frame && binary && binary->vf_frame_info.res.width) {
		/* only mark as allocated if buffer pointer available */
		if (vf_frame->data != mmgr_NULL)
			stage->vf_frame_allocated = true;
	}

	stage->args.cc_frame = stage_desc->cc_frame;
	stage->args.in_frame = stage_desc->in_frame;
	stage->args.out_frame = out_frame;
	stage->args.out_vf_frame = vf_frame;
	*new_stage = stage;
	return err;
ERR:
	if (stage != NULL)
		pipeline_stage_destroy(stage);
	return err;
}

static void pipeline_init_defaults(
	struct ia_css_pipeline *pipeline,
	enum ia_css_pipe_id pipe_id,
	unsigned int pipe_num)
{
	struct ia_css_frame init_frame;
	init_frame.dynamic_data_index = SH_CSS_INVALID_FRAME_ID;

	pipeline->pipe_id = pipe_id;
	pipeline->stages = NULL;
	pipeline->stop_requested = false;
	pipeline->current_stage = NULL;
	pipeline->in_frame = init_frame;
	pipeline->out_frame = init_frame;
	pipeline->vf_frame = init_frame;
	pipeline->num_execs = -1;
	pipeline->acquire_isp_each_stage = true;
	pipeline->pipe_num = pipe_num;
}
