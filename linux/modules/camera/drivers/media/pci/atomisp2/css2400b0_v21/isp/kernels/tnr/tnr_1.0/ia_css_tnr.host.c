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

#include "ia_css_tnr.host.h"

const struct ia_css_tnr_config default_tnr_config = {
	32768,
	32,
	32,
};

void
ia_css_tnr_encode(struct sh_css_isp_tnr_params *to,
		 const struct ia_css_tnr_config *from)
{
	to->coef =
	    uDIGIT_FITTING(from->gain, 16, SH_CSS_TNR_COEF_SHIFT);
	to->threshold_Y =
	    uDIGIT_FITTING(from->threshold_y, 16, SH_CSS_ISP_YUV_BITS);
	to->threshold_C =
	    uDIGIT_FITTING(from->threshold_uv, 16, SH_CSS_ISP_YUV_BITS);
}

void
ia_css_tnr_dump(const struct sh_css_isp_tnr_params *tnr, unsigned level)
{
	ia_css_debug_dtrace(level, "Temporal Noise Reduction:\n");
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"tnr_coef", tnr->coef);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"tnr_threshold_Y", tnr->threshold_Y);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n"
			"tnr_threshold_C", tnr->threshold_C);
}

void
ia_css_tnr_debug_dtrace(const struct ia_css_tnr_config *config, unsigned level)
{
	ia_css_debug_dtrace(level,
		"config.gain=%d, "
		"config.threshold_y=%d, config.threshold_uv=%d\n",
		config->gain,
		config->threshold_y, config->threshold_uv);
}
