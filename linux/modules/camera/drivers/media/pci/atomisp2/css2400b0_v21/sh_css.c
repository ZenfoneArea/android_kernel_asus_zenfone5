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

/*! \file */
//#include "stdio.h"

#include "ia_css.h"
#include "sh_css_hrt.h"		/* only for file 2 MIPI */
#include "ia_css_binary.h"
#include "sh_css_internal.h"
#include "sh_css_sp.h"		/* sh_css_sp_group */
#if !defined(HAS_NO_INPUT_SYSTEM)
#include "ia_css_isys.h"
#endif
#include "ia_css_frame.h"
#include "sh_css_defs.h"
#include "sh_css_firmware.h"
#include "sh_css_params.h"
#include "sh_css_params_internal.h"
#include "sh_css_param_shading.h"
#include "ia_css_refcount.h"
#include "ia_css_rmgr.h"
#include "ia_css_debug.h"
#include "ia_css_debug_pipe.h"
#include "ia_css_memory_access.h"
#include "ia_css_device_access.h"
#include "sh_css_legacy.h"
#include "ia_css_pipeline.h"
#include "ia_css_stream.h"
#include "ia_css_pipe.h"
#include "ia_css_util.h"
#include "ia_css_pipe_util.h"
#include "ia_css_pipe_binarydesc.h"
#include "ia_css_pipe_stagedesc.h"
//#include "ia_css_stream_manager.h"
/* #include "ia_css_rmgr_gen.h" */

#include "memory_access.h"
#include "tag.h"
#include "assert_support.h"
#include "math_support.h"
#include "ia_css_queue.h"
#include "sw_event_global.h"			/* Event IDs.*/
#include "ia_css_eventq.h"              /* ia_css_eventq_recv()*/
#if !defined(HAS_NO_INPUT_FORMATTER)
#include "ia_css_ifmtr.h"
#endif
#if !defined(HAS_NO_INPUT_SYSTEM)
#include "input_system.h"
#endif
#include "mmu_device.h"		/* mmu_set_page_table_base_index(), ... */
#include "gdc_device.h"		/* HRT_GDC_N */
#include "irq.h"			/* virq */
#include "sp.h"				/* cnd_sp_irq_enable() */
#include "isp.h"			/* cnd_isp_irq_enable, ISP_VEC_NELEMS */
#include "gp_device.h"		/* gp_device_reg_store() */
#if !defined(HAS_NO_GPIO)
#define __INLINE_GPIO__
#include "gpio.h"
#endif
#include "timed_ctrl.h"
#include "platform_support.h" /* hrt_sleep() */
#include "ia_css_inputfifo.h"
#define WITH_PC_MONITORING  0

#define SH_CSS_VIDEO_BUFFER_ALIGNMENT 0

#if WITH_PC_MONITORING
#define MULTIPLE_SAMPLES 1
#define NOF_SAMPLES      60
#include "linux/kthread.h"
#include "linux/sched.h"
#include "linux/delay.h"
#include "sh_css_metrics.h"
static int thread_alive;
#endif /* WITH_PC_MONITORING */

#define DVS_REF_TESTING 0
#if DVS_REF_TESTING
#include <stdio.h>
#endif
#include"ia_css_spctrl.h"

/* Name of the sp program: should not be built-in */
#define SP_PROG_NAME "sp"

/* Size of Refcount List */
#define REFCOUNT_SIZE 1000
/*********************************************************/
/* Global Queue objects used by CSS                      */
/*********************************************************/
struct sh_css_queues {
	/* Host2SP buffer queue */
	ia_css_queue_t host2sp_buffer_queue_handles
		[SH_CSS_MAX_SP_THREADS][SH_CSS_NUM_BUFFER_QUEUES];
	/* SP2Host buffer queue */
	ia_css_queue_t sp2host_buffer_queue_handles
		[SH_CSS_NUM_BUFFER_QUEUES];

	/* Host2SP event queue */
	ia_css_queue_t host2sp_event_queue_handle;
	/* SP2Host event queue */
	ia_css_queue_t sp2host_event_queue_handle;
};

/* for JPEG, we don't know the length of the image upfront,
 * but since we support sensor upto 16MP, we take this as
 * upper limit.
 */
#define JPEG_BYTES (16 * 1024 * 1024)

#define STATS_ENABLED(stage) (stage && stage->binary && stage->binary->info && \
	(stage->binary->info->sp.enable.s3a || stage->binary->info->sp.enable.dis))

#define PIPE_ENTRY_EMPTY_TOKEN                (~0)
#define PIPE_ENTRY_RESERVED_TOKEN             (0x1)

#define DEFAULT_IF_CONFIG \
{ \
	0,          /* start_line */\
	0,          /* start_column */\
	0,          /* left_padding */\
	0,          /* cropped_height */\
	0,          /* cropped_width */\
	0,          /* deinterleaving */\
	0,          /*.buf_vecs */\
	0,          /* buf_start_index */\
	0,          /* buf_increment */\
	0,          /* buf_eol_offset */\
	false,      /* is_yuv420_format */\
	false       /* block_no_reqs */\
}

#define DEFAULT_PLANES { {0, 0, 0, 0} }

#define DEFAULT_FRAME \
{ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* info */ \
	0,					/* data */ \
	0,					/* data_bytes */ \
	-1,					/* dynamic_data_index */ \
	IA_CSS_FRAME_FLASH_STATE_NONE,		/* flash_state */ \
	0,					/* exp_id */ \
	false,					/* valid */ \
	false,					/* contiguous  */ \
	{ 0 }					/* planes */ \
}

#define DEFAULT_PIPELINE \
{ \
	IA_CSS_PIPE_ID_PREVIEW, /* pipe_id */ \
	0,			/* pipe_num */ \
	false,			/* stop_requested */ \
	NULL,                   /* stages */ \
	NULL,                   /* current_stage */ \
	0,                      /* num_stages */ \
	DEFAULT_FRAME,          /* in_frame */ \
	DEFAULT_FRAME,          /* out_frame */ \
	DEFAULT_FRAME,          /* vf_frame */ \
	IA_CSS_FRAME_DELAY_1,   /* frame_delay */ \
	0,                      /* inout_port_config */ \
	-1,                     /* num_execs */ \
	true					/* acquire_isp_each_stage */\
}

#define DEFAULT_CAPTURE_CONFIG \
{ \
	IA_CSS_CAPTURE_MODE_PRIMARY,	/* mode (capture) */ \
	false,				/* enable_xnr */ \
	false				/* enable_raw_output */ \
}

#define DEFAULT_PIPE_CONFIG \
{ \
	IA_CSS_PIPE_MODE_PREVIEW,		/* mode */ \
	1,					/* isp_pipe_version */ \
	{ 0, 0 },				/* bayer_ds_out_res */ \
	{ 0, 0 },				/* vf_pp_in_res */ \
	{ 0, 0 },				/* capt_pp_in_res */ \
	{ 0, 0 },				/* dvs_crop_out_res */ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* output_info */ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* vf_output_info */ \
	NULL,					/* acc_extension */ \
	NULL,					/* acc_stages */ \
	0,					/* num_acc_stages */ \
	DEFAULT_CAPTURE_CONFIG,			/* default_capture_config */ \
	{ 0, 0 },				/* dvs_envelope */ \
	IA_CSS_FRAME_DELAY_1,			/* dvs_frame_delay */ \
	-1,					/* acc_num_execs */ \
	true					/* enable_dz */ \
}

#define DEFAULT_PIPE_EXTRA_CONFIG \
{ \
	false,				/* enable_raw_binning */ \
	false,				/* enable_yuv_ds */ \
	false,				/* enable_high_speed */ \
	false,				/* enable_dvs_6axis */ \
	false,				/* enable_reduced_pipe */ \
	false,				/* enable_fractional_ds */ \
	false,				/* disable_vf_pp */ \
}

#if defined(SYSTEM_css_skycam_a0t_system)
#define DEFAULT_3A_GRID_INFO \
{ \
 	0,				/* ae_enable */ \
   	{0,0,0,0,0,0,0,0},	        /* AE:     width,height,b_width,b_height,x_start,y_start,x_end,y_end*/ \
	0,				/* awb_enable */ \
	{0,0,0,0,0,0,0,0},              /* AWB:    width,height,b_width,b_height,x_start,y_start,x_end,y_end*/ \
	0,				/* af_enable */ \
	{0,0,0,0,0,0},			/* AF:     width,height,b_width,b_height,x_start,y_start*/ \
  	0,				/* awb_fr_enable */ \
	{0,0,0,0,0,0},                  /* AWB_FR: width,height,b_width,b_height,x_start,y_start*/ \
  	0,				/* elem_bit_depth */ \
}
#else
#define DEFAULT_3A_GRID_INFO \
{ \
	0,				/* enable */ \
	0,				/* use_dmem */ \
	0,				/* use_histogram */ \
	0,				/* width */ \
	0,				/* height */ \
	0,				/* aligned_width */ \
	0,				/* aligned_height */ \
	0,				/* bqs_per_grid_cell */ \
	0,				/* deci_factor_log2 */ \
	0,				/* elem_bit_depth */ \
}

#endif

#define DEFAULT_DVS_GRID_INFO \
{ \
	0,				/* enable */ \
	0,				/* width */ \
	0,				/* aligned_width */ \
	0,				/* height */ \
	0,				/* aligned_height */ \
	0,				/* bqs_per_grid_cell */ \
	0,				/* num_hor_coefs */ \
	0,				/* num_ver_coefs */ \
}

#define DEFAULT_GRID_INFO \
{ \
	0,				/* isp_in_width */ \
	0,				/* isp_in_height */ \
	DEFAULT_3A_GRID_INFO,		/* s3a_grid */ \
	DEFAULT_DVS_GRID_INFO,		/* dvs_grid */ \
	IA_CSS_VAMEM_TYPE_1		/* vamem_type */ \
}

#define DEFAULT_PIPE_INFO \
{ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* output_info */ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* vf_output_info */ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* raw_output_info */ \
	DEFAULT_GRID_INFO			/* grid_info */ \
}

#define DEFAULT_PIPE \
{ \
	false,					/* stop_requested */ \
	DEFAULT_PIPE_CONFIG,			/* config */ \
	DEFAULT_PIPE_EXTRA_CONFIG,		/* extra_config */ \
	DEFAULT_PIPE_INFO,			/* info */ \
	IA_CSS_PIPE_ID_ACC,			/* mode (pipe_id) */ \
	NULL,					/* shading_table */ \
	DEFAULT_PIPELINE,			/* pipeline */ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* output_info */ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* bds_output_info */ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* vf_output_info */ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* out_yuv_ds_input_info */ \
	IA_CSS_BINARY_DEFAULT_FRAME_INFO,	/* vf_yuv_ds_input_info */ \
	NULL,					/* output_stage */ \
	NULL,					/* vf_stage */ \
	SH_CSS_BDS_FACTOR_1_00,			/* required_bds_factor */ \
	IA_CSS_FRAME_DELAY_1,			/* dvs_frame_delay */ \
	0,					/* num_invalid_frames */ \
	true,					/* enable_viewfinder */ \
	NULL,					/* stream */ \
	DEFAULT_FRAME,				/* in_frame_struct */ \
	DEFAULT_FRAME,				/* out_frame_struct */ \
	DEFAULT_FRAME,				/* vf_frame_struct */ \
	{ NULL },				/* continuous_frames */ \
	{ DEFAULT_PREVIEW_SETTINGS },		/* pipe_settings */ \
	PIPE_ENTRY_EMPTY_TOKEN,				/* pipe_num */\
}

#define DEFAULT_PREVIEW_SETTINGS \
{ \
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* copy_binary */\
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* preview_binary */\
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* vf_pp_binary */\
	NULL,				/* copy_pipe */\
	NULL,				/* capture_pipe */\
}

#define DEFAULT_CAPTURE_SETTINGS \
{ \
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* copy_binary */\
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* primary_binary */\
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* pre_isp_binary */\
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* anr_gdc_binary */\
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* post_isp_binary */\
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* capture_pp_binary */\
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* vf_pp_binary */\
}

#define DEFAULT_VIDEO_SETTINGS \
{ \
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* copy_binary */ \
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* video_binary */ \
	IA_CSS_BINARY_DEFAULT_SETTINGS,	/* vf_pp_binary */ \
	{ NULL },			/* ref_frames */ \
	{ NULL },			/* tnr_frames */ \
	NULL,				/* vf_pp_in_frame */ \
	NULL,				/* copy_pipe */ \
	NULL,				/* capture_pipe */ \
}

struct sh_css {
	struct ia_css_pipe            *active_pipes[IA_CSS_PIPELINE_NUM_MAX];
	/* All of the pipes created at any point of time. At this moment there can
	 * be no more than MAX_SP_THREADS of them because pipe_num is reused as SP
	 * thread_id to which a pipe's pipeline is associated. At a later point, if
	 * we support more pipe objects, we should add test code to test that
	 * possibility. Also, active_pipes[] should be able to hold only
	 * SH_CSS_MAX_SP_THREADS objects. Anything else is misleading. */
	struct ia_css_pipe            *all_pipes[IA_CSS_PIPELINE_NUM_MAX];
	void *(*malloc) (size_t bytes, bool zero_mem);
	void (*free) (void *ptr);
	void (*flush) (struct ia_css_acc_fw *fw);
	bool                           check_system_idle;
	bool                           stop_copy_preview;
	unsigned int                   num_cont_raw_frames;
	unsigned int                   num_mipi_frames;
	struct ia_css_frame           *mipi_frames[NUM_MIPI_FRAMES];
	hrt_vaddress                   sp_bin_addr;
	hrt_data                       page_table_base_index;
	unsigned int                   size_mem_words;
#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)
	unsigned int                   mipi_sizes_for_check[N_CSI_PORTS][IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT];
#endif
	bool                           contiguous;
	enum ia_css_irq_type           irq_type;
	unsigned int                   pipe_counter;
	struct sh_css_queues	       queues;
};

#if defined(HAS_RX_VERSION_2)

#define DEFAULT_MIPI_CONFIG \
{ \
	MONO_4L_1L_0L, \
	MIPI_PORT0_ID, \
	0xffff4, \
	0, \
	0x28282828, \
	0x04040404, \
	MIPI_PREDICTOR_NONE, \
	false \
}

#elif defined(HAS_RX_VERSION_1) || defined(HAS_NO_RX)

#define DEFAULT_MIPI_CONFIG \
{ \
	MIPI_PORT1_ID, \
	1, \
	0xffff4, \
	0, \
	0, \
	MIPI_PREDICTOR_NONE, \
	false \
}

#else
#error "sh_css.c: RX version must be one of {RX_VERSION_1, RX_VERSION_2, NO_RX}"
#endif

int (*sh_css_printf) (const char *fmt, va_list args) = NULL;

static struct sh_css my_css;

/* pqiao NOTICE: this is for css internal buffer recycling when stopping pipeline,
   this array is temporary and will be replaced by resource manager*/
#define MAX_HMM_BUFFER_NUM (SH_CSS_NUM_BUFFER_QUEUES * (SH_CSS_CIRCULAR_BUF_NUM_ELEMS + 2))
static struct ia_css_rmgr_vbuf_handle *hmm_buffer_record_h[MAX_HMM_BUFFER_NUM];

static uint32_t ref_count_mipi_allocation = 0;

#define GPIO_FLASH_PIN_MASK (1 << HIVE_GPIO_STROBE_TRIGGER_PIN)

static enum sh_css_buffer_queue_id
	sh_css_buf_type_2_internal_queue_id[IA_CSS_BUFFER_TYPE_NUM] = {
		sh_css_s3a_buffer_queue,
		sh_css_dis_buffer_queue,
		sh_css_input_buffer_queue,
		sh_css_output_buffer_queue,
		sh_css_vf_output_buffer_queue,
		sh_css_output_buffer_queue,
		sh_css_input_buffer_queue,
		sh_css_output_buffer_queue,
#if defined (SH_CSS_ENABLE_METADATA)
		sh_css_metadata_buffer_queue,
#else
		sh_css_invalid_buffer_queue,
#endif
		sh_css_param_buffer_queue };

static bool fw_explicitly_loaded = false;

/**
 * Local prototypes
 */
static enum ia_css_err
ia_css_pipe_load_extension(struct ia_css_pipe *pipe,
			   struct ia_css_fw_info *firmware);

static void
ia_css_pipe_unload_extension(struct ia_css_pipe *pipe,
			     struct ia_css_fw_info *firmware);
static void
ia_css_reset_defaults(struct sh_css* css);

static void
sh_css_init_host_sp_control_vars(void);

static void
sh_css_mmu_set_page_table_base_index(hrt_data base_index);

static bool
need_capture_pp(const struct ia_css_pipe *pipe);

static enum ia_css_err
sh_css_pipe_load_binaries(struct ia_css_pipe *pipe);

static
enum ia_css_err sh_css_pipe_get_viewfinder_frame_info(
	struct ia_css_pipe *pipe,
	struct ia_css_frame_info *info);

static enum ia_css_err
sh_css_pipe_get_output_frame_info(struct ia_css_pipe *pipe,
				  struct ia_css_frame_info *info);

static enum ia_css_err
capture_start(struct ia_css_pipe *pipe);

static enum ia_css_err
video_start(struct ia_css_pipe *pipe);

static bool copy_on_sp(struct ia_css_pipe *pipe);

static enum ia_css_err
init_vf_frameinfo_defaults(struct ia_css_pipe *pipe,
	struct ia_css_frame *vf_frame);

static enum ia_css_err
init_in_frameinfo_memory_defaults(struct ia_css_pipe *pipe,
	struct ia_css_frame *frame);

static enum ia_css_err
init_out_frameinfo_defaults(struct ia_css_pipe *pipe,
	struct ia_css_frame *out_frame);

static enum ia_css_err
sh_css_pipeline_add_acc_stage(struct ia_css_pipeline *pipeline,
			      const void *acc_fw);

static enum ia_css_err
alloc_continuous_frames(
	struct ia_css_pipe *pipe, bool init_time);

static enum ia_css_err
allocate_mipi_frames(struct ia_css_pipe *pipe);

static void
pipe_global_init(void);

static uint8_t
pipe_generate_pipe_num(const struct ia_css_pipe *pipe);

static void
pipe_release_pipe_num(unsigned int pipe_num);

static enum ia_css_err
create_host_pipeline_object(struct ia_css_pipe *pipe);

static enum ia_css_err
create_host_pipeline_structure(struct ia_css_stream *stream);

static enum ia_css_err
create_host_pipeline(struct ia_css_stream *stream);

static enum ia_css_err
create_host_preview_pipeline(struct ia_css_pipe *pipe);

static enum ia_css_err
create_host_video_pipeline(struct ia_css_pipe *pipe);

static enum ia_css_err
create_host_copy_pipeline(struct ia_css_pipe *pipe,
    unsigned max_input_width,
    struct ia_css_frame *out_frame);

static enum ia_css_err
create_host_isyscopy_capture_pipeline(struct ia_css_pipe *pipe);

static enum ia_css_err
create_host_capture_pipeline(struct ia_css_pipe *pipe);

static enum ia_css_err
create_host_acc_pipeline(struct ia_css_pipe *pipe);

static void
free_mipi_frames(struct ia_css_pipe *pipe, bool uninit);

static unsigned int
sh_css_get_sw_interrupt_value(unsigned int irq);

#if 0
static enum ia_css_err
sh_css_pipeline_stop(struct ia_css_pipe *pipe);
#endif

static struct ia_css_binary *
ia_css_pipe_get_3a_binary (const struct ia_css_pipe *pipe);

static void
sh_css_pipe_free_shading_table(struct ia_css_pipe *pipe)
{
	assert(pipe != NULL);

	if (pipe->shading_table)
		ia_css_shading_table_free(pipe->shading_table);
	pipe->shading_table = NULL;
}

static enum ia_css_frame_format yuv420_copy_formats[] = {
	IA_CSS_FRAME_FORMAT_NV12,
	IA_CSS_FRAME_FORMAT_NV21,
	IA_CSS_FRAME_FORMAT_YV12,
	IA_CSS_FRAME_FORMAT_YUV420,
	IA_CSS_FRAME_FORMAT_YUV420_16
};

static enum ia_css_frame_format yuv422_copy_formats[] = {
	IA_CSS_FRAME_FORMAT_NV12,
	IA_CSS_FRAME_FORMAT_NV16,
	IA_CSS_FRAME_FORMAT_NV21,
	IA_CSS_FRAME_FORMAT_NV61,
	IA_CSS_FRAME_FORMAT_YV12,
	IA_CSS_FRAME_FORMAT_YV16,
	IA_CSS_FRAME_FORMAT_YUV420,
	IA_CSS_FRAME_FORMAT_YUV420_16,
	IA_CSS_FRAME_FORMAT_YUV422,
	IA_CSS_FRAME_FORMAT_YUV422_16,
	IA_CSS_FRAME_FORMAT_UYVY,
	IA_CSS_FRAME_FORMAT_YUYV
};

#define array_length(array) (sizeof(array)/sizeof(array[0]))

/* Verify whether the selected output format is can be produced
 * by the copy binary given the stream format.
 * */
static enum ia_css_err
verify_copy_out_frame_format(struct ia_css_pipe *pipe)
{
	enum ia_css_frame_format out_fmt = pipe->output_info.format;
	unsigned int i, found = 0;

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	switch (pipe->stream->config.format) {
	case IA_CSS_STREAM_FORMAT_YUV420_8_LEGACY:
	case IA_CSS_STREAM_FORMAT_YUV420_8:
		for (i=0; i<array_length(yuv420_copy_formats) && !found; i++)
			found = (out_fmt == yuv420_copy_formats[i]);
		break;
	case IA_CSS_STREAM_FORMAT_YUV420_10:
		found = (out_fmt == IA_CSS_FRAME_FORMAT_YUV420_16);
		break;
	case IA_CSS_STREAM_FORMAT_YUV422_8:
		for (i=0; i<array_length(yuv422_copy_formats) && !found; i++)
			found = (out_fmt == yuv422_copy_formats[i]);
		break;
	case IA_CSS_STREAM_FORMAT_YUV422_10:
		found = (out_fmt == IA_CSS_FRAME_FORMAT_YUV422_16 ||
			 out_fmt == IA_CSS_FRAME_FORMAT_YUV420_16);
		break;
	case IA_CSS_STREAM_FORMAT_RGB_444:
	case IA_CSS_STREAM_FORMAT_RGB_555:
	case IA_CSS_STREAM_FORMAT_RGB_565:
		found = (out_fmt == IA_CSS_FRAME_FORMAT_RGBA888 ||
			 out_fmt == IA_CSS_FRAME_FORMAT_RGB565);
		break;
	case IA_CSS_STREAM_FORMAT_RGB_666:
	case IA_CSS_STREAM_FORMAT_RGB_888:
		found = (out_fmt == IA_CSS_FRAME_FORMAT_RGBA888);
		break;
	case IA_CSS_STREAM_FORMAT_RAW_6:
	case IA_CSS_STREAM_FORMAT_RAW_7:
	case IA_CSS_STREAM_FORMAT_RAW_8:
	case IA_CSS_STREAM_FORMAT_RAW_10:
	case IA_CSS_STREAM_FORMAT_RAW_12:
	case IA_CSS_STREAM_FORMAT_RAW_14:
	case IA_CSS_STREAM_FORMAT_RAW_16:
		found = (out_fmt == IA_CSS_FRAME_FORMAT_RAW) ||
			(out_fmt == IA_CSS_FRAME_FORMAT_RAW_PACKED);
		break;
	case IA_CSS_STREAM_FORMAT_BINARY_8:
		found = (out_fmt == IA_CSS_FRAME_FORMAT_BINARY_8);
		break;
	default:
		break;
	}
	if (!found)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	return IA_CSS_SUCCESS;
}

/* next function takes care of getting the settings from kernel
 * commited to hmm / isp
 * TODO: see if needs to be made public
 */
static enum ia_css_err
sh_css_commit_isp_config(struct ia_css_stream *stream,
			 struct ia_css_pipeline *pipeline)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_pipeline_stage *stage;

	if (pipeline) {
		/* walk through pipeline and commit settings */
		/* TODO: check if this is needed (s3a is handled through this */
		for (stage = pipeline->stages; stage; stage = stage->next) {
			if (stage && stage->binary) {
				err = sh_css_params_write_to_ddr(stream,
								 stage);
				if (err != IA_CSS_SUCCESS)
					return err;
			}
		}
	}
	return err;
}

static void
configure_pipe_inout_port(struct ia_css_pipeline *me, bool continuous)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"configure_pipe_inout_port() enter: pipe_id(%d) continuous(%d)\n",
			me->pipe_id, continuous);
	switch(me->pipe_id) {
		case IA_CSS_PIPE_ID_PREVIEW:
		case IA_CSS_PIPE_ID_VIDEO:
			SH_CSS_PIPE_PORT_CONFIG_SET(me->inout_port_config,
						   (uint8_t)SH_CSS_PORT_INPUT,
						   (uint8_t)(continuous ? SH_CSS_COPYSINK_TYPE : SH_CSS_HOST_TYPE),1);
			SH_CSS_PIPE_PORT_CONFIG_SET(me->inout_port_config,
						   (uint8_t)SH_CSS_PORT_OUTPUT,
						   (uint8_t)SH_CSS_HOST_TYPE,1);
			break;
		case IA_CSS_PIPE_ID_COPY: /*Copy pipe ports configured to "offline" mode*/
			SH_CSS_PIPE_PORT_CONFIG_SET(me->inout_port_config,
						   (uint8_t)SH_CSS_PORT_INPUT,
						   (uint8_t)SH_CSS_HOST_TYPE,1);
			if (continuous) {
				SH_CSS_PIPE_PORT_CONFIG_SET(me->inout_port_config,
						   (uint8_t)SH_CSS_PORT_OUTPUT,
						   (uint8_t)SH_CSS_COPYSINK_TYPE,1);
				SH_CSS_PIPE_PORT_CONFIG_SET(me->inout_port_config,
						   (uint8_t)SH_CSS_PORT_OUTPUT,
						   (uint8_t)SH_CSS_TAGGERSINK_TYPE,1);
			} else {
				SH_CSS_PIPE_PORT_CONFIG_SET(me->inout_port_config,
						   (uint8_t)SH_CSS_PORT_OUTPUT,
						   (uint8_t)SH_CSS_HOST_TYPE,1);
			}
			break;
		case IA_CSS_PIPE_ID_CAPTURE:
			SH_CSS_PIPE_PORT_CONFIG_SET(me->inout_port_config,
						   (uint8_t)SH_CSS_PORT_INPUT,
						   (uint8_t)(continuous ? SH_CSS_TAGGERSINK_TYPE : SH_CSS_HOST_TYPE),1);
			SH_CSS_PIPE_PORT_CONFIG_SET(me->inout_port_config,
						   (uint8_t)SH_CSS_PORT_OUTPUT,
						   (uint8_t)SH_CSS_HOST_TYPE,1);
			break;
		case IA_CSS_PIPE_ID_ACC:
			SH_CSS_PIPE_PORT_CONFIG_SET(me->inout_port_config,
						   (uint8_t)SH_CSS_PORT_INPUT,
						   (uint8_t)SH_CSS_HOST_TYPE,1);
			SH_CSS_PIPE_PORT_CONFIG_SET(me->inout_port_config,
						   (uint8_t)SH_CSS_PORT_OUTPUT,
						   (uint8_t)SH_CSS_HOST_TYPE,1);
			break;
		default:
			break;
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"configure_pipe_inout_port() leave: inout_port_config(%x)\n",
		me->inout_port_config);
}

unsigned int
ia_css_stream_input_format_bits_per_pixel(struct ia_css_stream *stream)
{
	int bpp = 0;

	if (stream != NULL)
		bpp = ia_css_util_input_format_bpp(stream->config.format,
						stream->config.pixels_per_clock == 2);

	return bpp;
}

