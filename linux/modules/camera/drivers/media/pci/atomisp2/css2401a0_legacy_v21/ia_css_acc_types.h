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

#ifndef _IA_CSS_ACC_TYPES_H_
#define _IA_CSS_ACC_TYPES_H_

/*! \file */

/** @file ia_css_types.h
 * This file contains types used for the ia_css parameters.
 * These types are in a separate file because they are expected
 * to be used in software layers that do not access the CSS API
 * directly but still need to forward parameters for it.
 */

/* This code is also used by Silicon Hive in a simulation environment
 * Therefore, the following macro is used to differentiate when this
 * code is being included from within the Linux kernel source
 */
#include <system_types.h>	/* HAS_IRQ_MAP_VERSION_# */

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/string.h>       /* memcpy() */
#else
#include <stdarg.h>             /* printf() */
#include <stdlib.h>             /* size_t */
#include <string.h>             /* memcpy() */
#include "math_support.h"		/* min(), max() */
#endif

#include "ia_css.h"
#include "ia_css_types.h"
#include "platform_support.h"

#include "debug_global.h"

/* Types for the acceleration API.
 * These should be moved to sh_css_internal.h once the old acceleration
 * argument handling has been completed.
 * After that, interpretation of these structures is no longer needed
 * in the kernel and HAL.
*/

/** Type of acceleration.
 */
enum ia_css_acc_type {
	IA_CSS_ACC_NONE,	/**< Normal binary */
	IA_CSS_ACC_OUTPUT,	/**< Accelerator stage on output frame */
	IA_CSS_ACC_VIEWFINDER,	/**< Accelerator stage on viewfinder frame */
	IA_CSS_ACC_STANDALONE,	/**< Stand-alone acceleration */
};

/** Firmware types.
 */
enum ia_css_fw_type {
	ia_css_sp_firmware,		/**< Firmware for the SP */
#if defined(IS_ISP_2500_SYSTEM)
	ia_css_sp1_firmware,	/**< Firmware for the SP1 */
#endif
	ia_css_isp_firmware,	/**< Firmware for the ISP */
	ia_css_acc_firmware		/**< Firmware for accelrations */
};

#if defined(IS_ISP_2400_SYSTEM)
enum ia_css_isp_memories {
	IA_CSS_ISP_PMEM0 = 0,
	IA_CSS_ISP_DMEM0,
	IA_CSS_ISP_VMEM0,
	IA_CSS_ISP_VAMEM0,
	IA_CSS_ISP_VAMEM1,
	IA_CSS_ISP_VAMEM2,
	IA_CSS_ISP_HMEM0,
	N_IA_CSS_ISP_MEMORIES
};

/* Short hands */
#define IA_CSS_ISP_DMEM IA_CSS_ISP_DMEM0
#define IA_CSS_ISP_VMEM IA_CSS_ISP_VMEM0

#define IA_CSS_NUM_ISP_MEMORIES 7

#elif defined(IS_ISP_2500_SYSTEM)
enum ia_css_isp_memories {
	IA_CSS_ISP_PMEM0 = 0,
	IA_CSS_ISP_DMEM0,
	IA_CSS_ISP_VMEM0,
	IA_CSS_ISP_VAMEM0,
	IA_CSS_ISP_VAMEM1,
	IA_CSS_ISP_VAMEM2,
	IA_CSS_ISP_HMEM0,
	N_IA_CSS_ISP_MEMORIES
};

#define IA_CSS_NUM_ISP_MEMORIES 7

#else
#error "ia_css_types.h:  SYSTEM must be one of {ISP_2400_SYSTEM, ISP_2500_SYSTEM}"
#endif

/** CSS data descriptor */
struct ia_css_data {
	ia_css_ptr address; /* CSS virtual address */
	uint32_t   size;    /* Disabled if 0 */
};

/** Host data descriptor */
struct ia_css_host_data {
	char      *address; /* Host address */
	uint32_t   size;    /* Disabled if 0 */
};

/** ISP data descriptor */
struct ia_css_isp_data {
	uint32_t   address; /* ISP address */
	uint32_t   size;    /* Disabled if 0 */
};

