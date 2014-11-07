/**
 * linux/modules/drivers/modem_control/mdm_ctrl.c
 *
 * Version 1.0
 *
 * This code allows to power and reset IMC modems.
 * There is a list of commands available in include/linux/mdm_ctrl.h
 * Current version supports the following modems :
 * - IMC6260
 * - IMC6360
 * - IMC7160
 * - IMC7260
 * There is no guarantee for other modems
 *
 *
 * Intel Mobile driver for modem powering.
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#define MDM_BOOT_DEVNAME	CONFIG_MDM_CTRL_DEV_NAME

#define MDM_MODEM_READY_DELAY	60	/* Modem readiness wait duration (sec) */

extern void serial_hsu_set_rts_fixed(bool);
/**
 *  mdm_ctrl_handle_hangup - This function handle the modem reset/coredump
 *  @work: a reference to work queue element
 *
 */
static void mdm_ctrl_handle_hangup(struct work_struct *work)
{
	struct mdm_ctrl *drv = mdm_drv;
	int modem_rst;

	/* Check the hangup reason */
	modem_rst = drv->hangup_causes;

	if (modem_rst & MDM_CTRL_HU_RESET)
		mdm_ctrl_set_state(drv, MDM_CTRL_STATE_WARM_BOOT);

	if (modem_rst & MDM_CTRL_HU_COREDUMP)
		mdm_ctrl_set_state(drv, MDM_CTRL_STATE_COREDUMP);

	pr_info(DRVNAME ": %s (reasons: 0x%X)\n", __func__, drv->hangup_causes);
}

/*****************************************************************************
 *
 * Local driver functions
 *
 ****************************************************************************/

static int mdm_ctrl_cold_boot(struct mdm_ctrl *drv)
{
	unsigned long flags;

	struct mdm_ops *mdm = &drv->pdata->mdm;
	struct cpu_ops *cpu = &drv->pdata->cpu;
	struct pmic_ops *pmic = &drv->pdata->pmic;

	void *mdm_data = drv->pdata->modem_data;
	void *cpu_data = drv->pdata->cpu_data;
	void *pmic_data = drv->pdata->pmic_data;

	int rst, pwr_on, cflash_delay;

	pr_warn(DRVNAME ": Cold boot requested");

	/* Set the current modem state */
	mdm_ctrl_set_state(drv, MDM_CTRL_STATE_COLD_BOOT);

	/* AP request => just ignore the modem reset */
	atomic_set(&drv->rst_ongoing, 1);

	rst = cpu->get_gpio_rst(cpu_data);
	pwr_on = cpu->get_gpio_pwr(cpu_data);
	cflash_delay = mdm->get_cflash_delay(mdm_data);
	pmic->power_on_mdm(pmic_data);
	mdm->power_on(mdm_data, rst, pwr_on);

	mdm_ctrl_launch_timer(&drv->flashing_timer,
			      cflash_delay, MDM_TIMER_FLASH_ENABLE);
	return 0;
}

static int mdm_ctrl_normal_warm_reset(struct mdm_ctrl *drv)
{
	unsigned long flags;

	struct mdm_ops *mdm = &drv->pdata->mdm;
	struct cpu_ops *cpu = &drv->pdata->cpu;
	struct pmic_ops *pmic = &drv->pdata->pmic;

	void *mdm_data = drv->pdata->modem_data;
	void *cpu_data = drv->pdata->cpu_data;
	void *pmic_data = drv->pdata->pmic_data;

	int rst, wflash_delay;

	pr_info(DRVNAME ": Normal warm reset requested\r\n");

	/* AP requested reset => just ignore */
	atomic_set(&drv->rst_ongoing, 1);

	/* Set the current modem state */
	mdm_ctrl_set_state(drv, MDM_CTRL_STATE_WARM_BOOT);

	rst = cpu->get_gpio_rst(cpu_data);
	wflash_delay = mdm->get_wflash_delay(mdm_data);
	mdm->warm_reset(mdm_data, rst);

	mdm_ctrl_launch_timer(&drv->flashing_timer,
			      wflash_delay, MDM_TIMER_FLASH_ENABLE);

	return 0;
}

