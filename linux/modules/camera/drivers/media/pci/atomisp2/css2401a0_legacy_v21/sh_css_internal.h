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

#ifndef _SH_CSS_INTERNAL_H_
#define _SH_CSS_INTERNAL_H_

#include <system_global.h>
#ifdef __KERNEL__
#include <linux/kernel.h>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

#include "platform_support.h"

#if !defined(HAS_NO_INPUT_FORMATTER)
#include "input_formatter.h"
#endif
#if !defined(HAS_NO_INPUT_SYSTEM)
#include "input_system.h"
#endif

#include "ia_css_types.h"
#include "ia_css_acc_types.h"

//#include "sh_css_internal.h"
#include "ia_css_binary.h"
#include "sh_css_firmware.h"
#include "sh_css_legacy.h"
#include "sh_css_defs.h"
#include "sh_css_uds.h"
#include "dma.h"	/* N_DMA_CHANNEL_ID */
#include "ia_css_circbuf_comm.h" /* Circular buffer */
#include "ia_css_frame_comm.h"

/* TODO: Move to a more suitable place when sp pipeline design is done. */
#define IA_CSS_NUM_CB_SEM_READ_RESOURCE 	2
#define IA_CSS_NUM_CB_SEM_WRITE_RESOURCE	1
#define IA_CSS_NUM_CBS						2
#define IA_CSS_CB_MAX_ELEMS					2

/* Use case specific. index limited to IA_CSS_NUM_CB_SEM_READ_RESOURCE or
 * IA_CSS_NUM_CB_SEM_WRITE_RESOURCE for read and write respectively.
 * TODO: Enforce the limitation above.
*/
#define IA_CSS_COPYSINK_SEM_INDEX 	0
#define IA_CSS_TAGGER_SEM_INDEX 	1

/* Force generation of output event. Used by acceleration pipe. */
#define IA_CSS_POST_OUT_EVENT_FORCE		2

#define SH_CSS_MAX_BINARY_NAME	32

#define SP_DEBUG_NONE	(0)
#define SP_DEBUG_DUMP	(1)
#define SP_DEBUG_COPY	(2)
#define SP_DEBUG_TRACE	(3)
#define SP_DEBUG_MINIMAL (4)

#define SP_DEBUG SP_DEBUG_NONE
#define SP_DEBUG_MINIMAL_OVERWRITE 1


#ifdef __DISABLE_UNUSED_THREAD__
#define SH_CSS_MAX_SP_THREADS	1 /* preview */
#else
#if defined(HAS_SP_2500)
#define SH_CSS_MAX_SP_THREADS	3 /* preview, capture, acceleration */
#else
#define SH_CSS_MAX_SP_THREADS	4 /* raw_copy, preview, capture, acceleration */
#endif
#endif

#define SH_CSS_MAX_PIPELINES	SH_CSS_MAX_SP_THREADS

/* keep next up to date with the definition for MAX_CB_ELEMS_FOR_TAGGER in tagger.sp.c */
#if defined(HAS_SP_2400)
#define NUM_CONTINUOUS_FRAMES	15
#else
#define NUM_CONTINUOUS_FRAMES	10
#endif
#define NUM_MIPI_FRAMES		4

#define NUM_ONLINE_INIT_CONTINUOUS_FRAMES      2

#define NUM_TNR_FRAMES		2

#define NUM_VIDEO_REF_FRAMES	(3)  /* SN: Should this not always match NUM_REF_FRAMES ?*/
#define NUM_VIDEO_TNR_FRAMES	2
#define NR_OF_PIPELINES			5 /* Must match with IA_CSS_PIPE_ID_NUM */

#define SH_CSS_MAX_IF_CONFIGS	3 /* Must match with IA_CSS_NR_OF_CONFIGS (not defined yet).*/
#define SH_CSS_IF_CONFIG_NOT_NEEDED	0xFF

/* Currently sensor metadata support is only implemented for
 * the input system v2, not for v3 yet. */
#if defined(USE_INPUT_SYSTEM_VERSION_2)
#define SH_CSS_ENABLE_METADATA
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
#define CALC_ALIGNMENT_MEMBER(x, y)	(CEIL_MUL(x, y) - x)
#define SIZE_OF_HRT_VADDRESS		sizeof(hive_uint32)
#define SIZE_OF_IA_CSS_PTR		sizeof(uint32_t)

/**
 * The following macro can help to test the size of a struct at compile
 * time rather than at run-time. It does not work for all compilers; see
 * below.
 *
 * Depending on the value of 'condition', the following macro is expanded to:
 * - condition==true:
 *     an expression containing an array declaration with negative size,
 *     usually resulting in a compilation error
 * - condition==false:
 *     (void) 1; // C statement with no effect
 *
 * example:
 *  COMPILATION_ERROR_IF( sizeof(struct host_sp_queues) != SIZE_OF_HOST_SP_QUEUES_STRUCT);
 *
 * verify that the macro indeed triggers a compilation error with your compiler:
 *  COMPILATION_ERROR_IF( sizeof(struct host_sp_queues) != (sizeof(struct host_sp_queues)+1) );
 *
 * Not all compilers will trigger an error with this macro; use a search engine to search for
 * BUILD_BUG_ON to find other methods.
 */
