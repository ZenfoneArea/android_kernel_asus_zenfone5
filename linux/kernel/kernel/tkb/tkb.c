/*
 * tkb.c - Tinkerbell Kernel Module
 *
 * Copyright (C) 2013 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Yoshi Wang <yoshix.wang@intel.com>
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/tkb.h>
#include <asm/uaccess.h>

enum {
       DEBUG_TKB= 1U << 0,
};
static int debug_mask;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define TKB_NAME "tkb"
#define TKB_IOCTL_MAGIC 'x'
#define TKB_IOCTL_CPUFREQ_SET	    	_IOW(TKB_IOCTL_MAGIC, 0x01, char *)
#define TKB_IOCTL_THERMAL_SET 		_IOW(TKB_IOCTL_MAGIC, 0x02, char *)
#define TKB_IOCTL_STATUS_SHOW 		_IOW(TKB_IOCTL_MAGIC, 0x03, char *)

#define MAX_THERMAL_LEVEL		    10

#define TKB_THERMAL_STATE_INCREASE 	1
#define TKB_THERMAL_STATE_DECREASE 	2
#define TKB_THERMAL_FREQ_CHANGE    	3

#define pr_tkb(args...) \
	do { \
		if (debug_mask & DEBUG_TKB) { \
			pr_info(TKB_NAME ": " args); \
		} \
	} while (0)

struct thermal_request {
	unsigned char levels;
	int temp[MAX_THERMAL_LEVEL];
	int min_freq[MAX_THERMAL_LEVEL];
	int max_freq[MAX_THERMAL_LEVEL];
};

struct tkb_info {
/*for cpufreq*/
	struct workqueue_struct *cpufreq_wq;
	struct work_struct cpufreq_work;
	struct pm_qos_request *cpufreq_req_h_freq;
	struct pm_qos_request *cpufreq_req_l_freq;
	int cpufreq_set_h_freq;
	int cpufreq_set_l_freq;
/*for thermal*/
	struct thermal_request t_req;
	int current_state;
	int current_temp;
	int current_high;
	int current_low;
	unsigned int default_h_freq;
	unsigned int default_l_freq;
	int thermal_service_low;
	int thermal_service_high;
	bool en_thermal_service_low;
	bool en_thermal_service_high;
	bool thermal_enable;
	struct pm_qos_request *thermal_req_l_freq;
	struct pm_qos_request *thermal_req_h_freq;
	int thermal_set_l_freq;
	int thermal_set_h_freq;
#ifdef CONFIG_SENSORS_CORETEMP_INTERRUPT
	struct workqueue_struct *thermal_wq;
	struct work_struct set_threshold;
#endif
};

struct tkb_info *tkbi;

static int get_state_by_temp(int temp)
{
	int state = 0;

	for(state = 0; state < tkbi->t_req.levels; state++)
	{
		if(temp >= tkbi->t_req.temp[state])
			continue;
		else
			return state;
	}
	return state;
}

static void tkb_throttle_cpu_freq(int temp)
{
	int state;

	state = get_state_by_temp(temp);
	pr_tkb("state change from %d to %d\n", tkbi->current_state, state);
	tkbi->current_state = state;

	if (state == 0) {
		if (tkbi->thermal_set_h_freq != tkbi-> default_h_freq ||
				tkbi->thermal_set_l_freq != tkbi-> default_l_freq) {
			tkbi->thermal_set_h_freq = tkbi-> default_h_freq;
			tkbi->thermal_set_l_freq = tkbi-> default_l_freq;
			pm_qos_update_request(tkbi->thermal_req_h_freq,
					tkbi->thermal_set_h_freq);
			pm_qos_update_request(tkbi->thermal_req_l_freq,
					tkbi->thermal_set_l_freq);
			pr_tkb("throttle CPU freq (default), max_freq = %d, min_freq= %d\n",
					tkbi->thermal_set_h_freq, tkbi->thermal_set_l_freq);
		}
	} else if (state > 0) {
		if (tkbi->thermal_set_h_freq != tkbi->t_req.max_freq[state -1] ||
				tkbi->thermal_set_l_freq != tkbi->t_req.min_freq[state -1]) {
			tkbi->thermal_set_h_freq = tkbi->t_req.max_freq[state -1];
			tkbi->thermal_set_l_freq = tkbi->t_req.min_freq[state -1];

			if (tkbi->thermal_set_h_freq < 0)
				pm_qos_update_request(tkbi->thermal_req_h_freq,
					tkbi->default_h_freq);
			else
				pm_qos_update_request(tkbi->thermal_req_h_freq,
					tkbi->thermal_set_h_freq);

			if (tkbi->thermal_set_l_freq < 0)
				pm_qos_update_request(tkbi->thermal_req_l_freq,
					tkbi->default_l_freq);
			else
				pm_qos_update_request(tkbi->thermal_req_l_freq,
					tkbi->thermal_set_l_freq);

			pr_tkb("throttle CPU freq (%d), max_freq = %d, min_freq= %d\n",
					(state-1) , tkbi->thermal_set_h_freq,
					tkbi->thermal_set_l_freq);
		}
	}
}

