#
# Version: 1.6
#
# Copyright (C) 2008-2012 Intel Corporation.  All Rights Reserved.
#
#     This file is part of SEP Development Kit
#
#     SEP Development Kit is free software; you can redistribute it
#     and/or modify it under the terms of the GNU General Public License
#     version 2 as published by the Free Software Foundation.
#
#     SEP Development Kit is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with SEP Development Kit; if not, write to the Free Software
#     Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#     As a special exception, you may use this file as part of a free software
#     library without restriction.  Specifically, if other files instantiate
#     templates or use macros or inline functions from this file, or you compile
#     this file and link it with other files to produce an executable, this
#     file does not by itself cause the resulting executable to be covered by
#     the GNU General Public License.  This exception does not however
#     invalidate any other reasons why the executable file might be covered by
#     the GNU General Public License.
#

# -------------------- user configurable options ------------------------

# base name of SEP driver
DRIVER_NAME = sep3_10

# location to install driver
INSTALL = .

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
# Example flags are "-Werror", "-Wno-error", etc.
#EXTRA_CFLAGS += -I$(LDDINCDIR) -I$(LDDINCDIR1)
EXTRA_CFLAGS += -I$(M)/../include -I$(M)/inc

# if ARCH variable is set, unset it to avoid conflicts with kbuild
unexport ARCH

# platform details
ifeq ($(CONFIG_X86_64),y)
    MACH=x86_64
else
    MACH=i686
endif
ifeq ($(MACH),x86_64)
PLATFORM=x32_64
BUILD_CHIPSET=YES
endif
ifeq ($(PLATFORM),)
PLATFORM=x32
BUILD_CHIPSET=YES
endif

# eventual filename of SEP driver
DRIVER_MODE=$(DRIVER_NAME)
DRIVER_FILENAME=$(DRIVER_NAME).ko

# By default, build the EMON portions of the driver
# unless building IPF, which EMON doesn't support currently
EMON=YES

# build options ...
ifneq ($(KERNELRELEASE),)
	obj-m := $(DRIVER_NAME).o

ifeq ($(PLATFORM),x32)
	arch-objs :=              \
			core2.o           \
			sys32.o           \
			silvermont.o      \
			pci.o             \
			apic.o            \
			pebs.o
endif
ifeq ($(PLATFORM),x32_64)
	arch-objs :=    core2.o   \
			sys64.o           \
			silvermont.o      \
			pci.o             \
			apic.o            \
			pebs.o
endif

ifeq ($(EMON),YES)
    EXTRA_CFLAGS += -DEMON -DEMON_INTERNAL
endif
EXTRA_CFLAGS += -DDRV_ANDROID -DDRV_ATOM_ONLY

ifeq ($(BUILD_CHIPSET),YES)
    EXTRA_CFLAGS += -DBUILD_CHIPSET
	chipset-objs := pci.o     \
			chap.o    \
			gmch.o
endif

	$(DRIVER_NAME)-objs :=    \
			lwpmudrv.o        \
			control.o         \
			cpumon.o          \
			eventmux.o        \
			linuxos.o         \
			output.o          \
			pmi.o             \
			sys_info.o        \
			utility.o         \
			$(chipset-objs)   \
			$(arch-objs)

# targets ...

# Otherwise, we were called directly from the command
# line, so the kernel build system will be used.
else
	KERNEL_SRC_DIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

all: default

default:
	$(MAKE) -C $(KERNEL_SRC_DIR) M=$(PWD) LDDINCDIR=$(PWD)/../include LDDINCDIR1=$(PWD)/inc modules PWD=$(PWD)
	@(cd pax && make all)

endif

install:
	@cp $(DRIVER_NAME).ko $(INSTALL)/$(DRIVER_FILENAME)
	@echo "Installed $(DRIVER_NAME) driver to $(INSTALL)/$(DRIVER_FILENAME) ."
	@(cd pax && make install INSTALL=$(INSTALL)/pax)

clean:
	rm -f *.o .*.o.cmd .*.o.d .*.ko.cmd .*.ko.unsigned.cmd *.gcno
	rm -f $(DRIVER_NAME).ko $(DRIVER_NAME).ko.unsigned
	rm -f $(DRIVER_MODE)*$(DRIVER_TYPE).ko
	rm -f Module.symvers Modules.symvers *.mod.c modules.order Module.markers
	rm -rf .tmp_versions
	@(cd pax && make clean)

distclean: clean
	rm -f $(DRIVER_NAME)*.ko
	@(cd pax && make distclean)