static int mdm_ctrl_flashing_warm_reset(struct mdm_ctrl *drv)
{
	unsigned long flags;

	struct mdm_ops *mdm = &drv->pdata->mdm;
	struct cpu_ops *cpu = &drv->pdata->cpu;
	struct pmic_ops *pmic = &drv->pdata->pmic;

	void *mdm_data = drv->pdata->modem_data;
	void *cpu_data = drv->pdata->cpu_data;
	void *pmic_data = drv->pdata->pmic_data;

	int rst, wflash_delay;

	pr_info(DRVNAME ": Flashing warm reset requested");

	/* AP requested reset => just ignore */
	atomic_set(&drv->rst_ongoing, 1);

	/* Set the current modem state */
	mdm_ctrl_set_state(drv, MDM_CTRL_STATE_WARM_BOOT);

	rst = cpu->get_gpio_rst(cpu_data);
	wflash_delay = mdm->get_wflash_delay(mdm_data);
	mdm->warm_reset(mdm_data, rst);

	msleep(wflash_delay);

	return 0;
}

static int mdm_ctrl_power_off(struct mdm_ctrl *drv)
{
	unsigned long flags;

	struct mdm_ops *mdm = &drv->pdata->mdm;
	struct cpu_ops *cpu = &drv->pdata->cpu;
	struct pmic_ops *pmic = &drv->pdata->pmic;

	void *mdm_data = drv->pdata->modem_data;
	void *cpu_data = drv->pdata->cpu_data;
	void *pmic_data = drv->pdata->pmic_data;

	int rst;

	pr_info(DRVNAME ": Power OFF requested");

	/* AP requested reset => just ignore */
	atomic_set(&drv->rst_ongoing, 1);

	/* Set the modem state to OFF */
	mdm_ctrl_set_state(drv, MDM_CTRL_STATE_OFF);

	rst = cpu->get_gpio_rst(cpu_data);
	mdm->power_off(mdm_data, rst);
	pmic->power_off_mdm(pmic_data);

	return 0;
}

static int mdm_ctrl_cold_reset(struct mdm_ctrl *drv)
{
	pr_warn(DRVNAME ": Cold reset requested");

	serial_hsu_set_rts_fixed(true);

	mdm_ctrl_power_off(drv);
	mdm_ctrl_cold_boot(drv);

	return 0;
}

/**
 *  mdm_ctrl_coredump_it - Modem has signaled a core dump
 *  @irq: IRQ number
 *  @data: mdm_ctrl driver reference
 *
 *  Schedule a work to handle CORE_DUMP depending on current modem state.
 */
static irqreturn_t mdm_ctrl_coredump_it(int irq, void *data)
{
	struct mdm_ctrl *drv = data;

	pr_err(DRVNAME ": CORE_DUMP it");

	/* Ignoring event if we are in OFF state. */
	if (mdm_ctrl_get_state(drv) == MDM_CTRL_STATE_OFF) {
		pr_err(DRVNAME ": CORE_DUMP while OFF\r\n");
		goto out;
	}

	/* Ignoring if Modem reset is ongoing. */
	if (atomic_read(&drv->rst_ongoing) == 1) {
		pr_err(DRVNAME ": CORE_DUMP while Modem Reset is ongoing\r\n");
		goto out;
	}

	/* Set the reason & launch the work to handle the hangup */
	drv->hangup_causes |= MDM_CTRL_HU_COREDUMP;
	queue_work(drv->hu_wq, &drv->hangup_work);

 out:
	return IRQ_HANDLED;
}

/**
 *  mdm_ctrl_reset_it - Modem has changed reset state
 *  @irq: IRQ number
 *  @data: mdm_ctrl driver reference
 *
 *  Change current state and schedule work to handle unexpected resets.
 */