/* Should be included without the path.
   However, that requires adding the path to numerous makefiles
   that have nothing to do with isp parameters.
 */
#include "runtime/isp_param/interface/ia_css_isp_param_types.h"

struct ia_css_blob_descr;

struct ia_css_channel_descr {
	uint8_t		channel;  /* Dma channel used */
	uint8_t		height;   /* Buffer height */
	uint16_t	stride;   /* Buffer stride */
};

/** Blob descriptor.
 * This structure describes an SP or ISP blob.
 * It describes the test, data and bss sections as well as position in a
 * firmware file.
 * For convenience, it contains dynamic data after loading.
 */
struct ia_css_blob_info {
	/**< Static blob data */
	uint32_t offset;		/**< Blob offset in fw file */
	struct ia_css_isp_param_memory_offsets memory_offsets;  /**< offset wrt hdr in bytes */
	uint32_t prog_name_offset;  /**< offset wrt hdr in bytes */
	uint32_t size;			/**< Size of blob */
	uint32_t padding_size;	/**< total cummulative of bytes added due to section alignment */
	uint32_t icache_source;	/**< Position of icache in blob */
	uint32_t icache_size;	/**< Size of icache section */
	uint32_t icache_padding;/**< bytes added due to icache section alignment */
	uint32_t text_source;	/**< Position of text in blob */
	uint32_t text_size;		/**< Size of text section */
	uint32_t text_padding;	/**< bytes added due to text section alignment */
	uint32_t data_source;	/**< Position of data in blob */
	uint32_t data_target;	/**< Start of data in SP dmem */
	uint32_t data_size;		/**< Size of text section */
	uint32_t data_padding;	/**< bytes added due to data section alignment */
	uint32_t bss_target;	/**< Start position of bss in SP dmem */
	uint32_t bss_size;		/**< Size of bss section */
	/**< Dynamic data filled by loader */
	CSS_ALIGN(const void  *code, 8);		/**< Code section absolute pointer within fw, code = icache + text */
	CSS_ALIGN(const void  *data, 8);		/**< Data section absolute pointer within fw, data = data + bss */
};

/** Structure describing an ISP binary.
 * It describes the capabilities of a binary, like the maximum resolution,
 * support features, dma channels, uds features, etc.
 * This part is to be used by the SP.
 * Future refactoring should move binary properties to ia_css_binary_xinfo,
 * thereby making the SP code more binary independent.
 */