#ifdef CONFIG_SENSORS_CORETEMP_INTERRUPT
static int find_next_low_threshold(void)
{
	if(!tkbi->thermal_enable ||tkbi->current_state == 0 ||
		(tkbi->thermal_service_low >= tkbi->t_req.temp[tkbi->current_state - 1]
		&& tkbi->thermal_service_low < tkbi->current_temp)) {
		tkbi->en_thermal_service_low = true;
		tkbi->current_low = tkbi->thermal_service_low;
		pr_tkb("Next low threshold is set by thermal service, threshold = %d\n",
			tkbi->thermal_service_low);
		return tkbi->thermal_service_low;
	} else {
		tkbi->en_thermal_service_low = false;
		tkbi->current_low = tkbi->t_req.temp[tkbi->current_state - 1];
		pr_tkb("Next low threshold is set by tkb, threshold = %d\n",
			tkbi->t_req.temp[tkbi->current_state - 1]);
		return tkbi->t_req.temp[tkbi->current_state - 1];
	}
}

static int find_next_high_threshold(void)
{
	if(!tkbi->thermal_enable ||tkbi->current_state == tkbi->t_req.levels ||
		(tkbi->thermal_service_high <= tkbi->t_req.temp[tkbi->current_state]
		&& tkbi->thermal_service_high > tkbi->current_temp)) {
		tkbi->en_thermal_service_high = true;
		tkbi->current_high = tkbi->thermal_service_high;
		pr_tkb("Next high threshold is set by thermal service, threshold = %d\n",
			tkbi->thermal_service_high);
		return tkbi->thermal_service_high;
	} else {
		tkbi->en_thermal_service_high = false;
		tkbi->current_high = tkbi->t_req.temp[tkbi->current_state];
		pr_tkb("Next high threshold is set by tkb, threshold = %d\n",
				tkbi->t_req.temp[tkbi->current_state]);
		return tkbi->t_req.temp[tkbi->current_state];
	}
}

bool tkb_coretemp_trigger(int index, int temp, int event, int *l_thr,
	int *h_thr, int *is_enabled)
{
	int ret = true;

	pr_tkb("%s: index = %d, temp = %d, event = %d\n",
		__func__, index, temp, event);

	tkbi->current_temp = temp;

	if(!tkbi->thermal_enable ) {
		*is_enabled = 0;
		return ret;
	} else {
		*is_enabled = 1;
	}

	/*temp2_input only*/
	if (index == 2) {
		tkb_throttle_cpu_freq(temp);

		if (event == 0 && tkbi->en_thermal_service_low) {
			 /* trigger famework low thr */
			ret = true;
		} else if (event == 1 && tkbi->en_thermal_service_high) {
			/* trigger framework high thr. */
			ret = true;
		} else {
			/* triggered by tkb */
			ret = false;
		}

		*l_thr = find_next_low_threshold();
		*h_thr = find_next_high_threshold();
	}else {
		pr_tkb("%s: undemand index\n", __func__);
	}

	pr_tkb("%s: updated state = %d, low_thr = %d, high_thr = %d\n",
			 __func__, tkbi->current_state, *l_thr, *h_thr);

	return ret;
}

int tkb_get_coretemp_thr(int index, int val, int mask)
{
	pr_tkb("%s: index = %d, val = %d, mask = %d\n",
		__func__, index, val, mask);

	/*temp2_input only*/
	if(index == 2) {
		if (mask == THERM_MASK_THRESHOLD0){
			tkbi->thermal_service_low = val;

			if (tkbi->thermal_enable)
				return find_next_low_threshold();

		} else if (mask == THERM_MASK_THRESHOLD1) {
			tkbi->thermal_service_high = val;

			if (tkbi->thermal_enable)
				return find_next_high_threshold();

		} else {
			pr_tkb("%s: unknown mask\n", __func__);
			return val;
		}
	} else {
		pr_tkb("%s: undemand index\n", __func__);
		return val;
	}

	return val;
}
#endif