static irqreturn_t mdm_ctrl_reset_it(int irq, void *data)
{
	int value, reset_ongoing;
	struct mdm_ctrl *drv = data;
	unsigned long flags;

	value = drv->pdata->cpu.get_mdm_state(drv->pdata->cpu_data);

	/* Ignoring event if we are in OFF state. */
	if (mdm_ctrl_get_state(drv) == MDM_CTRL_STATE_OFF) {
		/* Logging event in order to minimise risk of hidding bug */
		pr_err(DRVNAME ": RESET_OUT 0x%x while OFF\r\n", value);
		goto out;
	}

	/* If reset is ongoing we expect falling if applicable and rising
	 * edge.
	 */
	reset_ongoing = atomic_read(&drv->rst_ongoing);
	if (reset_ongoing) {
		pr_err(DRVNAME ": RESET_OUT 0x%x\r\n", value);

		/* Rising EDGE (IPC ready) */
		if (value) {
			/* Reset the reset ongoing flag */
			atomic_set(&drv->rst_ongoing, 0);

			pr_err(DRVNAME ": IPC READY !\r\n");
			mdm_ctrl_set_state(drv, MDM_CTRL_STATE_IPC_READY);

			serial_hsu_set_rts_fixed(false);
		}

		goto out;
	}

	pr_err(DRVNAME ": Unexpected RESET_OUT 0x%x\r\n", value);

	/* Unexpected reset received */
	atomic_set(&drv->rst_ongoing, 1);

	/* Set the reason & launch the work to handle the hangup */
	drv->hangup_causes |= MDM_CTRL_HU_RESET;
	queue_work(drv->hu_wq, &drv->hangup_work);

 out:
	return IRQ_HANDLED;
}

/**
 *  clear_hangup_reasons - Clear the hangup reasons flag
 */
static void clear_hangup_reasons(void)
{
	mdm_drv->hangup_causes = MDM_CTRL_NO_HU;
}

/**
 *  get_hangup_reasons - Hangup reason flag accessor
 */
static int get_hangup_reasons(void)
{
	return mdm_drv->hangup_causes;
}

/*****************************************************************************
 *
 * Char device functions
 *
 ****************************************************************************/

/**
 *  mdm_ctrl_dev_open - Manage device access
 *  @inode: The node
 *  @filep: Reference to file
 *
 *  Called when a process tries to open the device file
 */
static int mdm_ctrl_dev_open(struct inode *inode, struct file *filep)
{
	mutex_lock(&mdm_drv->lock);
	/* Only ONE instance of this device can be opened */
	if (mdm_ctrl_get_opened(mdm_drv)) {
		mutex_unlock(&mdm_drv->lock);
		return -EBUSY;
	}

	/* Save private data for futur use */
	filep->private_data = mdm_drv;

	/* Set the open flag */
	mdm_ctrl_set_opened(mdm_drv, 1);
	mutex_unlock(&mdm_drv->lock);
	return 0;
}

/**
 *  mdm_ctrl_dev_close - Reset open state
 *  @inode: The node
 *  @filep: Reference to file
 *
 *  Called when a process closes the device file.
 */
static int mdm_ctrl_dev_close(struct inode *inode, struct file *filep)
{
	struct mdm_ctrl *drv = filep->private_data;

	/* Set the open flag */
	mutex_lock(&drv->lock);
	mdm_ctrl_set_opened(drv, 0);
	mutex_unlock(&drv->lock);
	return 0;
}

/**
 *  mdm_ctrl_dev_ioctl - Process ioctl requests
 *  @filep: Reference to file that stores private data.
 *  @cmd: Command that should be executed.
 *  @arg: Command's arguments.
 *
 */
