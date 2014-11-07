/*
 * linux/drivers/modem_control/mdm_util.c
 *
 * Version 1.0
 *
 * Utilities for modem control driver.
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Contact: Faouaz Tenoutit <faouazx.tenoutit@intel.com>
 *          Frederic Berat <fredericx.berat@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "mdm_util.h"
#include "mcd_mdm.h"
#include "mcd_cpu.h"
#include "mcd_pmic.h"

/* Modem control driver instance */
struct mdm_ctrl *mdm_drv;

/**
 *  mdm_ctrl_set_opened - Set the open device state
 *  @drv: Reference to the driver structure
 *  @value: Value to set
 *
 */
inline void mdm_ctrl_set_opened(struct mdm_ctrl *drv, int value)
{
	/* Set the open flag */
	drv->opened = value;
}

/**
 *  mdm_ctrl_get_opened - Return the device open state
 *  @drv: Reference to the driver structure
 *
 */
inline int mdm_ctrl_get_opened(struct mdm_ctrl *drv)
{
	int opened;

	/* Set the open flag */
	opened = drv->opened;
	return opened;
}

/**
 *  mdm_ctrl_enable_flashing - Set the modem state to FW_DOWNLOAD_READY
 *
 */
void mdm_ctrl_enable_flashing(unsigned long int param)
{
	struct mdm_ctrl *drv = (struct mdm_ctrl *)param;

	del_timer(&drv->flashing_timer);
	if (mdm_ctrl_get_state(drv) != MDM_CTRL_STATE_IPC_READY) {
		mdm_ctrl_set_state(drv, MDM_CTRL_STATE_FW_DOWNLOAD_READY);
	}
}

/**
 *  mdm_ctrl_launch_timer - Timer launcher helper
 *  @timer: Timer to activate
 *  @delay: Timer duration
 *  @timer_type: Timer type
 *
 *  Type can be MDM_TIMER_FLASH_ENABLE.
 *  Note: Type MDM_TIMER_FLASH_DISABLE is not used anymore.
 */
void mdm_ctrl_launch_timer(struct timer_list *timer, int delay,
			   unsigned int timer_type)
{
	timer->data = (unsigned long int)mdm_drv;
	switch (timer_type) {
	case MDM_TIMER_FLASH_ENABLE:
		timer->function = mdm_ctrl_enable_flashing;
		break;
	case MDM_TIMER_FLASH_DISABLE:
	default:
		pr_err(DRVNAME ": Unrecognized timer type %d", timer_type);
		del_timer(timer);
		return;
		break;
	}
	mod_timer(timer, jiffies + msecs_to_jiffies(delay));
}

/**
 *  mdm_ctrl_set_func - Set modem sequences functions to use
 *  @drv: Reference to the driver structure
 *
 */
void mdm_ctrl_set_func(struct mdm_ctrl *drv)
{
	int modem_type = 0;
	int cpu_type = 0;
	int pmic_type = 0;

	modem_type = drv->pdata->mdm_ver;
	cpu_type = drv->pdata->cpu_ver;
	pmic_type = drv->pdata->pmic_ver;

	switch (modem_type) {
	case MODEM_6260:
	case MODEM_6268:
	case MODEM_6360:
	case MODEM_7160:
	case MODEM_7260:
		drv->pdata->mdm.init = mcd_mdm_init;
		drv->pdata->mdm.power_on = mcd_mdm_cold_boot;
		drv->pdata->mdm.warm_reset = mcd_mdm_warm_reset;
		drv->pdata->mdm.power_off = mcd_mdm_power_off;
		drv->pdata->mdm.cleanup = mcd_mdm_cleanup;
		drv->pdata->mdm.get_wflash_delay = mcd_mdm_get_wflash_delay;
		drv->pdata->mdm.get_cflash_delay = mcd_mdm_get_cflash_delay;
		break;
	default:
		pr_info(DRVNAME ": Can't retrieve modem specific functions");
		drv->is_mdm_ctrl_disabled = true;
		break;
	}

	switch (cpu_type) {
	case CPU_PWELL:
	case CPU_CLVIEW:
	case CPU_TANGIER:
	case CPU_VVIEW2:
	case CPU_ANNIEDALE:
	case CPU_CHERRYVIEW:
		drv->pdata->cpu.init = cpu_init_gpio;
		drv->pdata->cpu.cleanup = cpu_cleanup_gpio;
		drv->pdata->cpu.get_mdm_state = get_gpio_mdm_state;
		drv->pdata->cpu.get_irq_cdump = get_gpio_irq_cdump;
		drv->pdata->cpu.get_irq_rst = get_gpio_irq_rst;
		drv->pdata->cpu.get_gpio_rst = get_gpio_rst;
		drv->pdata->cpu.get_gpio_pwr = get_gpio_pwr;
		break;
	default:
		pr_info(DRVNAME ": Can't retrieve cpu specific functions");
		drv->is_mdm_ctrl_disabled = true;
		break;
	}

	switch (pmic_type) {
	case PMIC_MFLD:
	case PMIC_MRFL:
	case PMIC_BYT:
	case PMIC_MOOR:
	case PMIC_CHT:
		drv->pdata->pmic.init = pmic_io_init;
		drv->pdata->pmic.power_on_mdm = pmic_io_power_on_mdm;
		drv->pdata->pmic.power_off_mdm = pmic_io_power_off_mdm;
		drv->pdata->pmic.cleanup = pmic_io_cleanup;
		drv->pdata->pmic.get_early_pwr_on = pmic_io_get_early_pwr_on;
		drv->pdata->pmic.get_early_pwr_off = pmic_io_get_early_pwr_off;
		break;
	case PMIC_CLVT:
		drv->pdata->pmic.init = pmic_io_init;
		drv->pdata->pmic.power_on_mdm = pmic_io_power_on_ctp_mdm;
		drv->pdata->pmic.power_off_mdm = pmic_io_power_off_mdm;
		drv->pdata->pmic.cleanup = pmic_io_cleanup;
		drv->pdata->pmic.get_early_pwr_on = pmic_io_get_early_pwr_on;
		drv->pdata->pmic.get_early_pwr_off = pmic_io_get_early_pwr_off;
		break;
	default:
		pr_info(DRVNAME ": Can't retrieve pmic specific functions");
		drv->is_mdm_ctrl_disabled = true;
		break;
	}
}