#define COMPILATION_ERROR_IF( condition ) ((void)sizeof(char[1 - 2*!!(condition)]))

 /*
 * JB: keep next enum in sync with thread id's
 * and pipe id's
 */
enum sh_css_pipe_config_override {
	SH_CSS_PIPE_CONFIG_OVRD_NONE     = 0,
	SH_CSS_PIPE_CONFIG_OVRD_NO_OVRD  = 0xffff
};

enum host2sp_commands {
	host2sp_cmd_error = 0,
	/*
	 * The host2sp_cmd_ready command is the only command written by the SP
	 * It acknowledges that is previous command has been received.
	 * (this does not mean that the command has been executed)
	 * It also indicates that a new command can be send (it is a queue
	 * with depth 1).
	 */
	host2sp_cmd_ready = 1,
	/* Command written by the Host */
	host2sp_cmd_dummy,		/* No action, can be used as watchdog */
	host2sp_cmd_start_flash,	/* Request SP to start the flash */
	host2sp_cmd_terminate,		/* SP should terminate itself */
	N_host2sp_cmd
};

/** Enumeration used to indicate the events that are produced by
 *  the SP and consumed by the Host.
 */
enum sh_css_sp_event_type {
	SH_CSS_SP_EVENT_OUTPUT_FRAME_DONE,
	SH_CSS_SP_EVENT_VF_OUTPUT_FRAME_DONE,
	SH_CSS_SP_EVENT_3A_STATISTICS_DONE,
	SH_CSS_SP_EVENT_DIS_STATISTICS_DONE,
	SH_CSS_SP_EVENT_PIPELINE_DONE,
	SH_CSS_SP_EVENT_FRAME_TAGGED,
	SH_CSS_SP_EVENT_INPUT_FRAME_DONE,
	SH_CSS_SP_EVENT_METADATA_DONE,
	SH_CSS_SP_EVENT_PORT_EOF,
	SH_CSS_SP_EVENT_NR_OF_TYPES		/* must be last */
};

#if !defined(IS_ISP_2500_SYSTEM)
/* xmem address map allocation per pipeline, css pointers */
struct sh_css_ddr_address_map {
	hrt_vaddress isp_param;
	hrt_vaddress isp_mem_param[SH_CSS_MAX_STAGES][IA_CSS_NUM_ISP_MEMORIES];
	hrt_vaddress macc_tbl;
	hrt_vaddress fpn_tbl;
	hrt_vaddress sc_tbl;
	hrt_vaddress sdis_hor_coef;
	hrt_vaddress sdis_ver_coef;
	hrt_vaddress tetra_r_x;
	hrt_vaddress tetra_r_y;
	hrt_vaddress tetra_gr_x;
	hrt_vaddress tetra_gr_y;
	hrt_vaddress tetra_gb_x;
	hrt_vaddress tetra_gb_y;
	hrt_vaddress tetra_b_x;
	hrt_vaddress tetra_b_y;
	hrt_vaddress tetra_ratb_x;
	hrt_vaddress tetra_ratb_y;
	hrt_vaddress tetra_batr_x;
	hrt_vaddress tetra_batr_y;
	hrt_vaddress dvs_6axis_params_y;
	hrt_vaddress anr_thres;
};
#define SIZE_OF_SH_CSS_DDR_ADDRESS_MAP_STRUCT					\
	(SIZE_OF_HRT_VADDRESS +							\
	(SH_CSS_MAX_STAGES * IA_CSS_NUM_ISP_MEMORIES * SIZE_OF_HRT_VADDRESS) +	\
	(19 * SIZE_OF_HRT_VADDRESS))
#endif

#if !defined(IS_ISP_2500_SYSTEM)
/* xmem address map allocation per pipeline */
struct sh_css_ddr_address_map_size {
	size_t isp_param;
	size_t isp_mem_param[SH_CSS_MAX_STAGES][IA_CSS_NUM_ISP_MEMORIES];
	size_t macc_tbl;
	size_t fpn_tbl;
	size_t sc_tbl;
	size_t sdis_hor_coef;
	size_t sdis_ver_coef;
	size_t tetra_r_x;
	size_t tetra_r_y;
	size_t tetra_gr_x;
	size_t tetra_gr_y;
	size_t tetra_gb_x;
	size_t tetra_gb_y;
	size_t tetra_b_x;
	size_t tetra_b_y;
	size_t tetra_ratb_x;
	size_t tetra_ratb_y;
	size_t tetra_batr_x;
	size_t tetra_batr_y;
	size_t dvs_6axis_params_y;
	size_t anr_thres;
};
#endif

