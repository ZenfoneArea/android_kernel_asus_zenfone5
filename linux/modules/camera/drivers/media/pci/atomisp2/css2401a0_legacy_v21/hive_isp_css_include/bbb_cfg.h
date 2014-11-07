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

#ifndef __HOST_BBB_CFG_H_INCLUDED__
#define __HOST_BBB_CFG_H_INCLUDED__

#define USE2400
//#define USE2600


/* consider to use same naming as cfg.dat from the SDK */
/* N --> number of lanes per vector
 * B --> bit depth
 * M --> lanes per slice
 * W --> bit depth multiplier (1w = single pre 2w = double
  */


#define NUM_VEC_ELEMS 1


#ifdef USE2400
#define NUM_BITS 14
#define NUM_SLICE_ELEMS 4
#define ROUNDMODE           ROUND_NEAREST_EVEN

#elif USE2600

#define NUM_BITS 16
#define NUM_SLICE_ELEMS 8
#define ROUNDMODE           ROUND_NEAREST_EVEN

#else
#error "unsupported system"
#endif


#endif //__HOST_BBB_CFG_H_INCLUDED__