long mdm_ctrl_dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct mdm_ctrl *drv = filep->private_data;
	struct mdm_ctrl_cmd cmd_params;
	long ret = 0;
	unsigned int mdm_state;
	unsigned int param;

	pr_info(DRVNAME ": ioctl request 0x%x received \r\n", cmd);
	mdm_state = mdm_ctrl_get_state(drv);

	switch (cmd) {
	case MDM_CTRL_POWER_OFF:
		/* Unconditionnal power off */
		mdm_ctrl_power_off(drv);
		break;

	case MDM_CTRL_POWER_ON:
		/* Only allowed when modem is OFF or in unkown state */
		if ((mdm_state == MDM_CTRL_STATE_OFF) ||
				(mdm_state == MDM_CTRL_STATE_UNKNOWN))
			mdm_ctrl_cold_boot(drv);
		else
			/* Specific log in COREDUMP state */
			if (mdm_state == MDM_CTRL_STATE_COREDUMP)
				pr_err(DRVNAME ": Power ON not allowed (coredump)");
			else
				pr_info(DRVNAME ": Powering on while already on");
		break;

	case MDM_CTRL_WARM_RESET:
		/* Allowed in any state unless OFF */
		if (mdm_state != MDM_CTRL_STATE_OFF)
			mdm_ctrl_normal_warm_reset(drv);
		else
			pr_err(DRVNAME ": Warm reset not allowed (Modem OFF)");
		break;

	case MDM_CTRL_FLASHING_WARM_RESET:
		/* Allowed in any state unless OFF */
		if (mdm_state != MDM_CTRL_STATE_OFF)
			mdm_ctrl_flashing_warm_reset(drv);
		else
			pr_err(DRVNAME ": Warm reset not allowed (Modem OFF)");
		break;

	case MDM_CTRL_COLD_RESET:
		/* Allowed in any state unless OFF */
		if (mdm_state != MDM_CTRL_STATE_OFF)
			mdm_ctrl_cold_reset(drv);
		else
			pr_err(DRVNAME ": Cold reset not allowed (Modem OFF)");
		break;

	case MDM_CTRL_SET_STATE:
		/* Read the user command params */
		ret = copy_from_user(&param, (void *)arg, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy from user failed ret = %ld\r\n",
				ret);
			goto out;
		}

		/* Filtering states. Allow any state ? */
		param &=
		    (MDM_CTRL_STATE_OFF |
		     MDM_CTRL_STATE_COLD_BOOT |
		     MDM_CTRL_STATE_WARM_BOOT |
		     MDM_CTRL_STATE_COREDUMP |
		     MDM_CTRL_STATE_IPC_READY |
		     MDM_CTRL_STATE_FW_DOWNLOAD_READY);

		mdm_ctrl_set_state(drv, param);
		break;

	case MDM_CTRL_GET_STATE:
		/* Return supposed current state.
		 * Real state can be different.
		 */
		param = mdm_state;

		ret = copy_to_user((void __user *)arg, &param, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy to user failed ret = %ld\r\n",
				ret);
			return ret;
		}
		break;

	case MDM_CTRL_GET_HANGUP_REASONS:
		/* Return last hangup reason. Can be cumulative
		 * if they were not cleared since last hangup.
		 */
		param = get_hangup_reasons();

		ret = copy_to_user((void __user *)arg, &param, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy to user failed ret = %ld\r\n",
				ret);
			return ret;
		}
		break;

	case MDM_CTRL_CLEAR_HANGUP_REASONS:
		clear_hangup_reasons();
		break;

	case MDM_CTRL_SET_POLLED_STATES:
		/* Set state to poll on. */
		/* Read the user command params */
		ret = copy_from_user(&param, (void *)arg, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy from user failed ret = %ld\r\n",
				ret);
			return ret;
		}
		drv->polled_states = param;
		/* Poll is active ? */
		if (waitqueue_active(&drv->wait_wq)) {
			mdm_state = mdm_ctrl_get_state(drv);
			/* Check if current state is awaited */
			if (mdm_state)
				drv->polled_state_reached = ((mdm_state & param)
							     == mdm_state);

			/* Waking up the wait work queue to handle any
			 * polled state reached.
			 */
			wake_up(&drv->wait_wq);
		} else {
			/* Assume that mono threaded client are probably
			 * not polling yet and that they are not interested
			 * in the current state. This state may change until
			 * they start the poll. May be an issue for some cases.
			 */
			drv->polled_state_reached = false;
		}

		pr_info(DRVNAME ": states polled = 0x%x\r\n",
			drv->polled_states);
		break;

	default:
		pr_err(DRVNAME ": ioctl command %x unknown\r\n", cmd);
		ret = -ENOIOCTLCMD;
	}

 out:
	return ret;
}