#if !defined(IS_ISP_2500_SYSTEM)
struct sh_css_ddr_address_map_compound {
	struct sh_css_ddr_address_map		map;
	struct sh_css_ddr_address_map_size	size;
};
#endif

/* this struct contains all arguments that can be passed to
   a binary. It depends on the binary which ones are used. */
struct sh_css_binary_args {
	struct ia_css_frame *cc_frame;       /* continuous capture frame */
	struct ia_css_frame *in_frame;	     /* input frame */
	struct ia_css_frame *in_ref_frame;   /* reference input frame */
	struct ia_css_frame *in_tnr_frame;   /* tnr input frame */
	struct ia_css_frame *out_frame;      /* output frame */
	struct ia_css_frame *out_ref_frame;  /* reference output frame */
	struct ia_css_frame *out_tnr_frame;  /* tnr output frame */
	struct ia_css_frame *out_vf_frame;   /* viewfinder output frame */
	struct ia_css_frame *extra_ref_frame;    /* reference extra frame */
	bool                 copy_vf;
	bool                 copy_output;
	unsigned             vf_downscale_log2;
};

#if SP_DEBUG == SP_DEBUG_DUMP

#define SH_CSS_NUM_SP_DEBUG 48

struct sh_css_sp_debug_state {
	unsigned int error;
	unsigned int debug[SH_CSS_NUM_SP_DEBUG];
};

#elif SP_DEBUG == SP_DEBUG_COPY

#define SH_CSS_SP_DBG_TRACE_DEPTH	(40)

struct sh_css_sp_debug_trace {
	uint16_t frame;
	uint16_t line;
	uint16_t pixel_distance;
	uint16_t mipi_used_dword;
	uint16_t sp_index;
};

struct sh_css_sp_debug_state {
	uint16_t if_start_line;
	uint16_t if_start_column;
	uint16_t if_cropped_height;
	uint16_t if_cropped_width;
	unsigned int index;
	struct sh_css_sp_debug_trace
		trace[SH_CSS_SP_DBG_TRACE_DEPTH];
};

#elif SP_DEBUG == SP_DEBUG_TRACE

#if 1
/* Example of just one global trace */
#define SH_CSS_SP_DBG_NR_OF_TRACES	(1)
#define SH_CSS_SP_DBG_TRACE_DEPTH	(40)
#else
/* E.g. if you like seperate traces for 4 threads */
#define SH_CSS_SP_DBG_NR_OF_TRACES	(4)
#define SH_CSS_SP_DBG_TRACE_DEPTH	(10)
#endif

#define SH_CSS_SP_DBG_TRACE_FILE_ID_BIT_POS (13)

struct sh_css_sp_debug_trace {
	uint16_t time_stamp;
	uint16_t location;	/* bit 15..13 = file_id, 12..0 = line nr. */
	uint32_t data;
};

struct sh_css_sp_debug_state {
	struct sh_css_sp_debug_trace
		trace[SH_CSS_SP_DBG_NR_OF_TRACES][SH_CSS_SP_DBG_TRACE_DEPTH];
	uint16_t index_last[SH_CSS_SP_DBG_NR_OF_TRACES];
	uint8_t index[SH_CSS_SP_DBG_NR_OF_TRACES];
};

#elif SP_DEBUG == SP_DEBUG_MINIMAL

#define SH_CSS_NUM_SP_DEBUG 128

struct sh_css_sp_debug_state {
	unsigned int error;
	unsigned int debug[SH_CSS_NUM_SP_DEBUG];
};

#endif


struct sh_css_sp_debug_command {
	/*
	 * The DMA software-mask,
	 *	Bit 31...24: unused.
	 *	Bit 23...16: unused.
	 *	Bit 15...08: reading-request enabling bits for DMA channel 7..0
	 *	Bit 07...00: writing-reqeust enabling bits for DMA channel 7..0
	 *
	 * For example, "0...0 0...0 11111011 11111101" indicates that the
	 * writing request through DMA Channel 1 and the reading request
	 * through DMA channel 2 are both disabled. The others are enabled.
	 */
	uint32_t dma_sw_reg;
};

#if !defined(HAS_NO_INPUT_FORMATTER)
/* SP input formatter configuration.*/
struct sh_css_sp_input_formatter_set {
	uint32_t				stream_format;
	input_formatter_cfg_t	config_a;
	input_formatter_cfg_t	config_b;
};
#endif

#if !defined(HAS_NO_INPUT_SYSTEM)
#define IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT (3)
#endif