struct ia_css_binary_info {
	CSS_ALIGN(uint32_t	id, 8); /* IA_CSS_BINARY_ID_* */
	uint32_t		mode;
	uint32_t		supported_bds_factors;
	uint32_t		min_input_width;
	uint32_t		min_input_height;
	uint32_t		max_input_width;
	uint32_t		max_input_height;
	uint32_t		min_output_width;
	uint32_t		min_output_height;
	uint32_t		max_output_width;
	uint32_t		max_output_height;
	uint32_t		max_internal_width;
	uint32_t		max_internal_height;
	uint32_t		max_dvs_envelope_width;
	uint32_t		max_dvs_envelope_height;
	uint32_t		variable_resolution;
	uint32_t		variable_output_format;
	uint32_t		variable_vf_veceven;
	uint32_t		max_vf_log_downscale;
	uint32_t		top_cropping;
	uint32_t		left_cropping;
	uint32_t		s3atbl_use_dmem;
	int32_t			input;
	uint32_t		c_subsampling;
	uint32_t		output_num_chunks;
	uint32_t		num_stripes;
	uint32_t		row_stripes_height;
	uint32_t		row_stripes_overlap_lines;
	uint32_t		pipelining;
	uint32_t		fixed_s3a_deci_log;
	uint32_t		isp_addresses;	/* Address in ISP dmem */
	uint32_t		main_entry;	/* Address of entry fct */
	uint32_t		in_frame;	/* Address in ISP dmem */
	uint32_t		out_frame;	/* Address in ISP dmem */
	uint32_t		in_data;	/* Address in ISP dmem */
	uint32_t		out_data;	/* Address in ISP dmem */
	uint32_t		block_width;
	uint32_t		block_height;
	uint32_t		output_block_height;
	uint32_t		dvs_in_block_width;
	uint32_t		dvs_in_block_height;
	struct ia_css_isp_param_isp_segments mem_initializers;
	uint32_t		sh_dma_cmd_ptr;     /* In ISP dmem */
	uint32_t		isp_pipe_version;
/* MW: Packing (related) bools in an integer ?? */
	struct {
#if defined(IS_ISP_2500_SYSTEM)
		uint8_t	input_feeder;
		uint8_t	obgrid;
		uint8_t	lin;
		uint8_t	dpc_acc;
		uint8_t	dpc_ff;
		uint8_t	bds_acc;
		uint8_t	shd_acc;
		uint8_t	shd_ff;
		uint8_t	stats_3a_raw_buffer;
		uint8_t	acc_bayer_denoise;
		uint8_t	bnr_ff;
		uint8_t	awb_acc;
		uint8_t	awb_fr_acc;
		uint8_t	anr_acc;
		uint8_t	rgbpp_acc;
		uint8_t	rgbpp_ff;
		uint8_t	demosaic_acc;
		uint8_t	demosaic_ff;
		uint8_t	yuvp1_acc;
		uint8_t	yuvp2_acc;
		uint8_t	ae;
		uint8_t	af;
		uint8_t	dergb;
		uint8_t	rgb2yuv;
		uint8_t	high_quality;
		uint8_t	kerneltest;
		uint8_t	bayer_output;
		uint8_t	routing_bnr_to_anr;
                uint8_t routing_anr_to_de;
#endif
		uint8_t	reduced_pipe;
		uint8_t	vf_veceven;
		uint8_t	dis;
		uint8_t	dvs_envelope;
		uint8_t	uds;
		uint8_t	dvs_6axis;
		uint8_t	block_output;
		uint8_t	streaming_dma;
		uint8_t	ds;
		uint8_t	fixed_bayer_ds;
		uint8_t	bayer_fir_6db;
		uint8_t	raw_binning;
		uint8_t	continuous;
		uint8_t	s3a;
		uint8_t	fpnr;
		uint8_t	sc;
		uint8_t	dis_crop;
		uint8_t	macc;
		uint8_t	output;
		uint8_t	ref_frame;
		uint8_t	tnr;
		uint8_t	xnr;
		uint8_t	raw;
		uint8_t	params;
		uint8_t	gamma;
		uint8_t	ctc;
		uint8_t	ca_gdc;
		uint8_t	isp_addresses;
		uint8_t	in_frame;
		uint8_t	out_frame;
		uint8_t	high_speed;
		uint8_t	input_chunking;
		uint8_t padding[2];
	} enable;
	struct {
/* DMA channel ID: [0,...,HIVE_ISP_NUM_DMA_CHANNELS> */
		uint8_t	crop_channel;
		uint8_t	multi_channel;
		uint8_t	raw_out_channel;
		uint8_t	ref_y_channel;
		uint8_t	ref_c_channel;
		uint8_t	tnr_out_channel;
		uint8_t	dvs_in_channel;
		uint8_t	dvs_coords_channel;
		uint8_t	output_channel;
		uint8_t	c_channel;
		uint8_t	vfout_channel;
		uint8_t	vfout_c_channel;
		uint8_t	claimed_by_isp;
		/* uint8_t padding[0]; */
		struct ia_css_channel_descr fpn;
		struct ia_css_channel_descr raw;
		struct ia_css_channel_descr sct;
		struct ia_css_channel_descr tnr;
	} dma;
	struct {
		uint16_t	bpp;
		uint16_t	use_bci;
		uint16_t	use_str;
		uint16_t	woix;
		uint16_t	woiy;
		uint16_t	extra_out_vecs;
		uint16_t	vectors_per_line_in;
		uint16_t	vectors_per_line_out;
		uint16_t	vectors_c_per_line_in;
		uint16_t	vectors_c_per_line_out;
		uint16_t	vmem_gdc_in_block_height_y;
		uint16_t	vmem_gdc_in_block_height_c;
		/* uint16_t padding; */
	} uds;
};

/** Structure describing an ISP binary.
 * It describes the capabilities of a binary, like the maximum resolution,
 * support features, dma channels, uds features, etc.
 */