void tkb_update_coretemp(int index, int temp)
{
	pr_tkb("%s: index = %d, temp = %d\n", __func__, index, temp);

	tkbi->current_temp = temp;

	if(!tkbi->thermal_enable)
		return;

	/*temp2_input only*/
	if(index == 2)
		tkb_throttle_cpu_freq(temp);
	else
		pr_tkb("%s: undemand index\n", __func__);
}

void tkb_print_thermal_thr (void)
{
	int i = 0;
	int levels = tkbi->t_req.levels;

	pr_tkb("levels = %d\n", levels);
	while(i < levels)
	{
		pr_tkb("Attr[%d]: threshold = %d, max_freq = %d, min_freq = %d\n", i,
			tkbi->t_req.temp[i], tkbi->t_req.max_freq[i],
			tkbi->t_req.min_freq[i]);
		i++;
	}
}

static void tkb_print_state(void)
{
	pr_tkb("--------tkb cpufreq status--------\n");
	pr_tkb("cpufreq_set_l_freq: %d\n", tkbi->cpufreq_set_l_freq);
	pr_tkb("cpufreq_set_h_freq: %d\n", tkbi->cpufreq_set_h_freq);
	pr_tkb("--------tkb thermal status--------\n");
	pr_tkb("enable: %d\n", tkbi->thermal_enable);
	if (tkbi->thermal_enable)
		tkb_print_thermal_thr();
	pr_tkb("current_state: %d\n", tkbi->current_state);
	pr_tkb("current_temp: %d\n", tkbi->current_temp);
	pr_tkb("thermal_set_l_freq: %d\n", tkbi->thermal_set_l_freq);
	pr_tkb("thermal_set_h_freq: %d\n", tkbi->thermal_set_h_freq);
	pr_tkb("current_high: %d\n", tkbi->current_high);
	pr_tkb("current_low: %d\n", tkbi->current_low);
	pr_tkb("thermal_service_low: %d\n", tkbi->thermal_service_low);
	pr_tkb("thermal_service_high: %d\n", tkbi->thermal_service_high);
	pr_tkb("en_thermal_service_low: %s\n",
		tkbi->en_thermal_service_low? "ture" : "false");
	pr_tkb("en_thermal_service_high: %s\n",
		tkbi->en_thermal_service_high? "ture" : "false");
	pr_tkb("default_l_freq: %d\n", tkbi-> default_l_freq);
	pr_tkb("default_h_freq: %d\n", tkbi-> default_h_freq);
	pr_tkb("---------------------------------\n");
}

static void tkb_set_cpu_freq(struct work_struct *work)
{
	long old_prio;

	old_prio = task_nice(current);
	set_user_nice(current, -20);

	pr_tkb("%s\n", __func__);

	if (tkbi->cpufreq_set_h_freq >= 0) {
		pm_qos_update_request(tkbi->cpufreq_req_h_freq,
			tkbi->cpufreq_set_h_freq);
	} else {
			pm_qos_update_request(tkbi->cpufreq_req_h_freq,
				tkbi-> default_h_freq);
	}

	if (tkbi->cpufreq_set_l_freq >= 0) {
		pm_qos_update_request(tkbi->cpufreq_req_l_freq,
			tkbi->cpufreq_set_l_freq);
	} else {
			pm_qos_update_request(tkbi->cpufreq_req_l_freq,
				tkbi-> default_l_freq);
	}

	pr_tkb("%s end\n", __func__);
	set_user_nice(current, old_prio);

}

#ifdef CONFIG_SENSORS_CORETEMP_INTERRUPT
static void tkb_set_thermal_threshold(struct work_struct *work)
{
	core_threshold_set(find_next_low_threshold(), find_next_high_threshold());
}
#endif

static int tkb_open(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);

        return 0;
}

static int tkb_release(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);

	return 0;
}

static int parse_cpufreq_request(char *cpufreq_request)
{
	char *pch = cpufreq_request;
	char *result;

	result = strsep(&pch, ",");
	if (result != NULL)
		tkbi->cpufreq_set_l_freq = simple_strtol(result, NULL, 10);
	else
		return -1;

	result = strsep(&pch, ",");
	if (result != NULL)
		tkbi->cpufreq_set_h_freq = simple_strtol(result, NULL, 10);
	else
		return -1;

	return 0;
}