/**
 *  mdm_ctrl_dev_read - Device read function
 *  @filep: Reference to file
 *  @data: User data
 *  @count: Bytes read.
 *  @ppos: Reference to position in file.
 *
 *  Called when a process, which already opened the dev file, attempts to
 *  read from it. Not allowed.
 */
static ssize_t mdm_ctrl_dev_read(struct file *filep,
				 char __user * data,
				 size_t count, loff_t * ppos)
{
	pr_err(DRVNAME ": Nothing to read\r\n");
	return -EINVAL;
}

/**
 *  mdm_ctrl_dev_write - Device write function
 *  @filep: Reference to file
 *  @data: User data
 *  @count: Bytes read.
 *  @ppos: Reference to position in file.
 *
 *  Called when a process writes to dev file.
 *  Not allowed.
 */
static ssize_t mdm_ctrl_dev_write(struct file *filep,
				  const char __user * data,
				  size_t count, loff_t * ppos)
{
	pr_err(DRVNAME ": Nothing to write to\r\n");
	return -EINVAL;
}

/**
 *  mdm_ctrl_dev_poll - Poll function
 *  @filep: Reference to file storing private data
 *  @pt: Reference to poll table structure
 *
 *  Flush the change state workqueue to ensure there is no new state pending.
 *  Relaunch the poll wait workqueue.
 *  Return POLLHUP|POLLRDNORM if any of the polled states was reached.
 */
static unsigned int mdm_ctrl_dev_poll(struct file *filep,
				      struct poll_table_struct *pt)
{
	struct mdm_ctrl *drv = filep->private_data;
	unsigned int ret = 0;

	/* Wait event change */
	poll_wait(filep, &drv->wait_wq, pt);

	/* State notify */
	if (drv->polled_state_reached ||
	    (mdm_ctrl_get_state(drv) & drv->polled_states)) {

		drv->polled_state_reached = false;
		ret |= POLLHUP | POLLRDNORM;
		pr_info(DRVNAME ": POLLHUP occured. Current state = 0x%x\r\n",
			mdm_ctrl_get_state(drv));
#ifdef CONFIG_HAS_WAKELOCK
		wake_unlock(&drv->stay_awake);
#endif
	}

	return ret;
}

/**
 * Device driver file operations
 */
static const struct file_operations mdm_ctrl_ops = {
	.open = mdm_ctrl_dev_open,
	.read = mdm_ctrl_dev_read,
	.write = mdm_ctrl_dev_write,
	.poll = mdm_ctrl_dev_poll,
	.release = mdm_ctrl_dev_close,
	.unlocked_ioctl = mdm_ctrl_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdm_ctrl_dev_ioctl
#endif
};

/**
 *  mdm_ctrl_module_init - initialises the Modem Control driver
 *
 */
static int mdm_ctrl_module_probe(struct platform_device *pdev)
{
	int ret;
	struct mdm_ctrl *new_drv;

	pr_err(DRVNAME ": probing mdm_ctrl");
	/* Allocate modem struct data */
	new_drv = kzalloc(sizeof(struct mdm_ctrl), GFP_KERNEL);
	if (!new_drv) {
		pr_err(DRVNAME ": Out of memory(new_drv)");
		ret = -ENOMEM;
		goto out;
	}

	pr_info(DRVNAME ": Getting device infos");
	/* Pre-initialisation: Retrieve platform device data */
	mdm_ctrl_get_device_info(new_drv, pdev);

	/* HERE new_drv variable must contain all modem specifics
	   (mdm name, cpu name, pmic, GPIO, early power on/off */

	if (new_drv->is_mdm_ctrl_disabled) {
		/* KW fix can't happen. */
		if (unlikely(new_drv->pdata))
			kfree(new_drv->pdata);
		ret = -ENODEV;
		goto free_drv;
	}

	/* Initialization */
	mutex_init(&new_drv->lock);
	init_waitqueue_head(&new_drv->wait_wq);

	/* Create a high priority ordered workqueue to change modem state */

	INIT_WORK(&new_drv->hangup_work, mdm_ctrl_handle_hangup);

	/* Create a workqueue to manage hangup */
	new_drv->hu_wq = create_singlethread_workqueue(DRVNAME "-hu_wq");
	if (!new_drv->hu_wq) {
		pr_err(DRVNAME ": Unable to create control workqueue");
		ret = -EIO;
		goto free_drv;
	}

#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&new_drv->stay_awake, WAKE_LOCK_SUSPEND,
		       "mcd_wakelock");