struct ia_css_binary_xinfo {
	/* Part that is of interest to the SP. */
	struct ia_css_binary_info    sp;

	/* Rest of the binary info, only interesting to the host. */
	enum ia_css_acc_type	     type;
	CSS_ALIGN(int32_t	     num_output_formats, 8);
	enum ia_css_frame_format     output_formats[IA_CSS_FRAME_FORMAT_NUM];
	uint8_t			     num_output_pins;
	ia_css_ptr		     xmem_addr;
	CSS_ALIGN(const struct ia_css_blob_descr *blob, 8);
	CSS_ALIGN(uint32_t blob_index, 8);
	CSS_ALIGN(union ia_css_all_memory_offsets mem_offsets, 8);
	CSS_ALIGN(struct ia_css_binary_xinfo *next, 8);
};

/** Structure describing the SP binary.
 * It contains several address, either in ddr, sp_dmem or
 * the entry function in pmem.
 */
struct ia_css_sp_info {
	uint32_t init_dmem_data; /**< data sect config, stored to dmem */
	uint32_t per_frame_data; /**< Per frame data, stored to dmem */
	uint32_t group;		/**< Per pipeline data, loaded by dma */
	uint32_t output;		/**< SP output data, loaded by dmem */
	uint32_t host_sp_queue;	/**< Host <-> SP queues */
	uint32_t host_sp_com;/**< Host <-> SP commands */
	uint32_t isp_started;	/**< Polled from sensor thread, csim only */
	uint32_t sw_state;	/**< Polled from css */
	uint32_t host_sp_queues_initialized; /**< Polled from the SP */
	uint32_t sleep_mode;  /**< different mode to halt SP */
	uint32_t invalidate_tlb;		/**< inform SP to invalidate mmu TLB */
	uint32_t stop_copy_preview;	/**< suspend copy and preview pipe when capture */
	uint32_t debug_buffer_ddr_address;	/**< inform SP the address
	of DDR debug queue */
	uint32_t perf_counter_input_system_error; /**< input system perf
	counter array */
#ifdef HAS_WATCHDOG_SP_THREAD_DEBUG
	uint32_t debug_wait; /**< thread/pipe post mortem debug */
	uint32_t debug_stage; /**< thread/pipe post mortem debug */
	uint32_t debug_stripe; /**< thread/pipe post mortem debug */
#endif
	uint32_t curr_binary_id;        /**< current binary id */
	uint32_t raw_copy_line_count;   /**< raw copy line counter */
	uint32_t ddr_parameter_address; /**< acc param ddrptr, sp dmem */
	uint32_t ddr_parameter_size;    /**< acc param size, sp dmem */
	/* Entry functions */
	uint32_t sp_entry;	/**< The SP entry function */
};

/** Accelerator firmware information.
 */
struct ia_css_acc_info {
	uint32_t per_frame_data; /**< Dummy for now */
};

/** Firmware information.
 */
union ia_css_fw_union {
	struct ia_css_binary_xinfo	isp; /**< ISP info */
	struct ia_css_sp_info		sp;  /**< SP info */
	struct ia_css_sp_info		sp1;  /**< SP info */
	struct ia_css_acc_info		acc; /**< Accelerator info */
};

/** Firmware information.
 */
struct ia_css_fw_info {
	size_t			 header_size; /**< size of fw header */
	CSS_ALIGN(uint32_t type, 8);
	union ia_css_fw_union	 info; /**< Binary info */
	struct ia_css_blob_info  blob; /**< Blob info */
	/* Dynamic part */
	struct ia_css_fw_info   *next;
	CSS_ALIGN(uint32_t       loaded, 8);	/**< Firmware has been loaded */
	CSS_ALIGN(const uint8_t *isp_code, 8);  /**< ISP pointer to code */
	/**< Firmware handle between user space and kernel */
	CSS_ALIGN(uint32_t	handle, 8);
	/**< Sections to copy from/to ISP */
	struct ia_css_isp_param_css_segments mem_initializers;
	/**< Initializer for local ISP memories */
};

