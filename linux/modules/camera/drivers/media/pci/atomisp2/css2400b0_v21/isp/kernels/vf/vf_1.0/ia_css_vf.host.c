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

#include "ia_css_frame.h"
#include "ia_css.h"
#include "ia_css_pipeline.h"
#define IA_CSS_INCLUDE_CONFIGURATIONS
#include HRTSTR(ia_css_isp_configs.SYSTEM.h)
#include "assert_support.h"

#include "isp.h"
#include "ia_css_vf.host.h"

void
ia_css_vf_config(
	struct sh_css_isp_vf_isp_config *to,
	const struct ia_css_vf_configuration  *from)
{
       to->vf_downscale_bits = from->vf_downscale_bits;
}

/* compute the log2 of the downscale factor needed to get closest
 * to the requested viewfinder resolution on the upper side. The output cannot
 * be smaller than the requested viewfinder resolution.
 */
enum ia_css_err
sh_css_vf_downscale_log2(const struct ia_css_frame_info *out_info,
			const struct ia_css_frame_info *vf_info,
			unsigned int *downscale_log2)
{
       unsigned int ds_log2 = 0;
       unsigned int out_width;

       if ((out_info == NULL) | (vf_info == NULL))
	       return IA_CSS_ERR_INVALID_ARGUMENTS;

       out_width = out_info->res.width;

       if (out_width == 0)
	       return IA_CSS_ERR_INVALID_ARGUMENTS;

       /* downscale until width smaller than the viewfinder width. We don't
	* test for the height since the vmem buffers only put restrictions on
	* the width of a line, not on the number of lines in a frame.
	*/
       while (out_width >= vf_info->res.width) {
	       ds_log2++;
	       out_width /= 2;
       }
       /* now width is smaller, so we go up one step */
       if ((ds_log2 > 0) && (out_width < ia_css_binary_max_vf_width()))
	       ds_log2--;
       /* TODO: use actual max input resolution of vf_pp binary */
       if ((out_info->res.width >> ds_log2) >= 2 * ia_css_binary_max_vf_width())
	       return IA_CSS_ERR_INVALID_ARGUMENTS;
       *downscale_log2 = ds_log2;
       return IA_CSS_SUCCESS;
}

static enum ia_css_err
configure_kernel(
	const struct ia_css_binary_info *info,
	const struct ia_css_frame_info *out_info,
	const struct ia_css_frame_info *vf_info,
	unsigned int *downscale_log2,
	struct ia_css_vf_configuration *config)
{
       enum ia_css_err err;
       unsigned vf_log_ds = 0;

       /* First compute value */
       if (vf_info) {
	       err = sh_css_vf_downscale_log2(out_info, vf_info, &vf_log_ds);
	       if (err != IA_CSS_SUCCESS)
		       return err;
       }
       vf_log_ds = min(vf_log_ds, info->max_vf_log_downscale);
       *downscale_log2 = vf_log_ds;

       /* Then store it in isp config section */
       config->vf_downscale_bits = vf_log_ds;
       return IA_CSS_SUCCESS;
}

enum ia_css_err
ia_css_vf_configure(
	const struct ia_css_binary *binary,
	const struct ia_css_frame_info *out_info,
	struct ia_css_frame_info *vf_info,
	unsigned int *downscale_log2)
{
	enum ia_css_err err;
	struct ia_css_vf_configuration config;
	const struct ia_css_binary_info *info = &binary->info->sp;

	err = configure_kernel(info, out_info, vf_info, downscale_log2, &config);
	if (binary) ia_css_configure_vf (binary, &config);
	return IA_CSS_SUCCESS;
}

