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

#ifndef _SH_CSS_PARAMS_SHADING_H_
#define _SH_CSS_PARAMS_SHADING_H_

#include "ia_css.h"
#include "ia_css_binary.h"

void
prepare_shading_table(const struct ia_css_shading_table *in_table,
		      unsigned int sensor_binning,
		      struct ia_css_shading_table **target_table,
		      const struct ia_css_binary *binary);

struct ia_css_shading_table *
ia_css_shading_table_alloc(unsigned int width, unsigned int height);
	
void
ia_css_shading_table_free(struct ia_css_shading_table *table);

#endif /* _SH_CSS_PARAMS_SHADING_H_ */