/* SP configuration information */
struct sh_css_sp_config {
	uint8_t			no_isp_sync; /* Signal host immediately after start */
#if !defined(HAS_NO_INPUT_FORMATTER)
	struct {
		uint8_t					a_changed;
		uint8_t					b_changed;
		uint8_t					isp_2ppc;
		struct sh_css_sp_input_formatter_set	set[SH_CSS_MAX_IF_CONFIGS]; /* CSI-2 port is used as index. */
	} input_formatter;
#endif
#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)
	sync_generator_cfg_t	sync_gen;
	tpg_cfg_t				tpg;
	prbs_cfg_t				prbs;
	input_system_cfg_t		input_circuit;
	uint8_t					input_circuit_cfg_changed;
	uint32_t				mipi_sizes_for_check[N_CSI_PORTS][IA_CSS_MIPI_SIZE_CHECK_MAX_NOF_ENTRIES_PER_PORT];
#endif
};

enum sh_css_stage_type {
  SH_CSS_SP_STAGE_TYPE  = 0,
  SH_CSS_ISP_STAGE_TYPE = 1
};
#define SH_CSS_NUM_STAGE_TYPES 2

#define SH_CSS_PIPE_CONFIG_SAMPLE_PARAMS 	(1 << 0)
#define SH_CSS_PIPE_CONFIG_SAMPLE_PARAMS_MASK \
	((SH_CSS_PIPE_CONFIG_SAMPLE_PARAMS << SH_CSS_MAX_SP_THREADS)-1)

#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2401)
struct sh_css_sp_pipeline_terminal {
	union {
		/* Input System 2401 */
		virtual_input_system_t		virtual_input_system;
	} context;

	/*
	 * zhengjie.lu@intel.com:
	 * TODO
	 * - Remove "virtual_input_system_cfg" when the ISYS2401 DLI is ready.
	 */
	union {
		/* Input System 2401 */
		virtual_input_system_cfg_t	virtual_input_system_cfg;
	} ctrl;
};

struct sh_css_sp_pipeline_io {
	struct sh_css_sp_pipeline_terminal	input;
	struct sh_css_sp_pipeline_terminal	output;
};

struct sh_css_sp_pipeline_io_status {
	uint32_t	active[N_INPUT_SYSTEM_CSI_PORT];
	uint32_t	running[N_INPUT_SYSTEM_CSI_PORT];
};

#endif
enum sh_css_port_dir {
	SH_CSS_PORT_INPUT  = 0,
	SH_CSS_PORT_OUTPUT  = 1
};

enum sh_css_port_type {
	SH_CSS_HOST_TYPE  = 0,
	SH_CSS_COPYSINK_TYPE  = 1,
	SH_CSS_TAGGERSINK_TYPE  = 2
};

/* Pipe inout settings: output port on 7-4bits, input port on 3-0bits */
#define SH_CSS_PORT_FLD_WIDTH_IN_BITS (4)
#define SH_CSS_PORT_TYPE_BIT_FLD(pt) (0x1 << (pt))
#define SH_CSS_PORT_FLD(pd) ((pd) ? SH_CSS_PORT_FLD_WIDTH_IN_BITS : 0)
#define SH_CSS_PIPE_PORT_CONFIG_ON(p,pd,pt) ( (p) |= (SH_CSS_PORT_TYPE_BIT_FLD(pt) << SH_CSS_PORT_FLD(pd)) )
#define SH_CSS_PIPE_PORT_CONFIG_OFF(p,pd,pt) ( (p) &= ~(SH_CSS_PORT_TYPE_BIT_FLD(pt) << SH_CSS_PORT_FLD(pd)) )
#define SH_CSS_PIPE_PORT_CONFIG_SET(p,pd,pt,val) ( (val)? SH_CSS_PIPE_PORT_CONFIG_ON(p,pd,pt):SH_CSS_PIPE_PORT_CONFIG_OFF(p,pd,pt) )
#define SH_CSS_PIPE_PORT_CONFIG_GET(p,pd,pt) ( (p) & (SH_CSS_PORT_TYPE_BIT_FLD(pt) << SH_CSS_PORT_FLD(pd)) )
#define SH_CSS_PIPE_PORT_CONFIG_IS_CONTINUOUS(p)  (!(SH_CSS_PIPE_PORT_CONFIG_GET(p,SH_CSS_PORT_INPUT,SH_CSS_HOST_TYPE) && \
					       SH_CSS_PIPE_PORT_CONFIG_GET(p,SH_CSS_PORT_OUTPUT,SH_CSS_HOST_TYPE)))

#define IA_CSS_ACQUIRE_ISP_POS	31

