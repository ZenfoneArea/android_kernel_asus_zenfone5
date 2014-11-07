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

#ifndef __IA_CSS_FRAME_COMM_H__
#define __IA_CSS_FRAME_COMM_H__
#include "type_support.h"
#include "platform_support.h"
#include <system_types.h>	 /* hrt_vaddress */

#define SH_CSS_NUM_FRAME_IDS (14)

/*
 * These structs are derived from structs defined in ia_css_types.h
 * (just take out the "_sp" from the struct name to get the "original")
 * All the fields that are not needed by the SP are removed.
 */
struct ia_css_frame_sp_plane {
	unsigned int offset;	/* offset in bytes to start of frame data */
				/* offset is wrt data in sh_css_sp_sp_frame */
};

struct ia_css_frame_sp_binary_plane {
	unsigned int size;
	struct ia_css_frame_sp_plane data;
};

struct ia_css_frame_sp_yuv_planes {
	struct ia_css_frame_sp_plane y;
	struct ia_css_frame_sp_plane u;
	struct ia_css_frame_sp_plane v;
};

struct ia_css_frame_sp_nv_planes {
	struct ia_css_frame_sp_plane y;
	struct ia_css_frame_sp_plane uv;
};

struct ia_css_frame_sp_rgb_planes {
	struct ia_css_frame_sp_plane r;
	struct ia_css_frame_sp_plane g;
	struct ia_css_frame_sp_plane b;
};

struct ia_css_frame_sp_plane6 {
	struct ia_css_frame_sp_plane r;
	struct ia_css_frame_sp_plane r_at_b;
	struct ia_css_frame_sp_plane gr;
	struct ia_css_frame_sp_plane gb;
	struct ia_css_frame_sp_plane b;
	struct ia_css_frame_sp_plane b_at_r;
};


/*
 * Frame info struct. This describes the contents of an image frame buffer.
 */
struct ia_css_frame_sp_info {
	uint16_t width;		/* width of valid data in pixels */
	uint16_t height;		/* Height of valid data in lines */
	uint16_t padded_width;		/* stride of line in memory
					(in pixels) */
	unsigned char format;		/* format of the frame data */
	unsigned char raw_bit_depth;	/* number of valid bits per pixel,
					only valid for RAW bayer frames */
	unsigned char raw_bayer_order;	/* bayer order, only valid
					for RAW bayer frames */
	unsigned char padding;
};


struct ia_css_frame_sp {
	struct ia_css_frame_sp_info info;
	union {
		struct ia_css_frame_sp_plane raw;
		struct ia_css_frame_sp_plane rgb;
		struct ia_css_frame_sp_rgb_planes planar_rgb;
		struct ia_css_frame_sp_plane yuyv;
		struct ia_css_frame_sp_yuv_planes yuv;
		struct ia_css_frame_sp_nv_planes nv;
		struct ia_css_frame_sp_plane6 plane6;
		struct ia_css_frame_sp_binary_plane binary;
	} planes;
};

struct ia_css_frames_sp {
	struct ia_css_frame_sp	in;
	struct ia_css_frame_sp	out;
	struct ia_css_resolution effective_in_res;
	struct ia_css_frame_sp	out_vf;
	struct ia_css_frame_sp	ref_in;
	/* ref_out_frame is same as ref_in_frame */
	struct ia_css_frame_sp	tnr_in;
	/* trn_out_frame is same as tnr_in_frame */
	struct ia_css_frame_sp_info internal_frame_info;
	hrt_vaddress static_frame_data[SH_CSS_NUM_FRAME_IDS];
};

#endif /*__IA_CSS_FRAME_COMM_H__*/

