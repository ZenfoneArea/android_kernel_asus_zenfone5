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

#ifndef __IA_CSS_SC_HOST_H
#define __IA_CSS_SC_HOST_H

#include "sh_css_params.h"

#include "ia_css_sc_types.h"
#include "ia_css_sc_param.h"

void
ia_css_sc_encode(struct sh_css_isp_sc_params *to,
		 struct ia_css_shading_table **from);

void
ia_css_sc_dump(const struct sh_css_isp_sc_params *sc, unsigned level);

#endif /* __IA_CSS_SC_HOST_H */
