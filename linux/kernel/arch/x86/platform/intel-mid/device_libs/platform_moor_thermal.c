/*
 * platform_moor_thermal.c: Platform data initilization file for
 *			Intel Moorefield Platform thermal driver
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Sumeet R Pawnikar <sumeet.r.pawnikar@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/mfd/intel_msic.h>
#include <linux/platform_device.h>
#include <asm/intel_mid_thermal.h>
#include <asm/intel-mid.h>
#include <asm/intel_mid_remoteproc.h>
#include "platform_moor_thermal.h"

/* 'enum' of Thermal ADC channels */
enum thermal_adc_channels { SYS0, SYS1, SYS2, PMIC_DIE };

static int linear_temp_correlation(void *info, long temp, long *res)
{
	struct intel_mid_thermal_sensor *sensor = info;

	*res = ((temp * sensor->slope) / 1000) + sensor->intercept;

	return 0;
}

/*
 * Naming convention:
 * skin0 -> front skin,
 * skin1--> back skin
 */
/* TODO: Update slope and intercept values as per HW Team.
 * Currentlry keeping these as default one. */
static struct intel_mid_thermal_sensor moor_sensors[] = {
	{
		.name = SYSTHERM0,
		.index = SYS0,
		.slope = 1000,
		.intercept = 0,
		.temp_correlation = linear_temp_correlation,
		.direct = false,
	},
	{
		.name = SYSTHERM1,
		.index = SYS1,
		.slope = 1000,
		.intercept = 0,
		.temp_correlation = linear_temp_correlation,
		.direct = false,
	},
	{
		.name = SYSTHERM2,
		.index = SYS2,
		.slope = 1000,
		.intercept = 0,
		.temp_correlation = linear_temp_correlation,
		.direct = false,
	},
	{
		.name = MSIC_DIE_NAME,
		.index = PMIC_DIE,
		.slope = 1000,
		.intercept = 0,
		.temp_correlation = linear_temp_correlation,
		.direct = true,
	},
};

static struct intel_mid_thermal_platform_data pdata[] = {
	[moor_thermal] = {
		.num_sensors = 4,
		.sensors = moor_sensors,
	},
};

void __init *moor_thermal_platform_data(void *info)
{
	struct platform_device *pdev;
	struct sfi_device_table_entry *entry = info;

	pdev = platform_device_alloc(MOOR_THERM_DEV_NAME, -1);
	if (!pdev) {
		pr_err("out of memory for SFI platform dev %s\n",
			MOOR_THERM_DEV_NAME);
		return NULL;
	}

	if (platform_device_add(pdev)) {
		pr_err("failed to add thermal platform device\n");
		platform_device_put(pdev);
		return NULL;
	}
	pdev->dev.platform_data = &pdata[moor_thermal];

	install_irq_resource(pdev, entry->irq);

	register_rpmsg_service("rpmsg_moor_thermal", RPROC_SCU,
				RP_SCOVE_THERMAL);

	return 0;
}
