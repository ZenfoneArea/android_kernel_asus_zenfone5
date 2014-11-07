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

#ifndef __IA_CSS_HB_HOST_H
#define __IA_CSS_HB_HOST_H

#include "sh_css_params.h"

#include "ia_css_bh_param.h"

void
ia_css_bh_hmem_decode(struct ia_css_3a_rgby_output *out_ptr,
		      hrt_vaddress ddr_ptr);

void
ia_css_bh_encode(struct sh_css_isp_bh_params *to,
		  const struct ia_css_3a_config *from);

void
ia_css_bh_hmem_encode(struct sh_css_isp_bh_hmem_params *to,
		  const struct ia_css_3a_config *from);

#endif /* __IA_CSS_HB_HOST_H */