static int parse_thermal_request(char *thermal_request)
{
	char *pch = thermal_request;
	int level_num = 0;
	char *result;

	result = strsep(&pch, ",");
	if (result != NULL)
		tkbi->t_req.levels = simple_strtol(result, NULL, 10);

	if (tkbi->t_req.levels ==0 ) {
		pr_tkb("%s: user disable thermal request", __func__);
		return -1;
	}

	if (tkbi->t_req.levels > MAX_THERMAL_LEVEL ) {
		pr_tkb("%s: over level %d(%d)", __func__, tkbi->t_req.levels,
			MAX_THERMAL_LEVEL);
		return -1;
	}

	for (level_num; level_num < tkbi->t_req.levels; level_num++) {

		result = strsep(&pch, ",");
		if (result != NULL)
			tkbi->t_req.temp[level_num]= simple_strtol(result, NULL, 10);
		else
			break;

		result = strsep(&pch, ",");
		if (result != NULL)
			tkbi->t_req.min_freq[level_num]= simple_strtol(result, NULL, 10);
		else
			break;

		result = strsep(&pch, ",");
		if (result != NULL)
			tkbi->t_req.max_freq[level_num]= simple_strtol(result, NULL, 10);
		else
			break;
	}

	if(level_num != tkbi->t_req.levels || tkbi->t_req.levels < 0) {
		pr_tkb("%s: incorrect user configuration data format", __func__);
		return -1;
	}

	return 0;
}

static long tkb_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	char buff[256];
	void __user *argp = (void __user *)arg;

	pr_tkb("%s : cmd:%d\n", __func__, cmd);

	switch (cmd) {
	case TKB_IOCTL_CPUFREQ_SET:
	{
		pr_tkb("receive TKB_IOCTL_CPUFREQ_SET cmd\n");
		if (copy_from_user(buff, argp, sizeof (buff)/sizeof(buff[0])))
			return -EFAULT;

		if(parse_cpufreq_request(buff))
			return -EFAULT;
		else
			queue_work(tkbi->cpufreq_wq, &tkbi->cpufreq_work);

		if (debug_mask & DEBUG_TKB)
			tkb_print_state();
		return 0;

	}
	case TKB_IOCTL_THERMAL_SET:
	{
		pr_tkb("receive TKB_IOCTL_THERMAL_SET cmd\n");

		if (copy_from_user(buff, argp, sizeof (buff)/sizeof(buff[0])))
			return -EFAULT;

		if (parse_thermal_request(buff) == 0) {
			tkbi->thermal_enable = true;
			tkbi->current_state = 0;
#ifdef CONFIG_SENSORS_CORETEMP_INTERRUPT
			schedule_work_on(0, &tkbi->set_threshold);
#endif
		} else {
			tkbi->thermal_enable = false;
			tkbi->current_state = -1;
			tkbi->thermal_set_l_freq = tkbi-> default_l_freq;
			tkbi->thermal_set_h_freq = tkbi-> default_h_freq;
			pm_qos_update_request(tkbi->thermal_req_l_freq,
				tkbi->thermal_set_l_freq);
			pm_qos_update_request(tkbi->thermal_req_h_freq,
				tkbi->thermal_set_h_freq);
#ifdef CONFIG_SENSORS_CORETEMP_INTERRUPT
			schedule_work_on(0, &tkbi->set_threshold);
#endif
		}

		if (debug_mask & DEBUG_TKB)
			tkb_print_state();
		return 0;
	}
	case TKB_IOCTL_STATUS_SHOW:
	{
		if (debug_mask & DEBUG_TKB)
			tkb_print_state();
		return 0;
	}
	default:
		pr_err("%s: Wrong command: %d\n", __func__, cmd);
		return 0;
	}
}

static const struct file_operations tkb_fops = {
	.owner		= THIS_MODULE,
	.open		= tkb_open,
	.release	= tkb_release,
	.unlocked_ioctl	= tkb_ioctl,
};

static struct miscdevice tkb_misc = {
	.fops		= &tkb_fops,
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= TKB_NAME,
};

