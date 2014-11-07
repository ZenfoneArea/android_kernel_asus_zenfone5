# This makefile is included from vendor/intel/common/AndroidBoard.mk.

# Force using bash as a shell, otherwise, on Ubuntu, dash will break some
# dependency due to its bad handling of echo \1
MAKE += SHELL=/bin/bash

.PHONY: menuconfig xconfig gconfig get_kernel_from_source
.PHONY: build_bzImage copy_modules_to_root
# This rule is useful for creating a kernel that will be
# shared with a tree that does not have kernel source.
make_kernel_tarball: get_kernel_from_source bootimage
	@echo Building kernel tarball: $(TARGET_KERNEL_TARBALL)
	@rm -rf $(PRODUCT_OUT)/kerneltarball
	@mkdir -p $(PRODUCT_OUT)/kerneltarball/root/lib/modules
	@cp $(PRODUCT_OUT)/root/lib/modules/* $(PRODUCT_OUT)/kerneltarball/lib/modules
	@cp $(PRODUCT_OUT)/bzImage $(PRODUCT_OUT)/kerneltarball/
	tar cvzf $(TARGET_KERNEL_TARBALL) -C $(PRODUCT_OUT)/kerneltarball bzImage root/lib/modules

KERNEL_SOC_medfield := mfld
KERNEL_SOC_clovertrail := ctp
KERNEL_SOC_merrifield := mrfl
KERNEL_SOC_baytrail := byt
KERNEL_SOC_cherrytrail := cht
KERNEL_SOC_moorefield := moor
KERNEL_SOC_morganfield := morg
KERNEL_SOC_carboncanyon := crc

KERNEL_SOC := $(KERNEL_SOC_$(TARGET_BOARD_PLATFORM))

ifeq ($(KERNEL_SOC),)
$(error cannot build kernel, TARGET_BOARD_PLATFORM is not defined)
endif

ifeq ($(BOARD_USE_64BIT_KERNEL),true)
KERNEL_ARCH := x86_64
else
KERNEL_ARCH := i386
endif
KERNEL_EXTRA_FLAGS := ANDROID_TOOLCHAIN_FLAGS=-mno-android
KERNEL_CROSS_COMP := $(notdir $(TARGET_TOOLS_PREFIX))

KERNEL_CCACHE :=$(firstword $(TARGET_CC))
KERNEL_PATH := $(ANDROID_BUILD_TOP)/vendor/intel/support
ifeq ($(notdir $(KERNEL_CCACHE)),ccache)
KERNEL_CROSS_COMP := "ccache $(KERNEL_CROSS_COMP)"
KERNEL_PATH := $(KERNEL_PATH):$(ANDROID_BUILD_TOP)/$(dir $(KERNEL_CCACHE))
endif

#remove time_macros from ccache options, it breaks signing process
KERNEL_CCSLOP := $(filter-out time_macros,$(subst $(comma), ,$(CCACHE_SLOPPINESS)))
KERNEL_CCSLOP := $(subst $(space),$(comma),$(KERNEL_CCSLOP))

KERNEL_OUT_DIR := $(PRODUCT_OUT)/linux/kernel
KERNEL_OUT_DIR_KDUMP := $(PRODUCT_OUT)/linux/kdump
KERNEL_MODINSTALL := modules_install
KERNEL_OUT_MODINSTALL := $(KERNEL_OUT_DIR)/$(KERNEL_MODINSTALL)
KERNEL_MODULES_ROOT := $(PRODUCT_OUT)/root/lib/modules
KERNEL_CONFIG := $(KERNEL_OUT_DIR)/.config
KERNEL_CONFIG_KDUMP := $(KERNEL_OUT_DIR_KDUMP)/.config
KERNEL_BLD_FLAGS := \
    ARCH=$(KERNEL_ARCH) \
    INSTALL_MOD_PATH=$(KERNEL_MODINSTALL) \
    INSTALL_MOD_STRIP=1 \
    DEPMOD=$(shell which true) \
    $(KERNEL_EXTRA_FLAGS)

KERNEL_BLD_FLAGS_KDUMP := $(KERNEL_BLD_FLAGS) \
    O=../../$(KERNEL_OUT_DIR_KDUMP) \

KERNEL_BLD_FLAGS :=$(KERNEL_BLD_FLAGS) \
     O=../../$(KERNEL_OUT_DIR) \

KERNEL_BLD_ENV := CROSS_COMPILE=$(KERNEL_CROSS_COMP) \
    PATH=$(KERNEL_PATH):$(PATH) \
    CCACHE_SLOPPINESS=$(KERNEL_CCSLOP)
KERNEL_FAKE_DEPMOD := $(KERNEL_OUT_DIR)/fakedepmod/lib/modules

KERNEL_DEFCONFIG := $(KERNEL_SRC_DIR)/arch/x86/configs/$(KERNEL_ARCH)_$(KERNEL_SOC)_defconfig
KERNEL_DEFCONFIG_KDUMP := $(KERNEL_DEFCONFIG)
KERNEL_DIFFCONFIG ?= $(TARGET_DEVICE_DIR)/$(TARGET_DEVICE)_diffconfig
KERNEL_VERSION_FILE := $(KERNEL_OUT_DIR)/include/config/kernel.release
KERNEL_VERSION_FILE_KDUMP := $(KERNEL_OUT_DIR_KDUMP)/include/config/kernel.release

$(KERNEL_CONFIG): $(KERNEL_DEFCONFIG) $(wildcard $(KERNEL_DIFFCONFIG))
	@echo PATH $(PATH)
	@echo KERNEL_PATH $(KERNEL_PATH)
	@echo KERNEL_BLD_ENV $(KERNEL_BLD_ENV)	
	@echo Regenerating kernel config $(KERNEL_OUT_DIR)
	@mkdir -p $(KERNEL_OUT_DIR)
	@cat $^ > $@
	@$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) defoldconfig

$(KERNEL_CONFIG_KDUMP): $(KERNEL_DEFCONFIG_KDUMP) $(wildcard $(KERNEL_DIFFCONFIG))
	@echo Regenerating kdump kernel config $(KERNEL_OUT_DIR_KDUMP)
	@mkdir -p $(KERNEL_OUT_DIR_KDUMP)
	@cat $^ > $@
	@$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS_KDUMP) defoldconfig

build_bzImage: $(KERNEL_CONFIG) openssl $(MINIGZIP)
	@$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS)
	@cp -f $(KERNEL_OUT_DIR)/arch/x86/boot/bzImage $(PRODUCT_OUT)/kernel

build_bzImage_kdump: $(KERNEL_CONFIG_KDUMP) openssl $(MINIGZIP)
	@echo Building the kdump bzimage
	@$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS_KDUMP)
	@cp -f $(KERNEL_OUT_DIR_KDUMP)/arch/x86/boot/bzImage $(PRODUCT_OUT)/kdumpbzImage

modules_install: build_bzImage
	@mkdir -p $(KERNEL_OUT_MODINSTALL)
	@$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) modules_install

clean_kernel:
	@$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) clean

#need to do this to have a modules.dep correctly set.
#it is not optimized (copying all modules for each rebuild) but better than kernel-build.sh
#fake depmod with a symbolic link to have /lib/modules/$(version_tag)/xxxx.ko
copy_modules_to_root: modules_install
	@$(RM) -rf $(KERNEL_MODULES_ROOT)
	@mkdir -p $(KERNEL_MODULES_ROOT)
	@find $(KERNEL_OUT_MODINSTALL)/lib/modules/`cat $(KERNEL_VERSION_FILE)` -name "*.ko" -exec cp -f {} $(KERNEL_MODULES_ROOT)/ \;
	@mkdir -p $(KERNEL_FAKE_DEPMOD)
	@echo "  DEPMOD `cat $(KERNEL_VERSION_FILE)`"
	@ln -fns $(ANDROID_BUILD_TOP)/$(KERNEL_MODULES_ROOT) $(KERNEL_FAKE_DEPMOD)/`cat $(KERNEL_VERSION_FILE)`
	@/sbin/depmod -b $(KERNEL_OUT_DIR)/fakedepmod `cat $(KERNEL_VERSION_FILE)`

get_kernel_from_source: copy_modules_to_root

#ramdisk depends on kernel modules
$(PRODUCT_OUT)/ramdisk.img: copy_modules_to_root

menuconfig xconfig gconfig: $(KERNEL_CONFIG)
	@$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) $@
ifeq ($(wildcard $(KERNEL_DIFFCONFIG)),)
	@cp -f $(KERNEL_CONFIG) $(KERNEL_DEFCONFIG)
	@echo ===========
	@echo $(KERNEL_DEFCONFIG) has been modified !
	@echo ===========
else
	@./$(KERNEL_SRC_DIR)/scripts/diffconfig -m $(KERNEL_DEFCONFIG) $(KERNEL_CONFIG) > $(KERNEL_DIFFCONFIG)
	@echo ===========
	@echo $(KERNEL_DIFFCONFIG) has been modified !
	@echo ===========
endif

TAGS_files := TAGS
tags_files := tags
gtags_files := GTAGS GPATH GRTAGS GSYMS
cscope_files := $(addprefix cscope.,files out out.in out.po)

TAGS tags gtags cscope: $(KERNEL_CONFIG)
	@$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) $@
	@rm -f $(KERNEL_SRC_DIR)/$($@_files)
	@cp -fs $(addprefix `pwd`/$(KERNEL_OUT_DIR)/,$($@_files)) $(KERNEL_SRC_DIR)/


#used to build out-of-tree kernel modules
#$(1) is source path relative Android top, $(2) is module name
#$(3) is extra flags

define build_kernel_module
.PHONY: $(2)

$(2): build_bzImage
	@echo Building kernel module $(2) in $(1)
	@mkdir -p $(KERNEL_OUT_DIR)/../../$(1)
	@+$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) M=../../$(1) $(3)

$(2)_install: $(2)
	@+$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) M=../../$(1) $(3) modules_install

$(2)_clean:
	@echo Cleaning kernel module $(2) in $(1)
	@$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) M=../../$(1) clean

$(addprefix $(2)_,TAGS tags gtags cscope): $(KERNEL_CONFIG)
	@$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) M=../../$(1) $$(subst $(2)_,,$$@)
	@rm -f $(1)/$$($$(subst $(2)_,,$$@)_files)
	@cp -fs $$(addprefix `pwd`/$(KERNEL_OUT_DIR)/,$$($$(subst $(2)_,,$$@)_files)) $(1)/

copy_modules_to_root: $(2)_install

clean_kernel: $(2)_clean

endef