#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)
static enum ia_css_err
sh_css_config_input_network(struct ia_css_pipe *pipe,
			    struct ia_css_binary *binary)
{
	unsigned int fmt_type;
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"sh_css_config_input_network() enter:\n");

	if (pipe->pipeline.stages)
		binary = pipe->pipeline.stages->binary;

	err = ia_css_isys_convert_stream_format_to_mipi_format(
				pipe->stream->config.format,
				pipe->stream->csi_rx_config.comp,
				&fmt_type);
	if (err != IA_CSS_SUCCESS)
		return err;
	sh_css_sp_program_input_circuit(fmt_type,
					pipe->stream->config.channel_id,
					pipe->stream->config.mode);

	if (binary && (binary->online || pipe->stream->config.continuous)) {
		err = ia_css_ifmtr_configure(&pipe->stream->config,
			binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	if (pipe->stream->config.mode == IA_CSS_INPUT_MODE_TPG ||
	    pipe->stream->config.mode == IA_CSS_INPUT_MODE_PRBS) {
		unsigned int hblank_cycles = 100,
			     vblank_lines = 6,
			     width,
			     height,
			     vblank_cycles;
		width  = (pipe->stream->config.input_res.width) / (1 + (pipe->stream->config.pixels_per_clock == 2));
		height = pipe->stream->config.input_res.height;
		vblank_cycles = vblank_lines * (width + hblank_cycles);
		sh_css_sp_configure_sync_gen(width, height, hblank_cycles,
					     vblank_cycles);
#if defined(IS_ISP_2400_SYSTEM)
		if (pipe->stream->config.mode == IA_CSS_INPUT_MODE_TPG) {
			/* TODO: move define to proper file in tools */
			#define GP_ISEL_TPG_MODE 0x90058
			ia_css_device_store_uint32(GP_ISEL_TPG_MODE, 0);
		}
#endif
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"sh_css_config_input_network() leave:\n");
	return IA_CSS_SUCCESS;
}
#elif !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2401)
static mipi_predictor_t sh_css_csi2_compression_type_2_mipi_predictor(enum ia_css_csi2_compression_type type)
{
	mipi_predictor_t predictor = MIPI_PREDICTOR_NONE;

	switch (type) {
	case IA_CSS_CSI2_COMPRESSION_TYPE_NONE:
		predictor = MIPI_PREDICTOR_NONE;
		break;
	case IA_CSS_CSI2_COMPRESSION_TYPE_1:
		predictor = MIPI_PREDICTOR_TYPE1;
		break;
	case IA_CSS_CSI2_COMPRESSION_TYPE_2:
		predictor = MIPI_PREDICTOR_TYPE2;
	default:
		break;
	}
	return predictor;
}

static unsigned int sh_css_stream_format_2_bits_per_subpixel(
		enum ia_css_stream_format format)
{
	unsigned int rval;

	switch (format) {
	case IA_CSS_STREAM_FORMAT_RGB_444:
		rval = 4;
		break;
	case IA_CSS_STREAM_FORMAT_RGB_555:
		rval = 5;
		break;
	case IA_CSS_STREAM_FORMAT_RGB_565:
	case IA_CSS_STREAM_FORMAT_RGB_666:
	case IA_CSS_STREAM_FORMAT_RAW_6:
		rval = 6;
		break;
	case IA_CSS_STREAM_FORMAT_RAW_7:
		rval = 7;
		break;
	case IA_CSS_STREAM_FORMAT_YUV420_8_LEGACY:
	case IA_CSS_STREAM_FORMAT_YUV420_8:
	case IA_CSS_STREAM_FORMAT_YUV422_8:
	case IA_CSS_STREAM_FORMAT_RGB_888:
	case IA_CSS_STREAM_FORMAT_RAW_8:
		rval = 8;
		break;
	case IA_CSS_STREAM_FORMAT_YUV420_10:
	case IA_CSS_STREAM_FORMAT_YUV422_10:
	case IA_CSS_STREAM_FORMAT_RAW_10:
		rval = 10;
		break;
	case IA_CSS_STREAM_FORMAT_RAW_12:
		rval = 12;
		break;
	case IA_CSS_STREAM_FORMAT_RAW_14:
		rval = 14;
		break;
	case IA_CSS_STREAM_FORMAT_RAW_16:
		rval = 16;
		break;
	default:
		rval = 0;
		break;
	}

	return rval;
}

static unsigned int csi2_protocol_calculate_max_subpixels_per_line(
		enum ia_css_stream_format	format,
		unsigned int			pixels_per_line)
{
	unsigned int rval;

	switch (format) {
	case IA_CSS_STREAM_FORMAT_YUV420_8_LEGACY:
		/*
		 * The frame format layout is shown below.
		 *
		 *		Line	0:	UYY UYY ... UYY
		 *		Line	1:	VYY VYY ... VYY
		 *		Line	2:	UYY UYY ... UYY
		 *		Line	3:	VYY VYY ... VYY
		 *		...
		 *		Line (n-2):	UYY UYY ... UYY
		 *		Line (n-1):	VYY VYY ... VYY
		 *
		 *	In this frame format, the even-line is
		 *	as wide as the odd-line.
		 */
		rval = pixels_per_line + pixels_per_line / 2;
		break;
	case IA_CSS_STREAM_FORMAT_YUV420_8:
	case IA_CSS_STREAM_FORMAT_YUV420_10:
		/*
		 * The frame format layout is shown below.
		 *
		 *		Line	0:	YYYY YYYY ... YYYY
		 *		Line	1:	UYVY UYVY ... UYVY UYVY
		 *		Line	2:	YYYY YYYY ... YYYY
		 *		Line	3:	UYVY UYVY ... UYVY UYVY
		 *		...
		 *		Line (n-2):	YYYY YYYY ... YYYY
		 *		Line (n-1):	UYVY UYVY ... UYVY UYVY
		 *
		 * In this frame format, the odd-line is twice
		 * wider than the even-line.
		 */
		rval = pixels_per_line * 2;
		break;
	case IA_CSS_STREAM_FORMAT_YUV422_8:
	case IA_CSS_STREAM_FORMAT_YUV422_10:
		/*
		 * The frame format layout is shown below.
		 *
		 *		Line	0:	UYVY UYVY ... UYVY
		 *		Line	1:	UYVY UYVY ... UYVY
		 *		Line	2:	UYVY UYVY ... UYVY
		 *		Line	3:	UYVY UYVY ... UYVY
		 *		...
		 *		Line (n-2):	UYVY UYVY ... UYVY
		 *		Line (n-1):	UYVY UYVY ... UYVY
		 *
		 * In this frame format, the even-line is
		 * as wide as the odd-line.
		 */
		rval = pixels_per_line * 2;
		break;
	case IA_CSS_STREAM_FORMAT_RGB_444:
	case IA_CSS_STREAM_FORMAT_RGB_555:
	case IA_CSS_STREAM_FORMAT_RGB_565:
	case IA_CSS_STREAM_FORMAT_RGB_666:
	case IA_CSS_STREAM_FORMAT_RGB_888:
		/*
		 * The frame format layout is shown below.
		 *
		 *		Line	0:	ABGR ABGR ... ABGR
		 *		Line	1:	ABGR ABGR ... ABGR
		 *		Line	2:	ABGR ABGR ... ABGR
		 *		Line	3:	ABGR ABGR ... ABGR
		 *		...
		 *		Line (n-2):	ABGR ABGR ... ABGR
		 *		Line (n-1):	ABGR ABGR ... ABGR
		 *
		 * In this frame format, the even-line is
		 * as wide as the odd-line.
		 */
		rval = pixels_per_line * 4;
		break;
	case IA_CSS_STREAM_FORMAT_RAW_6:
	case IA_CSS_STREAM_FORMAT_RAW_7:
	case IA_CSS_STREAM_FORMAT_RAW_8:
	case IA_CSS_STREAM_FORMAT_RAW_10:
	case IA_CSS_STREAM_FORMAT_RAW_12:
	case IA_CSS_STREAM_FORMAT_RAW_14:
	case IA_CSS_STREAM_FORMAT_RAW_16:
		/*
		 * The frame format layout is shown below.
		 *
		 *		Line	0:	Pixel Pixel ... Pixel
		 *		Line	1:	Pixel Pixel ... Pixel
		 *		Line	2:	Pixel Pixel ... Pixel
		 *		Line	3:	Pixel Pixel ... Pixel
		 *		...
		 *		Line (n-2):	Pixel Pixel ... Pixel
		 *		Line (n-1):	Pixel Pixel ... Pixel
		 *
		 * In this frame format, the even-line is
		 * as wide as the odd-line.
		 */
		rval = pixels_per_line;
		break;
	default:
		rval = 0;
		break;
	}

	return rval;
}

static bool sh_css_translate_stream_cfg_to_input_system_input_port_id(
		struct ia_css_stream_config *stream_cfg,
		ia_css_isys_descr_t	*isys_stream_descr)
{
	bool rc;

	rc = true;
	switch (stream_cfg->mode) {
	case IA_CSS_INPUT_MODE_TPG:

		if (stream_cfg->source.tpg.id == IA_CSS_TPG_ID0) {
			isys_stream_descr->input_port_id = INPUT_SYSTEM_PIXELGEN_PORT0_ID;
		} else if (stream_cfg->source.tpg.id == IA_CSS_TPG_ID1) {
			isys_stream_descr->input_port_id = INPUT_SYSTEM_PIXELGEN_PORT1_ID;
		} else if (stream_cfg->source.tpg.id == IA_CSS_TPG_ID2) {
			isys_stream_descr->input_port_id = INPUT_SYSTEM_PIXELGEN_PORT2_ID;
		}

		break;
	case IA_CSS_INPUT_MODE_PRBS:

		if (stream_cfg->source.prbs.id == IA_CSS_PRBS_ID0) {
			isys_stream_descr->input_port_id = INPUT_SYSTEM_PIXELGEN_PORT0_ID;
		} else if (stream_cfg->source.prbs.id == IA_CSS_PRBS_ID1) {
			isys_stream_descr->input_port_id = INPUT_SYSTEM_PIXELGEN_PORT1_ID;
		} else if (stream_cfg->source.prbs.id == IA_CSS_PRBS_ID2) {
			isys_stream_descr->input_port_id = INPUT_SYSTEM_PIXELGEN_PORT2_ID;
		}

		break;
	case IA_CSS_INPUT_MODE_BUFFERED_SENSOR:

		if (stream_cfg->source.port.port == IA_CSS_CSI2_PORT0) {
			isys_stream_descr->input_port_id = INPUT_SYSTEM_CSI_PORT0_ID;
		} else if (stream_cfg->source.port.port == IA_CSS_CSI2_PORT1) {
			isys_stream_descr->input_port_id = INPUT_SYSTEM_CSI_PORT1_ID;
		} else if (stream_cfg->source.port.port == IA_CSS_CSI2_PORT2) {
			isys_stream_descr->input_port_id = INPUT_SYSTEM_CSI_PORT2_ID;
		}

		break;
	default:
		rc = false;
		break;
	}

	return rc;
}

static bool sh_css_translate_stream_cfg_to_input_system_input_port_type(
		struct ia_css_stream_config *stream_cfg,
		ia_css_isys_descr_t	*isys_stream_descr)
{
	bool rc;

	rc = true;
	switch (stream_cfg->mode) {
	case IA_CSS_INPUT_MODE_TPG:

		isys_stream_descr->mode = INPUT_SYSTEM_SOURCE_TYPE_TPG;

		break;
	case IA_CSS_INPUT_MODE_PRBS:

		isys_stream_descr->mode = INPUT_SYSTEM_SOURCE_TYPE_PRBS;

		break;
	case IA_CSS_INPUT_MODE_BUFFERED_SENSOR:

		isys_stream_descr->mode = INPUT_SYSTEM_SOURCE_TYPE_SENSOR;

		break;
	default:
		rc = false;
		break;
	}

	return rc;
}

static bool sh_css_translate_stream_cfg_to_input_system_input_port_attr(
		struct ia_css_stream_config *stream_cfg,
		ia_css_isys_descr_t	*isys_stream_descr)
{
	bool rc;

	rc = true;
	switch (stream_cfg->mode) {
	case IA_CSS_INPUT_MODE_TPG:
		if (stream_cfg->source.tpg.mode == IA_CSS_TPG_MODE_RAMP) {
			isys_stream_descr->tpg_port_attr.mode = PIXELGEN_TPG_MODE_RAMP;
		} else if (stream_cfg->source.tpg.mode == IA_CSS_TPG_MODE_CHECKERBOARD) {
			isys_stream_descr->tpg_port_attr.mode = PIXELGEN_TPG_MODE_CHBO;
		} else if (stream_cfg->source.tpg.mode == IA_CSS_TPG_MODE_MONO) {
			isys_stream_descr->tpg_port_attr.mode = PIXELGEN_TPG_MODE_MONO;
		} else {
			rc = false;
		}

		/*
		 * zhengjie.lu@intel.com:
		 *
		 * TODO
		 * - Make "color_cfg" as part of "ia_css_tpg_config".
		 */
		isys_stream_descr->tpg_port_attr.color_cfg.R1 = 51;
		isys_stream_descr->tpg_port_attr.color_cfg.G1 = 102;
		isys_stream_descr->tpg_port_attr.color_cfg.B1 = 255;
		isys_stream_descr->tpg_port_attr.color_cfg.R2 = 0;
		isys_stream_descr->tpg_port_attr.color_cfg.G2 = 100;
		isys_stream_descr->tpg_port_attr.color_cfg.B2 = 160;

		isys_stream_descr->tpg_port_attr.mask_cfg.h_mask = stream_cfg->source.tpg.x_mask;
		isys_stream_descr->tpg_port_attr.mask_cfg.v_mask = stream_cfg->source.tpg.y_mask;
		isys_stream_descr->tpg_port_attr.mask_cfg.hv_mask = stream_cfg->source.tpg.xy_mask;

		isys_stream_descr->tpg_port_attr.delta_cfg.h_delta = stream_cfg->source.tpg.x_delta;
		isys_stream_descr->tpg_port_attr.delta_cfg.v_delta = stream_cfg->source.tpg.y_delta;

		/*
		 * zhengjie.lu@intel.com:
		 *
		 * TODO
		 * - Make "sync_gen_cfg" as part of "ia_css_tpg_config".
		 */
		isys_stream_descr->tpg_port_attr.sync_gen_cfg.hblank_cycles = 100;
		isys_stream_descr->tpg_port_attr.sync_gen_cfg.vblank_cycles = 100;
		isys_stream_descr->tpg_port_attr.sync_gen_cfg.pixels_per_clock = stream_cfg->pixels_per_clock;
		isys_stream_descr->tpg_port_attr.sync_gen_cfg.nr_of_frames = ~(0x0);
		isys_stream_descr->tpg_port_attr.sync_gen_cfg.pixels_per_line = stream_cfg->input_res.width;
		isys_stream_descr->tpg_port_attr.sync_gen_cfg.lines_per_frame = stream_cfg->input_res.height;

		break;
	case IA_CSS_INPUT_MODE_PRBS:

		isys_stream_descr->prbs_port_attr.seed0 = stream_cfg->source.prbs.seed;
		isys_stream_descr->prbs_port_attr.seed1 = stream_cfg->source.prbs.seed1;

		/*
		 * zhengjie.lu@intel.com:
		 *
		 * TODO
		 * - Make "sync_gen_cfg" as part of "ia_css_prbs_config".
		 */
		isys_stream_descr->prbs_port_attr.sync_gen_cfg.hblank_cycles = 100;
		isys_stream_descr->prbs_port_attr.sync_gen_cfg.vblank_cycles = 100;
		isys_stream_descr->prbs_port_attr.sync_gen_cfg.pixels_per_clock = stream_cfg->pixels_per_clock;
		isys_stream_descr->prbs_port_attr.sync_gen_cfg.nr_of_frames = ~(0x0);
		isys_stream_descr->prbs_port_attr.sync_gen_cfg.pixels_per_line = stream_cfg->input_res.width;
		isys_stream_descr->prbs_port_attr.sync_gen_cfg.lines_per_frame = stream_cfg->input_res.height;

		break;
	case IA_CSS_INPUT_MODE_BUFFERED_SENSOR:
	{
		enum ia_css_err err;
		unsigned int fmt_type;

		err = ia_css_isys_convert_stream_format_to_mipi_format(
			stream_cfg->format,
			sh_css_csi2_compression_type_2_mipi_predictor(stream_cfg->source.port.compression.type),
				&fmt_type);
		if (err != IA_CSS_SUCCESS)
			rc = false;

		isys_stream_descr->csi_port_attr.active_lanes = stream_cfg->source.port.num_lanes;
		isys_stream_descr->csi_port_attr.fmt_type = fmt_type;
		isys_stream_descr->csi_port_attr.ch_id = stream_cfg->channel_id;

		break;
	}
	default:
		rc = false;
		break;
	}

	return rc;
}

static bool sh_css_translate_stream_cfg_to_input_system_input_port_resolution(
		struct ia_css_stream_config *stream_cfg,
		ia_css_isys_descr_t	*isys_stream_descr)
{
	unsigned int bits_per_subpixel;
	/* unsigned int subpixels_per_color_pixel; */
	/* unsigned int color_pixels_per_line; */
	unsigned int max_subpixels_per_line;
	unsigned int lines_per_frame;

	bits_per_subpixel =
		sh_css_stream_format_2_bits_per_subpixel(stream_cfg->format);
	if (bits_per_subpixel == 0)
		return false;

	max_subpixels_per_line =
		csi2_protocol_calculate_max_subpixels_per_line(stream_cfg->format, stream_cfg->input_res.width);
	if (max_subpixels_per_line == 0)
		return false;

	/* for jpeg/embedded data with odd number of lines, round up to even.
	   this is required by hw ISYS2401 */
	lines_per_frame = CEIL_MUL2(stream_cfg->input_res.height, 2);
	if (lines_per_frame == 0)
		return false;

	/* HW needs subpixel info for their settings */
	isys_stream_descr->input_port_resolution.bits_per_pixel	= bits_per_subpixel;
	isys_stream_descr->input_port_resolution.pixels_per_line	= max_subpixels_per_line;
	isys_stream_descr->input_port_resolution.lines_per_frame = lines_per_frame;

	return true;
}

static bool sh_css_translate_stream_cfg_to_isys_stream_descr(
		struct ia_css_stream_config *stream_cfg,
		ia_css_isys_descr_t	*isys_stream_descr)
{
	bool rc;

	rc  = sh_css_translate_stream_cfg_to_input_system_input_port_id(stream_cfg, isys_stream_descr);
	rc &= sh_css_translate_stream_cfg_to_input_system_input_port_type(stream_cfg, isys_stream_descr);
	rc &= sh_css_translate_stream_cfg_to_input_system_input_port_attr(stream_cfg, isys_stream_descr);
	rc &= sh_css_translate_stream_cfg_to_input_system_input_port_resolution(stream_cfg, isys_stream_descr);

	return rc;
}

static enum ia_css_err
sh_css_config_input_network(struct ia_css_pipe *pipe,
		struct ia_css_binary *binary)
{
	bool					rc;
	ia_css_isys_descr_t			isys_stream_descr;
	unsigned int				sp_thread_id;
	struct sh_css_sp_pipeline_terminal	*sp_pipeline_input_terminal;

	(void)(binary);

	/* initialization */
	memset((void*)(&isys_stream_descr), 0, sizeof(ia_css_isys_descr_t));

	/* translate the stream configuration to the Input System (2401) configuration */
	rc = sh_css_translate_stream_cfg_to_isys_stream_descr(
			&(pipe->stream->config),
			&(isys_stream_descr));
	if (rc != true)
		return IA_CSS_ERR_INTERNAL_ERROR;

	/* get the SP thread id */
	rc = ia_css_pipeline_get_sp_thread_id(ia_css_pipe_get_pipe_num(pipe), &sp_thread_id);
	if (rc != true)
		return IA_CSS_ERR_INTERNAL_ERROR;

	/* get the target input terminal */
	sp_pipeline_input_terminal = &(sh_css_sp_group.pipe_io[sp_thread_id].input);

	/* create the virtual Input System (2401) */
	rc =  ia_css_isys_stream_create(
			&(isys_stream_descr),
			&(sp_pipeline_input_terminal->context.virtual_input_system));
	if (rc != true)
		return IA_CSS_ERR_INTERNAL_ERROR;

	/* calculate the configuration of the virtual Input System (2401) */
	rc = ia_css_isys_stream_calculate_cfg(
			&(sp_pipeline_input_terminal->context.virtual_input_system),
			&(isys_stream_descr),
			&(sp_pipeline_input_terminal->ctrl.virtual_input_system_cfg));
	if (rc != true) {
		ia_css_isys_stream_destroy(&(sp_pipeline_input_terminal->context.virtual_input_system));
		return IA_CSS_ERR_INTERNAL_ERROR;
	}

	return IA_CSS_SUCCESS;
}

static enum ia_css_err stream_register_with_csi_rx(
	struct ia_css_stream *stream)
{
	bool rc;
	enum ia_css_err retval = IA_CSS_ERR_INTERNAL_ERROR;
	unsigned int	sp_thread_id;

	if (stream && (stream->last_pipe != NULL)) {
		if (stream->config.mode == IA_CSS_INPUT_MODE_BUFFERED_SENSOR) {
			rc = ia_css_pipeline_get_sp_thread_id(
				ia_css_pipe_get_pipe_num(stream->last_pipe),
				&sp_thread_id);
			if (rc == true) {
				retval = ia_css_isys_csi_rx_register_stream(
					stream->config.source.port.port,
					sp_thread_id);
			}
		}
	}
	return retval;
}

static enum ia_css_err stream_unregister_with_csi_rx(
	struct ia_css_stream *stream)
{
	bool rc;
	enum ia_css_err retval = IA_CSS_ERR_INTERNAL_ERROR;
	unsigned int	sp_thread_id;
	struct ia_css_pipeline *me = NULL;

	if (stream && stream->last_pipe) {
		me = &stream->last_pipe->pipeline;
		if ((stream->config.mode == IA_CSS_INPUT_MODE_BUFFERED_SENSOR) &&
		    (stream->last_pipe->config.mode == IA_CSS_PIPE_MODE_COPY) &&
		    (me->num_stages == 1) &&
		    (me->stages && me->stages->sp_func == IA_CSS_PIPELINE_ISYS_COPY)) {
			rc = ia_css_pipeline_get_sp_thread_id(
				ia_css_pipe_get_pipe_num(stream->last_pipe),
				&sp_thread_id);
			if (rc == true) {
				retval = ia_css_isys_csi_rx_unregister_stream(
					stream->config.source.port.port,
					sp_thread_id);
			}
		}
	}
	return retval;
}
#endif

#if WITH_PC_MONITORING
static struct task_struct *my_kthread;    /* Handle for the monitoring thread */
static int sh_binary_running;         /* Enable sampling in the thread */

static void print_pc_histo(char *core_name, struct sh_css_pc_histogram *hist)
{
	unsigned i;
	unsigned cnt_run = 0;
	unsigned cnt_stall = 0;

	assert(hist != NULL);

	sh_css_print("%s histogram length = %d\n", core_name, hist->length);
	sh_css_print("%s PC\trun\tstall\n", core_name);

	for (i = 0; i < hist->length; i++) {
		if ((hist->run[i] == 0) && (hist->run[i] == hist->stall[i]))
			continue;
		sh_css_print("%s %d\t%d\t%d\n",
				core_name, i, hist->run[i], hist->stall[i]);
		cnt_run += hist->run[i];
		cnt_stall += hist->stall[i];
	}

	sh_css_print(" Statistics for %s, cnt_run = %d, cnt_stall = %d, "
	       "hist->length = %d\n",
			core_name, cnt_run, cnt_stall, hist->length);
}

static void print_pc_histogram(void)
{
	struct ia_css_binary_metrics *metrics;

	for (metrics = sh_css_metrics.binary_metrics;
	     metrics;
	     metrics = metrics->next) {
		if (metrics->mode == IA_CSS_BINARY_MODE_PREVIEW ||
		    metrics->mode == IA_CSS_BINARY_MODE_VF_PP) {
			sh_css_print("pc_histogram for binary %d is SKIPPED\n",
				metrics->id);
			continue;
		}

		sh_css_print(" pc_histogram for binary %d\n", metrics->id);
		print_pc_histo("  ISP", &metrics->isp_histogram);
		print_pc_histo("  SP",   &metrics->sp_histogram);
		sh_css_print("print_pc_histogram() done for binay->id = %d, "
			     "done.\n", metrics->id);
	}

	sh_css_print("PC_MONITORING:print_pc_histogram() -- DONE\n");
}

static int pc_monitoring(void *data)
{
	int i = 0;

	(void)data;
	while (true) {
		if (sh_binary_running) {
			sh_css_metrics_sample_pcs();
#if MULTIPLE_SAMPLES
			for (i = 0; i < NOF_SAMPLES; i++)
				sh_css_metrics_sample_pcs();
#endif
		}
		usleep_range(10, 50);
	}
	return 0;
}

static void spying_thread_create(void)
{
	my_kthread = kthread_run(pc_monitoring, NULL, "sh_pc_monitor");
	sh_css_metrics_enable_pc_histogram(1);
}

static void input_frame_info(struct ia_css_frame_info frame_info)
{
	sh_css_print("SH_CSS:input_frame_info() -- frame->info.res.width = %d, "
	       "frame->info.res.height = %d, format = %d\n",
			frame_info.res.width, frame_info.res.height, frame_info.format);
}
#endif /* WITH_PC_MONITORING */

static void
start_binary(struct ia_css_pipe *pipe,
	     struct ia_css_binary *binary)
{
	struct ia_css_stream *stream;

	assert(pipe != NULL);
	/* Acceleration uses firmware, the binary thus can be NULL */
	/* assert(binary != NULL); */

	(void)binary;

#if !defined(HAS_NO_INPUT_SYSTEM)
	stream = pipe->stream;
#else
	(void)pipe;
	(void)stream;
#endif

#ifdef THIS_CODE_IS_NO_LONGER_NEEDED_FOR_DUAL_STREAM
#if !defined(HAS_NO_INPUT_SYSTEM)
	if (stream && stream->reconfigure_css_rx)
		ia_css_isys_rx_disable();
#endif
#endif

	if (binary)
		sh_css_metrics_start_binary(&binary->metrics);

#if WITH_PC_MONITORING
	sh_css_print("PC_MONITORING: %s() -- binary id = %d , "
		     "enable_dvs_envelope = %d\n",
		     __func__, binary->info->sp.id,
		     binary->info->sp.enable.dvs_envelope);
	input_frame_info(binary->in_frame_info);

	if (binary && binary->info->sp.mode == IA_CSS_BINARY_MODE_VIDEO)
		sh_binary_running = true;
#endif

	/* sh_css_sp_start_isp(); */

#if !defined(HAS_NO_INPUT_SYSTEM) && !defined(USE_INPUT_SYSTEM_VERSION_2401)
	if (stream->reconfigure_css_rx) {
		ia_css_isys_rx_configure(&pipe->stream->csi_rx_config,
					 pipe->stream->config.mode);
		stream->reconfigure_css_rx = false;
	}
#endif
}

/* start the copy function on the SP */
static enum ia_css_err
start_copy_on_sp(struct ia_css_pipe *pipe,
		 struct ia_css_frame *out_frame)
{

	(void)out_frame;
	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	if ((pipe == NULL) || (pipe->stream == NULL))
		return IA_CSS_ERR_INVALID_ARGUMENTS;

#if !defined(HAS_NO_INPUT_SYSTEM) && !defined(USE_INPUT_SYSTEM_VERSION_2401)
	if (pipe->stream->reconfigure_css_rx)
		ia_css_isys_rx_disable();
#endif

	if (pipe->stream->config.format != IA_CSS_STREAM_FORMAT_BINARY_8)
		return IA_CSS_ERR_INTERNAL_ERROR;
	sh_css_sp_start_binary_copy(ia_css_pipe_get_pipe_num(pipe), out_frame, pipe->stream->config.pixels_per_clock == 2);

	//sh_css_sp_start_isp();

#if !defined(HAS_NO_INPUT_SYSTEM) && !defined(USE_INPUT_SYSTEM_VERSION_2401)
	if (pipe->stream->reconfigure_css_rx) {
		ia_css_isys_rx_configure(&pipe->stream->csi_rx_config, pipe->stream->config.mode);
		pipe->stream->reconfigure_css_rx = false;
	}
#endif

	return IA_CSS_SUCCESS;
}

void sh_css_binary_args_reset(struct sh_css_binary_args *args)
{
	args->in_frame      = NULL;
	args->out_frame     = NULL;
	args->in_ref_frame  = NULL;
	args->out_ref_frame = NULL;
	args->extra_ref_frame = NULL;
	args->in_tnr_frame  = NULL;
	args->out_tnr_frame = NULL;
	args->out_vf_frame  = NULL;
	args->copy_vf       = false;
	args->copy_output   = true;
	args->vf_downscale_log2 = 0;
}


static void
pipe_start(struct ia_css_pipe *pipe)
{
	struct ia_css_pipeline_stage *stage;
	assert(pipe != NULL);
	stage = pipe->pipeline.stages;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"pipe_start() enter:\n");
	if (!stage)
		return;
	pipe->pipeline.current_stage = stage;

	start_binary(pipe, stage->binary);
}

static enum ia_css_err start_pipe(
	struct ia_css_pipe *me,
	enum sh_css_pipe_config_override copy_ovrd,
	enum ia_css_input_mode input_mode)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	assert(me != NULL);

#if defined(HAS_NO_INPUT_SYSTEM)
	(void)input_mode;
#endif

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"start_pipe() enter:\n");

	configure_pipe_inout_port(&me->pipeline, me->stream->config.continuous);

	sh_css_sp_init_pipeline(&me->pipeline,
				me->mode,
				(uint8_t)ia_css_pipe_get_pipe_num(me),
				me->config.default_capture_config.enable_xnr,
				me->stream->config.pixels_per_clock == 2,
				me->stream->config.continuous,
				false,
				me->required_bds_factor,
				copy_ovrd,
				input_mode,
				&me->stream->config.metadata_config
#if !defined(HAS_NO_INPUT_SYSTEM)
				, (input_mode==IA_CSS_INPUT_MODE_MEMORY)?
					(mipi_port_ID_t)0:
				me->stream->config.source.port.port
#endif
				);

	if (me->config.mode != IA_CSS_PIPE_MODE_COPY){
		/* prepare update of params to ddr */
		err = sh_css_commit_isp_config(me->stream, &me->pipeline);
		if (err == IA_CSS_SUCCESS)
			pipe_start(me);
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"start_pipe() leave: return (%d)\n", err);
	return err;
}

void
sh_css_invalidate_shading_tables(struct ia_css_stream *stream)
{
	int i;
	assert(stream != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"sh_css_invalidate_shading_tables() enter:\n");

	for (i=0; i<stream->num_pipes; i++) {
		assert(stream->pipes[i] != NULL);
		sh_css_pipe_free_shading_table(stream->pipes[i]);
	}

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"sh_css_invalidate_shading_tables() leave: return_void\n");
}

static void
enable_interrupts(enum ia_css_irq_type irq_type)
{
	bool enable_pulse = irq_type != IA_CSS_IRQ_TYPE_EDGE;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "enable_interrupts() enter:\n");
/* Enable IRQ on the SP which signals that SP goes to idle (aka ready state) */
	cnd_sp_irq_enable(SP0_ID, true);
/* Set the IRQ device 0 to either level or pulse */
	irq_enable_pulse(IRQ0_ID, enable_pulse);

#if defined(IS_ISP_2500_SYSTEM)
	cnd_virq_enable_channel(virq_sp0, true);
	cnd_virq_enable_channel(virq_sp1, true);
#else
	cnd_virq_enable_channel(virq_sp, true);
#endif

	/* Triggered by SP to signal Host that there are new statistics */
	cnd_virq_enable_channel((virq_id_t)(IRQ_SW_CHANNEL1_ID + IRQ_SW_CHANNEL_OFFSET), true);
	/* Triggered by SP to signal Host that there is data in one of the
	 * SP->Host queues.*/
#if !defined(HAS_IRQ_MAP_VERSION_2)
/* IRQ_SW_CHANNEL2_ID does not exist on 240x systems */
	cnd_virq_enable_channel((virq_id_t)(IRQ_SW_CHANNEL2_ID + IRQ_SW_CHANNEL_OFFSET), true);
	virq_clear_all();
#endif

#if !defined(HAS_NO_INPUT_SYSTEM) && !defined(USE_INPUT_SYSTEM_VERSION_2401)
	ia_css_isys_rx_enable_all_interrupts();
#endif

#if defined(HRT_CSIM)
/*
 * Enable IRQ on the SP which signals that SP goes to idle
 * to get statistics for each binary
 */
	cnd_isp_irq_enable(ISP0_ID, true);
	cnd_virq_enable_channel(virq_isp, true);
#endif
}

static bool sh_css_setup_spctrl_config(const struct ia_css_fw_info *fw,
							const char * program,
							ia_css_spctrl_cfg  *spctrl_cfg)
{
	if((fw == NULL)||(spctrl_cfg == NULL))
		return false;
	spctrl_cfg->sp_entry = 0;
	spctrl_cfg->program_name = (char *)(program);

#if !defined(C_RUN) && !defined(HRT_UNSCHED)
	spctrl_cfg->ddr_data_offset =  fw->blob.data_source;
	spctrl_cfg->dmem_data_addr = fw->blob.data_target;
	spctrl_cfg->dmem_bss_addr = fw->blob.bss_target;
	spctrl_cfg->data_size = fw->blob.data_size ;
	spctrl_cfg->bss_size = fw->blob.bss_size;

	spctrl_cfg->spctrl_config_dmem_addr = fw->info.sp.init_dmem_data;
	spctrl_cfg->spctrl_state_dmem_addr = fw->info.sp.sw_state;

	spctrl_cfg->code_size = fw->blob.size;
	spctrl_cfg->code      = fw->blob.code;
	spctrl_cfg->sp_entry  = fw->info.sp.sp_entry; /* entry function ptr on SP */
#endif
	return true;
}
void
ia_css_unload_firmware(void)
{
	if (sh_css_num_binaries)
	{
		/* we have already loaded before so get rid of the old stuff */
		ia_css_binary_uninit();
		sh_css_unload_firmware();
	}
	fw_explicitly_loaded = false;
}

static void
ia_css_reset_defaults(struct sh_css* css)
{
	struct sh_css default_css;

	/* Reset everything to zero */
	memset(&default_css, 0, sizeof(default_css));

	/* Initialize the non zero values*/
	default_css.check_system_idle = true;
	default_css.num_cont_raw_frames = NUM_CONTINUOUS_FRAMES;
	default_css.num_mipi_frames = NUM_MIPI_FRAMES;
	default_css.contiguous = true;
	default_css.irq_type = IA_CSS_IRQ_TYPE_EDGE;

	/*Set the defaults to the output */
	*css = default_css;
}

enum ia_css_err
ia_css_load_firmware(const struct ia_css_env *env,
	    const struct ia_css_fw  *fw)
{
	enum ia_css_err err;

	if (env == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	if (fw == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_load_firmware() enter\n");

	ia_css_memory_access_init(&env->css_mem_env);

	/* make sure we initialize my_css */
	if ((my_css.malloc != env->cpu_mem_env.alloc) ||
	    (my_css.free != env->cpu_mem_env.free) ||
	    (my_css.flush != env->cpu_mem_env.flush)
	    )
	{
		ia_css_reset_defaults(&my_css);

		my_css.malloc = env->cpu_mem_env.alloc;
		my_css.free = env->cpu_mem_env.free;
		my_css.flush = env->cpu_mem_env.flush;
	}

	ia_css_unload_firmware(); /* in case we are called twice */
	err = sh_css_load_firmware(fw->data, fw->bytes);
	if (err == IA_CSS_SUCCESS) {
		err = ia_css_binary_init_infos();
		if (err == IA_CSS_SUCCESS)
			fw_explicitly_loaded = true;
	}

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_load_firmware() leave \n");
	return err;

}

enum ia_css_err
ia_css_init(const struct ia_css_env *env,
	    const struct ia_css_fw  *fw,
	    uint32_t                 mmu_l1_base,
	    enum ia_css_irq_type     irq_type)
{
	enum ia_css_err err;
	ia_css_spctrl_cfg spctrl_cfg;
	//uint32_t i = 0;

	void *(*malloc_func) (size_t size, bool zero_mem);
	void (*free_func) (void *ptr);
	void (*flush_func) (struct ia_css_acc_fw *fw);
#if !defined(HAS_NO_GPIO)
	hrt_data select, enable;
#endif

	/**
	 * The C99 standard does not specify the exact object representation of structs;
	 * the representation is compiler dependent.
	 *
	 * The structs that are communicated between host and SP/ISP should have the
	 * exact same object representation. The compiler that is used to compile the
	 * firmware is hivecc.
	 *
	 * To check if a different compiler, used to compile a host application, uses
	 * another object representation, macros are defined specifying the size of
	 * the structs as expected by the firmware.
	 *
	 * A host application shall verify that a sizeof( ) of the struct is equal to
	 * the SIZE_OF_XXX macro of the corresponding struct. If they are not
	 * equal, functionality will break.
	 */
#if !defined(IS_ISP_2500_SYSTEM)
	/* Check struct sh_css_ddr_address_map */
	COMPILATION_ERROR_IF( sizeof(struct sh_css_ddr_address_map)		!= SIZE_OF_SH_CSS_DDR_ADDRESS_MAP_STRUCT	);
#endif

	/* Check struct host_sp_queues */
	COMPILATION_ERROR_IF( sizeof(struct host_sp_queues)			!= SIZE_OF_HOST_SP_QUEUES_STRUCT		);
	COMPILATION_ERROR_IF( sizeof(struct ia_css_circbuf_desc_s)		!= SIZE_OF_IA_CSS_CIRCBUF_DESC_S_STRUCT		);
	COMPILATION_ERROR_IF( sizeof(struct ia_css_circbuf_elem_s)		!= SIZE_OF_IA_CSS_CIRCBUF_ELEM_S_STRUCT		);

	/* Check struct host_sp_communication */
	COMPILATION_ERROR_IF( sizeof(struct host_sp_communication)		!= SIZE_OF_HOST_SP_COMMUNICATION_STRUCT		);
	COMPILATION_ERROR_IF( sizeof(struct sh_css_event_irq_mask)		!= SIZE_OF_SH_CSS_EVENT_IRQ_MASK_STRUCT		);

	/* Check struct sh_css_hmm_buffer */
	COMPILATION_ERROR_IF( sizeof(struct sh_css_hmm_buffer)			!= SIZE_OF_SH_CSS_HMM_BUFFER_STRUCT		);
	COMPILATION_ERROR_IF( sizeof(struct ia_css_isp_3a_statistics)		!= SIZE_OF_IA_CSS_ISP_3A_STATISTICS_STRUCT	);
	COMPILATION_ERROR_IF( sizeof(struct ia_css_isp_dvs_statistics)		!= SIZE_OF_IA_CSS_ISP_DVS_STATISTICS_STRUCT	);
	COMPILATION_ERROR_IF( sizeof(struct ia_css_metadata)			!= SIZE_OF_IA_CSS_METADATA_STRUCT		);

	/* Check struct ia_css_init_dmem_cfg */
	COMPILATION_ERROR_IF( sizeof(struct ia_css_sp_init_dmem_cfg)		!= SIZE_OF_IA_CSS_SP_INIT_DMEM_CFG_STRUCT	);

	if (fw == NULL && !fw_explicitly_loaded)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	if (env == NULL)
	    return IA_CSS_ERR_INVALID_ARGUMENTS;

	malloc_func = env->cpu_mem_env.alloc;
	free_func   = env->cpu_mem_env.free;
	flush_func  = env->cpu_mem_env.flush;

	pipe_global_init();
	ia_css_pipeline_init();

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_init() enter: void\n");

	ia_css_device_access_init(&env->hw_access_env);
	ia_css_memory_access_init(&env->css_mem_env);

#if !defined(HAS_NO_GPIO)
	select = gpio_reg_load(GPIO0_ID, _gpio_block_reg_do_select)
						& (~GPIO_FLASH_PIN_MASK);
	enable = gpio_reg_load(GPIO0_ID, _gpio_block_reg_do_e)
							| GPIO_FLASH_PIN_MASK;
#endif
	sh_css_mmu_set_page_table_base_index(mmu_l1_base);

	if (malloc_func == NULL || free_func == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	ia_css_reset_defaults(&my_css);

	my_css.malloc = malloc_func;
	my_css.free = free_func;
	my_css.flush = flush_func;
	sh_css_printf = env->print_env.debug_print;

	err = ia_css_rmgr_init();
	if (err != IA_CSS_SUCCESS)
		return err;

	ia_css_debug_set_dtrace_level(9);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_init()\n");

	ref_count_mipi_allocation = 0;
	/* In case this has been programmed already, update internal
	   data structure ... DEPRECATED */
	my_css.page_table_base_index = mmu_get_page_table_base_index(MMU0_ID);

	my_css.irq_type = irq_type;
	enable_interrupts(my_css.irq_type);

#if !defined(HAS_NO_GPIO)
	/* configure GPIO to output mode */
	gpio_reg_store(GPIO0_ID, _gpio_block_reg_do_select, select);
	gpio_reg_store(GPIO0_ID, _gpio_block_reg_do_e, enable);
	gpio_reg_store(GPIO0_ID, _gpio_block_reg_do_0, 0);
#endif

	err = ia_css_refcount_init(REFCOUNT_SIZE);
	if (err != IA_CSS_SUCCESS)
		return err;
	err = sh_css_params_init();
	if (err != IA_CSS_SUCCESS)
		return err;
	if (fw)
	{
		ia_css_unload_firmware(); /* in case we already had firmware loaded */
		err = sh_css_load_firmware(fw->data, fw->bytes);
		if (err != IA_CSS_SUCCESS)
			return err;
		err = ia_css_binary_init_infos();
		if (err != IA_CSS_SUCCESS)
			return err;
		fw_explicitly_loaded = false;
	}
	if(!sh_css_setup_spctrl_config(&sh_css_sp_fw,SP_PROG_NAME,&spctrl_cfg))
		return IA_CSS_ERR_INTERNAL_ERROR;

	err = ia_css_spctrl_load_fw(SP0_ID, &spctrl_cfg);
	if (err != IA_CSS_SUCCESS) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_init() leave: return_err=%d\n",err);
		return err;
	}

#if defined(HRT_CSIM)
	/**
	 * In compiled simulator context include debug support by default.
	 * In all other cases (e.g. Android phone), the user (e.g. driver)
	 * must explicitly enable debug support by calling this function.
	 */
	if (!ia_css_debug_mode_init()) {
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_init() leave: return_err=%d\n",IA_CSS_ERR_INTERNAL_ERROR);
		return IA_CSS_ERR_INTERNAL_ERROR;
	}
#endif

#if WITH_PC_MONITORING
	if (!thread_alive) {
		thread_alive++;
		sh_css_print("PC_MONITORING: %s() -- create thread DISABLED\n",
			     __func__);
		spying_thread_create();
	}
	sh_css_printf = printk;
#endif
	if (!sh_css_hrt_system_is_idle()) {
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_init() leave: return_err=%d\n",IA_CSS_ERR_SYSTEM_NOT_IDLE);
		return IA_CSS_ERR_SYSTEM_NOT_IDLE;
	}
	/* can be called here, queuing works, but:
	   - when sp is started later, it will wipe queued items
	   so for now we leave it for later and make sure
	   updates are not called to frequently.
	sh_css_init_buffer_queues();
	*/

#if defined(HAS_INPUT_SYSTEM_VERSION_2) && defined(HAS_INPUT_SYSTEM_VERSION_2401)
#if	defined(USE_INPUT_SYSTEM_VERSION_2)
	gp_device_reg_store(GP_DEVICE0_ID, _REG_GP_SWITCH_ISYS2401_ADDR, 0);
#elif defined (USE_INPUT_SYSTEM_VERSION_2401)
	gp_device_reg_store(GP_DEVICE0_ID, _REG_GP_SWITCH_ISYS2401_ADDR, 1);
#endif
#endif

#if !defined(HAS_NO_INPUT_SYSTEM)
	if(ia_css_isys_init() != INPUT_SYSTEM_ERR_NO_ERROR)
		err = IA_CSS_ERR_INVALID_ARGUMENTS;
#endif
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_init() leave: return_err=%d\n",err);
	return err;
}

/* Suspend does not need to do anything for now, this may change
   in the future though. */
void
ia_css_suspend(void)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_suspend() enter & leave\n");
}

void
ia_css_resume(void)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_resume() enter: void\n");

	sh_css_sp_set_sp_running(false);
	/* reload the SP binary. ISP binaries are automatically
	   reloaded by the ISP upon execution. */
	sh_css_mmu_set_page_table_base_index(my_css.page_table_base_index);
#if !defined(IS_ISP_2500_SYSTEM)
	sh_css_params_reconfigure_gdc_lut();
#endif

	enable_interrupts(my_css.irq_type);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_resume() leave: return_void\n");
}

