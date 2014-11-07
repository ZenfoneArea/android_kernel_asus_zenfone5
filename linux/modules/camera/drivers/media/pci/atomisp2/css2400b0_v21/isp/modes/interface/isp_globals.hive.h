/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2010 - 2013 Intel Corporation.
 * All Rights Reserved.
 *
 * The source code contained or described herein and all documents
 * related to the source code ("Material") are owned by Intel Corporation
 * or licensors. Title to the Material remains with Intel
 * Corporation or its licensors. The Material contains trade
 * secrets and proprietary and confidential information of Intel or its
 * licensors. The Material is protected by worldwide copyright
 * and trade secret laws and treaty provisions. No part of the Material may
 * be used, copied, reproduced, modified, published, uploaded, posted,
 * transmitted, distributed, or disclosed in any way without Intel's prior
 * express written permission.
 *
 * No License under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or
 * delivery of the Materials, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights
 * must be express and approved by Intel in writing.
 */

#ifndef _isp_globals_hive_h_
#define _isp_globals_hive_h_

#ifndef SYNC_WITH
#define SYNC_WITH(x)
#endif
#ifndef MEM
#define MEM(x)
#endif
#ifndef NO_SYNC
#define NO_SYNC
#endif
#ifndef NO_HOIST
#define NO_HOIST
#endif

#include "isp_types.h"
#include <sh_css_internal.h>
#if !defined(__HOST)
#include "dma_proxy.common.h"
#endif
#include "input_buf.isp.h"

#include "uds/uds_1.0/ia_css_uds_param.h" /* sh_css_sp_uds_params */

/* Initialized by the SP: binary dependent */
/* Some of these globals are used inside dma transfer/configure commands.
   Therefore, their load will get a sync attribute. NO_SYNC prevents that.
*/
typedef struct s_isp_globals {
  unsigned input_width_not_padded;
  unsigned input_width;
  unsigned input_height;
  unsigned internal_width;
  unsigned internal_height;
  unsigned output_width;
  unsigned output_height;
  unsigned envelope_width;
  unsigned envelope_height;
  unsigned bits_per_pixel;
  unsigned deinterleaved;
  unsigned isp2ppc;
  unsigned copy_vf;
  unsigned copy_output;
  unsigned vectors_per_line;
  unsigned vectors_per_input_line;
  unsigned uv_internal_width_vecs;
  unsigned vf_output_width_vecs;
  unsigned sdis_horiproj_num;
  unsigned sdis_vertproj_num;
  unsigned sdis_horicoef_vectors;
  unsigned sdis_vertcoef_vectors;
  enum sh_stream_format    input_stream_format;
  enum ia_css_frame_format input_image_format;
  enum ia_css_frame_format output_image_format;
  enum ia_css_frame_format vf_image_format;

  int dp_threshold_single;
  int dp_threshold_2adjacent;

  unsigned out_crop_pos_y; /* Cropping for output image */
  unsigned vf_crop_pos_y;  /* Cropping for ViewFinder image */

  unsigned dma_vfout_skip_vecs;
  unsigned dma_vfout_cropping_a;
  unsigned dma_vfout_block_width_a;
  unsigned dma_vfout_block_width_b;

  unsigned dma_crop_skip_words;
  unsigned dma_crop_cropping_a;
  unsigned dma_crop_block_width_a;
  unsigned dma_crop_block_width_b;

  unsigned dma_tnr_stride_b;

/* DMA settings for output image */
  unsigned dma_output_skip_vecs;
  unsigned dma_output_block_width_a;
  unsigned dma_output_block_width_b;

  unsigned dma_c_skip_vecs;
  unsigned dma_c_block_width_a;
  unsigned dma_c_block_width_b;
} s_isp_globals;

#ifdef __SP
#define ISP_DMEM MEM(SP_XMEM)
#define PVECTOR short MEM(SP_XMEM) *
#else
#define ISP_DMEM
#define PVECTOR tmemvectoru MEM(VMEM) *
#endif

typedef void *pipeline_param_h;

