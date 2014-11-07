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

#ifndef __HOST_VEC_TYPES_H_INCLUDED__
#define __HOST_VEC_TYPES_H_INCLUDED__

#if !defined(__HIVECC) && !defined(__CRUN)
#include "mpmath.h"
#include "bbb_cfg.h"

typedef struct { spudata_t elem[NUM_VEC_ELEMS];}    tvectoru;
typedef struct { spsdata_t elem[NUM_VEC_ELEMS];}    tvector;
typedef struct { spsdata_t elem[NUM_SLICE_ELEMS];}    tvectorslice;
typedef struct { spsdata_t elem[NUM_VEC_ELEMS];}    tflags;

#define __register
#define MEM(a)

#endif


#endif //__HOST_VEC_TYPES_H_INCLUDED__
