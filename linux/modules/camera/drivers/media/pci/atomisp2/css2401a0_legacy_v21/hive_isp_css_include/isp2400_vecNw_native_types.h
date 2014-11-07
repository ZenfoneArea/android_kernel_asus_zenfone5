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

#ifndef __ISP2400_VECNW_NATIVE_TYPES_H_INCLUDED__
#define __ISP2400_VECNW_NATIVE_TYPES_H_INCLUDED__

/*
 * Defines only the types of the MP and SP fragments
 * as seen on the host and/or scalar memory
 *
 * "mp_fragments" are used to create both vector and
 * scalar multi precision (arithmetic) data.
 *
 * "sp_fragments" are used to create both vector and
 * scalar single precision (control) data, such as
 * flags, shift values and addresses.
 *
 * In both cases it is assumed that the scalar DMEM
 * (standard C) type is larger than the vector data path
 */

/* 
 * Multi-precision native types declaration
 * Version 0.1
 */

#define MP_FRAGMENT_BYTES   2
#define SP_FRAGMENT_BYTES   2  

typedef unsigned int        addr_t;

#if MP_FRAGMENT_BYTES==2
    typedef unsigned short  mp_fragment_t;
    typedef unsigned short  mp_fragment_u_t;
    typedef short           mp_fragment_s_t;
#elif MP_FRAGMENT_BYTES==4
    typedef unsigned int    mp_fragment_t;
    typedef unsigned int    mp_fragment_u_t;
    typedef int             mp_fragment_s_t;
#else
#   error "MP_FRAGMENT_BYTES must be in {2,4}"
#endif

#if SP_FRAGMENT_BYTES==2
    typedef unsigned short  sp_fragment_t;
    typedef unsigned short  sp_fragment_u_t;
    typedef short           sp_fragment_s_t;
#elif SP_FRAGMENT_BYTES==4
    typedef unsigned int    sp_fragment_t;
    typedef unsigned int    sp_fragment_u_t;
    typedef int             sp_fragment_s_t;
#else
#   error "SP_FRAGMENT_BYTES must be in {2,4}"
#endif

#define MAX_FRAGMENT_BITDEPTH   (MP_FRAGMENT_BYTES*8)


#endif /* __ISP2400_VECNW_NATIVE_TYPES_H_INCLUDED__ */
 