static int __init tkb_init(void)
{
	int rc = 0;

	pr_info("%s\n", __func__);

	tkbi = kzalloc(sizeof(struct tkb_info), GFP_KERNEL);
	tkbi->cpufreq_req_h_freq =
		kzalloc(sizeof(struct pm_qos_request), GFP_KERNEL);
	tkbi->cpufreq_req_l_freq =
		kzalloc(sizeof(struct pm_qos_request), GFP_KERNEL);
	tkbi->cpufreq_set_h_freq = PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE;
	tkbi->cpufreq_set_l_freq = PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE;

	pm_qos_add_request(tkbi->cpufreq_req_h_freq,
		PM_QOS_CPU_FREQ_MAX, PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);
	pm_qos_add_request(tkbi->cpufreq_req_l_freq,
		PM_QOS_CPU_FREQ_MIN, PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);

	if (!tkbi->cpufreq_wq) {
		tkbi->cpufreq_wq = create_freezable_workqueue(TKB_NAME);
		if (!tkbi->cpufreq_wq)
			goto error_wq;
	}

	INIT_WORK(&tkbi->cpufreq_work, tkb_set_cpu_freq);

#ifdef CONFIG_SENSORS_CORETEMP_INTERRUPT
	INIT_WORK(&tkbi->set_threshold,
					tkb_set_thermal_threshold);
#endif

	tkbi->thermal_enable = false;
	tkbi->current_state = -1;
	tkbi->current_temp= -1;
	tkbi->en_thermal_service_low = true;
	tkbi->en_thermal_service_high = true;
	tkbi->thermal_service_high = 90000;
	tkbi->thermal_service_low = 0;
	tkbi-> default_h_freq = PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE;
	tkbi-> default_l_freq = PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE;
	tkbi->thermal_req_h_freq =
		kzalloc(sizeof(struct pm_qos_request), GFP_KERNEL);
	tkbi->thermal_req_l_freq =
		kzalloc(sizeof(struct pm_qos_request), GFP_KERNEL);
	tkbi->thermal_set_h_freq = PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE;
	tkbi->thermal_set_l_freq = PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE;

	pm_qos_add_request(tkbi->thermal_req_h_freq,
		PM_QOS_CPU_FREQ_MAX, PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);
	pm_qos_add_request(tkbi->thermal_req_l_freq,
		PM_QOS_CPU_FREQ_MIN, PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);

	rc = misc_register(&tkb_misc);
	if (rc < 0)
		goto error_misc;

	return rc;

error_wq:
error_misc:
	pm_qos_remove_request(tkbi->cpufreq_req_l_freq);
	pm_qos_remove_request(tkbi->cpufreq_req_h_freq);
	pm_qos_remove_request(tkbi->thermal_req_l_freq);
	pm_qos_remove_request(tkbi->thermal_req_h_freq);
	kfree(tkbi->cpufreq_req_l_freq);
	kfree(tkbi->cpufreq_req_h_freq);
	kfree(tkbi->thermal_req_l_freq);
	kfree(tkbi->thermal_req_h_freq);
	cancel_work_sync(&tkbi->cpufreq_work);
#ifdef CONFIG_SENSORS_CORETEMP_INTERRUPT
	cancel_work_sync(&tkbi->set_threshold);
#endif
	kfree(tkbi);

	return -1;
}
module_init(tkb_init);

static void __exit tkb_exit(void)
{
	pr_info("%s\n", __func__);

	if (tkbi->cpufreq_wq)
		destroy_workqueue(tkbi->cpufreq_wq);

	if (tkbi->cpufreq_req_h_freq)
		pm_qos_remove_request(tkbi->cpufreq_req_h_freq);

	if (tkbi->cpufreq_req_l_freq)
		pm_qos_remove_request(tkbi->cpufreq_req_l_freq);

	if (tkbi->thermal_req_h_freq)
		pm_qos_remove_request(tkbi->thermal_req_h_freq);

	if (tkbi->thermal_req_l_freq)
		pm_qos_remove_request(tkbi->thermal_req_l_freq);

	kfree(tkbi->cpufreq_req_l_freq);
	kfree(tkbi->cpufreq_req_h_freq);
	kfree(tkbi->thermal_req_l_freq);
	kfree(tkbi->thermal_req_h_freq);
	cancel_work_sync(&tkbi->cpufreq_work);
#ifdef CONFIG_SENSORS_CORETEMP_INTERRUPT
	cancel_work_sync(&tkbi->set_threshold);
#endif

	kfree(tkbi);

	misc_deregister(&tkb_misc);
}
module_exit(tkb_exit);

MODULE_LICENSE("GPL");
