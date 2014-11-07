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

#ifndef __IA_CSS_SC_TYPES_H
#define __IA_CSS_SC_TYPES_H

/** Number of color planes in the shading table. */
#define IA_CSS_SC_NUM_COLORS           4

/** The 4 colors that a shading table consists of.
 *  For each color we store a grid of values.
 */
enum ia_css_sc_color {
	IA_CSS_SC_COLOR_GR, /**< Green on a green-red line */
	IA_CSS_SC_COLOR_R,  /**< Red */
	IA_CSS_SC_COLOR_B,  /**< Blue */
	IA_CSS_SC_COLOR_GB  /**< Green on a green-blue line */
};

/** Lens Shading Correction table.
 *
 *  This describes the color shading artefacts
 *  introduced by lens imperfections. To correct artefacts,
 *  bayer values should be multiplied by gains in this table.
 *
 *  ISP block: SC1
 *  ISP1: SC1 is used.
 *  ISP2: SC1 is used.
 */
struct ia_css_shading_table {
	uint32_t enable; /**< Set to false for no shading correction.
		          The data field can be NULL when enable == true */
	uint32_t sensor_width;  /**< Native sensor width in pixels. */
	uint32_t sensor_height; /**< Native sensor height in lines. */
	uint32_t width;  /**< Number of data points per line per color.
				u8.0, [0,81] */
	uint32_t height; /**< Number of lines of data points per color.
				u8.0, [0,61] */
	uint32_t fraction_bits; /**< Bits of fractional part in the data
				points.
				u8.0, [0,13] */
	uint16_t *data[IA_CSS_SC_NUM_COLORS];
	/**< Table data, one array for each color.
	     Use ia_css_sc_color to index this array.
	     u[13-fraction_bits].[fraction_bits], [0,8191] */
};

#endif /* __IA_CSS_SC_TYPES_H */