void *
sh_css_malloc(size_t size)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_malloc() enter: size=%d\n",size);
	if (size > 0 && my_css.malloc)
		return my_css.malloc(size, false);
	return NULL;
}

void *
sh_css_calloc(size_t N, size_t size)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_calloc() enter: N=%d, size=%d\n",N,size);
	if (size > 0 && my_css.malloc) {
		return my_css.malloc(N*size, true);
	}
	return NULL;
}

void
sh_css_free(void *ptr)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_free() enter:\n");
	if (ptr && my_css.free)
		my_css.free(ptr);
}

/* For Acceleration API: Flush FW (shared buffer pointer) arguments */
void
sh_css_flush(struct ia_css_acc_fw *fw)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_flush() enter:\n");
	if ((fw != NULL) && (my_css.flush != NULL))
		my_css.flush(fw);
}

/* Mapping sp threads. Currently, this is done when a stream is created and
 * pipelines are ready to be converted to sp pipelines. Be careful if you are
 * doing it from stream_create since we could run out of sp threads due to
 * allocation on inactive pipelines. */
static enum ia_css_err
map_sp_threads(struct ia_css_stream *stream, bool map)
{
	struct ia_css_pipe *main_pipe = NULL;
	struct ia_css_pipe *copy_pipe = NULL, *capture_pipe = NULL;
	enum ia_css_err err = IA_CSS_SUCCESS;
	enum ia_css_pipe_id pipe_id;

	assert(stream != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"map_sp_threads() enter: map = %d\n", map);

	main_pipe 	= stream->last_pipe;
	pipe_id 	= main_pipe->mode;

	ia_css_pipeline_map(&main_pipe->pipeline, map);

	switch (pipe_id) {
	case IA_CSS_PIPE_ID_PREVIEW:
		copy_pipe    = main_pipe->pipe_settings.preview.copy_pipe;
		capture_pipe = main_pipe->pipe_settings.preview.capture_pipe;
		break;

	case IA_CSS_PIPE_ID_VIDEO:
		copy_pipe    = main_pipe->pipe_settings.video.copy_pipe;
		capture_pipe = main_pipe->pipe_settings.video.capture_pipe;
		break;

	case IA_CSS_PIPE_ID_CAPTURE:
	case IA_CSS_PIPE_ID_ACC:
	default:
		break;
	}

	if(capture_pipe) {
		ia_css_pipeline_map(&capture_pipe->pipeline, map);
	}

	/* Firmware expects copy pipe to be the last pipe mapped. (if needed) */
	if(copy_pipe) {
		ia_css_pipeline_map(&copy_pipe->pipeline, map);
	}

	return err;
}

/* Create pipeline placeholder */
static enum ia_css_err
create_host_pipeline_object(struct ia_css_pipe *pipe)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"create_host_pipeline_object(): enter\n");

	err = ia_css_pipeline_create(&pipe->pipeline, pipe->mode, pipe->pipe_num);
	if (err != IA_CSS_SUCCESS) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"create_host_pipeline_object(): "
				"Unable to create pipe err=%d\n", err);
	}
	return err;
}

/* creates a host pipeline skeleton for all pipes in a stream. Called during
 * stream_create. */
static enum ia_css_err
create_host_pipeline_structure(struct ia_css_stream *stream)
{
	struct ia_css_pipe *copy_pipe = NULL, *capture_pipe = NULL;
	enum ia_css_pipe_id pipe_id;
	struct ia_css_pipe *main_pipe = NULL;
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(stream != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_pipeline_structure() enter:\n");

	main_pipe 	= stream->last_pipe;
	pipe_id 	= main_pipe->mode;

#if !defined(USE_INPUT_SYSTEM_VERSION_2401)
	/* Standalone Capture pipe cannot work with continuous capture. */
	if((pipe_id == IA_CSS_PIPE_ID_CAPTURE) && (stream->num_pipes == 1)) {
		if(!stream->config.online &&
			!main_pipe->pipe_settings.capture.copy_binary.info)
			goto ERR;
	}
#endif
	
	switch (pipe_id) {
	case IA_CSS_PIPE_ID_PREVIEW:
		copy_pipe    = main_pipe->pipe_settings.preview.copy_pipe;
		capture_pipe = main_pipe->pipe_settings.preview.capture_pipe;
		
		err = create_host_pipeline_object(main_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;

		break;

	case IA_CSS_PIPE_ID_VIDEO:
		copy_pipe    = main_pipe->pipe_settings.video.copy_pipe;
		capture_pipe = main_pipe->pipe_settings.video.capture_pipe;
		
		err = create_host_pipeline_object(main_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;

		break;

	case IA_CSS_PIPE_ID_CAPTURE:
		capture_pipe = main_pipe;

		break;

	case IA_CSS_PIPE_ID_ACC:
		err = create_host_pipeline_object(main_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;

		break;
	default:
		err = IA_CSS_ERR_INVALID_ARGUMENTS;
	}
	if (err != IA_CSS_SUCCESS)
		goto ERR;

	if(copy_pipe) {
		err = create_host_pipeline_object(copy_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}

	if(capture_pipe) {
		err = create_host_pipeline_object(capture_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}

ERR:
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_pipeline_structure() leave: return (%d)\n", err);
	return err;
}

/* creates a host pipeline for all pipes in a stream. Called during
 * stream_start. */
static enum ia_css_err
create_host_pipeline(struct ia_css_stream *stream)
{
	struct ia_css_pipe *copy_pipe = NULL, *capture_pipe = NULL;
	enum ia_css_pipe_id pipe_id;
	struct ia_css_pipe *main_pipe = NULL;
	enum ia_css_err err = IA_CSS_SUCCESS;
    unsigned max_input_width = 0;

	assert(stream != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_pipeline() enter:\n");

	main_pipe 	= stream->last_pipe;
	pipe_id 	= main_pipe->mode;

#if !defined(USE_INPUT_SYSTEM_VERSION_2401)
	/* Standalone Capture pipe cannot work with continuous capture. */
	if((pipe_id == IA_CSS_PIPE_ID_CAPTURE) && (stream->num_pipes == 1)) {
		if(!stream->config.online &&
			!main_pipe->pipe_settings.capture.copy_binary.info)
			goto ERR;
	}
#endif
	/* No continuous frame allocation for capture pipe. It uses the
	 * "main" pipe's frames. */
	if((pipe_id == IA_CSS_PIPE_ID_PREVIEW) ||
	   (pipe_id == IA_CSS_PIPE_ID_VIDEO)) {
		if(stream->config.continuous || pipe_id == IA_CSS_PIPE_ID_PREVIEW) {
			err = alloc_continuous_frames(main_pipe, true);
			if (err != IA_CSS_SUCCESS)
				goto ERR;
		}

#ifdef HRT_CSIM
		if(main_pipe->continuous_frames[0])
			ia_css_frame_zero(main_pipe->continuous_frames[0]);
#endif
	}

	if((pipe_id != IA_CSS_PIPE_ID_ACC) &&
	   (main_pipe->config.mode != IA_CSS_PIPE_MODE_COPY)) {
		err = allocate_mipi_frames(main_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}

	switch (pipe_id) {
	case IA_CSS_PIPE_ID_PREVIEW:
		copy_pipe    = main_pipe->pipe_settings.preview.copy_pipe;
		capture_pipe = main_pipe->pipe_settings.preview.capture_pipe;
		max_input_width =
			main_pipe->pipe_settings.preview.preview_binary.info->sp.max_input_width;

		err = create_host_preview_pipeline(main_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;

		break;

	case IA_CSS_PIPE_ID_VIDEO:
		copy_pipe    = main_pipe->pipe_settings.video.copy_pipe;
		capture_pipe = main_pipe->pipe_settings.video.capture_pipe;
		max_input_width =
			main_pipe->pipe_settings.video.video_binary.info->sp.max_input_width;

		err = create_host_video_pipeline(main_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;

		break;

	case IA_CSS_PIPE_ID_CAPTURE:
		capture_pipe = main_pipe;

		break;

	case IA_CSS_PIPE_ID_ACC:
		err = create_host_acc_pipeline(main_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;

		break;
	default:
		err = IA_CSS_ERR_INVALID_ARGUMENTS;
	}
	if (err != IA_CSS_SUCCESS)
		goto ERR;

	if(copy_pipe) {
		err = create_host_copy_pipeline(copy_pipe, max_input_width,
					  main_pipe->continuous_frames[0]);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}

	if(capture_pipe) {
		err = create_host_capture_pipeline(capture_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}

ERR:
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_pipeline() leave: return (%d)\n", err);
	return err;
}

static enum ia_css_err
init_pipe_defaults(enum ia_css_pipe_mode mode,
	       struct ia_css_pipe *pipe,
	       bool copy_pipe)
{
	static struct ia_css_pipe default_pipe = DEFAULT_PIPE;
	static struct ia_css_preview_settings prev  = DEFAULT_PREVIEW_SETTINGS;
	static struct ia_css_capture_settings capt  = DEFAULT_CAPTURE_SETTINGS;
	static struct ia_css_video_settings   video = DEFAULT_VIDEO_SETTINGS;

	assert(pipe != NULL);

	/* Initialize pipe to pre-defined defaults */
	*pipe = default_pipe;

	/* TODO: JB should not be needed, but temporary backward reference */
	switch (mode) {
	case IA_CSS_PIPE_MODE_PREVIEW:
		pipe->mode = IA_CSS_PIPE_ID_PREVIEW;
		pipe->pipe_settings.preview = prev;
		break;
	case IA_CSS_PIPE_MODE_CAPTURE:
		if (copy_pipe) {
			pipe->mode = IA_CSS_PIPE_ID_COPY;
		} else {
			pipe->mode = IA_CSS_PIPE_ID_CAPTURE;
		}
		pipe->pipe_settings.capture = capt;
		break;
	case IA_CSS_PIPE_MODE_VIDEO:
		pipe->mode = IA_CSS_PIPE_ID_VIDEO;
		pipe->pipe_settings.video = video;
		break;
	case IA_CSS_PIPE_MODE_ACC:
		pipe->mode = IA_CSS_PIPE_ID_ACC;
		break;
	case IA_CSS_PIPE_MODE_COPY:
		pipe->mode = IA_CSS_PIPE_ID_CAPTURE;
		break;
	default:
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}

	return IA_CSS_SUCCESS;
}

static void
pipe_global_init(void)
{
	uint8_t i;

	my_css.pipe_counter = 0;
	for (i = 0; i < IA_CSS_PIPELINE_NUM_MAX; i++) {
		my_css.all_pipes[i] = NULL;
	}
}

static uint8_t
pipe_generate_pipe_num(const struct ia_css_pipe *pipe)
{
	uint8_t i;
	uint8_t pipe_num = ~0; /* UINT8_MAX; but Linux does not have this macro */

	assert(pipe != NULL);
	/* Assign a new pipe_num .... search for empty place */
	for (i = 0; i < IA_CSS_PIPELINE_NUM_MAX; i++) {
		if (my_css.all_pipes[i] == NULL) {
			/*position is reserved */
			my_css.all_pipes[i] = (struct ia_css_pipe *)pipe;
			pipe_num = i;
			break;
		}
	}
	assert(pipe_num != (uint8_t)~0); /* UINT8_MAX; but Linux does not have this macro */
	my_css.pipe_counter++;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"pipe_generate_pipe_num (%d)\n", pipe_num);
	return pipe_num;
}

static void
pipe_release_pipe_num(unsigned int pipe_num)
{
	my_css.all_pipes[pipe_num] = NULL;
	my_css.pipe_counter--;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"pipe_release_pipe_num (%d)\n", pipe_num);
}

static enum ia_css_err
create_pipe(enum ia_css_pipe_mode mode,
	    struct ia_css_pipe **pipe,
	    bool copy_pipe)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_pipe *me = sh_css_malloc(sizeof(*me));

	assert(pipe != NULL);

	if (!me)
		return IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;

	err = init_pipe_defaults(mode, me, copy_pipe);
	if (err != IA_CSS_SUCCESS) {
		sh_css_free(me);
		return IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;
	}

	me->pipe_num = pipe_generate_pipe_num(me);
	*pipe = me;
	return IA_CSS_SUCCESS;
}

static struct ia_css_pipe *
find_pipe_by_num(uint8_t pipe_num)
{
	unsigned int i;
	for (i = 0; i < IA_CSS_PIPELINE_NUM_MAX; i++){
		if (my_css.all_pipes[i] &&
				ia_css_pipe_get_pipe_num(my_css.all_pipes[i]) == pipe_num) {
			return my_css.all_pipes[i];
		}
	}
	return NULL;
}

static void sh_css_pipe_free_acc_binaries (
    struct ia_css_pipe *pipe)
{
	unsigned int i;
	struct ia_css_pipeline *pipeline;

	assert(pipe != NULL);
	pipeline = &pipe->pipeline;

	/* loop through the stages and unload them */
	for (i = 0; i < pipeline->num_stages; i++) {
		struct ia_css_fw_info *firmware = (struct ia_css_fw_info *)
						pipeline->stages[i].firmware;
		if (firmware)
			ia_css_pipe_unload_extension(pipe, firmware);
	}
}

enum ia_css_err
ia_css_pipe_destroy(struct ia_css_pipe *pipe)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_pipe_destroy() enter\n");

	if (pipe == NULL) return IA_CSS_ERR_INVALID_ARGUMENTS;

	if (pipe->stream != NULL) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_pipe_destroy(): "
							"ia_css_stream_destroy not called!\n");
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}

	switch (pipe->config.mode) {
	case IA_CSS_PIPE_MODE_PREVIEW:
		/* need to take into account that this function is also called
		   on the internal copy pipe */
		if (pipe->mode == IA_CSS_PIPE_ID_PREVIEW) {
			ia_css_frame_free_multiple(NUM_CONTINUOUS_FRAMES,
					pipe->continuous_frames);
			if (pipe->pipe_settings.preview.copy_pipe) {
				err = ia_css_pipe_destroy(pipe->pipe_settings.preview.copy_pipe);
				ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_pipe_destroy(): "
					"destroyed internal copy pipe err=%d\n", err);
			}
		}
		break;
	case IA_CSS_PIPE_MODE_VIDEO:
		if (pipe->mode == IA_CSS_PIPE_ID_VIDEO) {
			ia_css_frame_free_multiple(NUM_CONTINUOUS_FRAMES,
				pipe->continuous_frames);
			if (pipe->pipe_settings.video.copy_pipe) {
				err = ia_css_pipe_destroy(pipe->pipe_settings.video.copy_pipe);
				ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_pipe_destroy(): "
					"destroyed internal copy pipe err=%d\n", err);
			}
		}
		ia_css_frame_free_multiple(NUM_TNR_FRAMES, pipe->pipe_settings.video.tnr_frames);
		ia_css_frame_free_multiple((pipe->dvs_frame_delay + 1), pipe->pipe_settings.video.ref_frames);
		break;
	case IA_CSS_PIPE_MODE_CAPTURE:
#if 0
		/* Do not destroy, these are shared with preview */
		ia_css_frame_free_multiple(NUM_CONTINUOUS_FRAMES,
				pipe->pipe_settings.capture.continuous_frames);
#endif
		break;
	case IA_CSS_PIPE_MODE_ACC:
		sh_css_pipe_free_acc_binaries(pipe);
		break;
	case IA_CSS_PIPE_MODE_COPY:
		break;
	}

	my_css.active_pipes[ia_css_pipe_get_pipe_num(pipe)] = NULL;
	sh_css_pipe_free_shading_table(pipe);

	ia_css_pipeline_destroy(&pipe->pipeline);
	pipe_release_pipe_num(ia_css_pipe_get_pipe_num(pipe));

	/* Temporarily, not every sh_css_pipe has an acc_extension. */
	if (pipe->config.acc_extension) {
		ia_css_pipe_unload_extension(pipe, pipe->config.acc_extension);
	}
	sh_css_free(pipe);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_pipe_destroy() exit, err=%d\n", err);
	return err;
}

void
ia_css_uninit(void)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_uninit() enter: void\n");
#if WITH_PC_MONITORING
	sh_css_print("PC_MONITORING: %s() -- started\n", __func__);
	print_pc_histogram();
#endif
	/* TODO: JB: implement decent check and handling of freeing mipi frames */
	//assert(ref_count_mipi_allocation == 0); //mipi frames are not freed
	/* cleanup generic data */
	sh_css_params_uninit();
	ia_css_refcount_uninit();

	ia_css_rmgr_uninit();

#if !defined(HAS_NO_INPUT_FORMATTER)
	/* needed for reprogramming the inputformatter after power cycle of css */
	ifmtr_set_if_blocking_mode_reset = true;
#endif

	if (fw_explicitly_loaded == false) {
		ia_css_unload_firmware();
	}
	ia_css_spctrl_unload_fw(SP0_ID);

	sh_css_sp_set_sp_running(false);
	/* check and free any remaining mipi frames */
	free_mipi_frames(NULL, true);

	sh_css_sp_reset_global_vars();

#if !defined(HAS_NO_INPUT_SYSTEM)
	ia_css_isys_uninit();
#endif

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_uninit() leave: return_void\n");
}

static unsigned int translate_sw_interrupt(unsigned value)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "translate_sw_interrupt() enter:\n");
	/* previous versions of sp would put info in the upper word
	   better safe than sorry so mask that away
	*/
	value = value & 0xffff;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "translate_sw_interrupt() leave: return %d\n", value);
	return value;
}

static unsigned int translate_sw_interrupt1(void)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "translate_sw_interrupt1() enter:\n");
	return translate_sw_interrupt(sh_css_get_sw_interrupt_value(1));
}

#if 0
static unsigned int translate_sw_interrupt2(void)
{
	/* By smart coding the flag/bits in value (on the SP side),
	 * no translation is required. The returned value can be
	 * binary ORed with existing interrupt info
	 * (it is compatible with enum ia_css_irq_info)
	 */
/* MW: No smart coding required, we should just keep interrupt info
   and local context info separated */
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "translate_sw_interrupt2() enter:\n");
	return translate_sw_interrupt(sh_css_get_sw_interrupt_value(2));
}
#endif

/* Deprecated, this is an HRT backend function (memory_access.h) */
static void
sh_css_mmu_set_page_table_base_index(hrt_data base_index)
{
	int i;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_mmu_set_page_table_base_index() enter: base_index=0x%08x\n",base_index);
	my_css.page_table_base_index = base_index;
	for (i = 0; i < (int)N_MMU_ID; i++) {
		mmu_ID_t mmu_id = (mmu_ID_t)i;
		mmu_set_page_table_base_index(mmu_id, base_index);
		mmu_invalidate_cache(mmu_id);
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_mmu_set_page_table_base_index() leave: return_void\n");
}

void
ia_css_mmu_invalidate_cache(void)
{
	const struct ia_css_fw_info *fw = &sh_css_sp_fw;
	unsigned int HIVE_ADDR_ia_css_dmaproxy_sp_invalidate_tlb;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_mmu_invalidate_cache() enter\n");

	/* if the SP is not running we should not access its dmem */
	if (sh_css_sp_is_running())
	{
		HIVE_ADDR_ia_css_dmaproxy_sp_invalidate_tlb = fw->info.sp.invalidate_tlb;

		(void)HIVE_ADDR_ia_css_dmaproxy_sp_invalidate_tlb; /* Suppres warnings in CRUN */

		sp_dmem_store_uint32(SP0_ID,
			(unsigned int)sp_address_of(ia_css_dmaproxy_sp_invalidate_tlb),
			true);
	}
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_mmu_invalidate_cache() leave\n");
}

#if defined(HAS_IRQ_MAP_VERSION_1) || defined(HAS_IRQ_MAP_VERSION_1_DEMO)
enum ia_css_err ia_css_irq_translate(
	unsigned int *irq_infos)
{
	virq_id_t	irq;
	enum hrt_isp_css_irq_status status = hrt_isp_css_irq_status_more_irqs;
	unsigned int infos = 0;

/* irq_infos can be NULL, but that would make the function useless */
/* assert(irq_infos != NULL); */
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_irq_translate() enter: irq_infos=%p\n",irq_infos);

	while (status == hrt_isp_css_irq_status_more_irqs) {
		status = virq_get_channel_id(&irq);
		if (status == hrt_isp_css_irq_status_error)
			return IA_CSS_ERR_INTERNAL_ERROR;

#if WITH_PC_MONITORING
		sh_css_print("PC_MONITORING: %s() irq = %d, "
			     "sh_binary_running set to 0\n", __func__, irq);
		sh_binary_running = 0 ;
#endif

		switch (irq) {
		case virq_sp:
			infos |= IA_CSS_IRQ_INFO_EVENTS_READY;
			break;
		case virq_isp:
#ifdef HRT_CSIM
			/* Enable IRQ which signals that ISP goes to idle
			 * to get statistics for each binary */
			infos |= IA_CSS_IRQ_INFO_ISP_BINARY_STATISTICS_READY;
#endif
			break;
		case virq_isys_csi:
			/* css rx interrupt, read error bits from css rx */
			infos |= IA_CSS_IRQ_INFO_CSS_RECEIVER_ERROR;
			break;
		case virq_isys_fifo_full:
			infos |=
			    IA_CSS_IRQ_INFO_CSS_RECEIVER_FIFO_OVERFLOW;
			break;
		case virq_isys_sof:
			infos |= IA_CSS_IRQ_INFO_CSS_RECEIVER_SOF;
			break;
		case virq_isys_eof:
			infos |= IA_CSS_IRQ_INFO_CSS_RECEIVER_EOF;
			break;
/* Temporarily removed, until we have a seperate flag for FRAME_READY irq */
#if 0
/* hmm, an interrupt mask, why would we have that ? */
		case virq_isys_sol:
			infos |= IA_CSS_IRQ_INFO_CSS_RECEIVER_SOL;
			break;
#endif
		case virq_isys_eol:
			infos |= IA_CSS_IRQ_INFO_CSS_RECEIVER_EOL;
			break;
/*
 * MW: The 2300 demo system does not have a receiver, and it
 * does not have the following three IRQ channels defined
 */
#if defined(HAS_IRQ_MAP_VERSION_1)
		case virq_ifmt_sideband_changed:
			infos |=
			    IA_CSS_IRQ_INFO_CSS_RECEIVER_SIDEBAND_CHANGED;
			break;
		case virq_gen_short_0:
			infos |= IA_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_0;
			break;
		case virq_gen_short_1:
			infos |= IA_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_1;
			break;
#endif
		case virq_ifmt0_id:
			infos |= IA_CSS_IRQ_INFO_IF_PRIM_ERROR;
			break;
		case virq_ifmt1_id:
			infos |= IA_CSS_IRQ_INFO_IF_PRIM_B_ERROR;
			break;
		case virq_ifmt2_id:
			infos |= IA_CSS_IRQ_INFO_IF_SEC_ERROR;
			break;
		case virq_ifmt3_id:
			infos |= IA_CSS_IRQ_INFO_STREAM_TO_MEM_ERROR;
			break;
		case virq_sw_pin_0:
			infos |= IA_CSS_IRQ_INFO_SW_0;
			break;
		case virq_sw_pin_1:
			infos |= translate_sw_interrupt1();
			/* pqiao TODO: also assumption here */
			break;
		default:
			break;
		}
	}

	if (irq_infos)
		*irq_infos = infos;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_irq_translate() "
		"leave: irq_infos=%p\n", infos);

	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_irq_enable(enum ia_css_irq_info info,
		  bool enable)
{
	virq_id_t	irq = N_virq_id;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_irq_enable() enter: info=%d, enable=%d\n",info,enable);

	switch (info) {
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_ERROR:
		irq = virq_isys_csi;
		break;
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_FIFO_OVERFLOW:
		irq = virq_isys_fifo_full;
		break;
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_SOF:
		irq = virq_isys_sof;
		break;
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_EOF:
		irq = virq_isys_eof;
		break;
/* Temporarily removed, until we have a seperate flag for FRAME_READY irq */
#if 0
/* hmm, an interrupt mask, why would we have that ? */
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_SOL:
		irq = virq_isys_sol;
		break;
#endif
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_EOL:
		irq = virq_isys_eol;
		break;
#if defined(HAS_IRQ_MAP_VERSION_1)
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_SIDEBAND_CHANGED:
		irq = virq_ifmt_sideband_changed;
		break;
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_0:
		irq = virq_gen_short_0;
		break;
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_GEN_SHORT_1:
		irq = virq_gen_short_1;
		break;
#endif
	case IA_CSS_IRQ_INFO_IF_PRIM_ERROR:
		irq = virq_ifmt0_id;
		break;
	case IA_CSS_IRQ_INFO_IF_PRIM_B_ERROR:
		irq = virq_ifmt1_id;
		break;
	case IA_CSS_IRQ_INFO_IF_SEC_ERROR:
		irq = virq_ifmt2_id;
		break;
	case IA_CSS_IRQ_INFO_STREAM_TO_MEM_ERROR:
		irq = virq_ifmt3_id;
		break;
	case IA_CSS_IRQ_INFO_SW_0:
		irq = virq_sw_pin_0;
		break;
	case IA_CSS_IRQ_INFO_SW_1:
		irq = virq_sw_pin_1;
		break;
	case IA_CSS_IRQ_INFO_SW_2:
		irq = virq_sw_pin_2;
		break;
	default:
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_irq_enable() leave: return_err=%d\n",IA_CSS_ERR_INVALID_ARGUMENTS);
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}

	cnd_virq_enable_channel(irq, enable);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_irq_enable() leave: return_err=%d\n",IA_CSS_SUCCESS);
	return IA_CSS_SUCCESS;
}

#elif defined(HAS_IRQ_MAP_VERSION_2)

enum ia_css_err ia_css_irq_translate(
	unsigned int *irq_infos)
{
	virq_id_t	irq;
	enum hrt_isp_css_irq_status status = hrt_isp_css_irq_status_more_irqs;
	unsigned int infos = 0;

/* irq_infos can be NULL, but that would make the function useless */
/* assert(irq_infos != NULL); */
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_irq_translate() enter: irq_infos=%p\n",irq_infos);

	while (status == hrt_isp_css_irq_status_more_irqs) {
		status = virq_get_channel_id(&irq);
		if (status == hrt_isp_css_irq_status_error)
			return IA_CSS_ERR_INTERNAL_ERROR;

#if WITH_PC_MONITORING
		sh_css_print("PC_MONITORING: %s() irq = %d, "
			     "sh_binary_running set to 0\n", __func__, irq);
		sh_binary_running = 0 ;
#endif

		switch (irq) {
#if defined(IS_ISP_2500_SYSTEM)
		case virq_sp0:
#else
		case virq_sp:
#endif
			infos |= IA_CSS_IRQ_INFO_EVENTS_READY;
			break;
		case virq_isp:
#ifdef HRT_CSIM
			/* Enable IRQ which signals that ISP goes to idle
			 * to get statistics for each binary */
			infos |= IA_CSS_IRQ_INFO_ISP_BINARY_STATISTICS_READY;
#endif
			break;
#if !defined(HAS_NO_INPUT_SYSTEM)
		case virq_isys_sof:
			infos |= IA_CSS_IRQ_INFO_CSS_RECEIVER_SOF;
			break;
		case virq_isys_eof:
			infos |= IA_CSS_IRQ_INFO_CSS_RECEIVER_EOF;
			break;
		case virq_isys_csi:
			infos |= IA_CSS_IRQ_INFO_INPUT_SYSTEM_ERROR;
			break;
#endif
#if !defined(HAS_NO_INPUT_FORMATTER)
		case virq_ifmt0_id:
			infos |= IA_CSS_IRQ_INFO_IF_ERROR;
			break;
#endif
		case virq_dma:
			infos |= IA_CSS_IRQ_INFO_DMA_ERROR;
			break;
		case virq_sw_pin_0:
			infos |= IA_CSS_IRQ_INFO_SW_0;
			break;
		case virq_sw_pin_1:
			infos |= translate_sw_interrupt1();
			/* pqiao TODO: also assumption here */
			break;
		default:
			break;
		}
	}

	if (irq_infos)
		*irq_infos = infos;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_irq_translate() "
		"leave: irq_infos=%p\n", infos);

	return IA_CSS_SUCCESS;
}

enum ia_css_err ia_css_irq_enable(
	enum ia_css_irq_info info,
	bool enable)
{
	virq_id_t	irq = N_virq_id;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_irq_enable() enter: info=%d, enable=%d\n",info,enable);

	switch (info) {
#if !defined(HAS_NO_INPUT_FORMATTER)
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_SOF:
		irq = virq_isys_sof;
		break;
	case IA_CSS_IRQ_INFO_CSS_RECEIVER_EOF:
		irq = virq_isys_eof;
		break;
	case IA_CSS_IRQ_INFO_INPUT_SYSTEM_ERROR:
		irq = virq_isys_csi;
		break;
#endif
#if !defined(HAS_NO_INPUT_FORMATTER)
	case IA_CSS_IRQ_INFO_IF_ERROR:
		irq = virq_ifmt0_id;
		break;
#endif
	case IA_CSS_IRQ_INFO_DMA_ERROR:
		irq = virq_dma;
		break;
	case IA_CSS_IRQ_INFO_SW_0:
		irq = virq_sw_pin_0;
		break;
	case IA_CSS_IRQ_INFO_SW_1:
		irq = virq_sw_pin_1;
		break;
	default:
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_irq_enable() leave: return_err=%d\n",IA_CSS_ERR_INVALID_ARGUMENTS);
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}

	cnd_virq_enable_channel(irq, enable);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_irq_enable() leave: return_err=%d\n",IA_CSS_SUCCESS);
	return IA_CSS_SUCCESS;
}

#else
#error "sh_css.c: IRQ MAP must be one of \
	{IRQ_MAP_VERSION_1, IRQ_MAP_VERSION_1_DEMO, IRQ_MAP_VERSION_2}"
#endif

static unsigned int
sh_css_get_sw_interrupt_value(unsigned int irq)
{
	unsigned int irq_value;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_get_sw_interrupt_value() enter: irq=%d\n",irq);
	irq_value = sh_css_sp_get_sw_interrupt_value(irq);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_get_sw_interrupt_value() leave: irq_value=%d\n",irq_value);
	return irq_value;
}

#if 0
/* Disabled because it is currently unused. */
static void
sh_css_pipe_get_extra_pixels_count(const struct sh_css_pipe *pipe,
				   struct ia_css_resolution *extra)
{
	int rows = SH_CSS_MAX_LEFT_CROPPING,
	    cols = SH_CSS_MAX_LEFT_CROPPING;

	assert(pipe != NULL);
	assert(extra != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_pipe_get_extra_pixels_count() enter: void\n");

	if (ia_css_ifmtr_lines_needed_for_bayer_order(&pipe->stream->config))
		rows += 2;

	if (ia_css_ifmtr_columns_needed_for_bayer_order(&pipe->stream->config))
		cols  += 2;

	extra->width  = cols;
	extra->height = rows;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"sh_css_pipe_get_extra_pixels_count() leave: extra_rows=%d, extra_cols=%d\n",
		rows, cols);
}
#endif

/* configure and load the copy binary, the next binary is used to
   determine whether the copy binary needs to do left padding. */
static enum ia_css_err load_copy_binary(
	struct ia_css_pipe *pipe,
	struct ia_css_binary *copy_binary,
	struct ia_css_binary *next_binary)
{
	struct ia_css_frame_info copy_out_info, copy_in_info;
	unsigned int left_padding;
	enum ia_css_err err;
	struct ia_css_binary_descr copy_descr;

	/* next_binary can be NULL */
	assert(pipe != NULL);
	assert(copy_binary != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"load_copy_binary() enter:\n");

	if (next_binary != NULL) {
		copy_out_info = next_binary->in_frame_info;
		left_padding = next_binary->left_padding;
	} else {
		copy_out_info = pipe->output_info;
		left_padding = 0;
	}

	ia_css_pipe_get_copy_binarydesc(pipe, &copy_descr,
		&copy_in_info, &copy_out_info);
	err = ia_css_binary_find(&copy_descr, copy_binary);
	if (err != IA_CSS_SUCCESS)
		return err;
	copy_binary->left_padding = left_padding;
	return IA_CSS_SUCCESS;
}

static enum ia_css_err
alloc_continuous_frames(
	struct ia_css_pipe *pipe, bool init_time)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_frame_info ref_info;
	enum ia_css_pipe_id pipe_id;
	bool continuous;
	unsigned int i, idx;
	unsigned int num_frames;
	unsigned int left_cropping = 0, top_cropping;
	uint8_t raw_binning = 0;

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	pipe_id = pipe->mode;
	continuous = pipe->stream->config.continuous;

	if (continuous) {
		if (init_time) {
			num_frames = pipe->stream->config.init_num_cont_raw_buf;
			pipe->stream->continuous_pipe = pipe;
		} else
			num_frames = pipe->stream->config.target_num_cont_raw_buf;
	} else {
	    num_frames = NUM_ONLINE_INIT_CONTINUOUS_FRAMES;
	}

	if (pipe_id == IA_CSS_PIPE_ID_PREVIEW) {
		left_cropping = pipe->pipe_settings.preview.preview_binary.info->sp.left_cropping;
		top_cropping = pipe->pipe_settings.preview.preview_binary.info->sp.top_cropping;
		ref_info = pipe->pipe_settings.preview.preview_binary.in_frame_info;
		raw_binning = pipe->pipe_settings.preview.preview_binary.info->sp.enable.raw_binning;
	} else if (pipe_id == IA_CSS_PIPE_ID_VIDEO) {
		left_cropping = pipe->pipe_settings.video.video_binary.info->sp.left_cropping;
		top_cropping = pipe->pipe_settings.video.video_binary.info->sp.top_cropping;
		ref_info = pipe->pipe_settings.video.video_binary.in_frame_info;
		raw_binning = pipe->pipe_settings.video.video_binary.info->sp.enable.raw_binning;
	}
	else {
		/* should not happen */
		assert(false);
		return IA_CSS_ERR_INTERNAL_ERROR;
	}

	if (pipe->stream->config.pack_raw_pixels) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			"alloc_continuous_frames() IA_CSS_FRAME_FORMAT_RAW_PACKED\n");
		ref_info.format = IA_CSS_FRAME_FORMAT_RAW_PACKED;
	}
	else {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			"alloc_continuous_frames() IA_CSS_FRAME_FORMAT_RAW\n");
		ref_info.format = IA_CSS_FRAME_FORMAT_RAW;
	}

	if (init_time)
	    idx = 0;
	else
		idx = pipe->stream->config.init_num_cont_raw_buf;

	for (i = idx; i < NUM_CONTINUOUS_FRAMES; i++) {
		/* free previous frame */
		if (pipe->continuous_frames[i]) {
			ia_css_frame_free(pipe->continuous_frames[i]);
			pipe->continuous_frames[i] = NULL;
		}
		/* check if new frame needed */
		if (i < num_frames) {
			/* allocate new frame */
			err = ia_css_frame_allocate_from_info(
				&pipe->continuous_frames[i],
				&ref_info);
			if (err != IA_CSS_SUCCESS)
				return err;
		}
	}
	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_alloc_continuous_frame_remain(struct ia_css_stream *stream)
{
	if (stream == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	return alloc_continuous_frames(stream->continuous_pipe, false);
}

static enum ia_css_err
allocate_mipi_frames(struct ia_css_pipe *pipe)
{
	enum ia_css_err err = IA_CSS_ERR_INTERNAL_ERROR;
	unsigned int i, j;
	struct ia_css_frame_info mipi_intermediate_info;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"allocate_mipi_frames(%p) enter:\n", pipe);

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	if (pipe->stream->config.mode != IA_CSS_INPUT_MODE_BUFFERED_SENSOR) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			"allocate_mipi_frames(%p) exit: no buffers needed for pipe mode\n",
			pipe);
		return IA_CSS_SUCCESS;
	}

	assert(my_css.size_mem_words != 0);
	if (my_css.size_mem_words == 0) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			"allocate_mipi_frames(%p) exit: mipi frame size not specified\n",
			pipe);
		return err;
	}

	ref_count_mipi_allocation++;
	if (ref_count_mipi_allocation > 1) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			"allocate_mipi_frames(%p) exit: already allocated\n",
			pipe);
		return IA_CSS_SUCCESS;
	}
	assert(ref_count_mipi_allocation == 1);

