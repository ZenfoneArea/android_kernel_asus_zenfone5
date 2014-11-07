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


#ifndef _PMI_H_
#define _PMI_H_

#include "lwpmudrv_defines.h"
#if defined(DRV_IA64)
#include <asm/perfmon.h>
#endif
#include <linux/ptrace.h>
#include <linux/version.h>
#if defined(DRV_IA64)
#include "cpumon.h"
#endif

#if defined(DRV_IA32)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
#define REGS_xcs(regs)       regs->xcs
#define REGS_eip(regs)       regs->eip
#define REGS_eflags(regs)    regs->eflags
#else
#define REGS_xcs(regs)       regs->cs
#define REGS_eip(regs)       regs->ip
#define REGS_eflags(regs)    regs->flags
#endif
#endif

#if defined(DRV_EM64T)
#define REGS_cs(regs)        regs->cs

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
#define REGS_rip(regs)       regs->rip
#define REGS_eflags(regs)    regs->eflags
#else
#define REGS_rip(regs)       regs->ip
#define REGS_eflags(regs)    regs->flags
#endif
#endif

#if defined(DRV_IA32) || defined(DRV_EM64T)
asmlinkage VOID PMI_Interrupt_Handler(struct pt_regs *regs);
#endif

#if defined(DRV_IA64)
#define REGS_cr_iip(regs)       regs->cr_iip
#define REGS_cr_ipsr(regs)      regs->cr_ipsr

#if defined(PERFMON_V2_ALT)
  VOID PMI_Interrupt_Handler(IN int irq, IN void *arg, IN struct pt_regs *regs);
#elif defined(PERFMON_V2)
  int PMI_Interrupt_Handler(
      IN struct task_struct *task,
      IN void *buf,
      IN pfm_ovfl_arg_t *arg,
      IN struct pt_regs *regs,
      IN unsigned long stamp);
#else
  irqreturn_t PMI_Interrupt_Handler(IN int irq, IN void *arg, IN struct pt_regs *regs);
#endif
#endif 

#endif  
