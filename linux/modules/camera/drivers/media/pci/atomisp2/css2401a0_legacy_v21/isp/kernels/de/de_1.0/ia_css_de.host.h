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

#ifndef __IA_CSS_DE_HOST_H
#define __IA_CSS_DE_HOST_H

#include "ia_css_de_types.h"
#include "ia_css_de_param.h"

extern const struct ia_css_de_config default_de_config;

void
ia_css_de_encode(struct sh_css_isp_de_params *to,
		 const struct ia_css_de_config *from);

void
ia_css_de_dump(const struct sh_css_isp_de_params *de, unsigned level);

void
ia_css_de_debug_dtrace(const struct ia_css_de_config *config, unsigned level);

#endif /* __IA_CSS_DE_HOST_H */