// This code needs to modified to allocate the MIPI frames in the correct normal way with a allocate from info
// by justin
	mipi_intermediate_info = pipe->pipe_settings.video.video_binary.internal_frame_info;
	mipi_intermediate_info.res.width = 0;
	mipi_intermediate_info.res.height = 0;
	// To indicate it is not (yet) valid format.
	mipi_intermediate_info.format = IA_CSS_FRAME_FORMAT_NUM;
	mipi_intermediate_info.padded_width = 0;
	mipi_intermediate_info.raw_bit_depth = 0;
	//mipi_intermediate_info.data_bytes = my_css.size_mem_words * HIVE_ISP_DDR_WORD_BYTES;
	//mipi_intermediate_info.contiguous = my_css.contiguous;
	// To indicate it is not valid frame.
	//mipi_intermediate_info.dynamic_data_index = SH_CSS_INVALID_FRAME_ID;

	for (i = 0; i < my_css.num_mipi_frames; i++) {
		/* free previous frame */
		if (my_css.mipi_frames[i]) {
			ia_css_frame_free(my_css.mipi_frames[i]);
			my_css.mipi_frames[i] = NULL;
		}
		/* check if new frame needed */
		if (i < my_css.num_mipi_frames) {
			/* allocate new frame */
			err = ia_css_frame_allocate_with_buffer_size(
				&my_css.mipi_frames[i],
				my_css.size_mem_words * HIVE_ISP_DDR_WORD_BYTES,
				my_css.contiguous);
			if (err != IA_CSS_SUCCESS) {
				for (j = 0; j < i; j++) {
					if (my_css.mipi_frames[j]) {
						ia_css_frame_free(my_css.mipi_frames[j]);
						my_css.mipi_frames[j] = NULL;
					}
				}
				return err;
			}
#ifdef HRT_CSIM
			ia_css_frame_zero(my_css.mipi_frames[i]);
#endif
		}
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"allocate_mipi_frames(%p) exit:\n", pipe);

	return err;
}

static void
free_mipi_frames(struct ia_css_pipe *pipe, bool uninit)
{
	unsigned int i;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"free_mipi_frames(%p, %d) enter:\n", pipe, uninit);
	if (!uninit) {
		assert(pipe != NULL);
		assert(pipe->stream != NULL);

		if (pipe->stream->config.mode != IA_CSS_INPUT_MODE_BUFFERED_SENSOR) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
				"free_mipi_frames() exit: wrong mode\n");
			return;
		}

		assert(ref_count_mipi_allocation > 0);
		ref_count_mipi_allocation--;

		if (ref_count_mipi_allocation > 0) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
				"free_mipi_frames(%p, %d) exit: "
				"not last pipe (ref_count=%d):\n",
				pipe, uninit, ref_count_mipi_allocation);
			return;
		}
	}

	for (i = 0; i < my_css.num_mipi_frames; i++) {
		if (my_css.mipi_frames[i] != NULL)
		{
			ia_css_frame_free(my_css.mipi_frames[i]);
			my_css.mipi_frames[i] = NULL;
		}
	}
	/* TODO: change into return error value instead of assert
	 * task is pending, disable for now
	 */
	//assert(ref_count_mipi_allocation == 0);
	ref_count_mipi_allocation = 0;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"free_mipi_frames(%p, %d) exit (deallocated):\n", pipe, uninit);
}


static void
send_mipi_frames (struct ia_css_pipe *pipe)
{
	unsigned int i;

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	/* multi stream video needs mipi buffers */
	if (pipe->stream->config.mode != IA_CSS_INPUT_MODE_BUFFERED_SENSOR)
		return;

	/* Hand-over the SP-internal mipi buffers */
	for (i = 0; i < my_css.num_mipi_frames; i++) {
		sh_css_update_host2sp_mipi_frame(i,
			my_css.mipi_frames[i]);
	}
	sh_css_update_host2sp_cont_num_mipi_frames
		(my_css.num_mipi_frames);

	/**********************************
	 *
	 * Hack for Baytrail.
	 *
	 * AUTHOR: zhengjie.lu@intel.com
	 * TIME: 2013-01-19, 14:38.
	 * LOCATION: Santa Clara, U.S.A.
	 * COMMENT:
	 * Send an event to inform the SP
	 * that all MIPI frames are passed.
	 *
	 **********************************/
	{
		ia_css_queue_t *q;
		q = sh_css_get_queue(sh_css_host2sp_event_queue, -1, -1);

		if (NULL == q) {
			/* q handle not available */
			/* Error as the queue is not initialized */
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
				"send_mipi_frames() leaving: host2sp_event_queue not available\n");
			return;
		}

		/* casting eventq payload to ensure it is encodable*/
		ia_css_eventq_send(q,
			SP_SW_EVENT_ID_6,	/* the event ID  */
			0,				/* not used */
			0,				/* not used */
			0				/* not used */);
	}
	/** End of hack of Baytrail **/
}

static enum ia_css_err
load_preview_binaries(struct ia_css_pipe *pipe)
{
	struct ia_css_frame_info prev_in_info,
				 prev_bds_out_info,
				 prev_out_info,
				 prev_vf_info;
	bool online;
	enum ia_css_err err = IA_CSS_SUCCESS;
	bool continuous, need_vf_pp = false;
	unsigned int left_cropping;
	bool need_isp_copy_binary = false;
#ifdef USE_INPUT_SYSTEM_VERSION_2401
	bool sensor = false;
#endif

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	online = pipe->stream->config.online;
	continuous = pipe->stream->config.continuous;
#ifdef USE_INPUT_SYSTEM_VERSION_2401
	sensor = pipe->stream->config.mode == IA_CSS_INPUT_MODE_SENSOR;
#endif

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"load_preview_binaries() enter:\n");

	if (pipe->pipe_settings.preview.preview_binary.info)
		return IA_CSS_SUCCESS;

	err = ia_css_util_check_input(&pipe->stream->config, false);
	if (err != IA_CSS_SUCCESS)
		return err;
	err = ia_css_frame_check_info(&pipe->output_info);
	if (err != IA_CSS_SUCCESS)
		return err;

	/* Note: the current selection of vf_pp binary and
	 * parameterization of the preview binary contains a few pieces
	 * of hardcoded knowledge. This needs to be cleaned up such that
	 * the binary selection becomes more generic.
	 * The vf_pp binary is needed if one or more of the following features
	 * are required:
	 * 1. YUV downscaling.
	 * 2. Digital zoom.
	 * 3. An output format that is not supported by the preview binary.
	 *    In practice this means something other than yuv_line or nv12.
	 * */
	if (pipe->vf_yuv_ds_input_info.res.width) {
		need_vf_pp = pipe->vf_yuv_ds_input_info.res.width != pipe->output_info.res.width &&
			pipe->vf_yuv_ds_input_info.res.height != pipe->output_info.res.height;
	}
	need_vf_pp |= pipe->config.enable_dz;
	need_vf_pp |= pipe->output_info.format != IA_CSS_FRAME_FORMAT_YUV_LINE &&
		      pipe->output_info.format != IA_CSS_FRAME_FORMAT_NV12;

	/* Preview */
	if (pipe->vf_yuv_ds_input_info.res.width)
		prev_vf_info = pipe->vf_yuv_ds_input_info;
	else
		prev_vf_info = pipe->output_info;
	/* If vf_pp is needed, then preview must output yuv_line.
	 * The exception is when vf_pp is manually disabled, that is only
	 * used in combination with a pipeline extension that requires
	 * yuv_line as input.
	 * */
	if (need_vf_pp)
		ia_css_frame_info_set_format(&prev_vf_info,
					     IA_CSS_FRAME_FORMAT_YUV_LINE);

	{
		struct ia_css_binary_descr preview_descr;

		err = ia_css_pipe_get_preview_binarydesc(pipe, &preview_descr,
			&prev_in_info, &prev_bds_out_info, &prev_out_info, &prev_vf_info);
		if (err != IA_CSS_SUCCESS)
			return err;
		err = ia_css_binary_find(&preview_descr,
					 &pipe->pipe_settings.preview.preview_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	if (need_vf_pp) {
		struct ia_css_binary_descr vf_pp_descr;

		/* Viewfinder post-processing */
		ia_css_pipe_get_vfpp_binarydesc(pipe, &vf_pp_descr,
			&pipe->pipe_settings.preview.preview_binary.out_frame_info,
			&pipe->output_info);
		err = ia_css_binary_find(&vf_pp_descr,
				 &pipe->pipe_settings.preview.vf_pp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

#ifdef USE_INPUT_SYSTEM_VERSION_2401
	/* When the input system is 2401, only the Direct Sensor Mode
	 * Offline Preview uses the ISP copy binary.
	 */
	need_isp_copy_binary = !online && sensor;
#else
	need_isp_copy_binary = !online && !continuous;
#endif

	/* Copy */
	if (need_isp_copy_binary) {
		err = load_copy_binary(pipe,
				       &pipe->pipe_settings.preview.copy_binary,
				       &pipe->pipe_settings.preview.preview_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	left_cropping = pipe->pipe_settings.preview.preview_binary.info->sp.left_cropping;

	if (pipe->shading_table) {
		ia_css_shading_table_free(pipe->shading_table);
		pipe->shading_table = NULL;
	}

	return IA_CSS_SUCCESS;
}

static const struct ia_css_fw_info *last_output_firmware(
	const struct ia_css_fw_info *fw)
{
	const struct ia_css_fw_info *last_fw = NULL;
/* fw can be NULL */
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"last_output_firmware() enter:\n");

	for (; fw; fw = fw->next) {
		const struct ia_css_fw_info *info = fw;
		if (info->info.isp.sp.enable.output)
			last_fw = fw;
	}
	return last_fw;
}

static enum ia_css_err add_firmwares(
	struct ia_css_pipeline *me,
	struct ia_css_binary *binary,
	const struct ia_css_fw_info *fw,
	const struct ia_css_fw_info *last_fw,
	unsigned int binary_mode,
	struct ia_css_frame *in_frame,
	struct ia_css_frame *out_frame,
	struct ia_css_frame *vf_frame,
	struct ia_css_pipeline_stage **my_stage,
	struct ia_css_pipeline_stage **vf_stage)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_pipeline_stage *extra_stage = NULL;
	struct ia_css_pipeline_stage_desc stage_desc;

/* all args can be NULL ??? */
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"add_firmwares() enter:\n");

	for (; fw; fw = fw->next) {
		struct ia_css_frame *out = NULL;
		struct ia_css_frame *in = NULL;
		struct ia_css_frame *vf = NULL;
		if ((fw == last_fw) && (fw->info.isp.sp.enable.out_frame  != 0)) {
			out = out_frame;
		}
		if (fw->info.isp.sp.enable.in_frame != 0) {
			in = in_frame;
		}
		if (fw->info.isp.sp.enable.out_frame != 0) {
			vf = vf_frame;
		}
		ia_css_pipe_get_firmwares_stage_desc(&stage_desc, binary,
			out, in, vf, fw, binary_mode);
		err = ia_css_pipeline_create_and_add_stage(me,
				&stage_desc,
				&extra_stage);
		if (err != IA_CSS_SUCCESS)
			return err;
		if (fw->info.isp.sp.enable.output != 0)
			in_frame = extra_stage->args.out_frame;
		if (my_stage && !*my_stage && extra_stage)
			*my_stage = extra_stage;
		if (vf_stage && !*vf_stage && extra_stage &&
		    fw->info.isp.sp.enable.vf_veceven)
			*vf_stage = extra_stage;
	}
	return err;
}

static enum ia_css_err add_vf_pp_stage(
	struct ia_css_pipe *pipe,
	struct ia_css_frame *out_frame,
	struct ia_css_binary *vf_pp_binary,
	struct ia_css_pipeline_stage *post_stage,
	struct ia_css_pipeline_stage **vf_pp_stage)
{

	struct ia_css_pipeline *me;
	const struct ia_css_fw_info *last_fw;
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_frame *in_frame;
	struct ia_css_pipeline_stage_desc stage_desc;

/* out_frame can be NULL ??? */

	assert(pipe != NULL);
	assert(vf_pp_binary != NULL);
	assert(post_stage != NULL);
	assert(vf_pp_stage != NULL);

	me = &pipe->pipeline;
	in_frame = post_stage->args.out_vf_frame;


	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
	"add_vf_pp_stage() enter:\n");

	*vf_pp_stage = NULL;

	if (in_frame == NULL)
		in_frame = post_stage->args.out_frame;

	last_fw = last_output_firmware(pipe->vf_stage);
	if (!pipe->extra_config.disable_vf_pp) {
		if (last_fw) {
			ia_css_pipe_get_generic_stage_desc(&stage_desc, vf_pp_binary,
				NULL, in_frame, NULL, NULL);
		} else{
			ia_css_pipe_get_generic_stage_desc(&stage_desc, vf_pp_binary,
				out_frame, in_frame, NULL, NULL);
		}
		err = ia_css_pipeline_create_and_add_stage(me, &stage_desc, vf_pp_stage);
		if (err != IA_CSS_SUCCESS)
			return err;
		in_frame = (*vf_pp_stage)->args.out_frame;
	}
	err = add_firmwares(me, vf_pp_binary, pipe->vf_stage, last_fw,
			    IA_CSS_BINARY_MODE_VF_PP,
			    in_frame, out_frame, NULL,
			    vf_pp_stage, NULL);
	return err;
}

static enum ia_css_err add_capture_pp_stage(
	struct ia_css_pipe *pipe,
	struct ia_css_pipeline *me,
	struct ia_css_frame *out_frame,
	struct ia_css_binary *capture_pp_binary,
	struct ia_css_pipeline_stage *capture_stage,
	struct ia_css_pipeline_stage **pre_vf_pp_stage)
{
	const struct ia_css_fw_info *last_fw;
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_frame *in_frame;
	struct ia_css_frame *vf_frame = NULL;
	struct ia_css_pipeline_stage_desc stage_desc;

	/* out_frame can be NULL ??? */
	assert(pipe != NULL);
	assert(me != NULL);
	assert(capture_pp_binary != NULL);
	assert(capture_stage != NULL);
	assert(pre_vf_pp_stage != NULL);
	in_frame = capture_stage->args.out_frame;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"add_capture_pp_stage() enter:\n");

	*pre_vf_pp_stage = NULL;

	if (in_frame == NULL)
		in_frame = capture_stage->args.out_frame;

	last_fw = last_output_firmware(pipe->output_stage);
	if (need_capture_pp(pipe)) {
		err = ia_css_frame_allocate_from_info(&vf_frame,
					    &capture_pp_binary->vf_frame_info);
		if (err != IA_CSS_SUCCESS)
			return err;
		if(last_fw)	{
			ia_css_pipe_get_generic_stage_desc(&stage_desc,
				capture_pp_binary, NULL, NULL, NULL, vf_frame);
		} else {
			ia_css_pipe_get_generic_stage_desc(&stage_desc,
				capture_pp_binary, out_frame, NULL, NULL, vf_frame);
		}
		err = ia_css_pipeline_create_and_add_stage(me,
			&stage_desc,
			pre_vf_pp_stage);
		if (err != IA_CSS_SUCCESS)
			return err;
		in_frame = (*pre_vf_pp_stage)->args.out_frame;
	}
	err = add_firmwares(me, capture_pp_binary, pipe->output_stage, last_fw,
			    IA_CSS_BINARY_MODE_CAPTURE_PP,
			    in_frame, out_frame, vf_frame,
			    NULL, pre_vf_pp_stage);
	/* If a firmware produce vf_pp output, we set that as vf_pp input */
	if (*pre_vf_pp_stage) {
		(*pre_vf_pp_stage)->args.vf_downscale_log2 =
		  capture_pp_binary->vf_downscale_log2;
	} else {
		*pre_vf_pp_stage = capture_stage;
	}
	return err;
}

static void
sh_css_init_buffer_queues(void)
{
	const struct ia_css_fw_info *fw;
	unsigned int HIVE_ADDR_host_sp_queues_initialized;
	unsigned int HIVE_ADDR_ia_css_bufq_host_sp_queue;
	ia_css_queue_remote_t remoteq;
	unsigned int i, j;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_init_buffer_queues() enter:\n");

	for (i = 0; i < MAX_HMM_BUFFER_NUM; i++)
		hmm_buffer_record_h[i] = NULL;


	sh_css_event_init_irq_mask();

	fw = &sh_css_sp_fw;
	HIVE_ADDR_host_sp_queues_initialized =
		fw->info.sp.host_sp_queues_initialized;

#ifdef C_RUN
	HIVE_ADDR_ia_css_bufq_host_sp_queue = (unsigned int)
		sp_address_of(ia_css_bufq_host_sp_queue);
#else
	HIVE_ADDR_ia_css_bufq_host_sp_queue = fw->info.sp.host_sp_queue;
#endif
	/* Setup queue location as SP and proc id as SP0_ID*/
	remoteq.location = IA_CSS_QUEUE_LOC_SP;
	remoteq.proc_id = SP0_ID;

	/* Setup all the local queue descriptors for Host2SP Buffer Queues */
	for (i = 0; i < SH_CSS_MAX_SP_THREADS; i++)
		for (j = 0; j < SH_CSS_NUM_BUFFER_QUEUES; j++) {
			remoteq.cb_desc_addr =
				HIVE_ADDR_ia_css_bufq_host_sp_queue
					+ offsetof(struct host_sp_queues,
					 host2sp_buffer_queues_desc[i][j]);

			remoteq.cb_elems_addr =
				HIVE_ADDR_ia_css_bufq_host_sp_queue
					+ offsetof(struct host_sp_queues,
					 host2sp_buffer_queues_elems[i][j]);

			/* Initialize the queue instance and obtain handle */
			ia_css_queue_remote_init(
				&my_css.queues.host2sp_buffer_queue_handles[i][j],
				&remoteq);
		}

	/* Setup all the local queue descriptors for SP2Host Buffer Queues */
	for (i = 0; i < SH_CSS_NUM_BUFFER_QUEUES; i++) {
		remoteq.cb_desc_addr = HIVE_ADDR_ia_css_bufq_host_sp_queue
			+ offsetof(struct host_sp_queues,
				 sp2host_buffer_queues_desc[i]);

		remoteq.cb_elems_addr = HIVE_ADDR_ia_css_bufq_host_sp_queue
			+ offsetof(struct host_sp_queues,
				 sp2host_buffer_queues_elems[i]);

		/* Initialize the queue instance and obtain handle */
		ia_css_queue_remote_init(
			&my_css.queues.sp2host_buffer_queue_handles[i],
			&remoteq);
	}

	/* Host2SP event queue */
	remoteq.cb_desc_addr = HIVE_ADDR_ia_css_bufq_host_sp_queue +
		offsetof(struct host_sp_queues, host2sp_event_queue_desc);
	remoteq.cb_elems_addr = HIVE_ADDR_ia_css_bufq_host_sp_queue +
		offsetof(struct host_sp_queues, host2sp_event_queue_elems);
	/* Initialize the queue instance and obtain handle */
	ia_css_queue_remote_init(&my_css.queues.host2sp_event_queue_handle,
		&remoteq);

	/* SP2Host queues event queue*/
	remoteq.cb_desc_addr = HIVE_ADDR_ia_css_bufq_host_sp_queue +
		offsetof(struct host_sp_queues, sp2host_event_queue_desc);
	remoteq.cb_elems_addr = HIVE_ADDR_ia_css_bufq_host_sp_queue +
		offsetof(struct host_sp_queues, sp2host_event_queue_elems);
	/* Initialize the queue instance and obtain handle */
	ia_css_queue_remote_init(&my_css.queues.sp2host_event_queue_handle,
		&remoteq);

	/* set "host_sp_queues_initialized" to "true" */
	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(host_sp_queues_initialized),
		(uint32_t)(1));

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_init_buffer_queues() leave:\n");
}

ia_css_queue_t*
sh_css_get_queue(enum sh_css_queue_type type, enum sh_css_buffer_queue_id id,
		 int thread)
{
	ia_css_queue_t* q = 0;
	if (!sh_css_sp_is_running()) {
		/* SP is not running. The queues are not valid */
		return NULL;
	}

	switch (type) {
	case sh_css_host2sp_buffer_queue:
		if (thread >= SH_CSS_MAX_SP_THREADS || thread < 0 ||
			id == sh_css_invalid_buffer_queue)
			break;
		q = &my_css.queues.host2sp_buffer_queue_handles[thread][id];
		break;
	case sh_css_sp2host_buffer_queue:
		if (id == sh_css_invalid_buffer_queue)
			break;
		q = &my_css.queues.sp2host_buffer_queue_handles[id];
		break;
	case sh_css_host2sp_event_queue:
		q = &my_css.queues.host2sp_event_queue_handle;
		break;
	case sh_css_sp2host_event_queue:
		q = &my_css.queues.sp2host_event_queue_handle;
		break;
	default:
		break;
	}

	return q;
}



static enum ia_css_err
init_vf_frameinfo_defaults(struct ia_css_pipe *pipe,
	struct ia_css_frame *vf_frame)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	sh_css_pipe_get_viewfinder_frame_info(pipe, &vf_frame->info);
	vf_frame->contiguous = false;
	vf_frame->flash_state = IA_CSS_FRAME_FLASH_STATE_NONE;
	vf_frame->dynamic_data_index = sh_css_frame_out_vf;

	err = ia_css_frame_init_planes(vf_frame);
	return err;
}

static enum ia_css_err
init_in_frameinfo_memory_defaults(struct ia_css_pipe *pipe,
	struct ia_css_frame *frame)
{
	struct ia_css_frame *in_frame;
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(frame != NULL);

	in_frame = frame;

	in_frame->info.format = IA_CSS_FRAME_FORMAT_RAW;
	in_frame->info.res.width = pipe->stream->config.input_res.width;
	in_frame->info.res.height = pipe->stream->config.input_res.height;
	in_frame->info.raw_bit_depth =
		ia_css_pipe_util_pipe_input_format_bpp(pipe);
	ia_css_frame_info_set_width(&in_frame->info, pipe->stream->config.input_res.width, 0);

	in_frame->contiguous = false;
	in_frame->flash_state = IA_CSS_FRAME_FLASH_STATE_NONE;
	in_frame->dynamic_data_index = sh_css_frame_in;

	err = ia_css_frame_init_planes(in_frame);

	return err;
}

static enum ia_css_err
init_out_frameinfo_defaults(struct ia_css_pipe *pipe,
	struct ia_css_frame *out_frame)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	sh_css_pipe_get_output_frame_info(pipe, &out_frame->info);
	out_frame->contiguous = false;
	out_frame->flash_state = IA_CSS_FRAME_FLASH_STATE_NONE;
	out_frame->dynamic_data_index = sh_css_frame_out;
	err = ia_css_frame_init_planes(out_frame);

	return err;
}

/* Create stages for video pipe */
static enum ia_css_err create_host_video_pipeline(struct ia_css_pipe *pipe)
{
	struct ia_css_pipeline_stage_desc stage_desc;
	struct ia_css_binary *copy_binary, *video_binary, *vf_pp_binary;
	struct ia_css_pipeline_stage *copy_stage  = NULL;
	struct ia_css_pipeline_stage *video_stage = NULL;
	struct ia_css_pipeline_stage *vf_pp_stage = NULL;
	struct ia_css_pipeline_stage *in_stage    = NULL;
	struct ia_css_pipeline *me;
	struct ia_css_frame *in_frame = NULL;
	struct ia_css_frame *out_frame;
	struct ia_css_frame *vf_frame;
	enum ia_css_err err = IA_CSS_SUCCESS;
	bool resolution_differs;
	bool need_vf_pp = false;
	unsigned num_output_pins;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_video_pipeline() enter:\n");

	in_frame = &pipe->in_frame_struct;
	out_frame = &pipe->out_frame_struct;
	vf_frame = &pipe->vf_frame_struct;

	pipe->out_frame_struct.data = 0;
	pipe->vf_frame_struct.data = 0;

	/* pipeline already created as part of create_host_pipeline_structure */
	me = &pipe->pipeline;
	ia_css_pipeline_clean(me);

	me->dvs_frame_delay = pipe->dvs_frame_delay;
	/* Construct in_frame info (only in case we have dynamic input */
	if (pipe->stream->config.mode == IA_CSS_INPUT_MODE_MEMORY) {
		err = init_in_frameinfo_memory_defaults(pipe, in_frame);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	} else {
		in_frame = NULL;
	}

	out_frame->data = 0;
	err = init_out_frameinfo_defaults(pipe, out_frame);
	if (err != IA_CSS_SUCCESS)
		goto ERR;

	if (!pipe->enable_viewfinder) {
		/* These situations don't support viewfinder output */
		vf_frame = NULL;
	} else {
		vf_frame->data = 0;
		err = init_vf_frameinfo_defaults(pipe, vf_frame);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}

	copy_binary  = &pipe->pipe_settings.video.copy_binary;
	video_binary = &pipe->pipe_settings.video.video_binary;
	vf_pp_binary = &pipe->pipe_settings.video.vf_pp_binary;
	num_output_pins = video_binary->info->num_output_pins;

	if (pipe->pipe_settings.video.copy_binary.info) {
		ia_css_pipe_get_generic_stage_desc(&stage_desc, copy_binary,
			NULL, NULL, NULL, NULL);
		err = ia_css_pipeline_create_and_add_stage(me,
			&stage_desc,
			&copy_stage);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
		in_frame = me->stages->args.out_frame;
		in_stage = copy_stage;
	} else if (pipe->stream->config.continuous) {
		in_frame = pipe->continuous_frames[0];
	}

	resolution_differs =
		(pipe->vf_output_info.res.width != pipe->output_info.res.width) ||
		(pipe->vf_output_info.res.height != pipe->output_info.res.height);

	need_vf_pp = pipe->enable_viewfinder && ((num_output_pins == 1) ||
		((num_output_pins == 2) && resolution_differs));

	/* when the video binary supports a second output pin,
	   it can directly produce the vf_frame.  */
	if(need_vf_pp) {
		ia_css_pipe_get_generic_stage_desc(&stage_desc, video_binary,
			out_frame, in_frame, NULL, NULL);
	} else {
		ia_css_pipe_get_generic_stage_desc(&stage_desc, video_binary,
			out_frame, in_frame, NULL, vf_frame);
	}
	err = ia_css_pipeline_create_and_add_stage(me,
			&stage_desc,
			&video_stage);
	if (err != IA_CSS_SUCCESS)
		goto ERR;

	/* If we use copy iso video, the input must be yuv iso raw */
	if(video_stage) {
		video_stage->args.copy_vf =
			video_binary->info->sp.mode == IA_CSS_BINARY_MODE_COPY;
		video_stage->args.copy_output = video_stage->args.copy_vf;
	}

	/* when the video binary supports only 1 output pin, vf_pp is needed to
	produce the vf_frame.*/
	if (need_vf_pp && video_stage) {
		err = add_vf_pp_stage(pipe, vf_frame, vf_pp_binary,
				      video_stage, &vf_pp_stage);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}

	pipe->pipeline.acquire_isp_each_stage = false;
	ia_css_pipeline_finalize_stages(&pipe->pipeline);

	if (video_stage) {
		video_stage->args.in_ref_frame = pipe->pipe_settings.video.ref_frames[0];
		video_stage->args.out_ref_frame = pipe->pipe_settings.video.ref_frames[1];
		video_stage->args.extra_ref_frame = pipe->pipe_settings.video.ref_frames[2];
		video_stage->args.in_tnr_frame = pipe->pipe_settings.video.tnr_frames[0];
		video_stage->args.out_tnr_frame = pipe->pipe_settings.video.tnr_frames[1];
	}

	/* update the arguments with the latest info */
	if (video_stage)
		video_stage->args.out_frame = out_frame;

	if (vf_pp_stage)
		vf_pp_stage->args.out_frame = vf_frame;

	if (pipe->stream->config.continuous) {
		if (video_stage)
			video_stage->args.in_frame =  pipe->continuous_frames[0];
	}

	configure_pipe_inout_port(&pipe->pipeline,
		pipe->stream->config.continuous);

ERR:
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_video_pipeline() leave: return (%x)\n", err);
	return err;
}

static enum ia_css_err
create_host_acc_pipeline(struct ia_css_pipe *pipe)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	unsigned int i;

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"create_host_acc_pipeline() enter\n");

	for (i=0; i<pipe->config.num_acc_stages; i++) {
		struct ia_css_fw_info *fw = pipe->config.acc_stages[i];
		err = sh_css_pipeline_add_acc_stage(&pipe->pipeline, fw);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}

	ia_css_pipeline_finalize_stages(&pipe->pipeline);
	configure_pipe_inout_port(&pipe->pipeline, false);

ERR:
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"create_host_acc_pipeline() leave: return(%d)\n", err);
	return err;
}

/* Create stages for preview */
static enum ia_css_err
create_host_preview_pipeline(struct ia_css_pipe *pipe)
{
	struct ia_css_pipeline_stage *post_stage;
	struct ia_css_pipeline_stage_desc stage_desc;
	struct ia_css_pipeline_stage *preview_stage, *out_stage = NULL;
	struct ia_css_pipeline *me = NULL;
	struct ia_css_binary *copy_binary, *preview_binary, *vf_pp_binary = NULL;
	struct ia_css_frame *in_frame = NULL, *cc_frame = NULL;
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_frame *out_frame;
	bool need_in_frameinfo_memory = false;
#ifdef USE_INPUT_SYSTEM_VERSION_2401
	bool sensor = false;
	bool buffered_sensor = false;
	bool online = false;
	bool continuous = false;
#endif

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"create_host_preview_pipeline(): enter\n");

	/* pipeline already created as part of create_host_pipeline_structure */
	me = &pipe->pipeline;
	ia_css_pipeline_clean(me);

#ifdef USE_INPUT_SYSTEM_VERSION_2401
	/* When the input system is 2401, always enable 'in_frameinfo_memory'
	 * except for the following:
	 * - Direct Sensor Mode Online Preview
	 * - Direct Sensor Mode Continuous Preview
	 * - Buffered Sensor Mode Continous Preview
	 */
	sensor = pipe->stream->config.mode == IA_CSS_INPUT_MODE_SENSOR;
	buffered_sensor = pipe->stream->config.mode == IA_CSS_INPUT_MODE_BUFFERED_SENSOR;
	online = pipe->stream->config.online;
	continuous = pipe->stream->config.continuous;
	need_in_frameinfo_memory =
		!((sensor && (online || continuous)) || (buffered_sensor && continuous));
#else
	/* Construct in_frame info (only in case we have dynamic input */
	need_in_frameinfo_memory = pipe->stream->config.mode == IA_CSS_INPUT_MODE_MEMORY;
#endif
	if (need_in_frameinfo_memory) {
		err = init_in_frameinfo_memory_defaults(pipe, &me->in_frame);
		if (err != IA_CSS_SUCCESS)
			goto ERR;

		in_frame = &me->in_frame;
	} else {
		in_frame = NULL;
	}

	err = init_out_frameinfo_defaults(pipe, &me->out_frame);
	if (err != IA_CSS_SUCCESS)
		goto ERR;
	out_frame = &me->out_frame;

	copy_binary    = &pipe->pipe_settings.preview.copy_binary;
	preview_binary = &pipe->pipe_settings.preview.preview_binary;
	if (pipe->pipe_settings.preview.vf_pp_binary.info)
		vf_pp_binary = &pipe->pipe_settings.preview.vf_pp_binary;

	if (pipe->pipe_settings.preview.copy_binary.info) {
		ia_css_pipe_get_generic_stage_desc(&stage_desc, copy_binary,
			NULL, NULL, NULL, NULL);
		err = ia_css_pipeline_create_and_add_stage(me,
				&stage_desc,
				&post_stage);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
		in_frame = me->stages->args.out_frame;
	} else {
#ifdef USE_INPUT_SYSTEM_VERSION_2401
		/* When continuous is enabled, configure in_frame with the
		 * last pipe, which is the copy pipe.
		 */
		if (continuous)
			in_frame = pipe->stream->last_pipe->continuous_frames[0];
#else
		in_frame = pipe->continuous_frames[0];
#endif
	}

	if (vf_pp_binary) {
		ia_css_pipe_get_generic_stage_desc(&stage_desc, preview_binary,
			NULL, in_frame, cc_frame, NULL);
	} else {
		ia_css_pipe_get_generic_stage_desc(&stage_desc, preview_binary,
			out_frame, in_frame, cc_frame, NULL);
	}
	err = ia_css_pipeline_create_and_add_stage(me,
		&stage_desc,
		&post_stage);
	if (err != IA_CSS_SUCCESS)
		goto ERR;
	/* If we use copy iso preview, the input must be yuv iso raw */
	post_stage->args.copy_vf =
		preview_binary->info->sp.mode == IA_CSS_BINARY_MODE_COPY;
	post_stage->args.copy_output = !post_stage->args.copy_vf;
	if (post_stage->args.copy_vf && !post_stage->args.out_vf_frame) {
		/* in case of copy, use the vf frame as output frame */
		post_stage->args.out_vf_frame =
			post_stage->args.out_frame;
	}
	if (vf_pp_binary) {
		err = add_vf_pp_stage(pipe, out_frame, vf_pp_binary,
				post_stage, &out_stage);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}

	pipe->pipeline.acquire_isp_each_stage = false;
	ia_css_pipeline_finalize_stages(&pipe->pipeline);

	if (vf_pp_binary) {
		err = ia_css_pipeline_get_output_stage(me, IA_CSS_BINARY_MODE_VF_PP,
						       &out_stage);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}

	err = ia_css_pipeline_get_stage(me, preview_binary->info->sp.mode,
					&preview_stage);
	if (err != IA_CSS_SUCCESS)
		goto ERR;

	if (!out_stage)
		out_stage = preview_stage;

	out_stage->args.out_frame = out_frame;
	if (pipe->stream->config.continuous) {
		preview_stage->args.in_frame = pipe->continuous_frames[0];
	}

	configure_pipe_inout_port(&pipe->pipeline,
		pipe->stream->config.continuous);

ERR:
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"create_host_preview_pipeline() leave: return(%d)\n", err);
	return err;
}

static void send_raw_frames(struct ia_css_pipe *pipe)
{
	if (pipe->stream->config.continuous) {
		struct ia_css_frame *in_frame = NULL;
		unsigned int i;

		in_frame = pipe->continuous_frames[0];

		sh_css_update_host2sp_cont_num_raw_frames
			(pipe->stream->config.init_num_cont_raw_buf, true);
		sh_css_update_host2sp_cont_num_raw_frames
			(pipe->stream->config.target_num_cont_raw_buf, false);

		/* Hand-over all the SP-internal buffers */
		for (i = 0; i < pipe->stream->config.init_num_cont_raw_buf; i++) {
			sh_css_update_host2sp_offline_frame(i,
				pipe->continuous_frames[i]);
		}
	}

	return;
}