/* Information for a pipeline */
struct sh_css_sp_pipeline {
	uint32_t	pipe_id;	/* the pipe ID */
	uint32_t	pipe_num;	/* the dynamic pipe number */
	uint32_t	thread_id;	/* the sp thread ID */
	uint32_t	pipe_config;	/* the pipe config */
	uint32_t    inout_port_config;
	uint32_t	required_bds_factor;
	uint32_t	dvs_frame_delay;
#if !defined(HAS_NO_INPUT_SYSTEM)
	uint32_t	input_system_mode;	/* enum ia_css_input_mode */
	mipi_port_ID_t	port_id;	/* port_id for input system */
#endif
	uint32_t	num_stages;		/* the pipe config */
	uint32_t	running;	/* needed for pipe termination */
	hrt_vaddress	sp_stage_addr[SH_CSS_MAX_STAGES];
	CSS_ALIGN(struct sh_css_sp_stage *stage, 8); /* Current stage for this pipeline */
	CSS_ALIGN(int32_t num_execs, 8); /* number of times to run if this is
					  an acceleration pipe. */
#if defined (SH_CSS_ENABLE_METADATA)
	uint32_t		md_format;		/* Metadata format in hrt format. */
	unsigned int	md_size;		/* Rounded up metadata size in bytes. */
#endif
	union {
		struct {
			CSS_ALIGN(unsigned int	bytes_available, 8);
		} bin;
		struct {
			CSS_ALIGN(unsigned int	height, 8);
			unsigned int	width;
			unsigned int	padded_width;
			unsigned int	max_input_width;
			unsigned int	raw_bit_depth;
		} raw;
	} copy;
};

/* MW: ALL CAPS, and is it too much trouble to suffix an ID with ID ? */
enum sh_css_frame_id {
	sh_css_frame_in,		/* Dynamic */
	sh_css_frame_out,		/* Dynamic */
	sh_css_frame_out_vf,		/* Dynamic */
	sh_css_frame_s3a,		/* Dynamic */
	sh_css_frame_dis,		/* Dynamic */
	sh_css_frame_metadata,	/* Dynamic */
	sh_css_frame_ref_in,
	sh_css_frame_ref_out,
	sh_css_frame_ref_extra,
	sh_css_frame_tnr_in,
	sh_css_frame_tnr_out,
	sh_css_frame_extra,
	sh_css_frame_raw_out,
	sh_css_frame_cust_in,
	sh_css_frame_cust_out,
};
/*
 * The first frames (with comment Dynamic) can be dynamic or static
 * The other frames (ref_in and below) can only be static
 * Static means that the data addres will not change during the life time
 * of the associated pipe. Dynamic means that the data address can
 * change with every (frame) iteration of the associated pipe
 *
 * s3a and dis are now also dynamic but (stil) handled seperately
 */
#define SH_CSS_NUM_DYNAMIC_BUFFER_IDS (5)
#define SH_CSS_NUM_DYNAMIC_FRAME_IDS (3)
#define SH_CSS_INVALID_FRAME_ID (-1)

/* Information for a single pipeline stage for an ISP */
struct sh_css_isp_stage {
	/*
	 * For compatability and portabilty, only types
	 * from "stdint.h" are allowed
	 *
	 * Use of "enum" and "bool" is prohibited
	 * Multiple boolean flags can be stored in an
	 * integer
	 */
	struct ia_css_blob_info	  blob_info;
	struct ia_css_binary_info binary_info;
	char			  binary_name[SH_CSS_MAX_BINARY_NAME];
	struct ia_css_isp_param_css_segments mem_initializers;
};

/* Information for a single pipeline stage */
struct sh_css_sp_stage {
	/*
	 * For compatability and portabilty, only types
	 * from "stdint.h" are allowed
	 *
	 * Use of "enum" and "bool" is prohibited
	 * Multiple boolean flags can be stored in an
	 * integer
	 */
	uint8_t			num; /* Stage number */
	uint8_t			isp_online;
	uint8_t			isp_copy_vf;
	uint8_t			isp_copy_output;
	uint8_t			sp_enable_xnr;
	uint8_t			isp_deci_log_factor;
	uint8_t			isp_vf_downscale_bits;
	uint8_t			deinterleaved;
/*
 * NOTE: Programming the input circuit can only be done at the
 * start of a session. It is illegal to program it during execution
 * The input circuit defines the connectivity
 */
	uint8_t			program_input_circuit;
/* enum ia_css_pipeline_stage_sp_func	func; */
	uint8_t			func;
	/* The type of the pipe-stage */
	/* enum sh_css_stage_type	stage_type; */
	uint8_t			stage_type;
	uint8_t			num_stripes;
	uint8_t			isp_pipe_version;
	struct {
		uint8_t		vf_output;
		uint8_t		s3a;
		uint8_t		sdis;
	} enable;
	/* Add padding to come to a word boundary */
	/* unsigned char			padding[0]; */

	struct sh_css_crop_pos		sp_out_crop_pos;
	struct ia_css_frames_sp		frames;
	struct ia_css_resolution	dvs_envelope;
	struct sh_css_uds_info		uds;
	hrt_vaddress			isp_stage_addr;
	hrt_vaddress			xmem_bin_addr;
	hrt_vaddress			xmem_map_addr;