#endif

	/* Register the device */
	ret = alloc_chrdev_region(&new_drv->tdev, 0, 1, MDM_BOOT_DEVNAME);
	if (ret) {
		pr_err(DRVNAME ": alloc_chrdev_region failed (err: %d)", ret);
		goto free_hu_wq;
	}

	new_drv->major = MAJOR(new_drv->tdev);
	cdev_init(&new_drv->cdev, &mdm_ctrl_ops);
	new_drv->cdev.owner = THIS_MODULE;

	ret = cdev_add(&new_drv->cdev, new_drv->tdev, 1);
	if (ret) {
		pr_err(DRVNAME ": cdev_add failed (err: %d)", ret);
		goto unreg_reg;
	}

	new_drv->class = class_create(THIS_MODULE, DRVNAME);
	if (IS_ERR(new_drv->class)) {
		pr_err(DRVNAME ": class_create failed (err: %d)", ret);
		ret = -EIO;
		goto del_cdev;
	}

	new_drv->dev = device_create(new_drv->class,
				     NULL,
				     new_drv->tdev, NULL, MDM_BOOT_DEVNAME);

	if (IS_ERR(new_drv->dev)) {
		pr_err(DRVNAME ": device_create failed (err: %ld)",
		       PTR_ERR(new_drv->dev));
		ret = -EIO;
		goto del_class;
	}

	mdm_ctrl_set_state(new_drv, MDM_CTRL_STATE_OFF);

	if (new_drv->pdata->mdm.init(new_drv->pdata->modem_data))
		goto del_dev;
	if (new_drv->pdata->cpu.init(new_drv->pdata->cpu_data))
		goto del_dev;
	if (new_drv->pdata->pmic.init(new_drv->pdata->pmic_data))
		goto del_dev;

	ret =
	    request_irq(new_drv->pdata->cpu.
			get_irq_rst(new_drv->pdata->cpu_data),
			mdm_ctrl_reset_it,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_NO_SUSPEND, DRVNAME, new_drv);
	if (ret) {
		pr_err(DRVNAME ": IRQ request failed for GPIO (RST_OUT)");
		ret = -ENODEV;
		goto del_dev;
	}

	ret =
	    request_irq(new_drv->pdata->cpu.
			get_irq_cdump(new_drv->pdata->cpu_data),
			mdm_ctrl_coredump_it,
			IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND, DRVNAME,
			new_drv);
	if (ret) {
		pr_err(DRVNAME ": IRQ request failed for GPIO (CORE DUMP)");
		ret = -ENODEV;
		goto free_all;
	}

	/* Everything is OK */
	mdm_drv = new_drv;
	/* Init driver */
	init_timer(&mdm_drv->flashing_timer);

	/* Modem power off sequence */
	if (mdm_drv->pdata->pmic.get_early_pwr_off(mdm_drv->pdata->pmic_data))
		mdm_ctrl_power_off(mdm_drv);

	/* Modem cold boot sequence */
	if (mdm_drv->pdata->pmic.get_early_pwr_on(mdm_drv->pdata->pmic_data))
		mdm_ctrl_cold_boot(mdm_drv);

	return 0;

 free_all:
	free_irq(new_drv->pdata->cpu.get_irq_rst(new_drv->pdata->cpu_data),
		 NULL);
 del_dev:
	device_destroy(new_drv->class, new_drv->tdev);

 del_class:
	class_destroy(new_drv->class);

 del_cdev:
	cdev_del(&new_drv->cdev);

 unreg_reg:
	unregister_chrdev_region(new_drv->tdev, 1);

 free_hu_wq:
	destroy_workqueue(new_drv->hu_wq);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&new_drv->stay_awake);