static enum ia_css_err
preview_start(struct ia_css_pipe *pipe)
{
	struct ia_css_pipeline *me ;
	struct ia_css_binary *copy_binary, *preview_binary, *vf_pp_binary = NULL;
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_pipe *copy_pipe, *capture_pipe;
	enum sh_css_pipe_config_override copy_ovrd;
	enum ia_css_input_mode preview_pipe_input_mode;

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	me = &pipe->pipeline;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
	"preview_start() enter: void\n");

	preview_pipe_input_mode = pipe->stream->config.mode;

	copy_pipe    = pipe->pipe_settings.preview.copy_pipe;
	capture_pipe = pipe->pipe_settings.preview.capture_pipe;

	copy_binary    = &pipe->pipe_settings.preview.copy_binary;
	preview_binary = &pipe->pipe_settings.preview.preview_binary;
	if (pipe->pipe_settings.preview.vf_pp_binary.info)
		vf_pp_binary = &pipe->pipe_settings.preview.vf_pp_binary;

	sh_css_metrics_start_frame();

	/* multi stream video needs mipi buffers */
	send_mipi_frames(pipe);
	send_raw_frames(pipe);

	{
		unsigned int thread_id;

		ia_css_pipeline_get_sp_thread_id(ia_css_pipe_get_pipe_num(pipe), &thread_id);
		copy_ovrd = 1 << thread_id;

		if (pipe->stream->cont_capt) {
			ia_css_pipeline_get_sp_thread_id(ia_css_pipe_get_pipe_num(capture_pipe), &thread_id);
			copy_ovrd |= 1 << thread_id;
		}
	}

#if !defined(HAS_NO_INPUT_SYSTEM)
	err = sh_css_config_input_network(pipe, copy_binary);
	if (err != IA_CSS_SUCCESS)
		goto ERR;
#endif

	/* Construct and load the copy pipe */
	if (pipe->stream->config.continuous) {
		sh_css_sp_init_pipeline(&copy_pipe->pipeline,
			IA_CSS_PIPE_ID_COPY,
			(uint8_t)ia_css_pipe_get_pipe_num(copy_pipe),
			false,
			pipe->stream->config.pixels_per_clock == 2, false,
			false, pipe->required_bds_factor,
			copy_ovrd,
			pipe->stream->config.mode, NULL
#if !defined(HAS_NO_INPUT_SYSTEM)
			, pipe->stream->config.source.port.port
#endif
			);

		/* make the preview pipe start with mem mode input, copy handles
		   the actual mode */
		preview_pipe_input_mode = IA_CSS_INPUT_MODE_MEMORY;
	}

	/* Construct and load the capture pipe */
	if (pipe->stream->cont_capt) {
		sh_css_sp_init_pipeline(&capture_pipe->pipeline,
			IA_CSS_PIPE_ID_CAPTURE,
			(uint8_t)ia_css_pipe_get_pipe_num(capture_pipe),
			pipe->config.default_capture_config.enable_xnr,
			capture_pipe->stream->config.pixels_per_clock == 2,
			true, /* continuous */
			false, /* offline */
			capture_pipe->required_bds_factor,
			0,
			IA_CSS_INPUT_MODE_MEMORY, NULL
#if !defined(HAS_NO_INPUT_SYSTEM)
			, (mipi_port_ID_t)0
#endif
			);
	}

	err = start_pipe(pipe, copy_ovrd, preview_pipe_input_mode);

#if !defined(HAS_NO_INPUT_SYSTEM)
ERR:
#endif
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"preview_start() leave: return (%d)\n", err);
	return err;
}

enum ia_css_err
ia_css_pipe_enqueue_buffer(struct ia_css_pipe *pipe,
			   const struct ia_css_buffer *buffer)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	unsigned int thread_id, i;
	enum sh_css_buffer_queue_id queue_id;
	struct ia_css_pipeline *pipeline;
	struct ia_css_pipeline_stage *stage;
	struct ia_css_rmgr_vbuf_handle p_vbuf;
	struct ia_css_rmgr_vbuf_handle *h_vbuf;
	struct sh_css_hmm_buffer ddr_buffer;
	enum ia_css_buffer_type buf_type;
	enum ia_css_pipe_id pipe_id;
	ia_css_queue_t* q;

	if (pipe == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	if (buffer == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	buf_type = buffer->type;
	pipe_id = pipe->mode;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipe_enqueue_buffer() enter: pipe_id=%d, buf_type=%d, buffer=%p\n",
		pipe_id, buf_type, buffer);

	assert(pipe_id < IA_CSS_PIPE_ID_NUM);
	assert(buf_type < IA_CSS_BUFFER_TYPE_NUM);

	//ia_css_pipeline_get_sp_thread_id(pipe_id, &thread_id);
	ia_css_pipeline_get_sp_thread_id(ia_css_pipe_get_pipe_num(pipe), &thread_id);

	sh_css_query_internal_queue_id(buf_type, &queue_id);
	if ((queue_id < 0) || (queue_id > sh_css_buffer_queue_id_last))
			return IA_CSS_ERR_INVALID_ARGUMENTS;

	/* Get the queue for communication */
	q = sh_css_get_queue(sh_css_host2sp_buffer_queue,
		queue_id, thread_id);
	if ( NULL == q ) {
		/* Error as the queue is not initialized */
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"ia_css_pipe_enqueue_buffer() leave: return_err=%d\n",
			IA_CSS_ERR_RESOURCE_NOT_AVAILABLE);
		return IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;
	}

	pipeline = &pipe->pipeline;

	assert(pipeline != NULL ||
	       pipe_id == IA_CSS_PIPE_ID_COPY ||
	       pipe_id == IA_CSS_PIPE_ID_ACC);

	assert(sizeof(NULL) <= sizeof(ddr_buffer.kernel_ptr));
	ddr_buffer.kernel_ptr = HOST_ADDRESS(NULL);
	ddr_buffer.cookie_ptr = buffer->driver_cookie;

	if (buf_type == IA_CSS_BUFFER_TYPE_3A_STATISTICS) {
		if (buffer->data.stats_3a == NULL)
			return IA_CSS_ERR_INVALID_ARGUMENTS;
		ddr_buffer.kernel_ptr = HOST_ADDRESS(buffer->data.stats_3a);
		ddr_buffer.payload.s3a = *buffer->data.stats_3a;
	} else if (buf_type == IA_CSS_BUFFER_TYPE_DIS_STATISTICS) {
		if (buffer->data.stats_dvs == NULL)
			return IA_CSS_ERR_INVALID_ARGUMENTS;
		ddr_buffer.kernel_ptr = HOST_ADDRESS(buffer->data.stats_dvs);
		ddr_buffer.payload.dis = *buffer->data.stats_dvs;
	} else if ((buf_type == IA_CSS_BUFFER_TYPE_METADATA)) {
		ddr_buffer.kernel_ptr = HOST_ADDRESS(buffer->data.metadata);
		ddr_buffer.payload.metadata = *buffer->data.metadata;
	} else if ((buf_type == IA_CSS_BUFFER_TYPE_INPUT_FRAME)
		|| (buf_type == IA_CSS_BUFFER_TYPE_OUTPUT_FRAME)
		|| (buf_type == IA_CSS_BUFFER_TYPE_VF_OUTPUT_FRAME)) {
		if (buffer->data.frame == NULL)
			return IA_CSS_ERR_INVALID_ARGUMENTS;
		ddr_buffer.kernel_ptr = HOST_ADDRESS(buffer->data.frame);
		ddr_buffer.payload.frame.frame_data = buffer->data.frame->data;
		ddr_buffer.payload.frame.flashed = 0;
	}
/* start of test for using rmgr for acq/rel memory */
	p_vbuf.vptr = 0;
	p_vbuf.count = 0;
	p_vbuf.size = sizeof(struct sh_css_hmm_buffer);
	h_vbuf = &p_vbuf;
	// TODO: change next to correct pool for optimization
	ia_css_rmgr_acq_vbuf(hmm_buffer_pool, &h_vbuf);

	assert(h_vbuf != NULL);
	assert(h_vbuf->vptr != 0x0);

	if ((h_vbuf == NULL) || (h_vbuf->vptr == 0x0))
		return IA_CSS_ERR_INTERNAL_ERROR;

	mmgr_store(h_vbuf->vptr,
				(void *)(&ddr_buffer),
				sizeof(struct sh_css_hmm_buffer));
	if ((buf_type == IA_CSS_BUFFER_TYPE_3A_STATISTICS)
		|| (buf_type == IA_CSS_BUFFER_TYPE_DIS_STATISTICS)) {
		assert(pipeline != NULL);
		for (stage = pipeline->stages; stage; stage = stage->next) {
			/* The SP will read the params
				after it got empty 3a and dis */
			if (STATS_ENABLED(stage)) {
				/* there is a stage that needs it */
				err = ia_css_queue_enqueue(q,
					(uint32_t)h_vbuf->vptr);
			}
		}
	} else if ((buf_type == IA_CSS_BUFFER_TYPE_INPUT_FRAME)
		|| (buf_type == IA_CSS_BUFFER_TYPE_OUTPUT_FRAME)
		|| (buf_type == IA_CSS_BUFFER_TYPE_VF_OUTPUT_FRAME)
		|| (buf_type == IA_CSS_BUFFER_TYPE_METADATA)) {
			err = ia_css_queue_enqueue(q,
				(uint32_t)h_vbuf->vptr);
	}

	if (err == IA_CSS_SUCCESS) {
		for (i = 0; i < MAX_HMM_BUFFER_NUM; i++) {
			if (hmm_buffer_record_h[i] == NULL) {
				hmm_buffer_record_h[i] = h_vbuf;
				ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
					     "ia_css_pipe_enqueue_buffer()"
					     " send vbuf=%x\n",h_vbuf);
				break;
			}
		}
	} else {
		ia_css_rmgr_rel_vbuf(hmm_buffer_pool, &h_vbuf);
	}

		/*
		 * Tell the SP which queues are not empty,
		 * by sending the software event.
		 */
	if (err == IA_CSS_SUCCESS) {
		ia_css_queue_t *q;
		q = sh_css_get_queue(sh_css_host2sp_event_queue, -1, -1);
		if (NULL == q) {
			/* Error as the queue is not initialized */
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
				"ia_css_pipe_enqueue_buffer() leaving: host2sp eventq not available\n");
			return IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;
		}
		ia_css_eventq_send(q,
				SP_SW_EVENT_ID_1,
				(uint8_t)thread_id,
				queue_id,
				0);
	}

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipe_enqueue_buffer() leave: return_err=%d\n",err);
	return err;
}

/*
 * TODO: Free up the hmm memory space.
	 */
enum ia_css_err
ia_css_pipe_dequeue_buffer(struct ia_css_pipe *pipe,
			   struct ia_css_buffer *buffer)
{
	enum ia_css_err err;
	enum sh_css_buffer_queue_id queue_id;
	hrt_vaddress ddr_buffer_addr;
	struct sh_css_hmm_buffer ddr_buffer;
	unsigned int i, found_record;
	enum ia_css_buffer_type buf_type;
	enum ia_css_pipe_id pipe_id;
	ia_css_queue_t* q;

	if (buffer == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	if (pipe == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	pipe_id = pipe->mode;

	buf_type = buffer->type;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipe_dequeue_buffer() enter: pipe_id=%d, buf_type=%d\n",
		(int)pipe_id, buf_type);

	ddr_buffer.kernel_ptr = 0;

	sh_css_query_internal_queue_id(buf_type, &queue_id);
	if ((queue_id < 0) || (queue_id > sh_css_buffer_queue_id_last))
			return IA_CSS_ERR_INVALID_ARGUMENTS;

	q = sh_css_get_queue(sh_css_sp2host_buffer_queue,
		queue_id, -1);
	if ( NULL == q ) {
		/* Error as the queue is not initialized */
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"ia_css_pipe_dequeue_buffer() leave: err=%d\n",
			IA_CSS_ERR_RESOURCE_NOT_AVAILABLE);
		return IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;
	}

	err = ia_css_queue_dequeue(q,
		(uint32_t *)&ddr_buffer_addr);
	if (err == IA_CSS_SUCCESS) {
		struct ia_css_frame *frame;
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			     "ia_css_pipe_dequeue_buffer() receive vbuf=%x\n",
			     (int)ddr_buffer_addr);

		found_record = 0;
		for (i = 0; i < MAX_HMM_BUFFER_NUM; i++) {
			if (hmm_buffer_record_h[i] != NULL && hmm_buffer_record_h[i]->vptr == ddr_buffer_addr) {
				ia_css_rmgr_rel_vbuf(hmm_buffer_pool, &hmm_buffer_record_h[i]);
				hmm_buffer_record_h[i] = NULL;
				found_record = 1;
				break;
			}
		}

		assert(found_record == 1);

		mmgr_load(ddr_buffer_addr,
				&ddr_buffer,
				sizeof(struct sh_css_hmm_buffer));

		assert(ddr_buffer.kernel_ptr != 0);

		/* if the pointer is 0 we only want to return an error;
		   we do not want to touch buffer
		*/
		if (ddr_buffer.kernel_ptr == 0)
			err = IA_CSS_ERR_INTERNAL_ERROR;
		else {
			/* buffer->exp_id : all instances to be removed later once the driver change
			 * is completed. See patch #5758 for reference */
			buffer->exp_id = 0;
			buffer->driver_cookie = ddr_buffer.cookie_ptr;

			switch (buf_type) {
			case IA_CSS_BUFFER_TYPE_INPUT_FRAME:
			case IA_CSS_BUFFER_TYPE_OUTPUT_FRAME:
				if ((pipe) && (pipe->stop_requested == true))
				{
					free_mipi_frames(pipe, false);
					pipe->stop_requested = false;
				}
			case IA_CSS_BUFFER_TYPE_VF_OUTPUT_FRAME:
				frame = (struct ia_css_frame*)HOST_ADDRESS(ddr_buffer.kernel_ptr);
				buffer->data.frame = frame;
				buffer->exp_id = ddr_buffer.payload.frame.exp_id;
				frame->exp_id = ddr_buffer.payload.frame.exp_id;
				if (ddr_buffer.payload.frame.flashed == 1)
					frame->flash_state =
						IA_CSS_FRAME_FLASH_STATE_PARTIAL;
				if (ddr_buffer.payload.frame.flashed == 2)
					frame->flash_state =
						IA_CSS_FRAME_FLASH_STATE_FULL;
				frame->valid = pipe->num_invalid_frames == 0;
				if (!frame->valid)
					pipe->num_invalid_frames--;
				if (frame->info.format ==
						IA_CSS_FRAME_FORMAT_BINARY_8) {
					frame->planes.binary.size =
					    sh_css_sp_get_binary_copy_size();
				}
				break;
			case IA_CSS_BUFFER_TYPE_3A_STATISTICS:
				buffer->data.stats_3a =
					(struct ia_css_isp_3a_statistics*)HOST_ADDRESS(ddr_buffer.kernel_ptr);
				buffer->exp_id = ddr_buffer.payload.s3a.exp_id;
				buffer->data.stats_3a->exp_id = ddr_buffer.payload.s3a.exp_id;
				break;
			case IA_CSS_BUFFER_TYPE_DIS_STATISTICS:
				buffer->data.stats_dvs =
					(struct ia_css_isp_dvs_statistics*)HOST_ADDRESS(ddr_buffer.kernel_ptr);
				buffer->exp_id = ddr_buffer.payload.dis.exp_id;
				buffer->data.stats_dvs->exp_id = ddr_buffer.payload.dis.exp_id;
				break;
			case IA_CSS_BUFFER_TYPE_METADATA:
				buffer->data.metadata =
					(struct ia_css_metadata*)HOST_ADDRESS(ddr_buffer.kernel_ptr);
				buffer->exp_id = ddr_buffer.payload.metadata.exp_id;
				buffer->data.metadata->exp_id = ddr_buffer.payload.metadata.exp_id;
				break;
			default:
				err = IA_CSS_ERR_INTERNAL_ERROR;
				break;
			}
		}
	}


	/*
	 * Tell the SP which queues are not full,
	 * by sending the software event.
	 */
	if (err == IA_CSS_SUCCESS) {
		ia_css_queue_t *q;
		q = sh_css_get_queue(sh_css_host2sp_event_queue, -1, -1);
		if (NULL == q) {
			/* Error as the queue is not initialized */
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
				"ia_css_pipe_dequeue_buffer() leaving: host2sp eventq not available\n");
			return IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;
		}
		ia_css_eventq_send(q,
				SP_SW_EVENT_ID_2,
				0,
				queue_id,
				0);
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipe_dequeue_buffer() leave: buffer=%p\n", buffer);

	return err;
}

/*
 * Cannot Move this to event module as it is of ia_css_event_type which is declared in ia_css.h
 * TODO: modify and move it if possible.
 * */
static enum ia_css_event_type convert_event_sp_to_host_domain[] = {
	IA_CSS_EVENT_TYPE_OUTPUT_FRAME_DONE,	/**< Output frame ready. */
	IA_CSS_EVENT_TYPE_VF_OUTPUT_FRAME_DONE,	/**< Viewfinder Output frame ready. */
	IA_CSS_EVENT_TYPE_3A_STATISTICS_DONE,	/**< Indication that 3A statistics are available. */
	IA_CSS_EVENT_TYPE_DIS_STATISTICS_DONE,	/**< Indication that DIS statistics are available. */
	IA_CSS_EVENT_TYPE_PIPELINE_DONE,	/**< Pipeline Done event, sent after last pipeline stage. */
	IA_CSS_EVENT_TYPE_FRAME_TAGGED,		/**< Frame tagged. */
	IA_CSS_EVENT_TYPE_INPUT_FRAME_DONE,	/**< Input frame ready. */
	IA_CSS_EVENT_TYPE_METADATA_DONE,	/**< Metadata ready. */
	IA_CSS_EVENT_TYPE_PORT_EOF,		/**< End Of Frame event, sent when in buffered sensor mode.  MUST BE LAST */
	0,					/** error if sp passes  SH_CSS_SP_EVENT_NR_OF_TYPES as a valid event. */
};

enum ia_css_err
ia_css_dequeue_event(struct ia_css_event *event)
{
	enum ia_css_pipe_id pipe_id;
	uint8_t payload[4] = {0,0,0,0};
	ia_css_queue_t* q;

	if (event == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

        /*TODO:
	 * a) use generic decoding function , same as the one used by sp.
	 * b) group decode and dequeue into eventQueue module
	 */
	q = sh_css_get_queue(sh_css_sp2host_event_queue, -1, -1);
	if ( NULL == q ) {
		/* Error as the queue is not initialized */
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"ia_css_dequeue_event() leaving: Not available\n");
		return IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;
	}

	/* dequeue the IRQ event */
	/* check whether the IRQ event is available or not */
	if (IA_CSS_SUCCESS != ia_css_eventq_recv(q, payload)) {
		//ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		//      "ia_css_dequeue_event() out: EVENT_QUEUE_EMPTY\n");
		return IA_CSS_ERR_QUEUE_IS_EMPTY;
	} else {
		/*
		 * Tell the SP which queues are not full,
		 * by sending the software event.
		 */
		ia_css_queue_t *send_q;
		send_q = sh_css_get_queue(sh_css_host2sp_event_queue, -1, -1);
		/* No need to check here as queue is definetly initialized
		 * by the time we reach here.*/
		ia_css_eventq_send(send_q, SP_SW_EVENT_ID_3, 0, 0, 0);
	}

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_dequeue_event() enter: queue not empty\n");
	/*  queue contains an event, it is decoded into 4 bytes of payload,
	 *  convert sp event type in payload to host event type,
	 *  TODO: can this enum conversion be eliminated */
	event->type = convert_event_sp_to_host_domain[payload[0]];
	pipe_id = (enum ia_css_pipe_id)payload[2];

	if (event->type != IA_CSS_EVENT_TYPE_PORT_EOF) {
		/* pipe related events */
		event->pipe = find_pipe_by_num(payload[1]);
		if (!event->pipe)
		{
			/* an event is generated of a port that is not longer available*/
			return IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;
		}
		event->port = 0;
		event->exp_id = 0;
		if (event->type == IA_CSS_EVENT_TYPE_FRAME_TAGGED)
		{
			/* find the capture pipe that goes with this */
			int i, n;
			n = event->pipe->stream->num_pipes;
			for (i = 0; i < n; i++)
			{
				if (event->pipe->stream->pipes[i]->config.mode == IA_CSS_PIPE_MODE_CAPTURE)
				{
					event->pipe = event->pipe->stream->pipes[i];
					break;
				}
			}
			event->exp_id = payload[3];
		}
	}
	else {
		/* handle IA_CSS_EVENT_TYPE_PORT_EOF */
		/* event is not directly correlated to a pipe */
		event->pipe = NULL;
		event->port = payload[1];
		event->exp_id = payload[3];
	}
#if 0
  /* temporary debug code to dump the intermediate vf buffer to raw file*/
	if (event->type == IA_CSS_EVENT_TYPE_VF_OUTPUT_FRAME_DONE)
	{
	  static int count = 0;
	  FILE* fp;
    char *xmem_y_addr  = (char*)(event->pipe->pipeline.stages->args.out_vf_frame->data + event->pipe->pipeline.stages->args.out_vf_frame->planes.yuyv.offset);
    unsigned stride = event->pipe->pipeline.stages->args.out_vf_frame->planes.yuyv.stride;
    unsigned height = event->pipe->pipeline.stages->args.out_vf_frame->planes.yuyv.height;
    //printf ("init_frame_pointers format %d\n", stage->frames.in.info.format);
    //if (stage->frames.in.info.format == IA_CSS_FRAME_FORMAT_YUV_LINE) {
    unsigned char pixel;
    unsigned char buf[5000];
    unsigned int i;
    mmgr_load(xmem_y_addr, &pixel, sizeof(char));
    printf ("vf_pp: line 0 pixel %d\n", pixel);
    mmgr_load(xmem_y_addr+stride, &pixel, sizeof(char));
    printf ("vf_pp: line 1 pixel %d\n", pixel);
    mmgr_load(xmem_y_addr+2*stride, &pixel, sizeof(char));
    printf ("vf_pp: line 2 pixel %d\n", pixel);
    mmgr_load(xmem_y_addr+3*stride, &pixel, sizeof(char));
    printf ("vf_pp: line 3 pixel %d\n", pixel);

    fp = fopen("dump.raw", "w+");

    for (i=0; i< height/2; i++)
    {
      mmgr_load(xmem_y_addr, buf, stride);
      fwrite(buf, 1, stride, fp);
      xmem_y_addr += stride;
      mmgr_load(xmem_y_addr, buf, stride);
      fwrite(buf, 1, stride, fp);
      xmem_y_addr += stride;
      xmem_y_addr += stride;

    }
    fclose(fp);

    count++;
	}
#endif
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_dequeue_event() leave: pipe_id=%d, event_id=%d\n",
				pipe_id, event->type);

	return IA_CSS_SUCCESS;
}

static enum ia_css_err
acc_start(struct ia_css_pipe *pipe)
{
	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	ia_css_pipeline_start(pipe->mode, &pipe->pipeline);
	return start_pipe(pipe, SH_CSS_PIPE_CONFIG_OVRD_NO_OVRD,
			pipe->stream->config.mode);
}

static enum ia_css_err
sh_css_pipe_start(struct ia_css_stream *stream)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	struct ia_css_pipe *pipe;
	enum ia_css_pipe_id pipe_id;
	unsigned int thread_id;
	ia_css_queue_t *q;

	assert(stream != NULL);
	pipe = stream->last_pipe;
	assert(pipe != NULL);

	pipe_id = pipe->mode;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"sh_css_pipe_start() enter: pipe_id=%d\n", pipe_id);
#ifdef __KERNEL__
	printk("sh_css_pipe_start() enter: pipe_id=%d\n", pipe_id);
#endif

	if(stream->started == true) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"Cannot start stream that is already started\n");
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"sh_css_pipe_start() leave: return_err=%d\n", err);
		return err;
	}

	pipe->stop_requested = false;

	switch (pipe_id) {
	case IA_CSS_PIPE_ID_PREVIEW:
		err = preview_start(pipe);
		break;
	case IA_CSS_PIPE_ID_VIDEO:
		err = video_start(pipe);
		break;
	case IA_CSS_PIPE_ID_CAPTURE:
		err = capture_start(pipe);
		break;
	case IA_CSS_PIPE_ID_ACC:
		err = acc_start(pipe);
		break;
	default:
		err = IA_CSS_ERR_INVALID_ARGUMENTS;
	}
	if (err != IA_CSS_SUCCESS) {
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"sh_css_pipe_start() leave: return_err=%d\n", err);
		return err;
	}

	/* Force ISP parameter calculation after a mode change
	 * Acceleration API examples pass NULL for stream but they
	 * don't use ISP parameters anyway. So this should be okay.
	 * The SP binary (jpeg) copy does not use any parameters.
	 */
	if (!copy_on_sp(pipe)) {
		sh_css_invalidate_params(stream);
		err = sh_css_param_update_isp_params(stream, true, NULL);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	ia_css_debug_pipe_graph_dump_epilogue();

	ia_css_pipeline_get_sp_thread_id(ia_css_pipe_get_pipe_num(pipe), &thread_id);

	q = sh_css_get_queue(sh_css_host2sp_event_queue, -1, -1);
	if (NULL == q) {
		/* Error as the queue is not initialized */
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"sh_css_pipe_start() leaving: host2sp_eventq not available\n");
		return IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;
	}
	/* casting eventq payload to ensure it is encodable*/
	ia_css_eventq_send(q, SP_SW_EVENT_ID_4, (uint8_t)thread_id, 0,  0);

	/* in case of continuous capture mode, we also start capture thread and copy thread*/
	if (pipe->stream->config.continuous) {
		struct ia_css_pipe *copy_pipe = NULL;

		if (pipe_id == IA_CSS_PIPE_ID_PREVIEW)
			copy_pipe = pipe->pipe_settings.preview.copy_pipe;
		else if (pipe_id == IA_CSS_PIPE_ID_VIDEO)
			copy_pipe = pipe->pipe_settings.video.copy_pipe;

		assert(copy_pipe != NULL);
		if (copy_pipe == NULL)
			return IA_CSS_ERR_INTERNAL_ERROR;
		ia_css_pipeline_get_sp_thread_id(ia_css_pipe_get_pipe_num(copy_pipe), &thread_id);
		 /* by the time we reach here q is initialized and handle is available.*/
		ia_css_eventq_send(q, SP_SW_EVENT_ID_4, (uint8_t)thread_id, 0,  0);
	}
	if (pipe->stream->cont_capt) {
		struct ia_css_pipe *capture_pipe = NULL;
		if (pipe_id == IA_CSS_PIPE_ID_PREVIEW)
			capture_pipe = pipe->pipe_settings.preview.capture_pipe;
		else if (pipe_id == IA_CSS_PIPE_ID_VIDEO)
			capture_pipe = pipe->pipe_settings.video.capture_pipe;

		assert(capture_pipe != NULL);
		if (capture_pipe == NULL)
			return IA_CSS_ERR_INTERNAL_ERROR;
		ia_css_pipeline_get_sp_thread_id(ia_css_pipe_get_pipe_num(capture_pipe), &thread_id);
		 /* by the time we reach here q is initialized and handle is available.*/
		ia_css_eventq_send(q, SP_SW_EVENT_ID_4, (uint8_t)thread_id, 0,  0);
	}

	stream->started = true;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"sh_css_pipe_start() leave: return_err=%d\n", err);
	return err;
}

#if 0
static enum ia_css_err sh_css_pipeline_stop(
	struct ia_css_pipe *pipe)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipeline_stop() enter\n");
	(void)pipe;
	/* TO BE IMPLEMENTED */
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipeline_stop() exit\n");
	return IA_CSS_SUCCESS;
}
#endif

/* this is a legacy from CSS 1.5, we still need this here because we need this info
   to configure the sensor ouput in CameraDriver, but it is not needed in real device.
   add this macro to make this explicit. */
#if defined(HRT_CSIM) || defined(HRT_RTL)
static enum ia_css_err
sh_css_pipe_get_input_resolution(struct ia_css_pipe *pipe,
				 unsigned int *width,
				 unsigned int *height)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(pipe != NULL);
	assert(pipe->stream != NULL);
	assert(width != NULL);
	assert(height != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_get_input_resolution() enter: void\n");
	if (pipe->mode == IA_CSS_PIPE_ID_CAPTURE && copy_on_sp(pipe) &&
	    pipe->stream->config.format == IA_CSS_STREAM_FORMAT_BINARY_8) {
		*width = JPEG_BYTES;
		*height = 1;
		return IA_CSS_SUCCESS;
	}

	{
		const struct ia_css_binary *binary = NULL;
		if (pipe->mode == IA_CSS_PIPE_ID_PREVIEW) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
				"sh_css_pipe_get_input_resolution: preview\n");
			if (pipe->pipe_settings.preview.copy_binary.info)
				binary = &pipe->pipe_settings.preview.copy_binary;
			else
				binary = &pipe->pipe_settings.preview.preview_binary;

		}
		else if (pipe->mode == IA_CSS_PIPE_ID_VIDEO) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
				"sh_css_pipe_get_input_resolution: video\n");
			if (pipe->pipe_settings.video.copy_binary.info)
				binary = &pipe->pipe_settings.video.copy_binary;
			else
				binary = &pipe->pipe_settings.video.video_binary;
		}
		else if (pipe->mode == IA_CSS_PIPE_ID_CAPTURE) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
				"sh_css_pipe_get_input_resolution: capture\n");
			if (pipe->pipe_settings.capture.copy_binary.info)
				binary = &pipe->pipe_settings.capture.copy_binary;
			else if (pipe->pipe_settings.capture.primary_binary.info)
				binary = &pipe->pipe_settings.capture.primary_binary;
			else
				binary = &pipe->pipe_settings.capture.pre_isp_binary;
		}
		if (binary) {
			*width  = binary->in_frame_info.res.width +
#if !defined(HAS_NO_INPUT_SYSTEM) && !defined(HAS_NO_INPUT_FORMATTER)
				ia_css_ifmtr_columns_needed_for_bayer_order(&pipe->stream->config);
#else
				0;
#endif

			*height = binary->in_frame_info.res.height +
#if !defined(HAS_NO_INPUT_SYSTEM) && !defined(HAS_NO_INPUT_FORMATTER)
				ia_css_ifmtr_lines_needed_for_bayer_order(&pipe->stream->config);
#else
				0;
#endif
		} else
			err = IA_CSS_ERR_INTERNAL_ERROR;
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"sh_css_pipe_get_input_resolution() leave: width=%d, height=%d err=%d\n",
		*width, *height, err);
	return err;
}
#endif

void
sh_css_enable_cont_capt(bool enable, bool stop_copy_preview)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"sh_css_enable_cont_capt() enter: enable=%d\n", enable);
	//my_css.cont_capt = enable;
	my_css.stop_copy_preview = stop_copy_preview;
}

bool
sh_css_continuous_is_enabled(uint8_t pipe_num)
{
	struct ia_css_pipe *pipe;
	bool continuous;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_continuous_is_enabled() enter: pipe_num=%d\n", pipe_num);

	pipe = find_pipe_by_num(pipe_num);
	continuous = pipe && pipe->stream->config.continuous;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"sh_css_continuous_is_enabled() leave: enable=%d\n",
		continuous);
	return continuous;
}

enum ia_css_err
ia_css_stream_get_max_buffer_depth(struct ia_css_stream *stream, int *buffer_depth)
{
	if (buffer_depth == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_get_max_buffer_depth() enter: void\n");
	(void)stream;
	*buffer_depth = NUM_CONTINUOUS_FRAMES;
	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_stream_set_buffer_depth(struct ia_css_stream *stream, int buffer_depth)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_set_buffer_depth() enter: num_frames=%d\n",buffer_depth);
	(void)stream;
	if (buffer_depth > NUM_CONTINUOUS_FRAMES || buffer_depth < 1)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	/* ok, value allowed */
	stream->config.target_num_cont_raw_buf = buffer_depth;
	// TODO: check what to regarding initialization
	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_stream_get_buffer_depth(struct ia_css_stream *stream, int *buffer_depth)
{
	if (buffer_depth == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_get_buffer_depth() enter: void\n");
	(void)stream;
	*buffer_depth = stream->config.target_num_cont_raw_buf;
	return IA_CSS_SUCCESS;
}

#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)
unsigned int
sh_css_get_mipi_sizes_for_check(const unsigned int port, const unsigned int idx)
{
	OP___assert(port < N_CSI_PORTS);
	OP___assert(idx  < IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_get_mipi_sizes_for_check(port %d, idx %d): %d\n",
		port, idx, my_css.mipi_sizes_for_check[port][idx]);
	return my_css.mipi_sizes_for_check[port][idx];
}
#endif

static enum ia_css_err sh_css_pipe_configure_output(
	struct ia_css_pipe *pipe,
	unsigned int width,
	unsigned int height,
	enum ia_css_frame_format format)

{
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_configure_output() enter:\n");

	err = ia_css_util_check_res(width, height);
	if (err != IA_CSS_SUCCESS)
		return err;
	if (pipe->output_info.res.width != width ||
	    pipe->output_info.res.height != height ||
	    pipe->output_info.format != format) {
		ia_css_frame_info_init(&pipe->output_info, width, height, format, pipe->mode==IA_CSS_PIPE_ID_VIDEO ? SH_CSS_VIDEO_BUFFER_ALIGNMENT : 0);
	}
	return IA_CSS_SUCCESS;
}

static enum ia_css_err
sh_css_pipe_get_grid_info(struct ia_css_pipe *pipe,
			  struct ia_css_grid_info *info)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_binary *s3a_binary = NULL;

	assert(pipe != NULL);
	assert(info != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_get_grid_info() enter:\n");

	s3a_binary = ia_css_pipe_get_3a_binary(pipe);
	if (s3a_binary)
		ia_css_binary_grid_info(s3a_binary, info);
	else
		memset(info, 0, sizeof(*info));

	return err;
}

/*
 * @GC: TEMPORARY CODE TO TEST DVS AGAINST THE REFERENCE
 * PLEASE DO NOT REMOVE IT!
 */
#if DVS_REF_TESTING
static enum ia_css_err alloc_frame_from_file(
	struct ia_css_pipe *pipe,
	int width,
	int height)
{
	FILE *fp;
	int len = 0, err;
	int bytes_per_pixel;
	const char *file = "../File_input/dvs_input2.yuv";
	char *y_buf, *u_buf, *v_buf;
	char *uv_buf;
	int offset = 0;
	int h, w;

	hrt_vaddress out_base_addr;
	hrt_vaddress out_y_addr;
	hrt_vaddress out_uv_addr;

	assert(pipe != NULL);

	out_base_addr = pipe->pipe_settings.video.ref_frames[0]->data;
	out_y_addr  = out_base_addr
		+ pipe->pipe_settings.video.ref_frames[0]->planes.yuv.y.offset;
	out_uv_addr = out_base_addr
		+ pipe->pipe_settings.video.ref_frames[0]->planes.yuv.u.offset;


	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "alloc_frame_from_file() enter:\n");


	bytes_per_pixel = sizeof(char);

	if (!file) {printf("Error: Input file for dvs is not specified\n"); return 1;}
	fp = fopen(file, "rb");
	if (!fp) {printf("Error: Input file for dvs is not found\n"); return 1;}

	err = fseek(fp, 0, SEEK_END);
	if (err) {
		fclose(fp);
		printf("Error: Fseek error\n");
		return 1;
	}
	len = ftell(fp);

	err = fseek(fp, 0, SEEK_SET);
	if (err) {
		fclose(fp);
		printf("Error: Fseek error2\n");
		return 1;
	}

	len = 2 * len / 3;
	if (len != width * height * bytes_per_pixel) {
		fclose(fp);
		printf("Error: File size mismatches with the internal resolution\n");
		return 1;
	}

	y_buf = (char *) malloc(len);
	u_buf = (char *) malloc(len/4);
	v_buf = (char *) malloc(len/4);
	uv_buf= (char *) malloc(len/2);

	fread(y_buf, 1, len, fp);
	fread(u_buf, 1, len/4, fp);
	fread(v_buf, 1, len/4, fp);

	for (h=0; h<height/2; h++) {
		for (w=0; w<width/2; w++) {
			*(uv_buf + offset + w) = *(u_buf++);
			*(uv_buf + offset + w + width/2) = *(v_buf++);
			//printf("width: %d\n", width);
			//printf("offset_u: %d\n", offset+w);
			//printf("offset_v: %d\n", offset+w+width/2);
		}
		offset += width;
	}

	mmgr_store(out_y_addr, y_buf, len);
	mmgr_store(out_uv_addr, uv_buf, len/2);

	out_base_addr = pipe->pipe_settings.video.ref_frames[1]->data;
	out_y_addr  = out_base_addr + pipe->pipe_settings.video.ref_frames[1]->planes.yuv.y.offset;
	out_uv_addr = out_base_addr + pipe->pipe_settings.video.ref_frames[1]->planes.yuv.u.offset;
	mmgr_store(out_y_addr, y_buf, len);
	mmgr_store(out_uv_addr, uv_buf, len/2);

	fclose(fp);

	return IA_CSS_SUCCESS;
}

/* MW: Why do we not pass the pointer to the struct ? */
static enum ia_css_err fill_ref_frame_for_dvs(
	struct ia_css_pipe *pipe,
	struct ia_css_frame_info ref_info)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "fill_ref_frame_for_dvs() enter:\n");
	/* Allocate tmp_frame which is used to store YUV420 input.
	 * Read YUV420 input from the file to tmp_frame.
	 * Convert from YUV420 to NV12 format */
	err = alloc_frame_from_file(pipe, ref_info.res.width, ref_info.res.height);

	return err;
}
#endif