	uint16_t		top_cropping;
	uint16_t		row_stripes_height;
	uint16_t		row_stripes_overlap_lines;
	uint8_t			if_config_index; /* Which should be applied by this stage. */
};

/*
 * Time: 2012-07-19, 17:40.
 * Author: zhengjie.lu@intel.com
 * Note: Add a new data memeber "debug" in "sh_css_sp_group". This
 * data member is used to pass the debugging command from the
 * Host to the SP.
 *
 * Time: Before 2012-07-19.
 * Author: unknown
 * Note:
 * Group all host initialized SP variables into this struct.
 * This is initialized every stage through dma.
 * The stage part itself is transfered through sh_css_sp_stage.
*/
struct sh_css_sp_group {
	struct sh_css_sp_config		config;
	struct sh_css_sp_pipeline	pipe[SH_CSS_MAX_SP_THREADS];
#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2401)
	struct sh_css_sp_pipeline_io	pipe_io[SH_CSS_MAX_SP_THREADS];
	struct sh_css_sp_pipeline_io_status	pipe_io_status;
#endif
	struct sh_css_sp_debug_command	debug;
};

/* Data in SP dmem that is set from the host every stage. */
struct sh_css_sp_per_frame_data {
	/* ddr address of sp_group and sp_stage */
	hrt_vaddress			sp_group_addr;
};

#define SH_CSS_NUM_SDW_IRQS 3

/* Output data from SP to css */
struct sh_css_sp_output {
	unsigned int			bin_copy_bytes_copied;
#if SP_DEBUG != SP_DEBUG_NONE
	struct sh_css_sp_debug_state	debug;
#endif
	unsigned int		sw_interrupt_value[SH_CSS_NUM_SDW_IRQS];
};

/**
 * @brief Data structure for the circular buffer.
 * The circular buffer is empty if "start == end". The
 * circular buffer is full if "(end + 1) % size == start".
 */
#if defined(HAS_SP_2400)
#define  SH_CSS_CIRCULAR_BUF_NUM_ELEMS             13
#else
#define  SH_CSS_CIRCULAR_BUF_NUM_ELEMS              6
#endif

struct sh_css_hmm_buffer {
	union {
		struct ia_css_isp_3a_statistics  s3a;
		struct ia_css_isp_dvs_statistics dis;
		struct ia_css_metadata	metadata;
		struct {
			hrt_vaddress	frame_data;
			uint32_t	flashed;
			uint32_t	exp_id;
		} frame;
		hrt_vaddress ddr_ptrs;
	} payload;
	/*
	 * kernel_ptr is present for host administration purposes only.
	 * type is uint64_t in order to be 64-bit host compatible.
	 */
	CSS_ALIGN(uint64_t cookie_ptr, 8);
	CSS_ALIGN(uint64_t kernel_ptr, 8);
};
#define SIZE_OF_FRAME_STRUCT						\
	(SIZE_OF_HRT_VADDRESS +						\
	(2 * sizeof(uint32_t)))

#define SIZE_OF_PAYLOAD_UNION						\
	(MAX(MAX(MAX(MAX(						\
	SIZE_OF_IA_CSS_ISP_3A_STATISTICS_STRUCT,			\
	SIZE_OF_IA_CSS_ISP_DVS_STATISTICS_STRUCT),			\
	SIZE_OF_IA_CSS_METADATA_STRUCT),				\
	SIZE_OF_FRAME_STRUCT),						\
	SIZE_OF_HRT_VADDRESS))

#define SIZE_OF_SH_CSS_HMM_BUFFER_STRUCT				\
	(SIZE_OF_PAYLOAD_UNION + 					\
	CALC_ALIGNMENT_MEMBER(SIZE_OF_PAYLOAD_UNION, 8) +		\
	sizeof(uint64_t) +						\
	sizeof(uint64_t))

enum sh_css_queue_type {
	sh_css_invalid_queue_type = -1,
	sh_css_host2sp_buffer_queue,
	sh_css_sp2host_buffer_queue,
	sh_css_host2sp_event_queue,
	sh_css_sp2host_event_queue
};

enum sh_css_buffer_queue_id {
	sh_css_invalid_buffer_queue     = -1,
	sh_css_input_buffer_queue       = 0,
	sh_css_output_buffer_queue      = 1,
	sh_css_vf_output_buffer_queue   = 2,
	sh_css_s3a_buffer_queue         = 3,
	sh_css_dis_buffer_queue         = 4,
	sh_css_param_buffer_queue       = 5,
	sh_css_tag_cmd_queue            = 6,
#if !defined (SH_CSS_ENABLE_METADATA)
	sh_css_buffer_queue_id_last     = sh_css_tag_cmd_queue,
#else
	sh_css_metadata_buffer_queue    = 7,
	sh_css_buffer_queue_id_last     = sh_css_metadata_buffer_queue,
#endif
};