#endif

 free_drv:
	kfree(new_drv);

 out:
	return ret;
}

/**
 *  mdm_ctrl_module_exit - Frees the resources taken by the control driver
 */
static int mdm_ctrl_module_remove(struct platform_device *pdev)
{
	if (!mdm_drv)
		return 0;

	if (mdm_drv->is_mdm_ctrl_disabled)
		goto out;

	/* Delete the modem hangup worqueue */
	destroy_workqueue(mdm_drv->hu_wq);
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&mdm_drv->stay_awake);
#endif

	/* Unregister the device */
	device_destroy(mdm_drv->class, mdm_drv->tdev);
	class_destroy(mdm_drv->class);
	cdev_del(&mdm_drv->cdev);
	unregister_chrdev_region(mdm_drv->tdev, 1);

	if (mdm_drv->pdata->cpu.get_irq_cdump(mdm_drv->pdata->cpu_data) > 0)
		free_irq(mdm_drv->pdata->cpu.
			 get_irq_cdump(mdm_drv->pdata->cpu_data), NULL);
	if (mdm_drv->pdata->cpu.get_irq_rst(mdm_drv->pdata->cpu_data) > 0)
		free_irq(mdm_drv->pdata->cpu.
			 get_irq_rst(mdm_drv->pdata->cpu_data), NULL);

	mdm_drv->pdata->mdm.cleanup(mdm_drv->pdata->modem_data);
	mdm_drv->pdata->cpu.cleanup(mdm_drv->pdata->cpu_data);
	mdm_drv->pdata->pmic.cleanup(mdm_drv->pdata->pmic_data);;

	del_timer(&mdm_drv->flashing_timer);
	mutex_destroy(&mdm_drv->lock);

	/* Unregister the device */
	device_destroy(mdm_drv->class, mdm_drv->tdev);
	class_destroy(mdm_drv->class);
	cdev_del(&mdm_drv->cdev);
	unregister_chrdev_region(mdm_drv->tdev, 1);

 out:
	/* Free the driver context */
	kfree(mdm_drv->pdata);
	kfree(mdm_drv);
	mdm_drv = NULL;
	return 0;
}

/* FOR ACPI HANDLING */
static struct acpi_device_id mdm_ctrl_acpi_ids[] = {
	/* ACPI IDs here */
	{"MCD0001", 0},
	{}
};

MODULE_DEVICE_TABLE(acpi, mdm_ctrl_acpi_ids);

static const struct platform_device_id mdm_ctrl_id_table[] = {
	{DEVICE_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(platform, mdm_ctrl_id_table);

static struct platform_driver mcd_driver = {
	.probe = mdm_ctrl_module_probe,
	.remove = mdm_ctrl_module_remove,
	.driver = {
		   .name = DRVNAME,
		   .owner = THIS_MODULE,
		   /* FOR ACPI HANDLING */
		   .acpi_match_table = ACPI_PTR(mdm_ctrl_acpi_ids),
		   },
	.id_table = mdm_ctrl_id_table,
};

static int __init mdm_ctrl_module_init(void)
{
	return platform_driver_register(&mcd_driver);
}

static void __exit mdm_ctrl_module_exit(void)
{
	platform_driver_unregister(&mcd_driver);
}

module_init(mdm_ctrl_module_init);
module_exit(mdm_ctrl_module_exit);

/**
 *  mdm_ctrl_modem_reset - Reset modem
 *
 *  Debug and integration purpose.
 */
static int mdm_ctrl_modem_reset(const char *val, struct kernel_param *kp)
{
	if (mdm_drv)
		mdm_ctrl_normal_warm_reset(mdm_drv);
	return 0;
}

module_param_call(modem_reset, mdm_ctrl_modem_reset, NULL, NULL, 0644);

MODULE_AUTHOR("Faouaz Tenoutit <faouazx.tenoutit@intel.com>");
MODULE_AUTHOR("Frederic Berat <fredericx.berat@intel.com>");
MODULE_DESCRIPTION("Intel Modem control driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DEVICE_NAME);