#define SH_CSS_TNR_BIT_DEPTH 8
#if defined(IS_ISP_2500_SYSTEM)
#define SH_CSS_REF_BIT_DEPTH 12   /* Skylake: reference frame is in 12 bit YUV domain */
#else
#define SH_CSS_REF_BIT_DEPTH 8
#endif

static enum ia_css_err load_video_binaries(struct ia_css_pipe *pipe)
{
	struct ia_css_frame_info video_in_info, ref_info, tnr_info,
				 *video_vf_info, video_bds_out_info;
	bool online;
	enum ia_css_err err = IA_CSS_SUCCESS;
#if !defined(IS_ISP_2500_SYSTEM)
	bool continuous = pipe->stream->config.continuous;
#endif
	int i;
	unsigned num_output_pins;
	bool resolution_differs = false;

	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "load_video_binaries() enter:\n");
	/* we only test the video_binary because offline video doesn't need a
	 * vf_pp binary and online does not (always use) the copy_binary.
	 * All are always reset at the same time anyway.
	 */
	if (pipe->pipe_settings.video.video_binary.info)
		return IA_CSS_SUCCESS;

	online = pipe->stream->config.online;
	err = ia_css_util_check_input(&pipe->stream->config, !online);
	if (err != IA_CSS_SUCCESS)
		return err;
	/* cannot have online video and input_mode memory */
	if (online && pipe->stream->config.mode == IA_CSS_INPUT_MODE_MEMORY)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	if (pipe->enable_viewfinder) {
		err = ia_css_util_check_vf_out_info(&pipe->output_info,
					&pipe->vf_output_info);
		if (err != IA_CSS_SUCCESS)
			return err;
	} else {
		err = ia_css_frame_check_info(&pipe->output_info);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	/* Video */
	if (pipe->enable_viewfinder){
		video_vf_info = &pipe->vf_output_info;
		resolution_differs = (video_vf_info->res.width != pipe->output_info.res.width) ||
				(video_vf_info->res.height != pipe->output_info.res.height);
	}
	else {
		video_vf_info = NULL;
	}

	{
		struct ia_css_binary_descr video_descr;

		err = ia_css_pipe_get_video_binarydesc(pipe,
			&video_descr, &video_in_info, &video_bds_out_info, video_vf_info,
			pipe->stream->config.left_padding);
		if (err != IA_CSS_SUCCESS)
			return err;

		err = ia_css_binary_find(&video_descr,
					 &pipe->pipe_settings.video.video_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	num_output_pins = pipe->pipe_settings.video.video_binary.info->num_output_pins;
	/* This is where we set the flag for invalid first frame */
	if (video_vf_info || (pipe->dvs_frame_delay == IA_CSS_FRAME_DELAY_2) )
		pipe->num_invalid_frames = 2;
	else
		pipe->num_invalid_frames = 1;

#if !defined(IS_ISP_2500_SYSTEM)    // skycam doesn't need the copy stage
	/* Copy */
	if (!online && !continuous) {
		/* TODO: what exactly needs doing, prepend the copy binary to
		 *	 video base this only on !online?
		 */
		err = load_copy_binary(pipe,
				       &pipe->pipe_settings.video.copy_binary,
				       &pipe->pipe_settings.video.video_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}
#endif

	/* Viewfinder post-processing */
	if (pipe->enable_viewfinder &&  // only when viewfinder is enabled.
	   ((num_output_pins == 1)        // when the binary has a single output pin, we need vf_pp
      || ((num_output_pins == 2) && resolution_differs)) ) { // when the binary has dual output pin, we only need vf_pp in case the resolution is different.
		struct ia_css_binary_descr vf_pp_descr;

		ia_css_pipe_get_vfpp_binarydesc(pipe, &vf_pp_descr,
			&pipe->pipe_settings.video.video_binary.vf_frame_info,
			&pipe->vf_output_info);
		err = ia_css_binary_find(&vf_pp_descr,
				&pipe->pipe_settings.video.vf_pp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	/* yuv copy does not use reference frames */
	if (ia_css_util_is_input_format_yuv(pipe->stream->config.format)) {
		return err;
	}

	ref_info = pipe->pipe_settings.video.video_binary.internal_frame_info;
#if defined(IS_ISP_2500_SYSTEM)
	ref_info.format = IA_CSS_FRAME_FORMAT_YUV420_16;   /* Skylake: reference frame is in 12 bit YUV domain */
#else
	ref_info.format = IA_CSS_FRAME_FORMAT_YUV420;
#endif
	ref_info.raw_bit_depth = SH_CSS_REF_BIT_DEPTH;

	/*Allocate the exact number of required reference buffers */
	  for (i = 0; i <= (int)pipe->dvs_frame_delay ; i++){
		if (pipe->pipe_settings.video.ref_frames[i]) {
			ia_css_frame_free(pipe->pipe_settings.video.ref_frames[i]);
			pipe->pipe_settings.video.ref_frames[i] = NULL;
		}
		err = ia_css_frame_allocate_from_info(
				&pipe->pipe_settings.video.ref_frames[i],
				&ref_info);
#ifdef HRT_CSIM
		ia_css_frame_zero(pipe->pipe_settings.video.ref_frames[i]);
#endif
		if (err != IA_CSS_SUCCESS)
			return err;
	}


#if DVS_REF_TESTING
	/* @GC: TEMPORARY CODE TO TEST DVS AGAINST THE REFERENCE
	 * To test dvs-6axis:
	 * 1. Enable this function call
	 * 2. Set "reqs.ref_out_requests" to "0" in lineloop.hive.c
	 */
	err = fill_ref_frame_for_dvs(pipe, ref_info);
	if (err != IA_CSS_SUCCESS)
		return err;
#endif


  if (pipe->pipe_settings.video.video_binary.info->sp.enable.block_output){
    tnr_info = pipe->pipe_settings.video.video_binary.out_frame_info;
  }
  else {
    tnr_info = pipe->pipe_settings.video.video_binary.internal_frame_info;
  }
	tnr_info.format = IA_CSS_FRAME_FORMAT_YUV_LINE;
	tnr_info.raw_bit_depth = SH_CSS_TNR_BIT_DEPTH;

	for (i = 0; i < NUM_TNR_FRAMES; i++) {
		if (pipe->pipe_settings.video.tnr_frames[i]) {
			ia_css_frame_free(pipe->pipe_settings.video.tnr_frames[i]);
			pipe->pipe_settings.video.tnr_frames[i] = NULL;
		}
		err = ia_css_frame_allocate_from_info(
				&pipe->pipe_settings.video.tnr_frames[i],
				&tnr_info);
#ifdef HRT_CSIM
		ia_css_frame_zero(pipe->pipe_settings.video.tnr_frames[i]);
#endif
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	return IA_CSS_SUCCESS;
}

static enum ia_css_err video_start(struct ia_css_pipe *pipe)
{
	struct ia_css_binary *copy_binary;
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_pipe *copy_pipe, *capture_pipe;
	enum sh_css_pipe_config_override copy_ovrd;
	enum ia_css_input_mode video_pipe_input_mode;

	assert(pipe != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "video_start() enter:\n");

	video_pipe_input_mode = pipe->stream->config.mode;

	copy_pipe    = pipe->pipe_settings.video.copy_pipe;
	capture_pipe = pipe->pipe_settings.video.capture_pipe;

	copy_binary  = &pipe->pipe_settings.video.copy_binary;

	sh_css_metrics_start_frame();

	/* multi stream video needs mipi buffers */
	send_mipi_frames(pipe);
	send_raw_frames(pipe);
	{
		unsigned int thread_id;

		ia_css_pipeline_get_sp_thread_id(ia_css_pipe_get_pipe_num(pipe), &thread_id);
		copy_ovrd = 1 << thread_id;

		if (pipe->stream->cont_capt) {
			ia_css_pipeline_get_sp_thread_id(ia_css_pipe_get_pipe_num(capture_pipe), &thread_id);
			copy_ovrd |= 1 << thread_id;
		}
	}

#if !defined(HAS_NO_INPUT_SYSTEM)
	err = sh_css_config_input_network(pipe, copy_binary);
	if (err != IA_CSS_SUCCESS)
		return err;
#endif

	/* Construct and load the copy pipe */
	if (pipe->stream->config.continuous) {
		sh_css_sp_init_pipeline(&copy_pipe->pipeline,
			IA_CSS_PIPE_ID_COPY,
			(uint8_t)ia_css_pipe_get_pipe_num(copy_pipe),
			false,
			pipe->stream->config.pixels_per_clock == 2, false,
			false, pipe->required_bds_factor,
			copy_ovrd,
			pipe->stream->config.mode, NULL
#if !defined(HAS_NO_INPUT_SYSTEM)
			, pipe->stream->config.source.port.port
#endif
			);

		/* make the video pipe start with mem mode input, copy handles
		   the actual mode */
		video_pipe_input_mode = IA_CSS_INPUT_MODE_MEMORY;
	}

	/* Construct and load the capture pipe */
	if (pipe->stream->cont_capt) {
		sh_css_sp_init_pipeline(&capture_pipe->pipeline,
			IA_CSS_PIPE_ID_CAPTURE,
			(uint8_t)ia_css_pipe_get_pipe_num(capture_pipe),
			pipe->config.default_capture_config.enable_xnr,
			capture_pipe->stream->config.pixels_per_clock == 2,
			true, /* continuous */
			false, /* offline */
			capture_pipe->required_bds_factor,
			0,
			IA_CSS_INPUT_MODE_MEMORY, NULL
#if !defined(HAS_NO_INPUT_SYSTEM)
			, (mipi_port_ID_t)0
#endif
			);
	}

	err = start_pipe(pipe, copy_ovrd, video_pipe_input_mode);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"video_start() leave: return (%d)\n", err);
	return err;
}

static
enum ia_css_err sh_css_pipe_get_viewfinder_frame_info(
	struct ia_css_pipe *pipe,
	struct ia_css_frame_info *info)
{
	assert(pipe != NULL);
	assert(info != NULL);

/* We could print the pointer as input arg, and the values as output */
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_get_viewfinder_frame_info() enter: void\n");

	if ( pipe->mode == IA_CSS_PIPE_ID_CAPTURE &&
	    (pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_RAW ||
	     pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_BAYER))
		return IA_CSS_ERR_MODE_HAS_NO_VIEWFINDER;
	/* offline video does not generate viewfinder output */
	if ( pipe->mode == IA_CSS_PIPE_ID_VIDEO &&
	    !pipe->stream->config.online && !pipe->stream->config.continuous) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			"sh_css_pipe_get_viewfinder_frame_info() leave: return_err=%d\n",
			IA_CSS_ERR_MODE_HAS_NO_VIEWFINDER);
		return IA_CSS_ERR_MODE_HAS_NO_VIEWFINDER;
	} else {
		*info = pipe->vf_output_info;
	}

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"sh_css_pipe_get_viewfinder_frame_info() leave: \
		info.res.width=%d, info.res.height=%d, \
		info.padded_width=%d, info.format=%d, \
		info.raw_bit_depth=%d, info.raw_bayer_order=%d\n",
		info->res.width,info->res.height,
		info->padded_width,info->format,
		info->raw_bit_depth,info->raw_bayer_order);

	return IA_CSS_SUCCESS;
}

static enum ia_css_err
sh_css_pipe_configure_viewfinder(struct ia_css_pipe *pipe,
				 unsigned int width,
				 unsigned int height,
				 enum ia_css_frame_format format)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"sh_css_pipe_configure_viewfinder() enter: \
		width=%d, height=%d format=%d\n",
		width, height, format);

	err = ia_css_util_check_res(width, height);
	if (err != IA_CSS_SUCCESS) {
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_configure_viewfinder() leave: return_err=%d\n",err);
		return err;
	}
	if (pipe->vf_output_info.res.width != width ||
	    pipe->vf_output_info.res.height != height ||
	    pipe->vf_output_info.format != format) {
		ia_css_frame_info_init(&pipe->vf_output_info,
				       width, height, format, pipe->mode==IA_CSS_PIPE_ID_VIDEO ? SH_CSS_VIDEO_BUFFER_ALIGNMENT : 0);
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_configure_viewfinder() leave: return_err=%d\n",IA_CSS_SUCCESS);
	return IA_CSS_SUCCESS;
}

static enum ia_css_err load_copy_binaries(struct ia_css_pipe *pipe)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "load_copy_binaries() enter:\n");

	if (pipe->pipe_settings.capture.copy_binary.info)
		return IA_CSS_SUCCESS;

	err = ia_css_frame_check_info(&pipe->output_info);
	if (err != IA_CSS_SUCCESS)
		return err;

	err = verify_copy_out_frame_format(pipe);
	if (err != IA_CSS_SUCCESS)
		return err;

	err = load_copy_binary(pipe,
				&pipe->pipe_settings.capture.copy_binary,
				NULL);
	if (err != IA_CSS_SUCCESS)
		return err;

	return err;
}

static bool need_capture_pp(
	const struct ia_css_pipe *pipe)
{
	assert(pipe != NULL);
	assert(pipe->mode == IA_CSS_PIPE_ID_CAPTURE);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "need_capture_pp() enter:\n");
	/* determine whether we need to use the capture_pp binary.
	 * This is needed for:
	 *   1. XNR or
	 *   2. Digital Zoom or
	 *   3. YUV downscaling
	 */
	if (pipe->out_yuv_ds_input_info.res.width &&
	    ((pipe->out_yuv_ds_input_info.res.width != pipe->output_info.res.width) ||
	     (pipe->out_yuv_ds_input_info.res.height != pipe->output_info.res.height)))
		return true;
	if (pipe->config.default_capture_config.enable_xnr)
		return true;

	if ((pipe->stream->isp_params_configs->dz_config.dx < HRT_GDC_N) ||
	    (pipe->stream->isp_params_configs->dz_config.dy < HRT_GDC_N) ||
	    pipe->config.enable_dz)
		return true;

	return false;
}

static enum ia_css_err load_primary_binaries(
	struct ia_css_pipe *pipe)
{
	bool online = false;
	bool memory = false;
	bool continuous = false;
	bool need_pp = false;
	bool need_isp_copy_binary = false;
#ifdef USE_INPUT_SYSTEM_VERSION_2401
	bool sensor = false;
#endif
	struct ia_css_frame_info prim_in_info,
				 prim_out_info, vf_info,
				 *vf_pp_in_info;
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_capture_settings *mycs;

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	online = pipe->stream->config.online;
	memory = pipe->stream->config.mode == IA_CSS_INPUT_MODE_MEMORY;
	continuous = pipe->stream->config.continuous;
#ifdef USE_INPUT_SYSTEM_VERSION_2401
	sensor = pipe->stream->config.mode == IA_CSS_INPUT_MODE_SENSOR;
#endif