struct ia_css_blob_descr {
	const unsigned char  *blob;
	struct ia_css_fw_info header;
	const char	     *name;
	union ia_css_all_memory_offsets mem_offsets;
};

struct ia_css_acc_fw;

/** Structure describing the SP binary of a stand-alone accelerator.
 */
 struct ia_css_acc_sp {
	void (*init) (struct ia_css_acc_fw *); /**< init for crun */
	uint32_t      sp_prog_name_offset; /**< program name offset wrt hdr
						in bytes */
	uint32_t      sp_blob_offset;	   /**< blob offset wrt hdr in bytes */
	void	     *entry;		   /**< Address of sp entry point */
	uint32_t *css_abort;	   /**< SP dmem abort flag */
	void	     *isp_code;		   /**< SP dmem address holding xmem
						address of isp code */
	struct ia_css_fw_info fw;	   /**< SP fw descriptor */
	const uint8_t *code;	   /**< ISP pointer of allocated
						SP code */
};

/** Acceleration firmware descriptor.
  * This descriptor descibes either SP code (stand-alone), or
  * ISP code (a separate pipeline stage).
  */
struct ia_css_acc_fw_hdr {
	enum ia_css_acc_type type;	/**< Type of accelerator */
	uint32_t	isp_prog_name_offset; /**< program name offset wrt
						   header in bytes */
	uint32_t	isp_blob_offset;      /**< blob offset wrt header
						   in bytes */
	uint32_t	isp_size;	      /**< Size of isp blob */
	const uint8_t  *isp_code;	      /**< ISP pointer to code */
	struct ia_css_acc_sp  sp;  /**< Standalone sp code */
	/**< Firmware handle between user space and kernel */
	uint32_t	handle;
	struct ia_css_data parameters; /**< Current SP parameters */
};

/** Firmware structure.
  * This contains the header and actual blobs.
  * For standalone, it contains SP and ISP blob.
  * For a pipeline stage accelerator, it contains ISP code only.
  * Since its members are variable size, their offsets are described in the
  * header and computed using the access macros below.
  */
struct ia_css_acc_fw {
	struct ia_css_acc_fw_hdr header; /**< firmware header */
	/*
	int8_t   isp_progname[];	  **< ISP program name
	int8_t   sp_progname[];	  **< SP program name, stand-alone only
	uint8_t sp_code[];  **< SP blob, stand-alone only
	uint8_t isp_code[]; **< ISP blob
	*/
};

/* Access macros for firmware */
#define IA_CSS_ACC_OFFSET(t, f, n) ((t)((uint8_t *)(f)+(f->header.n)))
#define IA_CSS_ACC_SP_PROG_NAME(f) IA_CSS_ACC_OFFSET(const char *, f, \
						 sp.sp_prog_name_offset)
#define IA_CSS_ACC_ISP_PROG_NAME(f) IA_CSS_ACC_OFFSET(const char *, f, \
						 isp_prog_name_offset)
#define IA_CSS_ACC_SP_CODE(f)      IA_CSS_ACC_OFFSET(uint8_t *, f, \
						 sp.sp_blob_offset)
#define IA_CSS_ACC_SP_DATA(f)      (IA_CSS_ACC_SP_CODE(f) + \
					(f)->header.sp.fw.blob.data_source)
#define IA_CSS_ACC_ISP_CODE(f)     IA_CSS_ACC_OFFSET(uint8_t*, f,\
						 isp_blob_offset)
#define IA_CSS_ACC_ISP_SIZE(f)     ((f)->header.isp_size)

/* Binary name follows header immediately */
#define IA_CSS_EXT_ISP_PROG_NAME(f)   ((const char *)(f)+(f)->blob.prog_name_offset)
#define IA_CSS_EXT_ISP_MEM_OFFSETS(f) \
	((const struct ia_css_memory_offsets *)((const char *)(f)+(f)->blob.mem_offsets))

enum ia_css_sp_sleep_mode {
	SP_DISABLE_SLEEP_MODE = 0,
	SP_SLEEP_AFTER_FRAME = 1 << 0,
	SP_SLEEP_AFTER_IRQ = 1 << 1
};
#endif /* _IA_CSS_TYPES_H_ */
