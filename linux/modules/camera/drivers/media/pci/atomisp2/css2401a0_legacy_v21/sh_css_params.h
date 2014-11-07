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

#ifndef _SH_CSS_PARAMS_H_
#define _SH_CSS_PARAMS_H_

/*! \file */

/* Forward declaration to break mutual dependency */
struct ia_css_isp_parameters;

#include "ia_css_types.h"
#include "ia_css.h"
#include "sh_css_internal.h"
#include "sh_css_legacy.h"
#if defined(IS_ISP_2500_SYSTEM)
#include "product_specific.host.h"
#endif
#include "sh_css_defs.h"	/* SH_CSS_MAX_STAGES */
#include "ia_css_pipeline.h"
#include HRTSTR(ia_css_isp_params.SYSTEM.h)
#if defined(IS_ISP_2500_SYSTEM)
#include HRTSTR(ia_css_isp_acc_params.SYSTEM.h)
#endif

#include "ob/ob_1.0/ia_css_ob_param.h"
#include "uds/uds_1.0/ia_css_uds_param.h"

/* Isp configurations per stream */
struct sh_css_isp_param_configs {
	/* OB (Optical Black) */
	struct sh_css_isp_ob_stream_config ob;
};

/* Isp parameters per stream */
struct ia_css_isp_parameters {
	/* UDS */
	struct sh_css_sp_uds_params uds[SH_CSS_MAX_STAGES];
#if !defined(IS_ISP_2500_SYSTEM)
	struct sh_css_isp_param_configs stream_configs;
#else
	struct sh_css_isp_params    isp_parameters;
#endif

	struct ia_css_fpn_table     fpn_config;
	struct ia_css_vector	    motion_config;
	const struct ia_css_morph_table   *morph_table;
	const struct ia_css_shading_table *sc_table;
	struct ia_css_shading_table *sc_config;
	struct ia_css_macc_table    macc_table;
	struct ia_css_gamma_table   gc_table;
	struct ia_css_ctc_table     ctc_table;
	struct ia_css_xnr_table     xnr_table;

	struct ia_css_dz_config     dz_config;
	struct ia_css_3a_config     s3a_config;
	struct ia_css_wb_config     wb_config;
	struct ia_css_cc_config     csc_config;
	struct ia_css_cc_config     yuv2rgb_config;
	struct ia_css_cc_config     rgb2yuv_config;
	struct ia_css_tnr_config    tnr_config;
	struct ia_css_ob_config     ob_config;
	struct ia_css_dp_config     dp_config;
	struct ia_css_nr_config     nr_config;
	struct ia_css_ee_config     ee_config;
	struct ia_css_de_config     de_config;
	struct ia_css_gc_config     gc_config;
	struct ia_css_anr_config    anr_config;
	struct ia_css_ce_config     ce_config;
	
	struct ia_css_dvs_6axis_config	*dvs_6axis_config;
	
	struct ia_css_ecd_config    ecd_config;
	struct ia_css_ynr_config    ynr_config;
	struct ia_css_yee_config    yee_config;
	struct ia_css_fc_config     fc_config;
	struct ia_css_cnr_config    cnr_config;
	struct ia_css_macc_config   macc_config;
	struct ia_css_ctc_config    ctc_config;
	struct ia_css_aa_config     aa_config;
	struct ia_css_aa_config     raw_config;
	struct ia_css_aa_config     raa_config;
	struct ia_css_rgb_gamma_table     r_gamma_table;
	struct ia_css_rgb_gamma_table     g_gamma_table;
	struct ia_css_rgb_gamma_table     b_gamma_table;
	struct ia_css_anr_thres     anr_thres;
	struct ia_css_xnr_config    xnr_config;
	
	bool isp_params_changed;
	bool isp_mem_params_changed
		[IA_CSS_PIPE_ID_NUM][SH_CSS_MAX_STAGES]
		[IA_CSS_NUM_ISP_MEMORIES];
	bool dz_config_changed;
	bool motion_config_changed;
	bool dis_coef_table_changed;
	bool dvs2_coef_table_changed;
	bool morph_table_changed;
	bool sc_table_changed;
	bool anr_thres_changed;
	bool dvs_6axis_config_changed;

	bool config_changed[IA_CSS_NUM_PARAMETER_IDS];

	unsigned int sensor_binning;
	/* local buffers, used to re-order the 3a statistics in vmem-format */
	const short *dis_hor_coef_tbl;
	const short *dis_ver_coef_tbl;
	struct ia_css_dvs2_coef_types dvs2_hor_coefs;
	struct ia_css_dvs2_coef_types dvs2_ver_coefs;
	struct sh_css_ddr_address_map pipe_ddr_ptrs[IA_CSS_PIPE_ID_NUM];
	struct sh_css_ddr_address_map_size pipe_ddr_ptrs_size[IA_CSS_PIPE_ID_NUM];
	struct sh_css_ddr_address_map ddr_ptrs;
	struct sh_css_ddr_address_map_size ddr_ptrs_size;
};

enum ia_css_err
sh_css_params_write_to_ddr(struct ia_css_stream *stream,
			   struct ia_css_pipeline_stage *stage);

#endif /* _SH_CSS_PARAMS_H_ */