typedef struct s_isp_addresses {
  struct {
    struct sh_css_sp_uds_params     ISP_DMEM *uds_params;
    unsigned                        ISP_DMEM *isp_deci_log_factor;
    struct s_isp_frames             ISP_DMEM *isp_frames;
    struct s_isp_globals            ISP_DMEM *isp_globals;
    unsigned                        ISP_DMEM *isp_online;
    struct s_output_dma_info        ISP_DMEM *output_dma_info;
    unsigned                        ISP_DMEM *vertical_upsampled;
    struct sh_css_ddr_address_map   ISP_DMEM *xmem_base;
    struct ia_css_isp_3a_statistics ISP_DMEM *s3a_data;
    sh_dma_cmd*                     ISP_DMEM *sh_dma_cmd_ptr;
    unsigned                        ISP_DMEM *g_isp_do_zoom;
    struct isp_uds_config           ISP_DMEM *uds_config;
    int                             ISP_DMEM *g_sdis_horiproj_tbl;
    int                             ISP_DMEM *g_sdis_vertproj_tbl;
    unsigned                        ISP_DMEM *isp_enable_xnr;
    struct s_isp_gdcac_config       ISP_DMEM *isp_gdcac_config;
    unsigned                        ISP_DMEM *stripe_id;
    unsigned                        ISP_DMEM *stripe_row_id;
    unsigned                        ISP_DMEM *isp_continuous;
    unsigned                        ISP_DMEM *required_bds_factor;
    unsigned                        ISP_DMEM *isp_raw_stride_b;
	unsigned			            ISP_DMEM *isp_raw_block_width_b;
	unsigned			            ISP_DMEM *isp_raw_line_width_b;
	unsigned			            ISP_DMEM *isp_raw_stripe_offset_b;
  } dmem;
  struct {
    PVECTOR  input_buf;
    PVECTOR  g_macc_coef;
    PVECTOR  g_sdis_horicoef_tbl;
    PVECTOR  g_sdis_vertcoef_tbl; /* Can be vmem or dmem */
    PVECTOR  vf_tmp;
    PVECTOR  uds_data_via_sp;
    PVECTOR  uds_ipxs_via_sp;
    PVECTOR  uds_ibuf_via_sp;
    PVECTOR  uds_obuf_via_sp;
    PVECTOR  aa_buf;
  } vmem;
  struct {
    unsigned uds_params;
    unsigned g_sdis_horiproj_tbl;
    unsigned g_sdis_vertproj_tbl;
    unsigned vf_tmp;
    unsigned aa_buf;
  } sizes;
} s_isp_addresses;

extern s_isp_globals   NO_SYNC NO_HOIST isp_globals;
extern s_isp_addresses NO_SYNC NO_HOIST isp_addresses;

#ifdef __ISP
#define isp_input_width            isp_globals.input_width
#define isp_input_height           isp_globals.input_height
#define isp_internal_width         isp_globals.internal_width
#define isp_internal_height        isp_globals.internal_height
#define isp_output_width           isp_globals.output_width
#define isp_output_height          isp_globals.output_height
#define isp_envelope_width         isp_globals.envelope_width
#define isp_envelope_height        isp_globals.envelope_height
#define isp_bits_per_pixel         isp_globals.bits_per_pixel
#define isp_deinterleaved          isp_globals.deinterleaved
#define isp_2ppc                   isp_globals.isp2ppc
#define isp_copy_vf                isp_globals.copy_vf
#define isp_copy_output            isp_globals.copy_output
#define isp_vectors_per_line       isp_globals.vectors_per_line
#define isp_vectors_per_input_line isp_globals.vectors_per_input_line
#define isp_uv_internal_width_vecs isp_globals.uv_internal_width_vecs
#define isp_vf_output_width_vecs   isp_globals.vf_output_width_vecs
#define isp_sdis_horiproj_num      isp_globals.sdis_horiproj_num
#define isp_sdis_vertproj_num      isp_globals.sdis_vertproj_num
#define isp_sdis_horicoef_vectors  isp_globals.sdis_horicoef_vectors
#define isp_sdis_vertcoef_vectors  isp_globals.sdis_vertcoef_vectors
#define isp_input_stream_format    isp_globals.input_stream_format
#define isp_output_image_format    isp_globals.output_image_format
#define isp_vf_image_format        isp_globals.vf_image_format

#define g_dp_threshold_single      isp_globals.dp_threshold_single
#define g_dp_threshold_2adjacent   isp_globals.dp_threshold_2adjacent

#define g_out_crop_pos_y           isp_globals.out_crop_pos_y
#define g_vf_crop_pos_y            isp_globals.vf_crop_pos_y