#define SH_CSS_NUM_BUFFER_QUEUES (sh_css_buffer_queue_id_last + 1)

struct sh_css_event_irq_mask {
	uint16_t or_mask;
	uint16_t and_mask;
};
#define SIZE_OF_SH_CSS_EVENT_IRQ_MASK_STRUCT				\
	(2 * sizeof(uint16_t))

struct host_sp_communication {
	/*
	 * Don't use enum host2sp_commands, because the sizeof an enum is
	 * compiler dependant and thus non-portable
	 */
	uint32_t host2sp_command;

	/*
	 * The frame buffers that are reused by the
	 * copy pipe in the offline preview mode.
	 *
	 * host2sp_offline_frames[0]: the input frame of the preview pipe.
	 * host2sp_offline_frames[1]: the output frame of the copy pipe.
	 *
	 * TODO:
	 *   Remove it when the Host and the SP is decoupled.
	 */
	hrt_vaddress host2sp_offline_frames[NUM_CONTINUOUS_FRAMES];
	hrt_vaddress host2sp_mipi_frames[NUM_MIPI_FRAMES];
	uint32_t host2sp_cont_avail_num_raw_frames;
	uint32_t host2sp_cont_extra_num_raw_frames;
	uint32_t host2sp_cont_target_num_raw_frames;
	uint32_t host2sp_cont_num_mipi_frames;
	struct sh_css_event_irq_mask host2sp_event_irq_mask[NR_OF_PIPELINES];

};
#define SIZE_OF_HOST_SP_COMMUNICATION_STRUCT				\
	(sizeof(uint32_t) +						\
	(NUM_CONTINUOUS_FRAMES * SIZE_OF_HRT_VADDRESS) +		\
	(NUM_MIPI_FRAMES * SIZE_OF_HRT_VADDRESS) +			\
	(4 *sizeof(uint32_t)) +						\
	(NR_OF_PIPELINES * SIZE_OF_SH_CSS_EVENT_IRQ_MASK_STRUCT))

struct host_sp_queues {
	/*
	 * Queues for the dynamic frame information,
	 * i.e. the "in_frame" buffer, the "out_frame"
	 * buffer and the "vf_out_frame" buffer.
	 */
	ia_css_circbuf_desc_t host2sp_buffer_queues_desc
		[SH_CSS_MAX_SP_THREADS][SH_CSS_NUM_BUFFER_QUEUES];
	ia_css_circbuf_elem_t host2sp_buffer_queues_elems
	[SH_CSS_MAX_SP_THREADS][SH_CSS_NUM_BUFFER_QUEUES]
	[SH_CSS_CIRCULAR_BUF_NUM_ELEMS];
	ia_css_circbuf_desc_t sp2host_buffer_queues_desc
		[SH_CSS_NUM_BUFFER_QUEUES];
	ia_css_circbuf_elem_t sp2host_buffer_queues_elems
	[SH_CSS_NUM_BUFFER_QUEUES][SH_CSS_CIRCULAR_BUF_NUM_ELEMS];

	/*
	 * The queue for the events.
	 */
	ia_css_circbuf_desc_t host2sp_event_queue_desc;
	ia_css_circbuf_elem_t host2sp_event_queue_elems[SH_CSS_CIRCULAR_BUF_NUM_ELEMS];
	ia_css_circbuf_desc_t sp2host_event_queue_desc;
	ia_css_circbuf_elem_t sp2host_event_queue_elems[SH_CSS_CIRCULAR_BUF_NUM_ELEMS];

};
#define SIZE_OF_HOST_SP_QUEUES_STRUCT 													\
	(((SH_CSS_MAX_SP_THREADS * SH_CSS_NUM_BUFFER_QUEUES) * SIZE_OF_IA_CSS_CIRCBUF_DESC_S_STRUCT) +					\
	((SH_CSS_MAX_SP_THREADS * SH_CSS_NUM_BUFFER_QUEUES * SH_CSS_CIRCULAR_BUF_NUM_ELEMS) * SIZE_OF_IA_CSS_CIRCBUF_ELEM_S_STRUCT) +	\
	(SH_CSS_NUM_BUFFER_QUEUES * SIZE_OF_IA_CSS_CIRCBUF_DESC_S_STRUCT) +								\
	((SH_CSS_NUM_BUFFER_QUEUES * SH_CSS_CIRCULAR_BUF_NUM_ELEMS) * SIZE_OF_IA_CSS_CIRCBUF_ELEM_S_STRUCT) +				\
	SIZE_OF_IA_CSS_CIRCBUF_DESC_S_STRUCT +												\
	(SH_CSS_CIRCULAR_BUF_NUM_ELEMS * SIZE_OF_IA_CSS_CIRCBUF_ELEM_S_STRUCT) +							\
	SIZE_OF_IA_CSS_CIRCBUF_DESC_S_STRUCT +												\
	(SH_CSS_CIRCULAR_BUF_NUM_ELEMS * SIZE_OF_IA_CSS_CIRCBUF_ELEM_S_STRUCT))

