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
#include "assert_support.h"

#include "ia_css_fpn.host.h"

void
ia_css_fpn_encode(struct sh_css_isp_fpn_params *to,
		 const struct ia_css_fpn_table *from)
{
	to->shift = from->shift;
	to->enabled = from->data != NULL;
}

void
ia_css_fpn_dump(const struct sh_css_isp_fpn_params *fpn, unsigned level)
{
	ia_css_debug_dtrace(level, "Fixed Pattern Noise Reduction:\n");
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"fpn_shift", fpn->shift);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n",
			"fpn_enabled", fpn->enabled);
}
