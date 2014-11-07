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

/* Generated code: do not edit or commmit. */

#include "ia_css.h"
#define IA_CSS_INCLUDE_CONFIGURATIONS
#include "ia_css_pipeline.h"
#include HRTSTR(ia_css_isp_configs.SYSTEM.h)
#include "ia_css_debug.h"
#include "assert_support.h"

void
ia_css_configure_ref(
	const struct ia_css_binary *binary,
	const struct ia_css_ref_configuration *config_dmem)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "ia_css_configure_ref() enter:\n");

	{
		short offset = -1;
		if (binary->info->mem_offsets.offsets.config)
			offset = binary->info->mem_offsets.offsets.config->dmem.ref;

		if (offset >= 0) {
			ia_css_ref_config((struct sh_css_isp_ref_isp_config *)
					&binary->mem_params.params[IA_CSS_PARAM_CLASS_CONFIG][IA_CSS_ISP_DMEM].address[offset],
					config_dmem);
		}
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "ia_css_configure_ref() leave:\n");
}

void
ia_css_configure_vf(
	const struct ia_css_binary *binary,
	const struct ia_css_vf_configuration *config_dmem)
{
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "ia_css_configure_vf() enter:\n");

	{
		short offset = -1;
		if (binary->info->mem_offsets.offsets.config)
			offset = binary->info->mem_offsets.offsets.config->dmem.vf;

		if (offset >= 0) {
			ia_css_vf_config((struct sh_css_isp_vf_isp_config *)
					&binary->mem_params.params[IA_CSS_PARAM_CLASS_CONFIG][IA_CSS_ISP_DMEM].address[offset],
					config_dmem);
		}
	}
	ia_css_debug_dtrace(IA_CSS_DEBUG_TRACE_PRIVATE, "ia_css_configure_vf() leave:\n");
}