	mycs = &pipe->pipe_settings.capture;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "load_primary_binaries() enter:\n");

	if (mycs->primary_binary.info)
		return IA_CSS_SUCCESS;

	err = ia_css_util_check_vf_out_info(&pipe->output_info,
			&pipe->vf_output_info);
	if (err != IA_CSS_SUCCESS)
		return err;
	need_pp = need_capture_pp(pipe);

	/* we use the vf output info to get the primary/capture_pp binary
	   configured for vf_veceven. It will select the closest downscaling
	   factor. */
	vf_info = pipe->vf_output_info;
	ia_css_frame_info_set_format(&vf_info, IA_CSS_FRAME_FORMAT_YUV_LINE);

	/* we build up the pipeline starting at the end */
	/* Capture post-processing */
	if (need_pp) {
		struct ia_css_binary_descr capture_pp_descr;
		ia_css_pipe_get_capturepp_binarydesc(pipe,
			&capture_pp_descr, &prim_out_info, &vf_info);
		err = ia_css_binary_find(&capture_pp_descr,
					&mycs->capture_pp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	} else {
		prim_out_info = pipe->output_info;
	}

	/* Primary */
	{
		struct ia_css_binary_descr prim_descr;

		ia_css_pipe_get_primary_binarydesc(pipe,
			&prim_descr, &prim_in_info, &prim_out_info, &vf_info);
		err = ia_css_binary_find(&prim_descr, &mycs->primary_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	/* Viewfinder post-processing */
	if (need_pp) {
		vf_pp_in_info =
		    &mycs->capture_pp_binary.vf_frame_info;
	} else {
		vf_pp_in_info =
		    &mycs->primary_binary.vf_frame_info;
	}

	{
		struct ia_css_binary_descr vf_pp_descr;

		ia_css_pipe_get_vfpp_binarydesc(pipe,
			&vf_pp_descr, vf_pp_in_info, &pipe->vf_output_info);
		err = ia_css_binary_find(&vf_pp_descr, &mycs->vf_pp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

#ifdef USE_INPUT_SYSTEM_VERSION_2401
	/* When the input system is 2401, only the Direct Sensor Mode
	 * Offline Capture uses the ISP copy binary.
	 */
	need_isp_copy_binary = !online && sensor;
#else
	need_isp_copy_binary = !online && !continuous && !memory;
#endif
	/* ISP Copy */
	if (need_isp_copy_binary) {
		err = load_copy_binary(pipe,
				       &mycs->copy_binary,
				       &mycs->primary_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	return IA_CSS_SUCCESS;
}

static enum ia_css_err load_advanced_binaries(
	struct ia_css_pipe *pipe)
{
	struct ia_css_frame_info pre_in_info, gdc_in_info,
				 post_in_info, post_out_info,
				 vf_info, *vf_pp_in_info;
	bool need_pp;
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "load_advanced_binaries() enter:\n");

	if (pipe->pipe_settings.capture.pre_isp_binary.info)
		return IA_CSS_SUCCESS;

	vf_info = pipe->vf_output_info;
	err = ia_css_util_check_vf_out_info(&pipe->output_info, &vf_info);
	if (err != IA_CSS_SUCCESS)
		return err;
	need_pp = need_capture_pp(pipe);

	ia_css_frame_info_set_format(&vf_info,
				     IA_CSS_FRAME_FORMAT_YUV_LINE);

	/* we build up the pipeline starting at the end */
	/* Capture post-processing */
	if (need_pp) {
		struct ia_css_binary_descr capture_pp_descr;

		ia_css_pipe_get_capturepp_binarydesc(pipe,
			&capture_pp_descr, &post_out_info, &vf_info);
		err = ia_css_binary_find(&capture_pp_descr,
				&pipe->pipe_settings.capture.capture_pp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	} else {
		post_out_info = pipe->output_info;
	}

	/* Post-gdc */
	{
		struct ia_css_binary_descr post_gdc_descr;

		ia_css_pipe_get_post_gdc_binarydesc(pipe,
			&post_gdc_descr, &post_in_info, &post_out_info, &vf_info);
		err = ia_css_binary_find(&post_gdc_descr,
					 &pipe->pipe_settings.capture.post_isp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	/* Gdc */
	{
		struct ia_css_binary_descr gdc_descr;

		ia_css_pipe_get_gdc_binarydesc(pipe, &gdc_descr, &gdc_in_info,
			       &pipe->pipe_settings.capture.post_isp_binary.in_frame_info);
		err = ia_css_binary_find(&gdc_descr,
					 &pipe->pipe_settings.capture.anr_gdc_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}
	pipe->pipe_settings.capture.anr_gdc_binary.left_padding =
		pipe->pipe_settings.capture.post_isp_binary.left_padding;

	/* Pre-gdc */
	{
		struct ia_css_binary_descr pre_gdc_descr;

		ia_css_pipe_get_pre_gdc_binarydesc(pipe, &pre_gdc_descr, &pre_in_info,
				   &pipe->pipe_settings.capture.anr_gdc_binary.in_frame_info);
		err = ia_css_binary_find(&pre_gdc_descr,
					 &pipe->pipe_settings.capture.pre_isp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}
	pipe->pipe_settings.capture.pre_isp_binary.left_padding =
		pipe->pipe_settings.capture.anr_gdc_binary.left_padding;

	/* Viewfinder post-processing */
	if (need_pp) {
		vf_pp_in_info =
		    &pipe->pipe_settings.capture.capture_pp_binary.vf_frame_info;
	} else {
		vf_pp_in_info =
		    &pipe->pipe_settings.capture.post_isp_binary.vf_frame_info;
	}

	{
		struct ia_css_binary_descr vf_pp_descr;

		ia_css_pipe_get_vfpp_binarydesc(pipe,
			&vf_pp_descr, vf_pp_in_info, &pipe->vf_output_info);
		err = ia_css_binary_find(&vf_pp_descr,
					 &pipe->pipe_settings.capture.vf_pp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	/* Copy */
	err = load_copy_binary(pipe,
			       &pipe->pipe_settings.capture.copy_binary,
			       &pipe->pipe_settings.capture.pre_isp_binary);
	if (err != IA_CSS_SUCCESS)
		return err;

	return IA_CSS_SUCCESS;
}

static enum ia_css_err load_bayer_isp_binaries(
	struct ia_css_pipe *pipe)
{
	struct ia_css_frame_info pre_isp_in_info;
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_binary_descr pre_de_descr;

	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "load_pre_isp_binaries() enter:\n");

	if (pipe->pipe_settings.capture.pre_isp_binary.info)
		return IA_CSS_SUCCESS;

	err = ia_css_frame_check_info(&pipe->output_info);
	if (err != IA_CSS_SUCCESS)
		return err;

	ia_css_pipe_get_pre_de_binarydesc(pipe, &pre_de_descr,
				&pre_isp_in_info,
				&pipe->output_info);

	err = ia_css_binary_find(&pre_de_descr,
				 &pipe->pipe_settings.capture.pre_isp_binary);

	return err;
}

static enum ia_css_err load_low_light_binaries(
	struct ia_css_pipe *pipe)
{
	struct ia_css_frame_info pre_in_info, anr_in_info,
				 post_in_info, post_out_info,
				 vf_info, *vf_pp_in_info;
	bool need_pp;
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "load_low_light_binaries() enter:\n");

	if (pipe->pipe_settings.capture.pre_isp_binary.info)
		return IA_CSS_SUCCESS;

	vf_info = pipe->vf_output_info;
	err = ia_css_util_check_vf_out_info(&pipe->output_info,
				&vf_info);
	if (err != IA_CSS_SUCCESS)
		return err;
	need_pp = need_capture_pp(pipe);

	ia_css_frame_info_set_format(&vf_info,
				     IA_CSS_FRAME_FORMAT_YUV_LINE);

	/* we build up the pipeline starting at the end */
	/* Capture post-processing */
	if (need_pp) {
		struct ia_css_binary_descr capture_pp_descr;

		ia_css_pipe_get_capturepp_binarydesc(pipe,
			&capture_pp_descr, &post_out_info, &vf_info);
		err = ia_css_binary_find(&capture_pp_descr,
				&pipe->pipe_settings.capture.capture_pp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	} else {
		post_out_info = pipe->output_info;
	}

	/* Post-anr */
	{
		struct ia_css_binary_descr post_anr_descr;

		ia_css_pipe_get_post_anr_binarydesc(pipe,
			&post_anr_descr, &post_in_info, &post_out_info, &vf_info);
		err = ia_css_binary_find(&post_anr_descr,
					 &pipe->pipe_settings.capture.post_isp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	/* Anr */
	{
		struct ia_css_binary_descr anr_descr;

		ia_css_pipe_get_anr_binarydesc(pipe, &anr_descr, &anr_in_info,
				&pipe->pipe_settings.capture.post_isp_binary.in_frame_info);
		err = ia_css_binary_find(&anr_descr,
					 &pipe->pipe_settings.capture.anr_gdc_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}
	pipe->pipe_settings.capture.anr_gdc_binary.left_padding =
		pipe->pipe_settings.capture.post_isp_binary.left_padding;

	/* Pre-anr */
	{
		struct ia_css_binary_descr pre_anr_descr;

		ia_css_pipe_get_pre_anr_binarydesc(pipe, &pre_anr_descr, &pre_in_info,
				   &pipe->pipe_settings.capture.anr_gdc_binary.in_frame_info);
		err = ia_css_binary_find(&pre_anr_descr,
				&pipe->pipe_settings.capture.pre_isp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}
	pipe->pipe_settings.capture.pre_isp_binary.left_padding =
		pipe->pipe_settings.capture.anr_gdc_binary.left_padding;

	/* Viewfinder post-processing */
	if (need_pp) {
		vf_pp_in_info =
		    &pipe->pipe_settings.capture.capture_pp_binary.vf_frame_info;
	} else {
		vf_pp_in_info =
		    &pipe->pipe_settings.capture.post_isp_binary.vf_frame_info;
	}

	{
		struct ia_css_binary_descr vf_pp_descr;

		ia_css_pipe_get_vfpp_binarydesc(pipe,
			&vf_pp_descr, vf_pp_in_info, &pipe->vf_output_info);
		err = ia_css_binary_find(&vf_pp_descr,
					 &pipe->pipe_settings.capture.vf_pp_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	/* Copy */
	err = load_copy_binary(pipe,
			       &pipe->pipe_settings.capture.copy_binary,
			       &pipe->pipe_settings.capture.pre_isp_binary);
	if (err != IA_CSS_SUCCESS)
		return err;

	return IA_CSS_SUCCESS;
}

static bool copy_on_sp(struct ia_css_pipe *pipe)
{
	bool rval;

	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "copy_on_sp() enter:\n");

	rval = true;

	rval &=	(pipe->mode == IA_CSS_PIPE_ID_CAPTURE);

	rval &= (pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_RAW);

	rval &= ((pipe->stream->config.format == IA_CSS_STREAM_FORMAT_BINARY_8) ||
		(pipe->config.mode == IA_CSS_PIPE_MODE_COPY));

	return rval;
}

static enum ia_css_err load_capture_binaries(
	struct ia_css_pipe *pipe)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	bool must_be_raw;

	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "load_capture_binaries() enter:\n");

	if (pipe->pipe_settings.preview.preview_binary.info &&
	    pipe->pipe_settings.preview.vf_pp_binary.info)
		return IA_CSS_SUCCESS;

	/* in primary, advanced,low light or bayer,
						the input format must be raw */
	must_be_raw =
		pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_ADVANCED ||
		pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_BAYER ||
		pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_LOW_LIGHT;
	err = ia_css_util_check_input(&pipe->stream->config, must_be_raw);
	if (err != IA_CSS_SUCCESS)
		return err;
	if (copy_on_sp(pipe) &&
	    pipe->stream->config.format == IA_CSS_STREAM_FORMAT_BINARY_8) {
		ia_css_frame_info_init(
			&pipe->output_info,
			JPEG_BYTES, 1, IA_CSS_FRAME_FORMAT_BINARY_8, 0);
		return IA_CSS_SUCCESS;
	}

	switch (pipe->config.default_capture_config.mode) {
	case IA_CSS_CAPTURE_MODE_RAW:
		err = load_copy_binaries(pipe);
		break;
	case IA_CSS_CAPTURE_MODE_BAYER:
		err = load_bayer_isp_binaries(pipe);
		break;
	case IA_CSS_CAPTURE_MODE_PRIMARY:
		err = load_primary_binaries(pipe);
		break;
	case IA_CSS_CAPTURE_MODE_ADVANCED:
		err = load_advanced_binaries(pipe);
		break;
	case IA_CSS_CAPTURE_MODE_LOW_LIGHT:
		err = load_low_light_binaries(pipe);
		break;
	}
	if (err != IA_CSS_SUCCESS)
		return err;

	return err;
}

static enum ia_css_err
sh_css_pipe_load_binaries(struct ia_css_pipe *pipe)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_load_binaries() enter:\n");

	/* PIPE_MODE_COPY has no binaries, but has output frames to outside*/
	if (pipe->config.mode == IA_CSS_PIPE_MODE_COPY)
		return err;

	switch (pipe->mode) {
	case IA_CSS_PIPE_ID_PREVIEW:
		err = load_preview_binaries(pipe);
		break;
	case IA_CSS_PIPE_ID_VIDEO:
		err = load_video_binaries(pipe);
		break;
	case IA_CSS_PIPE_ID_CAPTURE:
		err = load_capture_binaries(pipe);
		break;
	default:
		err = IA_CSS_ERR_INTERNAL_ERROR;
		break;
	}
	return err;
}

static enum ia_css_err
create_host_copy_pipeline(struct ia_css_pipe *pipe,
    unsigned max_input_width,
    struct ia_css_frame *out_frame)
{
	struct ia_css_pipeline *me;
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_pipeline_stage_desc stage_desc;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_copy_pipeline() enter:\n");

	/* pipeline already created as part of create_host_pipeline_structure */
	me = &pipe->pipeline;
	ia_css_pipeline_clean(me);

	/* Construct out_frame info */
	out_frame->contiguous = false;
	out_frame->flash_state = IA_CSS_FRAME_FLASH_STATE_NONE;

	if (copy_on_sp(pipe) &&
	    pipe->stream->config.format == IA_CSS_STREAM_FORMAT_BINARY_8) {
		ia_css_frame_info_init(&out_frame->info, JPEG_BYTES, 1,
				IA_CSS_FRAME_FORMAT_BINARY_8, 0);
	} else if (out_frame->info.format == IA_CSS_FRAME_FORMAT_RAW) {
		out_frame->info.raw_bit_depth =
			ia_css_pipe_util_pipe_input_format_bpp(pipe);
	}

	me->num_stages = 1;
	me->pipe_id = IA_CSS_PIPE_ID_COPY;
	pipe->mode  = IA_CSS_PIPE_ID_COPY;

	ia_css_pipe_get_sp_func_stage_desc(&stage_desc, out_frame,
		IA_CSS_PIPELINE_RAW_COPY, max_input_width);
	err = ia_css_pipeline_create_and_add_stage(me,
		&stage_desc,
		NULL);

	ia_css_pipeline_finalize_stages(&pipe->pipeline);

	configure_pipe_inout_port(&pipe->pipeline, true);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_copy_pipeline() leave:\n");

	return err;
}

static enum ia_css_err
create_host_isyscopy_capture_pipeline(struct ia_css_pipe *pipe)
{
	struct ia_css_pipeline *me = &pipe->pipeline;
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_pipeline_stage_desc stage_desc;
	struct ia_css_frame *out_frame = &me->out_frame;
	struct ia_css_pipeline_stage *out_stage = NULL;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_isyscopy_capture_pipeline() enter:\n");
	ia_css_pipeline_clean(me);

	/* Construct out_frame info */
	err = sh_css_pipe_get_output_frame_info(pipe, &out_frame->info);
	if (err != IA_CSS_SUCCESS)
		return err;
	out_frame->contiguous = false;
	out_frame->flash_state = IA_CSS_FRAME_FLASH_STATE_NONE;
	out_frame->dynamic_data_index = sh_css_frame_out;

	me->num_stages = 1;
	me->pipe_id = IA_CSS_PIPE_ID_CAPTURE;
	pipe->mode  = IA_CSS_PIPE_ID_CAPTURE;
	ia_css_pipe_get_sp_func_stage_desc(&stage_desc, out_frame,
		IA_CSS_PIPELINE_ISYS_COPY, 0);
	err = ia_css_pipeline_create_and_add_stage(me,
		&stage_desc, &out_stage);
	if(err != IA_CSS_SUCCESS)
		return err;

	ia_css_pipeline_finalize_stages(me);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_isyscopy_capture_pipeline() leave:\n");

	return err;
}

static enum ia_css_err
create_host_regular_capture_pipeline(struct ia_css_pipe *pipe)
{
	struct ia_css_pipeline *me;
	enum ia_css_err err = IA_CSS_SUCCESS;
	enum ia_css_capture_mode mode;
	struct ia_css_pipeline_stage *out_stage = NULL,
				     *vf_pp_stage = NULL,
				     *copy_stage = NULL,
				     *in_stage = NULL,
				     *post_stage = NULL;
	struct ia_css_frame *cc_frame = NULL;
	struct ia_css_binary *copy_binary,
			     *primary_binary,
			     *vf_pp_binary,
			     *pre_isp_binary,
			     *anr_gdc_binary,
			     *post_isp_binary,
			     *capture_pp_binary,
			     *sc_binary = NULL;
	bool need_pp = false;
	bool raw;

	struct ia_css_frame *in_frame;
	struct ia_css_frame *out_frame;
	struct ia_css_frame *vf_frame;
	struct ia_css_pipeline_stage_desc stage_desc;
	bool need_in_frameinfo_memory = false;
#ifdef USE_INPUT_SYSTEM_VERSION_2401
	bool sensor = false;
	bool buffered_sensor = false;
	bool online = false;
	bool continuous = false;
#endif

	assert(pipe != NULL);
	assert(pipe->stream != NULL);

	me = &pipe->pipeline;
	mode = pipe->config.default_capture_config.mode;
	raw = (mode == IA_CSS_CAPTURE_MODE_RAW);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		 "create_host_regular_capture_pipeline() enter:\n");
	ia_css_pipeline_clean(me);

#ifdef USE_INPUT_SYSTEM_VERSION_2401
	/* When the input system is 2401, always enable 'in_frameinfo_memory'
	 * except for the following:
	 * - Direct Sensor Mode Online Capture
	 * - Direct Sensor Mode Continuous Capture
	 * - Buffered Sensor Mode Continous Capture
	 */
	sensor = pipe->stream->config.mode == IA_CSS_INPUT_MODE_SENSOR;
	buffered_sensor = pipe->stream->config.mode == IA_CSS_INPUT_MODE_BUFFERED_SENSOR;
	online = pipe->stream->config.online;
	continuous = pipe->stream->config.continuous;
	need_in_frameinfo_memory =
		!((sensor && (online || continuous)) || (buffered_sensor && continuous));
#else
	/* Construct in_frame info (only in case we have dynamic input */
	need_in_frameinfo_memory = pipe->stream->config.mode == IA_CSS_INPUT_MODE_MEMORY;
#endif
	if (need_in_frameinfo_memory) {
		err = init_in_frameinfo_memory_defaults(pipe, &me->in_frame);
		if (err != IA_CSS_SUCCESS)
			return err;

		in_frame = &me->in_frame;
	} else {
		in_frame = NULL;
	}

	err = init_out_frameinfo_defaults(pipe, &me->out_frame);
	if (err != IA_CSS_SUCCESS)
		return err;
	out_frame = &me->out_frame;

	/* Construct vf_frame info (only in case we have VF) */
	if (mode == IA_CSS_CAPTURE_MODE_RAW ||
			mode == IA_CSS_CAPTURE_MODE_BAYER) {
		/* These modes don't support viewfinder output */
		vf_frame = NULL;
	} else {
		init_vf_frameinfo_defaults(pipe, &me->vf_frame);
		vf_frame = &me->vf_frame;
	}

	copy_stage = NULL;
	in_stage = NULL;

	copy_binary       = &pipe->pipe_settings.capture.copy_binary;
	primary_binary    = &pipe->pipe_settings.capture.primary_binary;
	vf_pp_binary      = &pipe->pipe_settings.capture.vf_pp_binary;
	pre_isp_binary    = &pipe->pipe_settings.capture.pre_isp_binary;
	anr_gdc_binary    = &pipe->pipe_settings.capture.anr_gdc_binary;
	post_isp_binary   = &pipe->pipe_settings.capture.post_isp_binary;
	capture_pp_binary = &pipe->pipe_settings.capture.capture_pp_binary;
	need_pp = (need_capture_pp(pipe) || pipe->output_stage) &&
		  mode != IA_CSS_CAPTURE_MODE_RAW &&
		  mode != IA_CSS_CAPTURE_MODE_BAYER;

	if (pipe->pipe_settings.capture.copy_binary.info) {
		if(raw) {
			ia_css_pipe_get_generic_stage_desc(&stage_desc, copy_binary,
				out_frame, NULL, NULL, NULL);
		} else {
			ia_css_pipe_get_generic_stage_desc(&stage_desc, copy_binary,
				in_frame, NULL, NULL, NULL);
		}

		err = ia_css_pipeline_create_and_add_stage(me,
			&stage_desc,
			&post_stage);
		if (err != IA_CSS_SUCCESS)
			return err;
		in_stage = post_stage;
	} else if (pipe->stream->config.continuous) {
		in_frame = pipe->stream->last_pipe->continuous_frames[0];
	}

	if (mode == IA_CSS_CAPTURE_MODE_PRIMARY) {

		if(need_pp) {
			ia_css_pipe_get_generic_stage_desc(&stage_desc, primary_binary,
				NULL, in_frame, cc_frame, NULL);
		} else {
			ia_css_pipe_get_generic_stage_desc(&stage_desc, primary_binary,
				out_frame, in_frame, cc_frame, NULL);
		}

		err = ia_css_pipeline_create_and_add_stage(me,
			&stage_desc,
			&post_stage);
		if (err != IA_CSS_SUCCESS)
			return err;
		/* If we use copy iso primary,
		   the input must be yuv iso raw */
		post_stage->args.copy_vf =
			primary_binary->info->sp.mode ==
			IA_CSS_BINARY_MODE_COPY;
		post_stage->args.copy_output = post_stage->args.copy_vf;
		sc_binary = primary_binary;
	} else if (mode == IA_CSS_CAPTURE_MODE_ADVANCED ||
	           mode == IA_CSS_CAPTURE_MODE_LOW_LIGHT) {
		ia_css_pipe_get_generic_stage_desc(&stage_desc, pre_isp_binary,
			NULL, in_frame, cc_frame, NULL);
		err = ia_css_pipeline_create_and_add_stage(me,
				&stage_desc, NULL);
		if (err != IA_CSS_SUCCESS)
			return err;
		ia_css_pipe_get_generic_stage_desc(&stage_desc, anr_gdc_binary,
			NULL, NULL, NULL, NULL);
		err = ia_css_pipeline_create_and_add_stage(me,
				&stage_desc, NULL);
		if (err != IA_CSS_SUCCESS)
			return err;

		if(need_pp) {
			ia_css_pipe_get_generic_stage_desc(&stage_desc, post_isp_binary,
				NULL, NULL, NULL, NULL);
		} else {
			ia_css_pipe_get_generic_stage_desc(&stage_desc, post_isp_binary,
				out_frame, NULL, NULL, NULL);
		}

		err = ia_css_pipeline_create_and_add_stage(me,
				&stage_desc, &post_stage);
		if (err != IA_CSS_SUCCESS)
			return err;
		sc_binary = pre_isp_binary;
	} else if (mode == IA_CSS_CAPTURE_MODE_BAYER) {
		ia_css_pipe_get_generic_stage_desc(&stage_desc, pre_isp_binary,
			out_frame, in_frame, cc_frame, NULL);
		err = ia_css_pipeline_create_and_add_stage(me,
			&stage_desc,
			NULL);
		if (err != IA_CSS_SUCCESS)
			return err;
		sc_binary = pre_isp_binary;
	}
	if (!in_stage)
		in_stage = post_stage;

	if (need_pp) {
		err = add_capture_pp_stage(pipe, me, out_frame,
					   capture_pp_binary,
					   post_stage, &post_stage);
		if (err != IA_CSS_SUCCESS)
			return err;
	}
	if (mode != IA_CSS_CAPTURE_MODE_RAW &&
		mode != IA_CSS_CAPTURE_MODE_BAYER) {
		err = add_vf_pp_stage(pipe, vf_frame, vf_pp_binary,
				      post_stage, &vf_pp_stage);
		if (err != IA_CSS_SUCCESS)
			return err;
	}

	ia_css_pipeline_finalize_stages(&pipe->pipeline);

	/**
	 * Maybe we can return earlier but this was the original position
	 * in the original version of capture_start()
	 */
	if (pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_RAW ||
	    pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_BAYER) {
		if (copy_on_sp(pipe))
			return IA_CSS_SUCCESS;
	}

	if (mode == IA_CSS_CAPTURE_MODE_RAW) {
		err = ia_css_pipeline_get_stage(me, copy_binary->info->sp.mode,
						&out_stage);
		if (err != IA_CSS_SUCCESS)
			return err;
		copy_stage = out_stage;
	} else if (mode == IA_CSS_CAPTURE_MODE_BAYER) {
		err = ia_css_pipeline_get_stage(me,
				pre_isp_binary->info->sp.mode,
				&out_stage);
		if (err != IA_CSS_SUCCESS)
			return err;
	} else {
		if (copy_binary->info) {
			err = ia_css_pipeline_get_stage(me,
							copy_binary->info->sp.mode,
							&copy_stage);
			if (err != IA_CSS_SUCCESS)
				return err;
		}
		if (capture_pp_binary->info) {
			err = ia_css_pipeline_get_stage(me,
					capture_pp_binary->info->sp.mode,
					&out_stage);
			if (err != IA_CSS_SUCCESS)
				return err;
		} else if (mode ==
			   IA_CSS_CAPTURE_MODE_PRIMARY) {
			err = ia_css_pipeline_get_stage(me,
					primary_binary->info->sp.mode, &out_stage);
			if (err != IA_CSS_SUCCESS)
				return err;
		} else if (mode ==
			   IA_CSS_CAPTURE_MODE_LOW_LIGHT) {
			err = ia_css_pipeline_get_stage(me,
					post_isp_binary->info->sp.mode,
					&out_stage);
			if (err != IA_CSS_SUCCESS)
				return err;
		} else {
			err = ia_css_pipeline_get_stage(me,
					post_isp_binary->info->sp.mode,
					&out_stage);
			if (err != IA_CSS_SUCCESS)
				return err;
		}
	}
	if (mode != IA_CSS_CAPTURE_MODE_RAW &&
	    mode != IA_CSS_CAPTURE_MODE_BAYER &&
		vf_pp_stage)
		vf_pp_stage->args.out_frame = vf_frame;

	/* rvanimme: why is this? */
	/* TODO: investigate if this can be removed */
	if (!pipe->output_stage)
		out_stage->args.out_frame = out_frame;

	if (copy_stage && in_frame)
		copy_stage->args.out_frame = in_frame;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_regular_capture_pipeline() leave:\n");

	return IA_CSS_SUCCESS;
}

static enum ia_css_err
create_host_capture_pipeline(struct ia_css_pipe *pipe)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_capture_pipeline() enter:\n");

	if (pipe->config.mode == IA_CSS_PIPE_MODE_COPY)
		err = create_host_isyscopy_capture_pipeline(pipe);
	else
		err = create_host_regular_capture_pipeline(pipe);
	if (err != IA_CSS_SUCCESS)
		return err;

	/* pipeline already created as part of create_host_pipeline_structure */
	configure_pipe_inout_port(&pipe->pipeline,
		pipe->stream->config.continuous);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"create_host_capture_pipeline() leave:\n");

	return err;
}

static enum ia_css_err capture_start(
	struct ia_css_pipe *pipe)
{
	struct ia_css_pipeline *me;

	enum ia_css_err err = IA_CSS_SUCCESS;
	enum sh_css_pipe_config_override copy_ovrd;

	assert(pipe != NULL);

	me = &pipe->pipeline;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "capture_start() enter:\n");

#if !defined(HAS_NO_INPUT_SYSTEM)
	if (pipe->stream->config.mode != IA_CSS_INPUT_MODE_MEMORY) {
		err = sh_css_config_input_network(pipe,
			&pipe->pipe_settings.capture.copy_binary);
		if (err != IA_CSS_SUCCESS)
			return err;
	}
#endif

	if ((pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_RAW   ||
	     pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_BAYER   ) &&
		(pipe->config.mode != IA_CSS_PIPE_MODE_COPY)) {
		if (copy_on_sp(pipe)) {
			return start_copy_on_sp(pipe, &me->out_frame);
		}
	}

	/* multi stream video needs mipi buffers */
	if (pipe->config.mode != IA_CSS_PIPE_MODE_COPY)
		send_mipi_frames(pipe);

	{
		unsigned int thread_id;

		ia_css_pipeline_get_sp_thread_id(ia_css_pipe_get_pipe_num(pipe), &thread_id);
		copy_ovrd = 1 << thread_id;

	}
	err = start_pipe(pipe, copy_ovrd, pipe->stream->config.mode);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"capture_start() leave: return (%d)\n", err);
	return err;

}

static enum ia_css_err
sh_css_pipe_get_output_frame_info(struct ia_css_pipe *pipe,
				  struct ia_css_frame_info *info)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(pipe != NULL);
	assert(info != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_get_output_frame_info() enter:\n");
	*info = pipe->output_info;
	if (copy_on_sp(pipe) &&
	    pipe->stream->config.format == IA_CSS_STREAM_FORMAT_BINARY_8) {
		ia_css_frame_info_init(info, JPEG_BYTES, 1,
				IA_CSS_FRAME_FORMAT_BINARY_8, 0);
	} else if (info->format == IA_CSS_FRAME_FORMAT_RAW ||
		   info->format == IA_CSS_FRAME_FORMAT_RAW_PACKED) {
		info->raw_bit_depth =
			ia_css_pipe_util_pipe_input_format_bpp(pipe);
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_get_output_frame_info() leave:\n");
	return err;
}

#if !defined(HAS_NO_INPUT_SYSTEM)
void
ia_css_stream_send_input_frame(const struct ia_css_stream *stream,
			       const unsigned short *data,
			       unsigned int width,
			       unsigned int height)
{
	assert(stream != NULL);

	ia_css_inputfifo_send_input_frame(
			data, width, height,
			stream->config.channel_id,
			stream->config.format,
			stream->config.pixels_per_clock == 2);
}

void
ia_css_stream_start_input_frame(const struct ia_css_stream *stream)
{
	assert(stream != NULL);

	ia_css_inputfifo_start_frame(
			stream->config.channel_id,
			stream->config.format,
			stream->config.pixels_per_clock == 2);
}

void
ia_css_stream_send_input_line(const struct ia_css_stream *stream,
			      const unsigned short *data,
			      unsigned int width,
			      const unsigned short *data2,
			      unsigned int width2)
{
	assert(stream != NULL);

	ia_css_inputfifo_send_line(stream->config.channel_id,
					       data, width, data2, width2);
}

void
ia_css_stream_send_input_embedded_line(const struct ia_css_stream *stream,
		enum ia_css_stream_format format,
		const unsigned short *data,
		unsigned int width)
{
	assert(stream != NULL);
	if (data == NULL || width == 0)
		return;
	ia_css_inputfifo_send_embedded_line(stream->config.channel_id,
			format, data, width);
}

void
ia_css_stream_end_input_frame(const struct ia_css_stream *stream)
{
	assert(stream != NULL);

	ia_css_inputfifo_end_frame(stream->config.channel_id);
}
#endif

enum ia_css_err
ia_css_mipi_frame_specify(const unsigned int size_mem_words,
				const bool contiguous)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	my_css.size_mem_words	= size_mem_words;
	my_css.contiguous		= contiguous;


	return err;
}

/* Assumptions:
 *	- A line is multiple of 4 bytes = 1 word.
 *	- Each frame has SOF and EOF (each 1 word).
 *	- Each line has format header and optionally SOL and EOL (each 1 word).
 *	- Odd and even lines of YUV420 format are different in bites per pixel size.
 *	- Custom size of embedded data.
 *  -- Interleaved frames are not taken into account.
 *  -- Lines are multiples of 8B, and not necessary of (custom 3B, or 7B
 *  etc.).
 * Result is given in DDR mem words, 32B or 256 bits
 */
enum ia_css_err
ia_css_mipi_frame_calculate_size(const unsigned int width,
				const unsigned int height,
				const enum ia_css_stream_format format,
				const bool hasSOLandEOL,
				const unsigned int embedded_data_size_words,
				unsigned int *size_mem_words)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	unsigned int bits_per_pixel = 0;
	unsigned int even_line_bytes = 0;
	unsigned int odd_line_bytes = 0;
	unsigned int words_per_odd_line = 0;
	unsigned int words_for_first_line = 0;
	unsigned int words_per_even_line = 0;
	unsigned int mem_words_per_even_line = 0;
	unsigned int mem_words_per_odd_line = 0;
	unsigned int mem_words_for_first_line = 0;
	unsigned int mem_words_for_EOF = 0;
	unsigned int mem_words = 0;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_mipi_frame_calculate_size() "
		"enter: width=%d, height=%d, format=%d, hasSOLandEOL=%d, embedded_data_size_words=%d\n",
		width, height, format, hasSOLandEOL, embedded_data_size_words);

	switch (format) {
		case IA_CSS_STREAM_FORMAT_RAW_6:			/* 4p, 3B, 24bits */
			bits_per_pixel = 6;	break;
		case IA_CSS_STREAM_FORMAT_RAW_7:			/* 8p, 7B, 56bits */
			bits_per_pixel = 7;		break;
		case IA_CSS_STREAM_FORMAT_RAW_8:			/* 1p, 1B, 8bits */
		case IA_CSS_STREAM_FORMAT_BINARY_8:      /*  8bits, TODO: check. */
		case IA_CSS_STREAM_FORMAT_YUV420_8:		/* odd 2p, 2B, 16bits, even 2p, 4B, 32bits */
			bits_per_pixel = 8;		break;
		case IA_CSS_STREAM_FORMAT_YUV420_10:		/* odd 4p, 5B, 40bits, even 4p, 10B, 80bits */
		case IA_CSS_STREAM_FORMAT_RAW_10:		/* 4p, 5B, 40bits */
			bits_per_pixel = 10;	break;
		case IA_CSS_STREAM_FORMAT_YUV420_8_LEGACY:	/* 2p, 3B, 24bits */
		case IA_CSS_STREAM_FORMAT_RAW_12:			/* 2p, 3B, 24bits */
			bits_per_pixel = 12;	break;
		case IA_CSS_STREAM_FORMAT_RAW_14:		/* 4p, 7B, 56bits */
			bits_per_pixel = 14;	break;
		case IA_CSS_STREAM_FORMAT_RGB_444:		/* 1p, 2B, 16bits */
		case IA_CSS_STREAM_FORMAT_RGB_555:		/* 1p, 2B, 16bits */
		case IA_CSS_STREAM_FORMAT_RGB_565:		/* 1p, 2B, 16bits */
		case IA_CSS_STREAM_FORMAT_YUV422_8:		/* 2p, 4B, 32bits */
			bits_per_pixel = 16;	break;
		case IA_CSS_STREAM_FORMAT_RGB_666:		/* 4p, 9B, 72bits */
			bits_per_pixel = 18;	break;
		case IA_CSS_STREAM_FORMAT_YUV422_10:		/* 2p, 5B, 40bits */
			bits_per_pixel = 20;	break;
		case IA_CSS_STREAM_FORMAT_RGB_888:		/* 1p, 3B, 24bits */
			bits_per_pixel = 24;	break;

		case IA_CSS_STREAM_FORMAT_RAW_16:        /* TODO: not specified in MIPI SPEC, check */
		default:
			return IA_CSS_ERR_INVALID_ARGUMENTS;
	}

	odd_line_bytes = (width * bits_per_pixel + 7) >> 3; /* ceil ( bits per line / 8 ) */

	/* Even lines for YUV420 formats are double in bits_per_pixel. */
	if (format == IA_CSS_STREAM_FORMAT_YUV420_8
		|| format == IA_CSS_STREAM_FORMAT_YUV420_10) {
		even_line_bytes = (width * 2 * bits_per_pixel + 7) >> 3; /* ceil ( bits per line / 8 ) */
	} else {
		even_line_bytes = odd_line_bytes;
	}

   /*  a frame represented in memory:  ()- optional; data - payload words.
	*  addr		0		1		2		3		4		5		6		7:
	*  first	SOF		(SOL)	PACK_H	data	data	data	data	data
	*	data	data	data	data	data	data	data	data
	*           ...
	*			data	data	0		0		0		0		0		0
	*  second   (EOL)	(SOL)	PACK_H	data	data	data	data	data
	*	data	data	data	data	data	data	data	data
	*           ...
	*			data	data	0		0		0		0		0		0
	*  ...
	*  last		(EOL)	EOF		0		0		0		0		0		0
	*
	*  Embedded lines are regular lines stored before the first and after
	*  payload lines.
	*/


	words_per_odd_line	 = ((odd_line_bytes   + 3) >> 2 );		/* ceil(odd_line_bytes/4); word = 4 bytes */
	words_per_even_line  = ((even_line_bytes  + 3) >> 2 );
    words_for_first_line = words_per_odd_line + 2 + (hasSOLandEOL ? 1 : 0); /* + SOF +packet header + optionally (SOL), but (EOL) is not in the first line */
	words_per_odd_line	+= (1 + (hasSOLandEOL ? 2 : 0));  /* each non-first line has format header, and optionally (SOL) and (EOL). */
	words_per_even_line += (1 + (hasSOLandEOL ? 2 : 0));

	mem_words_per_odd_line	 = ((words_per_odd_line + 7) >> 3);	/* ceil(words_per_odd_line/8); mem_word = 32 bytes, 8 words */
	mem_words_for_first_line = ((words_for_first_line + 7) >> 3);
	mem_words_per_even_line  = ((words_per_even_line + 7) >> 3);
	mem_words_for_EOF        = 1; /* last line consisit of the optional (EOL) and EOF */

	mem_words = ((embedded_data_size_words + 7) >> 3) +
		mem_words_for_first_line +
				(((height + 1) >> 1) - 1) * mem_words_per_odd_line + /* ceil (height/2) - 1 (first line is calculated separatelly) */
				(  height      >> 1     ) * mem_words_per_even_line + /* floor(height/2) */
				mem_words_for_EOF;

	*size_mem_words = mem_words; /* ceil(words/8); mem word is 32B = 8words. */ //Check if this is still needed.

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_mipi_frame_calculate_size() leave: return_err=%d\n",err);
	return err;
}

#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)
enum ia_css_err
ia_css_mipi_frame_enable_check_on_size(const enum ia_css_csi2_port port,
				const unsigned int	size_mem_words)
{
	uint32_t idx;

	enum ia_css_err err = IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;

	OP___assert(port < N_CSI_PORTS);
	OP___assert(size_mem_words != 0);


	for (idx = 0; idx < IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT &&
		my_css.mipi_sizes_for_check[port][idx] != 0;
		idx++){}
	if (idx < IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT) {
		my_css.mipi_sizes_for_check[port][idx] = size_mem_words;
		err = IA_CSS_SUCCESS;
	}

	return err;
}
#endif

static void
append_firmware(struct ia_css_fw_info **l, struct ia_css_fw_info *firmware)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "append_firmware() enter:\n");
	while (*l)
		l = &(*l)->next;
	*l = firmware;
	firmware->next = NULL;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "append_firmware() leave:\n");
}

static void
remove_firmware(struct ia_css_fw_info **l, struct ia_css_fw_info *firmware)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "remove_firmware() enter:\n");
	while (*l && *l != firmware)
		l = &(*l)->next;
	if (!*l)
		return;
	*l = firmware->next;
	firmware->next = NULL;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "remove_firmware() leave:\n");
}

#if !defined(C_RUN) && !defined(HRT_UNSCHED)
static enum ia_css_err
upload_isp_code(struct ia_css_fw_info *firmware)
{
	hrt_vaddress binary = firmware->info.isp.xmem_addr;
	if (!binary) {
		unsigned size = firmware->blob.size;
		const unsigned char *blob;
		const unsigned char *binary_name;
		binary_name =
			(const unsigned char *)(IA_CSS_EXT_ISP_PROG_NAME(
						firmware));
		blob = binary_name +
			strlen((const char *)binary_name) +
			1;
		binary = sh_css_load_blob(blob, size);
		firmware->info.isp.xmem_addr = binary;
	}

	if (!binary)
		return IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;
	return IA_CSS_SUCCESS;
}
#endif

static enum ia_css_err
acc_load_extension(struct ia_css_fw_info *firmware)
{
#if !defined(C_RUN) && !defined(HRT_UNSCHED)
	enum ia_css_err err = upload_isp_code(firmware);
	if (err != IA_CSS_SUCCESS)
		return err;
#endif

	firmware->loaded = true;
	return IA_CSS_SUCCESS;
}

static void
acc_unload_extension(struct ia_css_fw_info *firmware)
{
	if (firmware->info.isp.xmem_addr) {
		mmgr_free(firmware->info.isp.xmem_addr);
		firmware->info.isp.xmem_addr = mmgr_NULL;
	}
	firmware->isp_code = NULL;
	firmware->loaded = false;
}
/* Load firmware for extension */
static enum ia_css_err
ia_css_pipe_load_extension(struct ia_css_pipe *pipe,
			   struct ia_css_fw_info *firmware)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	assert(firmware != NULL);
	assert(pipe != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_load_extension() enter:\n");
	if (firmware->info.isp.type == IA_CSS_ACC_OUTPUT)
		append_firmware(&pipe->output_stage, firmware);
	else if (firmware->info.isp.type == IA_CSS_ACC_VIEWFINDER) {
		append_firmware(&pipe->vf_stage, firmware);
	}
	err = acc_load_extension(firmware);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_load_extension() leave:\n");
	return err;
}

/* Unload firmware for extension */
static void
ia_css_pipe_unload_extension(struct ia_css_pipe *pipe,
			     struct ia_css_fw_info *firmware)
{
	assert(firmware != NULL);
	assert(pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_unload_extension() enter:\n");
	if (firmware->info.isp.type == IA_CSS_ACC_OUTPUT)
		remove_firmware(&pipe->output_stage, firmware);
	else if (firmware->info.isp.type == IA_CSS_ACC_VIEWFINDER)
		remove_firmware(&pipe->vf_stage, firmware);
	acc_unload_extension(firmware);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "sh_css_pipe_unload_extension() leave:\n");
}

bool
ia_css_pipeline_uses_params(struct ia_css_pipeline *me)
{
	struct ia_css_pipeline_stage *stage;

	assert(me != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipeline_uses_params() enter: me=%p\n", me);

	for (stage = me->stages; stage; stage = stage->next)
		if (stage->binary_info && stage->binary_info->enable.params) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
				"ia_css_pipeline_uses_params() leave: "
				"return_bool=true\n");
			return true;
		}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipeline_uses_params() leave: return_bool=false\n");
	return false;
}

static enum ia_css_err
sh_css_pipeline_add_acc_stage(struct ia_css_pipeline *pipeline,
			      const void *acc_fw)
{
	struct ia_css_fw_info *fw = (struct ia_css_fw_info *)acc_fw;
	enum ia_css_err	err = acc_load_extension(fw);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"sh_css_pipeline_add_acc_stage() enter: pipeline=%p,"
		" acc_fw=%p\n", pipeline, acc_fw);

	if (err == IA_CSS_SUCCESS) {
		struct ia_css_pipeline_stage_desc stage_desc;
		ia_css_pipe_get_acc_stage_desc(&stage_desc, NULL, fw);
		err = ia_css_pipeline_create_and_add_stage(pipeline,
			&stage_desc,
			NULL);
	}

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"sh_css_pipeline_add_acc_stage() leave: return_err=%d\n",err);
	return err;
}

/**
 * @brief Query the internal frame ID.
 * Refer to "sh_css_internal.h" for details.
 */
bool sh_css_query_internal_queue_id(
	enum ia_css_buffer_type key,
	enum sh_css_buffer_queue_id *val)
{
	assert(key < IA_CSS_BUFFER_TYPE_NUM);
	assert(val != NULL);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"sh_css_query_internal_queue_id() enter: key=%d\n", key);
	*val = sh_css_buf_type_2_internal_queue_id[key];
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"sh_css_query_internal_queue_id() leave: return_val=%d\n",
		*val);
	return true;
}

/**
 * @brief Tag a specific frame in continuous capture.
 * Refer to "sh_css_internal.h" for details.
 */
enum ia_css_err ia_css_stream_capture_frame(struct ia_css_stream *stream,
				unsigned int exp_id)
{
	struct sh_css_tag_descr tag_descr;
	unsigned int encoded_tag_descr;
	enum ia_css_err err;
	ia_css_queue_t* q;

	(void)stream;
	assert(stream != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_stream_capture_frame() enter: exp_id=%d\n",
		exp_id);

	if (exp_id == 0) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"ia_css_stream_capture_frame() "
			"leave: return_err=%d\n",
			IA_CSS_ERR_INVALID_ARGUMENTS);
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}

	/* Create the tag descriptor from the parameters */
	sh_css_create_tag_descr(0, 0, 0, exp_id, &tag_descr);


	/* Encode the tag descriptor into a 32-bit value */
	encoded_tag_descr = sh_css_encode_tag_descr(&tag_descr);

	q = sh_css_get_queue(sh_css_host2sp_buffer_queue,
		sh_css_tag_cmd_queue, 0);
	if ( NULL == q ) {
		/* Error as the queue is not initialized */
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"ia_css_stream_capture_frame() "
			"leave: return_err=%d\n",
			IA_CSS_ERR_RESOURCE_NOT_AVAILABLE);
		return IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;
	}

	/* Enqueue the encoded tag to the host2sp queue.
	 * Note: The pipe and stage IDs for tag_cmd queue are hard-coded to 0
	 * on both host and the SP side.
	 * It is mainly because it is enough to have only one tag_cmd queue */
	err = ia_css_queue_enqueue(q,
		(uint32_t)encoded_tag_descr);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_stream_capture_frame() leave: return_err=%d\n",
		err);
	return err;
}

/**
 * @brief Configure the continuous capture.
 * Refer to "sh_css_internal.h" for details.
 */
enum ia_css_err ia_css_stream_capture(
	struct ia_css_stream *stream,
	int num_captures,
	unsigned int skip,
	int offset)
{
	struct sh_css_tag_descr tag_descr;
	unsigned int encoded_tag_descr;
	enum ia_css_err err;
	ia_css_queue_t* q;

	if (stream == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_stream_capture() enter: num_captures=%d,"
		" skip=%d, offset=%d\n", num_captures, skip,offset);

	/* Check if the tag descriptor is valid */
	if (num_captures < SH_CSS_MINIMUM_TAG_ID) {
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_stream_capture() leave: return_err=%d\n",
		IA_CSS_ERR_INVALID_ARGUMENTS);
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}

	/* Create the tag descriptor from the parameters */
	sh_css_create_tag_descr(num_captures, skip, offset, 0, &tag_descr);


	/* Encode the tag descriptor into a 32-bit value */
	encoded_tag_descr = sh_css_encode_tag_descr(&tag_descr);

	q = sh_css_get_queue(sh_css_host2sp_buffer_queue,
		sh_css_tag_cmd_queue, 0);
	if ( NULL == q ) {
		/* Error as the queue is not initialized */
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"ia_css_stream_capture() leave: return_err=%d\n",
			IA_CSS_ERR_RESOURCE_NOT_AVAILABLE);
		return IA_CSS_ERR_RESOURCE_NOT_AVAILABLE;
	}

	/* Enqueue the encoded tag to the host2sp queue.
	 * Note: The pipe and stage IDs for tag_cmd queue are hard-coded to 0
	 * on both host and the SP side.
	 * It is mainly because it is enough to have only one tag_cmd queue */
	err = ia_css_queue_enqueue(q,
		(uint32_t)encoded_tag_descr);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_stream_capture() leave: return_err=%d\n",
		err);
	return err;
}

void ia_css_stream_request_flash(struct ia_css_stream *stream)
{
	(void)stream;

	assert(stream != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_request_flash() enter: void\n");

	sh_css_write_host2sp_command(host2sp_cmd_start_flash);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_stream_request_flash() leave: return_void\n");
}

static void
sh_css_init_host_sp_control_vars(void)
{
	const struct ia_css_fw_info *fw;
	unsigned int HIVE_ADDR_ia_css_ispctrl_sp_isp_started;

	unsigned int HIVE_ADDR_host_sp_queues_initialized;
	unsigned int HIVE_ADDR_sp_sleep_mode;
	unsigned int HIVE_ADDR_ia_css_dmaproxy_sp_invalidate_tlb;
	unsigned int HIVE_ADDR_sp_stop_copy_preview;
	unsigned int HIVE_ADDR_host_sp_com;
	unsigned int o = offsetof(struct host_sp_communication, host2sp_command)
				/ sizeof(int);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"sh_css_init_host_sp_control_vars() enter: void\n");

	fw = &sh_css_sp_fw;
	HIVE_ADDR_ia_css_ispctrl_sp_isp_started = fw->info.sp.isp_started;

	HIVE_ADDR_host_sp_queues_initialized =
		fw->info.sp.host_sp_queues_initialized;
	HIVE_ADDR_sp_sleep_mode = fw->info.sp.sleep_mode;
	HIVE_ADDR_ia_css_dmaproxy_sp_invalidate_tlb = fw->info.sp.invalidate_tlb;
	HIVE_ADDR_sp_stop_copy_preview = fw->info.sp.stop_copy_preview;
	HIVE_ADDR_host_sp_com = fw->info.sp.host_sp_com;

	(void)HIVE_ADDR_ia_css_ispctrl_sp_isp_started; /* Suppres warnings in CRUN */

	(void)HIVE_ADDR_sp_sleep_mode;
	(void)HIVE_ADDR_ia_css_dmaproxy_sp_invalidate_tlb;
	(void)HIVE_ADDR_sp_stop_copy_preview;
	(void)HIVE_ADDR_host_sp_com;

	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(ia_css_ispctrl_sp_isp_started),
		(uint32_t)(0));

	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(host_sp_queues_initialized),
		(uint32_t)(0));
	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_sleep_mode),
		(uint32_t)(0));
	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(ia_css_dmaproxy_sp_invalidate_tlb),
		(uint32_t)(false));
	sp_dmem_store_uint32(SP0_ID,
		(unsigned int)sp_address_of(sp_stop_copy_preview),
		my_css.stop_copy_preview?(uint32_t)(1):(uint32_t)(0));
	store_sp_array_uint(host_sp_com, o, host2sp_cmd_ready);
	sh_css_update_host2sp_cont_num_mipi_frames
			(my_css.num_mipi_frames);

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"sh_css_init_host_sp_control_vars() leave: return_void\n");
}

void
ia_css_get_properties(struct ia_css_properties *properties)
{
	assert(properties != NULL);
#if defined(HAS_GDC_VERSION_2) || defined(HAS_GDC_VERSION_3)
/*
 * MW: We don't want to store the coordinates
 * full range in memory: Truncate
 */
	properties->gdc_coord_one = gdc_get_unity(GDC0_ID)/HRT_GDC_COORD_SCALE;
#else
#error "Unknown GDC version"
#endif

	properties->l1_base_is_index = true;

#if defined(HAS_VAMEM_VERSION_1)
	properties->vamem_type = IA_CSS_VAMEM_TYPE_1;
#elif defined(HAS_VAMEM_VERSION_2)
	properties->vamem_type = IA_CSS_VAMEM_TYPE_2;
#else
#error "Unknown VAMEM version"
#endif
}

/**
 * create the internal structures and fill in the configuration data
 */
void ia_css_pipe_config_defaults(struct ia_css_pipe_config *pipe_config)
{
	struct ia_css_pipe_config def_config = {
		IA_CSS_PIPE_MODE_PREVIEW, /* mode */
		1,      /* isp_pipe_version */
		{0, 0}, /* bayer_ds_out_res */
		{0, 0}, /* capt_pp_in_res */
		{0, 0}, /* vf_pp_in_res */
		{0, 0}, /* dvs_crop_out_res */
		{{0, 0}, 0, 0, 0, 0}, /* output_info */
		{{0, 0}, 0, 0, 0, 0}, /* vf_output_info */
		NULL,   /* acc_extension */
		NULL,   /* acc_stages */
		0,      /* num_acc_stages */
		{
			IA_CSS_CAPTURE_MODE_RAW, /* mode */
			false, /* enable_xnr */
			false  /* enable_raw_output */
		},      /* default_capture_config */
		{0, 0}, /* dvs_envelope */
		1,      /* dvs_frame_delay */
		-1,     /* acc_num_execs */
		true,   /* enable_dz */
	};
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_pipe_config_defaults()\n");
	*pipe_config = def_config;
}

void
ia_css_pipe_extra_config_defaults(struct ia_css_pipe_extra_config *extra_config)
{
	assert(extra_config != NULL);

	extra_config->enable_raw_binning = false;
	extra_config->enable_yuv_ds = false;
	extra_config->enable_high_speed = false;
	extra_config->enable_dvs_6axis = false;
	extra_config->enable_reduced_pipe = false;
	extra_config->disable_vf_pp = false;
	extra_config->enable_fractional_ds = false;
}

void ia_css_stream_config_defaults(struct ia_css_stream_config *stream_config)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_config_defaults()\n");
	assert(stream_config != NULL);
	memset(stream_config, 0, sizeof(*stream_config));
	stream_config->online = true;
	stream_config->left_padding = -1;
}

static enum ia_css_err
ia_css_acc_pipe_create(struct ia_css_pipe *pipe)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	struct ia_css_pipeline *pipeline;

	assert(pipe != NULL);
	pipeline = &pipe->pipeline;

	/* There is not meaning for num_execs = 0 semantically. Run atleast once. */
	if (pipe->config.acc_num_execs == 0)
		pipe->config.acc_num_execs = 1;

	pipeline->num_execs = pipe->config.acc_num_execs;
	return err;
}

