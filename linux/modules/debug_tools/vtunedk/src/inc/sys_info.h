/*
    Copyright (C) 2005-2012 Intel Corporation.  All Rights Reserved.
 
    This file is part of SEP Development Kit
 
    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.
 
    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 
    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/


#ifndef _SYS_INFO_H_
#define _SYS_INFO_H_

#include "lwpmudrv_defines.h"

typedef struct __generic_ioctl {
    U32    size;
    S32    ret;
    U64    rsv[3];
} GENERIC_IOCTL;

#define GENERIC_IOCTL_size(gio)     (gio)->size
#define GENERIC_IOCTL_ret(gio)      (gio)->ret

//
// This one is unusual in that it's really a variable
// size. The system_info field is just a easy way
// to access the base information, but the actual size
// when used tends to be much larger that what is 
// shown here.
//
typedef struct __system_info {
    GENERIC_IOCTL gen;
    VTSA_SYS_INFO sys_info;
} IOCTL_SYS_INFO;

#define  IOCTL_SYS_INFO_gen(isi)         (isi)->gen
#define  IOCTL_SYS_INFO_sys_info(isi)    (isi)->sys_info

extern  U32   SYS_INFO_Build (void);
extern  void  SYS_INFO_Transfer (PVOID out_buf, unsigned long out_buf_len);
extern  void  SYS_INFO_Destroy (void);

#if defined(DRV_IA64)
//
// from PAL spec for PAL_LOGICAL_TO_PHYSICAL
//
typedef struct __logical_overview {
    union {
        U64 value;
        struct {
            U16 num_log;    // total number of logical processors
            U8  tpc;        // threads per core
            U8  rv0;        // reserved
            U8  cpp;        // cores per processor die
            U8  rv1;        // reserved
            U8  ppid;       // physical processor die ID
            U8  rv2;        // reserved
        };
    };
} LOGICAL_OVERVIEW, *PLOGICAL_OVERVIEW;

typedef struct __proc_n_log_info1 {
    union {
        U64 value;
        struct {
            U16 tid;        // thread id
            U16 rv0;        // reserved
            U16 cid;        // core id
            U16 rv1;        // reserved
        };
    };
} PROC_N_LOG_INFO1, *PPROC_N_LOG_INFO1;

typedef struct __proc_n_log_info2 {
    union {
        U64 value;
        struct {
            U16 la;         // logical address
            U16 rv0;        // reserved
            U32 rv1;        // reserved
        };
    };
} PROC_N_LOG_INFO2, *PPROC_N_LOG_INFO2;

#define DATA_UNIFIED_CACHE   2

typedef struct __config_info_2 {
    union {
        struct {
            U32 cache_size;
            U8  alias_boundary;
            U8  tag_ls_bit;
            U8  tag_ms_bit;
            U8  reserved;
        };
        U64 data;
    };
} CONFIG_INFO_2, *PCONFIG_INFO_2;
#endif

#endif  
