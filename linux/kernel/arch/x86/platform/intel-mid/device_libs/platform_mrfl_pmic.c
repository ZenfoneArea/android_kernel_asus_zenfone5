/*
 * platform_mrfl_pmic.c: Platform data for Merrifield PMIC driver
 *
 * (C) Copyright 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <linux/init.h>
#include <linux/sfi.h>
#include <asm/intel-mid.h>
#include <asm/pmic_pdata.h>
#include <asm/intel_mid_remoteproc.h>
#include <linux/power/bq24261_charger.h>

#include "platform_ipc.h"
#include "platform_mrfl_pmic.h"

static struct temp_lookup basincove_adc_tbl[] = {
	{0x24, 125, 0}, {0x28, 120, 0},
	{0x2D, 115, 0}, {0x32, 110, 0},
	{0x38, 105, 0}, {0x40, 100, 0},
	{0x48, 95, 0}, {0x51, 90, 0},
	{0x5C, 85, 0}, {0x68, 80, 0},
	{0x77, 75, 0}, {0x87, 70, 0},
	{0x99, 65, 0}, {0xAE, 60, 0},
	{0xC7, 55, 0}, {0xE2, 50, 0},
	{0x101, 45, 0}, {0x123, 40, 0},
	{0x149, 35, 0}, {0x172, 30, 0},
	{0x19F, 25, 0}, {0x1CE, 20, 0},
	{0x200, 15, 0}, {0x233, 10, 0},
	{0x266, 5, 0}, {0x299, 0, 0},
	{0x2CA, -5, 0}, {0x2F9, -10, 0},
	{0x324, -15, 0}, {0x34B, -20, 0},
	{0x36D, -25, 0}, {0x38A, -30, 0},
	{0x3A4, -35, 0}, {0x3B8, -40, 0},
};

static struct temp_lookup shadycove_adc_tbl[] = {
	{0x10, 125, 0}, {0x12, 120, 0},
	{0x14, 115, 0}, {0x17, 110, 0},
	{0x1A, 105, 0}, {0x1D, 100, 0},
	{0x22, 95, 0}, {0x26, 90, 0},
	{0x2C, 85, 0}, {0x33, 80, 0},
	{0x3B, 75, 0}, {0x44, 70, 0},
	{0x4F, 65, 0}, {0x5C, 60, 0},
	{0x6C, 55, 0}, {0x7F, 50, 0},
	{0x97, 45, 0}, {0xB3, 40, 0},
	{0xD5, 35, 0}, {0xFF, 30, 0},
	{0x133, 25, 0}, {0x173, 20, 0},
	{0x1C2, 15, 0}, {0x226, 10, 0},
	{0x2A4, 5, 0}, {0x343, 0, 0},
	{0x410, -5, 0}, {0x519, -10, 0},
	{0x66F, -15, 0}, {0x82F, -20, 0},
	{0xA81, -25, 0}, {0xD99, -30, 0},
	{0x11C6, -35, 0}, {0x1778, -40, 0},
};

void __init *mrfl_pmic_ccsm_platform_data(void *info)
{
	struct sfi_device_table_entry *entry = info;
	static struct pmic_platform_data pmic_pdata;
	struct platform_device *pdev = NULL;
	int ret;

	pdev = platform_device_alloc(entry->name, -1);
	if (!pdev) {
		pr_err("Out of memory for SFI platform dev %s\n", entry->name);
		goto out;
	}
	pdev->dev.platform_data = &pmic_pdata;
	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("Failed to add adc platform device\n");
		platform_device_put(pdev);
		goto out;
	}
	install_irq_resource(pdev, entry->irq);

	if (INTEL_MID_BOARD(1, PHONE, MRFL) ||
			INTEL_MID_BOARD(1, TABLET, MRFL)) {
		pmic_pdata.max_tbl_row_cnt = ARRAY_SIZE(basincove_adc_tbl);
		pmic_pdata.adc_tbl = basincove_adc_tbl;
	} else if (INTEL_MID_BOARD(1, PHONE, MOFD) ||
			INTEL_MID_BOARD(1, TABLET, MOFD)) {
		pmic_pdata.max_tbl_row_cnt = ARRAY_SIZE(shadycove_adc_tbl);
		pmic_pdata.adc_tbl = shadycove_adc_tbl;
	}

#ifdef CONFIG_BQ24261_CHARGER
	pmic_pdata.cc_to_reg = bq24261_cc_to_reg;
	pmic_pdata.cv_to_reg = bq24261_cv_to_reg;
	pmic_pdata.inlmt_to_reg = bq24261_inlmt_to_reg;
#endif
	register_rpmsg_service("rpmsg_pmic_ccsm", RPROC_SCU,
				RP_PMIC_CCSM);
out:
	return &pmic_pdata;
}

