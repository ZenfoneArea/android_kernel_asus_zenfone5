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

#ifndef __ISP2400_VEC1W_H_INCLUDED__
#define __ISP2400_VEC1W_H_INCLUDED__

/*
 * This file is part of the Multi-precision vector operations exstension package of the HiveFlex isp2400 series.
 * The accompanying manaul describes the data-types, multi-precision operations and multi-precision overloading possibilities.
 */
 
/* 
 * Single-precision vector operations
 * Version 0.2
 */
 
#include "storage_class.h"

/* The vector native types definition */ 
#include "isp2400_vecNw_native_types.h"

#ifndef __INLINE_ISP2400_VEC1W__
#define STORAGE_CLASS_ISP2400_VEC1W_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_ISP2400_VEC1W_C 
#include "isp2400_vec1w_public.h"
#else  /* __INLINE_VECTOR_FUNC__ */
#define STORAGE_CLASS_ISP2400_VEC1W_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_ISP2400_VEC1W_C STORAGE_CLASS_INLINE
#include "isp2400_vec1w_private.h"
#endif /* __INLINE_ISP2400_VEC1W__ */

#endif /* __ISP2400_VEC1W_H_INCLUDED__ */
