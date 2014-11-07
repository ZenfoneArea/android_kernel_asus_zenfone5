/*
 * Copyright (C) 2013 Intel Corp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/efi.h>
#include <linux/ucs2_string.h>
#include <linux/nls.h>

#include "reboot_target.h"

static struct efivar_entry *uefi_get_var_entry(wchar_t *varname)
{
	struct efivar_entry *entry;

	efivar_entry_iter_begin();
	entry = efivar_entry_find(varname, EFI_GLOBAL_VARIABLE_GUID,
				  &efivar_sysfs_list, false);
	efivar_entry_iter_end();

	return entry;
}

static void uefi_string_to_varname(const char *name, int size, wchar_t *varname)
{
	utf8s_to_utf16s(name, size, UTF16_LITTLE_ENDIAN, varname,
			size * sizeof(*varname) / sizeof(*name));
	varname[size - 1] = 0;
}

static const efi_guid_t LOADER_GUID =
	EFI_GUID(0x4a67b082, 0x0a4c, 0x41cf, 0xb6, 0xc7, 0x44, 0x0b, 0x29, 0xbb, 0x8c, 0x4f);
static const char TARGET_VARNAME[] = "LoaderEntryOneShot";

static int uefi_set_loader_entry_one_shot(const char *name)
{
	wchar_t varname[sizeof(TARGET_VARNAME)];
	u32 attributes = EFI_VARIABLE_NON_VOLATILE
		| EFI_VARIABLE_BOOTSERVICE_ACCESS
		| EFI_VARIABLE_RUNTIME_ACCESS;
	wchar_t name16[strlen(name) + 1];

	uefi_string_to_varname(TARGET_VARNAME, sizeof(TARGET_VARNAME), varname);
	uefi_string_to_varname(name, strlen(name) + 1, name16);

	return efivar_entry_set_safe(varname, LOADER_GUID, attributes, true,
				     sizeof(name16), name16);
}

static const char RESCUE_MODE_TARGET[]		     = "dnx";
static const char OS_INDICATIONS_SUPPORTED_VARNAME[] = "OsIndicationsSupported";
static const char OS_INDICATIONS_VARNAME[] 	     = "OsIndications";
static const u64  EFI_OS_INDICATION_RESCUE_MODE      = 1 << 5;

static int uefi_read_u64_entry(struct efivar_entry *entry, u64 *value,
			       u32 *attributes)
{
	unsigned long size;
	int ret;

	ret = efivar_entry_size(entry, &size);
	if (ret || size != sizeof(*value))
		return ret ? ret : -EINVAL;

	ret = efivar_entry_get(entry, attributes, &size, value);
	if (ret || size != sizeof(*value))
		return ret ? ret : -EINVAL;

	return 0;
}

static bool uefi_is_os_indication_supported(const u64 os_indication)
{
	wchar_t varname[sizeof(OS_INDICATIONS_SUPPORTED_VARNAME)];
	struct efivar_entry *entry;
	u64 value;
	u32 attributes;
	int ret;

	uefi_string_to_varname(OS_INDICATIONS_SUPPORTED_VARNAME,
			       sizeof(OS_INDICATIONS_SUPPORTED_VARNAME),
			       varname);

	entry = uefi_get_var_entry(varname);
	if (!entry) {
		pr_err("%s: %s EFI variable not available\n",
		       __func__, OS_INDICATIONS_SUPPORTED_VARNAME);
		return false;
	}

	ret = uefi_read_u64_entry(entry, &value, &attributes);
	if (ret)
		pr_err("%s: Failed to read %s EFI variable, return=%d\n",
		       __func__, OS_INDICATIONS_SUPPORTED_VARNAME, ret);

	return ret ? false : !!(value & os_indication);
}

static int uefi_ask_for_rescue_mode(void)
{
	wchar_t varname[sizeof(OS_INDICATIONS_VARNAME)];
	struct efivar_entry *entry;
	u64 value = EFI_OS_INDICATION_RESCUE_MODE;
	u32 attributes = EFI_VARIABLE_NON_VOLATILE
		| EFI_VARIABLE_BOOTSERVICE_ACCESS
		| EFI_VARIABLE_RUNTIME_ACCESS;
	int ret;

	if (!uefi_is_os_indication_supported(EFI_OS_INDICATION_RESCUE_MODE)) {
		pr_err("%s: Rescue mode OS indication is not supported\n",
		       __func__);
		return -ENODEV;
	}

	uefi_string_to_varname(OS_INDICATIONS_VARNAME,
			       sizeof(OS_INDICATIONS_VARNAME), varname);

	entry = uefi_get_var_entry(varname);
	if (!entry)
		return efivar_entry_set_safe(varname, EFI_GLOBAL_VARIABLE_GUID,
					     attributes, true, sizeof(value),
					     (void *)&value);

	ret = uefi_read_u64_entry(entry, &value, &attributes);
	if (ret) {
		pr_err("%s: Failed to read %s EFI variable, return=%d\n",
		       __func__, OS_INDICATIONS_VARNAME, ret);
		return ret;
	}

	value |= EFI_OS_INDICATION_RESCUE_MODE;
	return efivar_entry_set(entry, attributes, sizeof(u64), &value, NULL);
}

static const u16 ANDROID_LOAD_OPTION_MASK = 1 << 8;

static int uefi_set_reboot_target(const char *name, const int id)
{
	if (strcmp(name, RESCUE_MODE_TARGET) == 0)
		return uefi_ask_for_rescue_mode();

	return uefi_set_loader_entry_one_shot(name);
}

struct reboot_target reboot_target_uefi = {
	.set_reboot_target = uefi_set_reboot_target,
};

static int reboot_target_uefi_probe(struct platform_device *pdev)
{
	return reboot_target_register(&reboot_target_uefi);
}

static int reboot_target_uefi_remove(struct platform_device *pdev)
{
	return reboot_target_unregister(&reboot_target_uefi);
}

struct platform_driver reboot_target_uefi_driver = {
	.probe = reboot_target_uefi_probe,
	.remove = reboot_target_uefi_remove,
	.driver.name = KBUILD_MODNAME,
	.driver.owner = THIS_MODULE,
};

static struct platform_device *uefi_pdev;

static int __init reboot_target_uefi_init(void)
{
	if (efi_enabled(EFI_BOOT) && efi_enabled(EFI_RUNTIME_SERVICES)) {
		 uefi_pdev = platform_device_register_simple(KBUILD_MODNAME, -1,
							     NULL, 0);
		 if (IS_ERR(uefi_pdev))
			 return PTR_ERR(uefi_pdev);
	} else
		return -ENODEV;

	return platform_driver_register(&reboot_target_uefi_driver);
}

static void __exit reboot_target_uefi_exit(void)
{
	platform_device_unregister(uefi_pdev);
	platform_driver_unregister(&reboot_target_uefi_driver);
}

module_init(reboot_target_uefi_init);
module_exit(reboot_target_uefi_exit);

MODULE_AUTHOR("Jeremy Compostella <jeremy.compostella@intel.com>");
MODULE_DESCRIPTION("Intel Reboot Target UEFI implementation");
MODULE_LICENSE("GPL v2");