/**
 *  mdm_ctrl_set_state -  Effectively sets the modem state on work execution
 *  @state : New state to set
 *
 */
inline void mdm_ctrl_set_state(struct mdm_ctrl *drv, int state)
{
	/* Set the current modem state */
	atomic_set(&drv->modem_state, state);
	if (likely(state != MDM_CTRL_STATE_UNKNOWN) &&
	    (state & drv->polled_states)) {

		drv->polled_state_reached = true;
		/* Waking up the poll work queue */
		wake_up(&drv->wait_wq);
		pr_info(DRVNAME ": Waking up polling 0x%x\r\n", state);
#ifdef CONFIG_HAS_WAKELOCK
		/* Grab the wakelock for 10 ms to avoid
		   the system going to sleep */
		if (drv->opened)
			wake_lock_timeout(&drv->stay_awake, msecs_to_jiffies(10));
#endif

	}
}

/**
 *  mdm_ctrl_get_state - Return the local current modem state
 *  @drv: Reference to the driver structure
 *
 *  Note: Real current state may be different in case of self-reset
 *	  or if user has manually changed the state.
 */
inline int mdm_ctrl_get_state(struct mdm_ctrl *drv)
{
	return atomic_read(&drv->modem_state);
}

/**
 *  modem_ctrl_create_pdata - Create platform data
 *
 *  pdata is created base on information given by platform.
 *  Data used is the modem type, the cpu type and the pmic type.
 */
struct mcd_base_info *modem_ctrl_get_dev_data(struct platform_device *pdev)
{
	struct mcd_base_info *info = NULL;

	if (!pdev->dev.platform_data) {
		pr_info(DRVNAME
			"%s: No platform data available, checking ACPI...",
			__func__);
		/* FOR ACPI HANDLING */
		if (retrieve_modem_platform_data(pdev)) {
			pr_err(DRVNAME
			       "%s: No registered info found. Disabling driver.",
			       __func__);
			return NULL;
		}
	}

	info = pdev->dev.platform_data;

	pr_err(DRVNAME ": cpu: %d mdm: %d pmic: %d.", info->cpu_ver,
	       info->mdm_ver, info->pmic_ver);
	if ((info->mdm_ver == MODEM_UNSUP)
	    || (info->cpu_ver == CPU_UNSUP)
	    || (info->pmic_ver == PMIC_UNSUP)) {
		/* mdm_ctrl is disabled as some components */
		/* of the platform are not supported */
		kfree(info);
		return NULL;
	}

	return info;
}

/**
 *  mdm_ctrl_get_device_info - Create platform and modem data.
 *  @drv: Reference to the driver structure
 *
 *  Platform are build from SFI table data.
 */
void mdm_ctrl_get_device_info(struct mdm_ctrl *drv,
			      struct platform_device *pdev)
{
	drv->pdata = modem_ctrl_get_dev_data(pdev);

	if (!drv->pdata) {
		drv->is_mdm_ctrl_disabled = true;
		pr_info(DRVNAME ": Disabling driver. No known device.");
		goto out;
	}

	mdm_ctrl_set_func(drv);
 out:
	return;
}