enum ia_css_err
ia_css_pipe_create(const struct ia_css_pipe_config *config,
		   struct ia_css_pipe **pipe)
{
	if (config == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	if (pipe == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	return ia_css_pipe_create_extra(config, NULL, pipe);
}

enum ia_css_err
ia_css_pipe_create_extra(const struct ia_css_pipe_config *config,
			 const struct ia_css_pipe_extra_config *extra_config,
			 struct ia_css_pipe **pipe)
{
	enum ia_css_err err = IA_CSS_ERR_INTERNAL_ERROR;
	struct ia_css_pipe *internal_pipe = NULL;

	(void)extra_config;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_pipe_create()\n");
	if (pipe == NULL)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	err = create_pipe(config->mode, &internal_pipe, false);
	if (internal_pipe == NULL)
		return IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;

	/* now we have a pipe structure to fill */
	internal_pipe->config = *config;
	if (extra_config)
		internal_pipe->extra_config = *extra_config;
	else
		ia_css_pipe_extra_config_defaults(&internal_pipe->extra_config);

	if (config->mode == IA_CSS_PIPE_MODE_ACC) {
		/* Temporary hack to migrate acceleration to CSS 2.0.
		 * In the future the code for all pipe types should be
		 * unified. */
		*pipe = internal_pipe;
		return ia_css_acc_pipe_create(internal_pipe);
	}

	/*Use config value when dvs_frame_delay setting equal to 2, otherwise always 1 by default */
	internal_pipe->dvs_frame_delay =
		(internal_pipe->config.dvs_frame_delay == IA_CSS_FRAME_DELAY_2) ?
		IA_CSS_FRAME_DELAY_2 : IA_CSS_FRAME_DELAY_1;

	/* we still keep enable_raw_binning for backward compatibility, for any new
	   fractional bayer downscaling, we should use bayer_ds_out_res. if both are
	   specified, bayer_ds_out_res will take precedence.if none is specified, we
	   set bayer_ds_out_res equal to IF output resolution(IF may do cropping on
	   sensor output) or use default decimation factor 1. */
	if (internal_pipe->extra_config.enable_raw_binning &&
		 internal_pipe->config.bayer_ds_out_res.width) {
		/* fill some code here, if no code is needed, please remove it during integration */
	}

	/* YUV downscaling */
	if ((internal_pipe->config.vf_pp_in_res.width ||
		 internal_pipe->config.capt_pp_in_res.width) &&
	     internal_pipe->config.mode == IA_CSS_PIPE_MODE_CAPTURE) {
		enum ia_css_frame_format format;
		if (internal_pipe->config.vf_pp_in_res.width) {
			format = IA_CSS_FRAME_FORMAT_YUV_LINE;
			ia_css_frame_info_init(
				&internal_pipe->vf_yuv_ds_input_info,
				internal_pipe->config.vf_pp_in_res.width,
				internal_pipe->config.vf_pp_in_res.height,
				format, 0);
		}
		if (internal_pipe->config.capt_pp_in_res.width) {
			format = IA_CSS_FRAME_FORMAT_YUV420;
			ia_css_frame_info_init(
				&internal_pipe->out_yuv_ds_input_info,
				internal_pipe->config.capt_pp_in_res.width,
				internal_pipe->config.capt_pp_in_res.height,
				format, 0);
		}
	}
	if (internal_pipe->config.vf_pp_in_res.width &&
	    internal_pipe->config.mode == IA_CSS_PIPE_MODE_PREVIEW) {
		ia_css_frame_info_init(
				&internal_pipe->vf_yuv_ds_input_info,
				internal_pipe->config.vf_pp_in_res.width,
				internal_pipe->config.vf_pp_in_res.height,
				IA_CSS_FRAME_FORMAT_YUV_LINE, 0);
	}
	/* handle bayer downscaling output info */
	if (internal_pipe->config.bayer_ds_out_res.width) {
			ia_css_frame_info_init(
				&internal_pipe->bds_output_info,
				internal_pipe->config.bayer_ds_out_res.width,
				internal_pipe->config.bayer_ds_out_res.height,
				IA_CSS_FRAME_FORMAT_RAW, 0);
	}
	/* handle output info, asume always needed */
	if (internal_pipe->config.output_info.res.width) {
		err = sh_css_pipe_configure_output(
				internal_pipe,
				internal_pipe->config.output_info.res.width,
				internal_pipe->config.output_info.res.height,
				internal_pipe->config.output_info.format);
		if (err != IA_CSS_SUCCESS) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_ERROR, "ia_css_pipe_create: "
							"invalid output info\n");
			sh_css_free(internal_pipe);
			internal_pipe = NULL;
			return err;
		}
	}
	/* handle vf output info, when configured */
	internal_pipe->enable_viewfinder = (internal_pipe->config.vf_output_info.res.width != 0);
	if (internal_pipe->config.vf_output_info.res.width) {
		err = sh_css_pipe_configure_viewfinder(
				internal_pipe,
				internal_pipe->config.vf_output_info.res.width,
				internal_pipe->config.vf_output_info.res.height,
				internal_pipe->config.vf_output_info.format);
		if (err != IA_CSS_SUCCESS) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_ERROR, "ia_css_pipe_create: "
							"invalid vf output info\n");
			sh_css_free(internal_pipe);
			internal_pipe = NULL;
			return err;
		}
	}
	if (internal_pipe->config.acc_extension) {
		ia_css_pipe_load_extension(internal_pipe,
			internal_pipe->config.acc_extension);
	}
	/* set all info to zeroes first */
	memset(&internal_pipe->info, 0, sizeof(internal_pipe->info));

	/* all went well, return the pipe */
	*pipe = internal_pipe;
	return IA_CSS_SUCCESS;
}


enum ia_css_err
ia_css_pipe_get_info(const struct ia_css_pipe *pipe,
		     struct ia_css_pipe_info *pipe_info)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_pipe_get_info()\n");
	assert(pipe_info != NULL);
	if (pipe_info == NULL) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_ERROR,
			"ia_css_pipe_get_info: pipe_info cannot be NULL\n");
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}
	if (pipe == NULL || pipe->stream == NULL) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_ERROR,
			"ia_css_pipe_get_info: ia_css_stream_create needs to"
			" be called before ia_css_[stream/pipe]_get_info\n");
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}
	/* we succeeded return the info */
	*pipe_info = pipe->info;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_pipe_get_info() leave\n");
	return IA_CSS_SUCCESS;
}

#if !defined(HAS_NO_INPUT_SYSTEM)
static enum ia_css_err
ia_css_stream_configure_rx(struct ia_css_stream *stream)
{
#if !defined(USE_INPUT_SYSTEM_VERSION_2401)
	struct ia_css_input_port *config;
#endif
	assert(stream != NULL);
#if defined(HAS_RX_VERSION_1)

	config = &stream->config.source.port;
	if (config->port == IA_CSS_CSI2_PORT1)
		stream->csi_rx_config.port = MIPI_PORT1_ID;
	else if (config->port == IA_CSS_CSI2_PORT0)
		stream->csi_rx_config.port = MIPI_PORT0_ID;
	else
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	stream->csi_rx_config.num_lanes  = config->num_lanes;
	stream->csi_rx_config.timeout    = config->timeout;
	stream->csi_rx_config.uncomp_bpp =
		config->compression.uncompressed_bits_per_pixel;
	stream->csi_rx_config.comp_bpp   =
		config->compression.compressed_bits_per_pixel;
	if (config->compression.type == IA_CSS_CSI2_COMPRESSION_TYPE_NONE)
		stream->csi_rx_config.comp = MIPI_PREDICTOR_NONE;
	else if (config->compression.type == IA_CSS_CSI2_COMPRESSION_TYPE_1)
		stream->csi_rx_config.comp = MIPI_PREDICTOR_TYPE1;
	else if (config->compression.type == IA_CSS_CSI2_COMPRESSION_TYPE_2)
		stream->csi_rx_config.comp = MIPI_PREDICTOR_TYPE2;
	else
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	stream->csi_rx_config.is_two_ppc = (stream->config.pixels_per_clock == 2);
	stream->reconfigure_css_rx = true;
#elif defined(HAS_RX_VERSION_2) && !defined(USE_INPUT_SYSTEM_VERSION_2401)

	config = &stream->config.source.port;
// AM: this code is not reliable, especially for 2400
	if (config->num_lanes == 1)
		stream->csi_rx_config.mode = MONO_1L_1L_0L;
	else if (config->num_lanes == 2)
		stream->csi_rx_config.mode = MONO_2L_1L_0L;
	else if (config->num_lanes == 3)
		stream->csi_rx_config.mode = MONO_3L_1L_0L;
	else if (config->num_lanes == 4)
		stream->csi_rx_config.mode = MONO_4L_1L_0L;
	else if (config->num_lanes != 0)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

	if (config->port == IA_CSS_CSI2_PORT1)
		stream->csi_rx_config.port = MIPI_PORT1_ID;
	else if (config->port == IA_CSS_CSI2_PORT2)
		stream->csi_rx_config.port = MIPI_PORT2_ID;
	else if (config->port == IA_CSS_CSI2_PORT0)
		stream->csi_rx_config.port = MIPI_PORT0_ID;
	else
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	stream->csi_rx_config.timeout    = config->timeout;
	stream->csi_rx_config.initcount  = 0;
	stream->csi_rx_config.synccount  = 0x28282828;
	stream->csi_rx_config.rxcount    = 0x04040404;
	if (config->compression.type == IA_CSS_CSI2_COMPRESSION_TYPE_NONE)
		stream->csi_rx_config.comp = MIPI_PREDICTOR_NONE;
	else {
		/* not implemented yet, requires extension of the rx_cfg_t
		 * struct */
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}
	stream->csi_rx_config.is_two_ppc = (stream->config.pixels_per_clock == 2);
	stream->reconfigure_css_rx = true;
#else
	/* No CSS receiver */
	(void)stream;
#endif
	return IA_CSS_SUCCESS;
}
#endif

static struct ia_css_pipe *
find_pipe(struct ia_css_pipe *pipes[],
		unsigned int num_pipes,
		enum ia_css_pipe_mode mode,
		bool copy_pipe)
{
	unsigned i;
	assert(pipes != NULL);
	for (i = 0; i < num_pipes; i++) {
		assert(pipes[i] != NULL);
		if (pipes[i]->config.mode != mode)
			continue;
		if (copy_pipe && pipes[i]->mode != IA_CSS_PIPE_ID_COPY)
			continue;
		return pipes[i];
	}
	return NULL;
}

static enum ia_css_err
ia_css_acc_stream_create(struct ia_css_stream *stream)
{
	int i;
	enum ia_css_err err = IA_CSS_SUCCESS;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"ia_css_acc_stream_create() enter:\n");

	assert(stream != NULL);

	for (i=0; i< stream->num_pipes; i++) {
		struct ia_css_pipe *pipe = stream->pipes[i];
		assert(pipe != NULL);
		pipe->stream = stream;
	}

	err = create_host_pipeline_structure(stream);
	if (err != IA_CSS_SUCCESS)
		return err;

	stream->started = false;

	/* Map SP threads before doing anything. */
	err = map_sp_threads(stream, true);
	if (err != IA_CSS_SUCCESS) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			"ia_css_stream_start(): map_sp_threads: return_err=%d\n", err);
		return err;
	}
	
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
		"ia_css_acc_stream_create() leave:\n");

	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_stream_create(const struct ia_css_stream_config *stream_config,
					 int num_pipes,
					 struct ia_css_pipe *pipes[],
					 struct ia_css_stream **stream)
{
	struct ia_css_pipe *curr_pipe;
	struct ia_css_stream *curr_stream = NULL;
	bool spcopyonly;
	bool sensor_binning_changed;
	int i;
	enum ia_css_err err = IA_CSS_ERR_INTERNAL_ERROR;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
		"ia_css_stream_create() enter, num_pipes=%d\n", num_pipes);
	/* some checks */
	if (num_pipes == 0 ||
		stream == NULL ||
		pipes == NULL) {
		err = IA_CSS_ERR_INVALID_ARGUMENTS;
		goto ERR;
	}

	/* We don't support metadata for JPEG stream, since they both use str2mem */
	if (stream_config->format == IA_CSS_STREAM_FORMAT_BINARY_8
			&& stream_config->metadata_config.size > 0)
		return IA_CSS_ERR_INVALID_ARGUMENTS;

#if !defined(HAS_NO_INPUT_SYSTEM)
	ia_css_debug_pipe_graph_dump_stream_config(stream_config);

	/* check if mipi size specified */
	if (stream_config->mode == IA_CSS_INPUT_MODE_BUFFERED_SENSOR) {
		if (my_css.size_mem_words == 0) {
			ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
				"ia_css_stream_create() exit, need to set mipi frame size\n");
			assert(my_css.size_mem_words != 0);
			err = IA_CSS_ERR_INTERNAL_ERROR;
			goto ERR;
		}
	}
#endif

	/* Currently we only supported metadata up to a certain size. */
	if (stream_config->metadata_config.size > SH_CSS_MAX_METADATA_BUFFER_SIZE) {
		err = IA_CSS_ERR_RESOURCE_EXHAUSTED;
		goto ERR;
	}

	/* allocate the stream instance */
	curr_stream = sh_css_malloc(sizeof(struct ia_css_stream));
	if (curr_stream == NULL) {
		err = IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;
		goto ERR;
	}
	/* default all to 0 */
	memset(curr_stream, 0, sizeof(struct ia_css_stream));

#if !defined(HAS_NO_INPUT_SYSTEM) && !defined(USE_INPUT_SYSTEM_VERSION_2401)
	/* default mipi config */
#if defined(HAS_RX_VERSION_2)
	curr_stream->csi_rx_config.mode = MONO_4L_1L_0L; /* The HW config */
	curr_stream->csi_rx_config.port = MIPI_PORT0_ID; /* The port ID to apply the control on */
	curr_stream->csi_rx_config.timeout = 0xffff4;
	curr_stream->csi_rx_config.initcount = 0;
	curr_stream->csi_rx_config.synccount = 0x28282828;
	curr_stream->csi_rx_config.rxcount = 0x04040404;
	curr_stream->csi_rx_config.comp = MIPI_PREDICTOR_NONE;	/* Just for backward compatibility */
	curr_stream->csi_rx_config.is_two_ppc = false;
	curr_stream->reconfigure_css_rx = true;
#else
	curr_stream->csi_rx_config = (rx_cfg_t)DEFAULT_MIPI_CONFIG;
#endif
#endif

	/* allocate pipes */
	curr_stream->num_pipes = num_pipes;
	curr_stream->pipes = sh_css_malloc(num_pipes * sizeof(struct ia_css_pipe *));
	if (curr_stream->pipes == NULL) {
		sh_css_free(curr_stream);
		err = IA_CSS_ERR_CANNOT_ALLOCATE_MEMORY;
		goto ERR;
	}
	/* store pipes */
	spcopyonly = (num_pipes == 1) && (pipes[0]->config.mode == IA_CSS_PIPE_MODE_COPY);
	for (i = 0; i < num_pipes; i++)
		curr_stream->pipes [i] = pipes[i];
	curr_stream->last_pipe = curr_stream->pipes[0];
	/* take over stream config */
	curr_stream->config = *stream_config;

	/* TO BE REMOVED when all drivers move to CSS API 2.1 */
	if (curr_stream->config.pixels_per_clock == 0)
		curr_stream->config.pixels_per_clock =
			curr_stream->config.two_pixels_per_clock ? 2 : 1;

	/* in case driver doesn't configure init number of raw buffers, configure it here */
	if (curr_stream->config.target_num_cont_raw_buf == 0)
		curr_stream->config.target_num_cont_raw_buf = NUM_CONTINUOUS_FRAMES;
	if (curr_stream->config.init_num_cont_raw_buf == 0)
		curr_stream->config.init_num_cont_raw_buf = curr_stream->config.target_num_cont_raw_buf;

	/* copy mode specific stuff */
	switch (curr_stream->config.mode) {
		case IA_CSS_INPUT_MODE_SENSOR:
		case IA_CSS_INPUT_MODE_BUFFERED_SENSOR:
#if !defined(HAS_NO_INPUT_SYSTEM)
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_create: mode port\n");
		/* CSI RX configuration */
		ia_css_stream_configure_rx(curr_stream);
#endif
		break;
	case IA_CSS_INPUT_MODE_TPG:
#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
			"ia_css_stream_create tpg_configuration: "
			"x_mask=%d, y_mask=%d, x_delta=%d, "
			"y_delta=%d, xy_mask=%d\n",
			curr_stream->config.source.tpg.x_mask,
			curr_stream->config.source.tpg.y_mask,
			curr_stream->config.source.tpg.x_delta,
			curr_stream->config.source.tpg.y_delta,
			curr_stream->config.source.tpg.xy_mask);

		sh_css_sp_configure_tpg(
			curr_stream->config.source.tpg.x_mask,
			curr_stream->config.source.tpg.y_mask,
			curr_stream->config.source.tpg.x_delta,
			curr_stream->config.source.tpg.y_delta,
			curr_stream->config.source.tpg.xy_mask);
#endif
		break;
	case IA_CSS_INPUT_MODE_PRBS:
#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_create: mode prbs\n");
		sh_css_sp_configure_prbs(curr_stream->config.source.prbs.seed);
#endif
		break;
	case IA_CSS_INPUT_MODE_MEMORY:
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_create: mode memory\n");
		curr_stream->reconfigure_css_rx = false;
		break;
	default:
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_create: mode sensor/default\n");
	}
	err = ia_css_stream_isp_parameters_init(curr_stream);
	if (err != IA_CSS_SUCCESS)
		goto ERR;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_create: isp_params_configs: %p\n",curr_stream->isp_params_configs);

	if (num_pipes == 1 && pipes[0]->config.mode == IA_CSS_PIPE_MODE_ACC) {
		*stream = curr_stream;
		err = ia_css_acc_stream_create(curr_stream);
		goto ERR;
	}
	/* sensor binning */
	if (!spcopyonly){
		sensor_binning_changed =
			sh_css_params_set_binning_factor(curr_stream, curr_stream->config.sensor_binning_factor);
	} else {
		sensor_binning_changed = false;
	}

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_create: sensor_binning=%d, changed=%d\n",
		curr_stream->config.sensor_binning_factor, sensor_binning_changed);
	/* loop over pipes */
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_create: num_pipes=%d\n",
		num_pipes);
	curr_stream->cont_capt = false;
	/* Temporary hack: we give the preview pipe a reference to the capture
	 * pipe in continuous capture mode. */
	if (curr_stream->config.continuous) {
		/* Search for the preview pipe and create the copy pipe */
		struct ia_css_pipe *preview_pipe;
		struct ia_css_pipe *video_pipe;
		struct ia_css_pipe *capture_pipe = NULL;
		struct ia_css_pipe *copy_pipe = NULL;

		if (num_pipes >= 2)
			curr_stream->cont_capt = true;

		/* Create copy pipe here, since it may not be exposed to the driver */
		preview_pipe = find_pipe(pipes, num_pipes,
						IA_CSS_PIPE_MODE_PREVIEW, false);
		video_pipe = find_pipe(pipes, num_pipes,
						IA_CSS_PIPE_MODE_VIDEO, false);
		if (curr_stream->cont_capt == true) {
			capture_pipe = find_pipe(pipes, num_pipes,
						IA_CSS_PIPE_MODE_CAPTURE, false);
			if (capture_pipe == NULL) {
				err = IA_CSS_ERR_INTERNAL_ERROR;
				goto ERR;
			}
		}
		/* We do not support preview and video pipe at the same time */
		if (preview_pipe && video_pipe) {
			err = IA_CSS_ERR_INVALID_ARGUMENTS;
			goto ERR;
		}

		if (preview_pipe && !preview_pipe->pipe_settings.preview.copy_pipe) {
			err = create_pipe(IA_CSS_PIPE_MODE_CAPTURE, &copy_pipe, true);
			if (err != IA_CSS_SUCCESS)
				goto ERR;
			ia_css_pipe_config_defaults(&copy_pipe->config);
			preview_pipe->pipe_settings.preview.copy_pipe = copy_pipe;
			copy_pipe->stream = curr_stream;
		}
		if (preview_pipe && (curr_stream->cont_capt == true)) {
			preview_pipe->pipe_settings.preview.capture_pipe = capture_pipe;
		}
		if (video_pipe && !video_pipe->pipe_settings.video.copy_pipe) {
			err = create_pipe(IA_CSS_PIPE_MODE_CAPTURE, &copy_pipe, true);
			if (err != IA_CSS_SUCCESS)
				goto ERR;
			ia_css_pipe_config_defaults(&copy_pipe->config);
			video_pipe->pipe_settings.video.copy_pipe = copy_pipe;
			copy_pipe->stream = curr_stream;
		}
		if (video_pipe && (curr_stream->cont_capt == true)) {
			video_pipe->pipe_settings.video.capture_pipe = capture_pipe;
		}
	}
	for (i = 0; i < num_pipes; i++) {
		curr_pipe = pipes[i];
		/* set current stream */
		curr_pipe->stream = curr_stream;
		/* take over effective info */
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_create: effective_res=%dx%d\n",
			curr_stream->config.effective_res.width,
			curr_stream->config.effective_res.height);
		err = ia_css_util_check_res(
			curr_stream->config.effective_res.width,
			curr_stream->config.effective_res.height);
		if (err != IA_CSS_SUCCESS)
			goto ERR;

		if (curr_stream->info.effective_info.width != curr_stream->config.effective_res.width ||
			curr_stream->info.effective_info.height != curr_stream->config.effective_res.height) {
			curr_stream->info.effective_info.width = curr_stream->config.effective_res.width;
			curr_stream->info.effective_info.height = curr_stream->config.effective_res.height;
		}
		/* sensor binning per pipe */
		if (sensor_binning_changed)
			sh_css_pipe_free_shading_table(curr_pipe);
	}

	/* now pipes have been configured, info should be available */
	for (i = 0; i < num_pipes; i++) {
		struct ia_css_pipe_info *pipe_info = NULL;
		curr_pipe = pipes[i];

		err = sh_css_pipe_load_binaries(curr_pipe);
		if (err != IA_CSS_SUCCESS)
			goto ERR;

		/* handle each pipe */
		pipe_info = &curr_pipe->info;
		err = sh_css_pipe_get_output_frame_info(curr_pipe,
					&pipe_info->output_info);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
		if (!spcopyonly){
			err = sh_css_pipe_get_grid_info(curr_pipe,
						&pipe_info->grid_info);
			if (err != IA_CSS_SUCCESS)
				goto ERR;
			sh_css_pipe_get_viewfinder_frame_info(curr_pipe,
						&pipe_info->vf_output_info);
			if (err != IA_CSS_SUCCESS)
				goto ERR;
		}

		my_css.active_pipes[ia_css_pipe_get_pipe_num(curr_pipe)] = curr_pipe;
	}
	/* this is a legacy from CSS 1.5, we still need this here because we need this info
	   to configure the sensor ouput in CameraDriver, but it is not needed in real device.
	   add this macro to make this explicit. */
#if defined(HRT_CSIM) || defined(HRT_RTL)
	/* stream has been configured, info should be available */
	{
		/* use first pipe in list, should be identical anyway */
		// TODO: should come from stream something
		struct ia_css_stream_info *stream_info = &curr_stream->info;
		// TODO: JB implement stream info
		err = sh_css_pipe_get_input_resolution(pipes[0],
				&stream_info->raw_info.width,
				&stream_info->raw_info.height);
		if (err != IA_CSS_SUCCESS)
			goto ERR;
	}
#endif

	curr_stream->started = false;

	/* Create host side pipeline objects without stages */
	err = create_host_pipeline_structure(curr_stream);
	if (err != IA_CSS_SUCCESS)
		return err;
	
	/* assign curr_stream */
	*stream = curr_stream;
	
	/* Map SP threads before doing anything. */
	err = map_sp_threads(curr_stream, true);
	if (err != IA_CSS_SUCCESS) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			"ia_css_stream_start(): map_sp_threads: return_err=%d\n", err);
		return err;
	}
	
ERR:
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_create() leave, err=%d\n",
			err);
	return err;
}

enum ia_css_err
ia_css_stream_destroy(struct ia_css_stream *stream)
{
	int i;
	enum ia_css_err err = IA_CSS_SUCCESS;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_destroy: enter\n");
	assert(stream != NULL);

	ia_css_stream_isp_parameters_uninit(stream);

#if defined(USE_INPUT_SYSTEM_VERSION_2401)
	stream_unregister_with_csi_rx(stream);
#endif

	err = map_sp_threads(stream, false);
	if (err != IA_CSS_SUCCESS) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE,
			"ia_css_stream_destroy(): map_sp_threads: return_err=%d\n", err);
		return err;
	}

	/* remove references from pipes to stream */
	for (i = 0; i < stream->num_pipes; i++) {
		struct ia_css_pipe *entry = stream->pipes[i];
		assert(entry != NULL);
		if (entry != NULL) {
			/* clear reference to stream */
			entry->stream = NULL;
			/* check internal copy pipe */
			if (entry->mode == IA_CSS_PIPE_ID_PREVIEW &&
			    entry->pipe_settings.preview.copy_pipe) {
				ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
					"ia_css_stream_destroy: "
					"clearing stream on internal preview copy pipe\n");
				entry->pipe_settings.preview.copy_pipe->stream = NULL;
			}
                        if (entry->mode == IA_CSS_PIPE_ID_VIDEO &&
                            entry->pipe_settings.video.copy_pipe) {
                                ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE,
                                        "ia_css_stream_destroy: "
                                        "clearing stream on internal video copy pipe\n");
                                entry->pipe_settings.video.copy_pipe->stream = NULL;
                        }



		}
	}
	/* free associated memory of stream struct */
	sh_css_free(stream->pipes);
	stream->pipes = NULL;
	stream->num_pipes = 0;
	sh_css_free(stream);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_destroy: leave\n");

	return err;
}

enum ia_css_err
ia_css_stream_get_info(const struct ia_css_stream *stream,
		       struct ia_css_stream_info *stream_info)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_get_info: enter/exit\n");
	assert(stream != NULL);
	assert(stream_info != NULL);

	*stream_info = stream->info;
	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_stream_load(struct ia_css_stream *stream)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_load() enter/exit\n");
	assert(stream != NULL);
	(void)stream;

	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_stream_start(struct ia_css_stream *stream)
{
	enum ia_css_err err = IA_CSS_SUCCESS;
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_start()\n");
	assert(stream != NULL);
	assert(stream->last_pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_start: starting %d\n",
		stream->last_pipe->mode);

	/* Create host side pipeline. */
	err = create_host_pipeline(stream);
	if (err != IA_CSS_SUCCESS)
		return err;

#if !defined(HAS_NO_INPUT_SYSTEM)
#if defined(USE_INPUT_SYSTEM_VERSION_2401)
	if((stream->config.mode == IA_CSS_INPUT_MODE_SENSOR) ||
	   (stream->config.mode == IA_CSS_INPUT_MODE_BUFFERED_SENSOR))
		stream_register_with_csi_rx(stream);
#endif
#endif

#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)
	// Initialize mipi size checks
	if (stream->config.mode == IA_CSS_INPUT_MODE_BUFFERED_SENSOR)
	{
		unsigned int idx;
		unsigned int port = (unsigned int) (stream->config.source.port.port) ;

		for (idx = 0; idx < IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT; idx++) {
			sh_css_sp_group.config.mipi_sizes_for_check[port][idx] =  sh_css_get_mipi_sizes_for_check(port, idx);
		}
	}
#endif

	return sh_css_pipe_start(stream);
}

enum ia_css_err
ia_css_stream_stop(struct ia_css_stream *stream)
{
	enum ia_css_err err = IA_CSS_SUCCESS;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_stop() enter/exit\n");
	assert(stream != NULL);
	assert(stream->last_pipe != NULL);
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_stop: stopping %d\n",
		stream->last_pipe->mode);

	// Check if this is the last pipe, if not return else stop everything
	//if (pipe_num_counter > 1) return IA_CSS_SUCCESS;

#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)
	// De-initialize mipi size checks
	if (stream->config.mode == IA_CSS_INPUT_MODE_BUFFERED_SENSOR)
	{
		unsigned int idx;
		unsigned int port = (unsigned int) (stream->config.source.port.port) ;

		for (idx = 0; idx < IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT; idx++) {
			sh_css_sp_group.config.mipi_sizes_for_check[port][idx] = 0;
		}
	}
#endif
	err = ia_css_pipeline_request_stop(&stream->last_pipe->pipeline);
	if (err != IA_CSS_SUCCESS)
		return err;

	/* Ideally, unmapping should happen after pipeline_stop, but current
	 * semantics do not allow that. */
	/* err = map_sp_threads(stream, false); */

	return err;
}

bool
ia_css_stream_has_stopped(struct ia_css_stream *stream)
{
	bool stopped;
	assert(stream != NULL);

	stopped = ia_css_pipeline_has_stopped(&stream->last_pipe->pipeline);

	return stopped;
}

enum ia_css_err
ia_css_stream_unload(struct ia_css_stream *stream)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_stream_unload() enter/exit\n");
	assert(stream != NULL);

	return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_temp_pipe_to_pipe_id(const struct ia_css_pipe *pipe, enum ia_css_pipe_id *pipe_id)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_temp_pipe_to_pipe_id() enter/exit\n");
	if (pipe != NULL)
		*pipe_id = pipe->mode;
	else
		*pipe_id = IA_CSS_PIPE_ID_COPY;

	return IA_CSS_SUCCESS;
}

enum ia_css_stream_format
ia_css_stream_get_format(const struct ia_css_stream *stream)
{
	return stream->config.format;
}

bool
ia_css_stream_get_two_pixels_per_clock(const struct ia_css_stream *stream)
{
	return (stream->config.pixels_per_clock == 2);
}

struct ia_css_binary *
ia_css_stream_get_dvs_binary(const struct ia_css_stream *stream)
{
	int i;
	struct ia_css_pipe *video_pipe = NULL;

	/* First we find the video pipe */
	for (i=0; i<stream->num_pipes; i++) {
		struct ia_css_pipe *pipe = stream->pipes[i];
		if (pipe->config.mode == IA_CSS_PIPE_MODE_VIDEO) {
			video_pipe = pipe;
			break;
		}
	}
	if (video_pipe)
		return &video_pipe->pipe_settings.video.video_binary;
	return NULL;
}

struct ia_css_binary *
ia_css_stream_get_3a_binary(const struct ia_css_stream *stream)
{
	struct ia_css_pipe *pipe;

	assert(stream != NULL);

	pipe = stream->pipes[0];

	if (stream->num_pipes == 2) {
		assert(stream->pipes[1] != NULL);
		if (stream->pipes[1]->config.mode == IA_CSS_PIPE_MODE_VIDEO ||
		    stream->pipes[1]->config.mode == IA_CSS_PIPE_MODE_PREVIEW)
			pipe = stream->pipes[1];
	}

	return ia_css_pipe_get_3a_binary(pipe);
}

static struct ia_css_binary *
ia_css_pipe_get_3a_binary (const struct ia_css_pipe *pipe)
{
	struct ia_css_binary *s3a_binary = NULL;

	assert(pipe != NULL);

	switch (pipe->config.mode) {
	case IA_CSS_PIPE_MODE_PREVIEW:
		s3a_binary = (struct ia_css_binary*)&pipe->pipe_settings.preview.preview_binary;
		break;
	case IA_CSS_PIPE_MODE_VIDEO:
		s3a_binary = (struct ia_css_binary*)&pipe->pipe_settings.video.video_binary;
		break;
	case IA_CSS_PIPE_MODE_CAPTURE:
		if (pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_PRIMARY)
			s3a_binary = (struct ia_css_binary*)&pipe->pipe_settings.capture.primary_binary;
		else if (pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_ADVANCED ||
			 pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_LOW_LIGHT ||
			 pipe->config.default_capture_config.mode == IA_CSS_CAPTURE_MODE_BAYER) {
			s3a_binary
				= (struct ia_css_binary*)&pipe->pipe_settings.capture.pre_isp_binary;
		}
		break;
	default:
		break;
	}

	if (s3a_binary && s3a_binary->info->sp.enable.s3a)
		return s3a_binary;

	return NULL;
}

#if defined(IS_ISP_2500_SYSTEM)
void ia_css_pipe_get_bds_resolution(const struct ia_css_pipe *pipe, struct ia_css_resolution *res)
{
	assert(pipe != NULL);

	*res = pipe->bds_output_info.res;

}
#endif

struct ia_css_pipeline *
ia_css_pipe_get_pipeline(const struct ia_css_pipe *pipe)
{
	assert(pipe != NULL);

	return (struct ia_css_pipeline*)&pipe->pipeline;
}

unsigned int
ia_css_pipe_get_pipe_num(const struct ia_css_pipe *pipe)
{
	assert(pipe != NULL);

	/* KW was not sure this function was not returning a value
	   that was out of range; so added an assert, and, for the
	   case when asserts are not enabled, clip to the largest
	   value; pipe_num is unsigned so the value cannot be too small
	*/
	assert(pipe->pipe_num < IA_CSS_PIPELINE_NUM_MAX);

	if (pipe->pipe_num >= IA_CSS_PIPELINE_NUM_MAX)
		return (IA_CSS_PIPELINE_NUM_MAX - 1);

	return pipe->pipe_num;
}


unsigned int
ia_css_pipe_get_isp_pipe_version(const struct ia_css_pipe *pipe)
{
	assert(pipe != NULL);

	return pipe->config.isp_pipe_version;
}

#define SP_START_TIMEOUT_US 30000000

enum ia_css_err
ia_css_start_sp(void)
{
	unsigned long timeout;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_start_sp() enter\n");
	sh_css_sp_start_isp();

	/* waiting for the SP is completely started */
	timeout = SP_START_TIMEOUT_US;
	while((ia_css_spctrl_get_state(SP0_ID) != IA_CSS_SP_SW_INITIALIZED) && timeout) {
		timeout--;
		hrt_sleep();
	}
	if (timeout == 0) {
		ia_css_debug_dtrace(IA_CSS_DEBUG_ERROR, "ia_css_start_sp() timeout\n");
		return IA_CSS_ERR_INTERNAL_ERROR;
	}

	/* Workaround, in order to run two streams in parallel. See TASK 4271*/
	/* TODO: Fix this. */

	sh_css_init_host_sp_control_vars();

	/* buffers should be initialized only when sp is started */
	/* AM: At the moment it will be done only when there is no stream active. */
	sh_css_init_buffer_queues();
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "ia_css_start_sp() exit\n");
	return IA_CSS_SUCCESS;
}

/**
 *	Time to wait SP for termincate. Only condition when this can happen
 *	is a fatal hw failure, but we must be able to detect this and emit
 *	a proper error trace.
 */
#define SP_SHUTDOWN_TIMEOUT_US 200000

enum ia_css_err
ia_css_stop_sp(void)
{
	unsigned int i;
	unsigned long timeout;

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_stop_sp() enter\n");
	/* For now, stop whole SP */
	sh_css_write_host2sp_command(host2sp_cmd_terminate);
	sh_css_sp_set_sp_running(false);
#ifdef __KERNEL__
	printk("STOP_FUNC: reach point 1\n");
#endif
	timeout = SP_SHUTDOWN_TIMEOUT_US;
	while ((ia_css_spctrl_get_state(SP0_ID)!= IA_CSS_SP_SW_TERMINATED) && timeout) {
		timeout--;
		hrt_sleep();
	}
	if (timeout == 0) {
		ia_css_debug_dump_debug_info("sh_css_stop_sp point1");
		ia_css_debug_dump_sp_sw_debug_info();
#ifdef __KERNEL__
		printk(KERN_ERR "%s poll timeout point 1!!!\n", __func__);
#endif
	}
#ifdef __KERNEL__
	printk("STOP_FUNC: reach point 2\n");
#endif
	while (!isp_ctrl_getbit(ISP0_ID, ISP_SC_REG, ISP_IDLE_BIT) && timeout) {
		timeout--;
		hrt_sleep();
	}
	if (timeout == 0) {
		ia_css_debug_dump_debug_info("sh_css_stop_sp point2");
		ia_css_debug_dump_sp_sw_debug_info();
#ifdef __KERNEL__
		printk(KERN_ERR "%s poll timeout point 2!!!\n", __func__);
#endif
	}
#ifdef __KERNEL__
	printk("STOP_FUNC: reach point 3\n");
#endif

	for (i = 0; i < MAX_HMM_BUFFER_NUM; i++) {
		if (hmm_buffer_record_h[i] != NULL) {
			ia_css_rmgr_rel_vbuf(hmm_buffer_pool, &hmm_buffer_record_h[i]);
		}
	}

	/* clear pending param sets from refcount */
	sh_css_param_clear_param_sets();

	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE, "sh_css_stop_sp() exit\n");
	return IA_CSS_SUCCESS;
}

void
ia_css_update_continuous_frames(struct ia_css_stream *stream)
{
	struct ia_css_pipe *pipe = stream->continuous_pipe;
	unsigned int i;

	ia_css_debug_dtrace(
	    IA_CSS_DEBUG_TRACE,
	    "sh_css_update_continuous_frames() enter:\n");

	for (i = stream->config.init_num_cont_raw_buf;
				i < stream->config.target_num_cont_raw_buf; i++) {
		sh_css_update_host2sp_offline_frame(i,
				pipe->continuous_frames[i]);
	}
	sh_css_update_host2sp_cont_num_raw_frames
			(stream->config.target_num_cont_raw_buf, true);
	ia_css_debug_dtrace(
	    IA_CSS_DEBUG_TRACE,
	    "sh_css_update_continuous_frames() leave: return_void\n");
}
