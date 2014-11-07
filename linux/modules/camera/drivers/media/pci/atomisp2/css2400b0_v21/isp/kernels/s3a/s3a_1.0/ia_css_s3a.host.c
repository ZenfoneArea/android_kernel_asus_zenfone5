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

#include "ia_css_types.h"
#include "sh_css_defs.h"
#include "ia_css_debug.h"
#include "sh_css_frac.h"
#include "assert_support.h"

#include "bh/bh_2/ia_css_bh.host.h"
#include "ia_css_s3a.host.h"

const struct ia_css_3a_config default_3a_config = {
	25559,
	32768,
	7209,
	65535,
	0,
	65535,
	{-3344, -6104, -19143, 19143, 6104, 3344, 0},
	{1027, 0, -9219, 16384, -9219, 1027, 0}
};

static unsigned int s3a_raw_bit_depth;

void
ia_css_s3a_configure(unsigned int raw_bit_depth)
{
  s3a_raw_bit_depth = raw_bit_depth;
}

static void
ia_css_ae_encode(struct sh_css_isp_ae_params *to,
	 const struct ia_css_3a_config *from)
{
	/* coefficients to calculate Y */
	to->y_coef_r =
	    uDIGIT_FITTING(from->ae_y_coef_r, 16, SH_CSS_AE_YCOEF_SHIFT);
	to->y_coef_g =
	    uDIGIT_FITTING(from->ae_y_coef_g, 16, SH_CSS_AE_YCOEF_SHIFT);
	to->y_coef_b =
	    uDIGIT_FITTING(from->ae_y_coef_b, 16, SH_CSS_AE_YCOEF_SHIFT);
}

static void
ia_css_awb_encode(struct sh_css_isp_awb_params *to,
	 const struct ia_css_3a_config *from)
{
	/* AWB level gate */
	to->lg_high_raw =
		uDIGIT_FITTING(from->awb_lg_high_raw, 16, s3a_raw_bit_depth);
	to->lg_low =
		uDIGIT_FITTING(from->awb_lg_low, 16, SH_CSS_BAYER_BITS);
	to->lg_high =
		uDIGIT_FITTING(from->awb_lg_high, 16, SH_CSS_BAYER_BITS);
}

static void
ia_css_af_encode(struct sh_css_isp_af_params *to,
	 const struct ia_css_3a_config *from)
{
	unsigned int i;

	/* af fir coefficients */
	for (i = 0; i < 7; ++i) {
		to->fir1[i] =
		  sDIGIT_FITTING(from->af_fir1_coef[i], 15,
				 SH_CSS_AF_FIR_SHIFT);
		to->fir2[i] =
		  sDIGIT_FITTING(from->af_fir2_coef[i], 15,
				 SH_CSS_AF_FIR_SHIFT);
	}
}

void
ia_css_s3a_encode(struct sh_css_isp_s3a_params *to,
		  const struct ia_css_3a_config *from)
{
	ia_css_ae_encode(&to->ae,  from);
	ia_css_awb_encode(&to->awb, from);
	ia_css_af_encode(&to->af,  from);
}

#if 0
void
ia_css_process_s3a(unsigned pipe_id,
		  const struct ia_css_pipeline_stage *stage,
		  struct ia_css_isp_parameters *params)
{
	short dmem_offset = stage->binary->info->mem_offsets->dmem.s3a;

	assert(params != NULL);

	if (dmem_offset >= 0) {
		ia_css_s3a_encode((struct sh_css_isp_s3a_params *)
				&stage->isp_mem_params[IA_CSS_ISP_DMEM0].address[dmem_offset],
				&params->s3a_config);
		ia_css_bh_encode((struct sh_css_isp_bh_params *)
				&stage->isp_mem_params[IA_CSS_ISP_DMEM0].address[dmem_offset],
				&params->s3a_config);
		params->isp_params_changed = true;
		params->isp_mem_params_changed[pipe_id][stage->stage_num][IA_CSS_ISP_DMEM0] = true;
	}

	params->isp_params_changed = true;
}
#endif

void
ia_css_ae_dump(const struct sh_css_isp_ae_params *ae, unsigned level)
{
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"ae_y_coef_r", ae->y_coef_r);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"ae_y_coef_g", ae->y_coef_g);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"ae_y_coef_b", ae->y_coef_b);
}

void
ia_css_awb_dump(const struct sh_css_isp_awb_params *awb, unsigned level)
{
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"awb_lg_high_raw", awb->lg_high_raw);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"awb_lg_low", awb->lg_low);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"awb_lg_high", awb->lg_high);
}

void
ia_css_af_dump(const struct sh_css_isp_af_params *af, unsigned level)
{
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir1[0]", af->fir1[0]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir1[1]", af->fir1[1]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir1[2]", af->fir1[2]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir1[3]", af->fir1[3]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir1[4]", af->fir1[4]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir1[5]", af->fir1[5]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir1[6]", af->fir1[6]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir2[0]", af->fir2[0]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir2[1]", af->fir2[1]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir2[2]", af->fir2[2]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir2[3]", af->fir2[3]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir2[4]", af->fir2[4]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir2[5]", af->fir2[5]);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"af_fir2[6]", af->fir2[6]);
}

void
ia_css_s3a_dump(const struct sh_css_isp_s3a_params *s3a, unsigned level)
{
	ia_css_debug_dtrace(level, "S3A Support:\n");
	ia_css_ae_dump  (&s3a->ae, level);
	ia_css_awb_dump (&s3a->awb, level);
	ia_css_af_dump  (&s3a->af, level);
}

void
ia_css_3a_config_debug_dtrace(const struct ia_css_3a_config *config, unsigned level)
{
	ia_css_debug_dtrace(level,
		"config.ae_y_coef_r=%d, config.ae_y_coef_g=%d, "
		"config.ae_y_coef_b=%d, config.awb_lg_high_raw=%d, "
		"config.awb_lg_low=%d, config.awb_lg_high=%d\n",
		config->ae_y_coef_r, config->ae_y_coef_g,
		config->ae_y_coef_b, config->awb_lg_high_raw,
		config->awb_lg_low, config->awb_lg_high);
}