#define g_dma_vfout_skip_vecs      isp_globals.dma_vfout_skip_vecs
#define g_dma_vfout_cropping_a     isp_globals.dma_vfout_cropping_a
#define g_dma_vfout_block_width_a  isp_globals.dma_vfout_block_width_a
#define g_dma_vfout_block_width_b  isp_globals.dma_vfout_block_width_b

#define g_dma_crop_skip_words      isp_globals.dma_crop_skip_words
#define g_dma_crop_cropping_a      isp_globals.dma_crop_cropping_a
#define g_dma_crop_block_width_a   isp_globals.dma_crop_block_width_a
#define g_dma_crop_block_width_b   isp_globals.dma_crop_block_width_b
#define g_dma_tnr_stride_b         isp_globals.dma_tnr_stride_b

#define g_dma_output_skip_vecs     isp_globals.dma_output_skip_vecs
#define g_dma_output_block_width_a isp_globals.dma_output_block_width_a
#define g_dma_output_block_width_b isp_globals.dma_output_block_width_b
#define g_dma_c_skip_vecs          isp_globals.dma_c_skip_vecs
#define g_dma_c_block_width_a      isp_globals.dma_c_block_width_a
#define g_dma_c_block_width_b      isp_globals.dma_c_block_width_b

#if ENABLE_VF_VECEVEN
#define isp_vf_downscale_bits      isp_dmem_configurations.vf.vf_downscale_bits
#else
#define isp_vf_downscale_bits      0
#endif

#endif /* __ISP */

extern unsigned isp_deci_log_factor;
extern unsigned isp_online;

extern struct sh_css_ddr_address_map xmem_base;
extern struct ia_css_isp_3a_statistics s3a_data;
extern struct s_isp_frames isp_frames;
extern struct isp_uds_config uds_config;

/* *****************************************************************
 * 		uds parameters
 * *****************************************************************/
extern NO_HOIST struct sh_css_sp_uds_params uds_params[SH_CSS_MAX_STAGES];

/* DMA settings for viewfinder image */

#define isp_do_zoom (ENABLE_DVS_ENVELOPE ? 1 : g_isp_do_zoom)
extern unsigned g_isp_do_zoom;

#if defined(__ISP)
#if MODE != IA_CSS_BINARY_MODE_COPY

typedef struct {
  tmemvectoru raw[INPUT_BUF_HEIGHT][RAW_BUF_LINES][RAW_BUF_STRIDE]; /* 2 bayer lines */
} raw_line_type;

#if ENABLE_CONTINUOUS
#define OUTPUT_BUF_LINES 4
#else
#define OUTPUT_BUF_LINES 2
#endif

typedef struct {
  tmemvectoru raw[OUTPUT_BUF_HEIGHT][OUTPUT_BUF_LINES][MAX_VECTORS_PER_LINE]; /* 2 bayer lines */
} output_line_type;

extern input_line_type SYNC_WITH (INPUT_BUF) MEM (VMEM) input_buf;
extern output_line_type SYNC_WITH (OUTPUT_BUF) MEM (VMEM) raw_output_buf;

#endif /* MODE != IA_CSS_BINARY_MODE_COPY */
#endif /* __ISP */

/* DMA proxy buffer */
#if !defined(__HOST)
extern unsigned     NO_SYNC sh_dma_cmd_buffer_idx;
extern unsigned     NO_SYNC sh_dma_cmd_buffer_cnt;
extern unsigned     NO_SYNC sh_dma_cmd_buffer_need_ack;
extern unsigned     NO_SYNC sh_dma_cmd_buffer_enabled;
#if defined(__ISP) && defined(__HIVECC)
extern sh_dma_cmd SYNC_WITH(DMA_PROXY_BUF) * NO_SYNC sh_dma_cmd_ptr;
#else
extern sh_dma_cmd *sh_dma_cmd_ptr;
#endif
#endif

/* striped-ISP information */
extern unsigned stripe_id;
extern unsigned stripe_row_id;

extern unsigned isp_continuous;
extern unsigned required_bds_factor;
extern unsigned isp_raw_stride_b;
extern unsigned isp_raw_block_width_b;
extern unsigned isp_raw_line_width_b;
extern unsigned isp_raw_stripe_offset_b;

#endif /* _isp_globals_hive_h_ */