extern int (*sh_css_printf) (const char *fmt, va_list args);

#ifndef __HIVECC
STORAGE_CLASS_INLINE void
sh_css_print(const char *fmt, ...)
{
	va_list ap;

	if (sh_css_printf) {
		va_start(ap, fmt);
		sh_css_printf(fmt, ap);
		va_end(ap);
	}
}

STORAGE_CLASS_INLINE void
sh_css_vprint(const char *fmt, va_list args)
{
	if (sh_css_printf)
		sh_css_printf(fmt, args);
}
#endif

hrt_vaddress
sh_css_params_ddr_address_map(void);

enum ia_css_err
sh_css_params_init(void);

void
sh_css_params_uninit(void);

void
sh_css_params_reconfigure_gdc_lut(void);

void *
sh_css_malloc(size_t size);

void *
sh_css_calloc(size_t N, size_t size);

void
sh_css_free(void *ptr);

/* For Acceleration API: Flush FW (shared buffer pointer) arguments */
extern void
sh_css_flush(struct ia_css_acc_fw *fw);


void sh_css_binary_args_reset(struct sh_css_binary_args *args);

/* Check two frames for equality (format, resolution, bits per element) */
bool
sh_css_frame_equal_types(const struct ia_css_frame *frame_a,
			 const struct ia_css_frame *frame_b);

bool
sh_css_frame_info_equal_resolution(const struct ia_css_frame_info *info_a,
				   const struct ia_css_frame_info *info_b);

unsigned int
sh_css_input_format_bits_per_pixel(enum ia_css_stream_format format,
				   bool two_ppc);

void
sh_css_capture_enable_bayer_downscaling(bool enable);

void
sh_css_binary_print(const struct ia_css_binary *binary);

void
sh_css_frame_info_set_width(struct ia_css_frame_info *info,
			    unsigned int width,
			    unsigned int aligned); // this can be used for an extra alignemt requirement. when 0, no extra alignment is done.

#if !defined(HAS_NO_INPUT_SYSTEM) && defined(USE_INPUT_SYSTEM_VERSION_2)

unsigned int
sh_css_get_mipi_sizes_for_check(const unsigned int port, const unsigned int idx);

#endif

hrt_vaddress
sh_css_store_sp_group_to_ddr(void);

hrt_vaddress
sh_css_store_sp_stage_to_ddr(unsigned pipe, unsigned stage);

hrt_vaddress
sh_css_store_isp_stage_to_ddr(unsigned pipe, unsigned stage);

void
sh_css_frame_info_init(struct ia_css_frame_info *info,
		       unsigned int width,
		       unsigned int height,
		       enum ia_css_frame_format format,
		       unsigned int aligned);

bool
sh_css_enqueue_frame(unsigned int pipe_num,
		     enum sh_css_frame_id frame_id,
		     struct ia_css_frame *frame);


/**
 * @brief Query the internal frame ID.
 *
 * @param[in]	key	The query key.
 * @param[out]	val	The query value.
 *
 * @return
 *	true, if the query succeeds;
 *	false, if the query fails.
 */
bool
sh_css_query_internal_queue_id(enum ia_css_buffer_type key,
		enum sh_css_buffer_queue_id *val);

void
sh_css_update_uds_and_crop_info(
		const struct ia_css_binary_info *info,
		const struct ia_css_frame_info *in_frame_info,
		const struct ia_css_frame_info *out_frame_info,
		const struct ia_css_resolution *dvs_env,
		bool preview_mode,
		const struct ia_css_dz_config *zoom,
		const struct ia_css_vector *motion_vector,
		struct sh_css_uds_info *uds,		/* out */
		struct sh_css_crop_pos *sp_out_crop_pos	/* out */
		);

void
sh_css_invalidate_shading_tables(struct ia_css_stream *stream);

#if defined(IS_ISP_2500_SYSTEM)
void
ia_css_pipe_get_bds_resolution(const struct ia_css_pipe *pipe, struct ia_css_resolution *res);
#endif

struct ia_css_pipeline *
ia_css_pipe_get_pipeline(const struct ia_css_pipe *pipe);

unsigned int
ia_css_pipe_get_pipe_num(const struct ia_css_pipe *pipe);

unsigned int
ia_css_pipe_get_isp_pipe_version(const struct ia_css_pipe *pipe);

bool
sh_css_continuous_is_enabled(uint8_t pipe_num);

#endif /* _SH_CSS_INTERNAL_H_ */
