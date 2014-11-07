/*
 * intel_moor_thermal.c - Intel Moorefield Platform Thermal Driver
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
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Sumeet R Pawnikar <sumeet.r.pawnikar@intel.com>
 *
 * Intel Moorefield platform - Shadycove PMIC: Thermal Monitor
 * This driver exposes temperature and thresholds through sysfs interface
 * to user space.
 */

#define pr_fmt(fmt)  "intel_moor_thermal: " fmt

#include <linux/pm.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/rpmsg.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/iio/consumer.h>

#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_mid_thermal.h>


#define DRIVER_NAME "scove_thrm"

/* Number of trip points */
#define MOORE_THERMAL_TRIPS	2
/* Writabilities of trip points */
#define MOORE_TRIPS_RW		0x03

/* Number of Thermal sensors on the PMIC */
#define PMIC_THERMAL_SENSORS	4

/* Registers that govern Thermal Monitoring */
#define THRMMONCTL      0xB2
#define STHRMIRQ	0xB0
#define MIRQLVL1	0x0C

/* Set 10 sec interval between temperature measurements in Active Mode */
#define THERM_EN_ACTIVE_MODE	(3 << 0)
/* Set 30 Sec interval between temperature measurements in standby */
#define THERM_EN_STANDBY_MODE	(3 << 3)

/* PMIC SRAM base address and offset for Thermal register */
#define PMIC_SRAM_BASE_ADDR	0xFFFFF610
#define PMIC_SRAM_THRM_OFFSET	0x03
#define IOMAP_SIZE		0x04

#define PMICALRT	(1 << 3)
#define SYS2ALRT	(1 << 2)
#define SYS1ALRT	(1 << 1)
#define SYS0ALRT	(1 << 0)
#define THERM_ALRT	(1 << 2)
#define MTHERM_IRQ	(1 << 2)

/* ADC to Temperature conversion table length */
#define TABLE_LENGTH	34
#define TEMP_INTERVAL	5

/* Default _max 85 C */
#define DEFAULT_MAX_TEMP	85

/* Constants defined in ShadyCove PMIC spec */
#define PMIC_DIE_ADC_MIN	53
#define PMIC_DIE_ADC_MAX	15000
#define PMIC_DIE_TEMP_MIN       -40
#define PMIC_DIE_TEMP_MAX       125
#define ADC_COEFFICIENT         269
#define TEMP_OFFSET             273150

/* Structure for thermal event notification */
struct thermal_event {
	int sensor;	/* Sensor type */
	int event;	/* Event type: LOW or HIGH */
};

/* 'enum' of Thermal sensors */
enum thermal_sensors { SYS0, SYS1, SYS2, PMIC_DIE, _COUNT };

/*
 * Alert registers store the 'alert' temperature for each sensor,
 * as 12 bit ADC code. The higher four bits are stored in bits[0:3] of
 * alert_regs_h. The lower eight bits are stored in alert_regs_l.
 * The hysteresis value is stored in bits[4:7] of alert_regs_h.
 * Order: SYS0 SYS1 SYS2 PMIC_DIE
 *
 * static const int alert_regs_l[] = { 0xBA, 0xBE, 0xCe, 0xC8 };
 */
static const int alert_regs_h[] = { 0xB9, 0xBD, 0xC1, 0xC7 };

/*
 * ADC code vs Temperature table
 * This table will be different for different thermistors
 * Row 0: ADC code
 * Row 1: Temperature (in degree celsius)
 */
static const int adc_code[2][TABLE_LENGTH] = {
	{6008, 4550, 3481, 2689, 2095, 1647, 1305, 1040, 835, 676,
	550, 450, 371, 307, 255, 213, 179, 151, 127, 108,
	92, 79, 68, 59, 51, 44, 38, 34, 29, 26,
	23, 20, 18, 16},
	{-40, -35, -30, -25, -20, -15, -10, -5, 0, 5,
	10, 15, 20, 25, 30, 35, 40, 45, 50, 55,
	60, 65, 70, 75, 80, 85, 90, 95, 100, 105,
	110, 115, 120, 125},
	};

static DEFINE_MUTEX(thrm_update_lock);

struct thermal_device_info {
	struct intel_mid_thermal_sensor *sensor;
};

struct thermal_data {
	struct platform_device *pdev;
	struct iio_channel *iio_chan;
	struct thermal_zone_device **tzd;
	struct intel_mid_thermal_sensor *sensors;
	unsigned int irq;
	/* Caching information */
	unsigned long last_updated;
	int cached_vals[PMIC_THERMAL_SENSORS];
	int num_sensors;
	bool is_initialized;
	void *thrm_addr;
};
static struct thermal_data *tdata;

/*
 * adc_to_pmic_die_temp - calculates the PMIC DIE temperature
 * @adc_val: received the adc value from gpadc channel
 *
 * This function calculates the PMIC DIE temperature as per
 * the formula given in ShadyCove PMIC spec.
 * return value: temperature value in `C
 */
static inline int adc_to_pmic_die_temp(unsigned int adc_val)
{
	/* return temperature in mC */
	return (ADC_COEFFICIENT * (adc_val - 2047)) - TEMP_OFFSET;
}

/*
 * pmic_die_temp_to_adc - calculates the adc value
 * @temp: received the PMIC DIE temp value in `C
 *
 * This function calculates the adc value as per
 * the formula given in ShadyCove PMIC spec.
 * return value: adc value
 */

static inline int pmic_die_temp_to_adc(int temp)
{
	/* 'temp' is in C, convert to mC and then do calculations */
	return (((temp * 1000) + TEMP_OFFSET) / ADC_COEFFICIENT) + 2047;
}

/*
 * find_adc_code - searches the ADC code using binary search
 * @val: value to find in the array
 *
 * This function does binary search on an array sorted in 'descending' order.
 * Can sleep
 */
static int find_adc_code(uint16_t val)
{
	int left = 0;
	int right = TABLE_LENGTH - 1;
	int mid;

	while (left <= right) {
		mid = (left + right)/2;
		if (val == adc_code[0][mid] ||
			(mid > 0 &&
			val > adc_code[0][mid] &&
			val < adc_code[0][mid-1]))
			return mid;
		else if (val > adc_code[0][mid])
			right = mid - 1;
		else if (val < adc_code[0][mid])
			left = mid + 1;
	}
	return -EINVAL;
}

/*
 * adc_to_temp - converts the ADC code to temperature in mC
 * @direct: true if the sensor uses direct conversion
 * @adc_val: the ADC code to be converted
 * @tp: temperature return value
 *
 * Can sleep
 */
static int adc_to_temp(int direct, uint16_t adc_val, unsigned long *tp)
{
	int x0, x1, y0, y1;
	int nr, dr;		/* Numerator & Denominator */
	int indx;
	int x = adc_val;

	/* Direct conversion for pmic die temperature */
	if (direct) {
		if (adc_val < PMIC_DIE_ADC_MIN || adc_val > PMIC_DIE_ADC_MAX)
			return -EINVAL;

		*tp = adc_to_pmic_die_temp(adc_val);
		return 0;
	}

	indx = find_adc_code(adc_val);
	if (indx < 0)
		return -EINVAL;

	if (adc_code[0][indx] == adc_val) {
		*tp = adc_code[1][indx] * 1000;
		return 0;
	}

	/*
	 * The ADC code is in between two values directly defined in the
	 * table. So, do linear interpolation to calculate the temperature.
	 */
	x0 = adc_code[0][indx];
	x1 = adc_code[0][indx - 1];
	y0 = adc_code[1][indx];
	y1 = adc_code[1][indx - 1];

	/*
	 * Find y:
	 * Of course, we can avoid these variables, but keep them
	 * for readability and maintainability.
	 */
	nr = (x-x0)*y1 + (x1-x)*y0;
	dr =  x1-x0;

	if (!dr)
		return -EINVAL;
	/*
	 * We have to report the temperature in milli degree celsius.
	 * So, to reduce the loss of precision, do (Nr*1000)/Dr, instead
	 * of (Nr/Dr)*1000.
	 */
	*tp = (nr * 1000)/dr;

	return 0;
}

/*
 * temp_to_adc - converts the temperature(in C) to ADC code
 * @direct: true if the sensor uses direct conversion
 * @temp: the temperature to be converted
 * @adc_val: ADC code return value
 *
 * Can sleep
 */
static int temp_to_adc(int direct, int temp, uint16_t *adc_val)
{
	int indx;
	int x0, x1, y0, y1;
	int nr, dr;		/* Numerator & Denominator */
	int x = temp;

	/* Direct conversion for pmic die temperature */
	if (direct) {
		if (temp < PMIC_DIE_TEMP_MIN || temp > PMIC_DIE_TEMP_MAX)
			return -EINVAL;

		*adc_val = pmic_die_temp_to_adc(temp);
		return 0;
	}

	if (temp < adc_code[1][0] || temp > adc_code[1][TABLE_LENGTH - 1])
		return -EINVAL;

	/* Find the 'indx' of this 'temp' in the table */
	indx = (temp - adc_code[1][0]) / TEMP_INTERVAL;

	if (temp == adc_code[1][indx]) {
		*adc_val = adc_code[0][indx];
		return 0;
	}

	/*
	 * Temperature is not a multiple of 'TEMP_INTERVAL'. So,
	 * do linear interpolation to obtain a better ADC code.
	 */
	x0 = adc_code[1][indx];
	x1 = adc_code[1][indx + 1];
	y0 = adc_code[0][indx];
	y1 = adc_code[0][indx + 1];

	nr = (x-x0)*y1 + (x1-x)*y0;
	dr =  x1-x0;

	if (!dr)
		return -EINVAL;

	*adc_val = nr/dr;

	return 0;
}

/*
 * get_adc_value - gets the ADC code from the register
 * @alert_reg_h: The 'high' register address
 *
 * Not protected. Calling function should handle synchronization.
 * Can sleep
 */
static int get_adc_value(uint16_t alert_reg_h)
{
	int ret;
	uint16_t adc_val;
	uint8_t l, h;

	/* Reading high register address */
	ret = intel_scu_ipc_ioread8(alert_reg_h, &h);
	if (ret)
		goto exit;

	/* Get the address of alert_reg_l */
	++alert_reg_h;

	/* Reading low register address */
	ret = intel_scu_ipc_ioread8(alert_reg_h, &l);
	if (ret)
		goto exit;

	/* Concatenate 'h' and 'l' to get 12-bit ADC code */
	adc_val = ((h & 0x0F) << 8) | l;

	return adc_val;

exit:
	return ret;

}

/*
 * set_tmax - sets the given 'adc_val' to the 'alert_reg'
 * @alert_reg: register address
 * @adc_val: ADC value to be programmed
 *
 * Not protected. Calling function should handle synchronization.
 * Can sleep
 */
static int set_tmax(uint16_t alert_reg, uint16_t adc_val)
{
	int ret;

	/* Set bits[0:3] of alert_reg_h to bits[8:11] of 'adc_val' */
	ret = intel_scu_ipc_update_register(alert_reg, (adc_val >> 8), 0x0F);
	if (ret)
		return ret;

	/* Extract bits[0:7] of 'adc_val' and write them into alert_reg_l */
	return intel_scu_ipc_iowrite8(alert_reg + 1, adc_val & 0xFF);
}

/*
 * program_tmax - programs a default _max value for each sensor
 * @dev: device pointer
 *
 * Can sleep
 */
static int program_tmax(struct device *dev)
{
	int i, ret;
	uint16_t pmic_die_val;
	uint16_t adc_val;

	ret = temp_to_adc(0, DEFAULT_MAX_TEMP, &adc_val);
	if (ret)
		return ret;

	ret = temp_to_adc(1, DEFAULT_MAX_TEMP, &pmic_die_val);
	if (ret)
		return ret;
	/*
	 * Since this function sets max value, do for all sensors even if
	 * the sensor does not register as a thermal zone.
	 */
	for (i = 0; i < PMIC_THERMAL_SENSORS - 1; i++) {
		ret = set_tmax(alert_regs_h[i], adc_val);
		if (ret)
			goto exit_err;
	}

	/* Set _max for pmic die sensor */
	ret = set_tmax(alert_regs_h[i], pmic_die_val);
	if (ret)
		goto exit_err;

	return ret;

exit_err:
	dev_err(dev, "set_tmax for channel %d failed:%d\n", i, ret);
	return ret;
}

static int store_trip_hyst(struct thermal_zone_device *tzd,
				int trip, long hyst)
{
	int ret;
	uint8_t data;
	struct thermal_device_info *td_info = tzd->devdata;
	uint16_t alert_reg = alert_regs_h[td_info->sensor->index];

	/* Hysteresis value is 5 bits wide */
	if (hyst > 31)
		return -EINVAL;

	mutex_lock(&thrm_update_lock);

	ret = intel_scu_ipc_ioread8(alert_reg, &data);
	if (ret)
		goto ipc_fail;

	/* Set bits [4:7] to value of hyst */
	data = (data & 0xF) | (hyst << 4);

	ret = intel_scu_ipc_iowrite8(alert_reg, data);

ipc_fail:
	mutex_unlock(&thrm_update_lock);
	return ret;
}

static int show_trip_hyst(struct thermal_zone_device *tzd,
				int trip, long *hyst)
{
	int ret;
	uint8_t data;
	struct thermal_device_info *td_info = tzd->devdata;
	uint16_t alert_reg = alert_regs_h[td_info->sensor->index];

	mutex_lock(&thrm_update_lock);

	ret = intel_scu_ipc_ioread8(alert_reg, &data);
	if (!ret)
		*hyst = (data >> 4) & 0x0F; /* Extract bits[4:7] of data */

	mutex_unlock(&thrm_update_lock);

	return ret;
}

static int store_trip_temp(struct thermal_zone_device *tzd,
				int trip, long trip_temp)
{
	int ret;
	uint16_t adc_val;
	struct thermal_device_info *td_info = tzd->devdata;
	uint16_t alert_reg = alert_regs_h[td_info->sensor->index];

	if (trip_temp < 1000) {
		dev_err(&tzd->device, "Temperature should be in mC\n");
		return -EINVAL;
	}

	mutex_lock(&thrm_update_lock);

	/* Convert from mC to C */
	trip_temp /= 1000;

	ret = temp_to_adc(td_info->sensor->direct, (int)trip_temp, &adc_val);
	if (ret)
		goto exit;

	ret =  set_tmax(alert_reg, adc_val);
exit:
	mutex_unlock(&thrm_update_lock);
	return ret;
}

static int show_trip_temp(struct thermal_zone_device *tzd,
				int trip, long *trip_temp)
{
	int ret = -EINVAL;
	uint16_t adc_val;
	struct thermal_device_info *td_info = tzd->devdata;
	uint16_t alert_reg_h = alert_regs_h[td_info->sensor->index];

	mutex_lock(&thrm_update_lock);

	adc_val = get_adc_value(alert_reg_h);
	if (adc_val < 0)
		goto exit;

	ret = adc_to_temp(td_info->sensor->direct, adc_val, trip_temp);

exit:
	mutex_unlock(&thrm_update_lock);
	return ret;
}

static int show_trip_type(struct thermal_zone_device *tzd,
			int trip, enum thermal_trip_type *trip_type)
{
	/* All are passive trip points */
	*trip_type = THERMAL_TRIP_PASSIVE;
	return 0;
}

static int update_temp(struct thermal_zone_device *tzd, long *temp)
{
	int ret;
	struct thermal_device_info *td_info = tzd->devdata;
	int indx = td_info->sensor->index;

	if (!tdata->iio_chan)
		return -EINVAL;

	if (!tdata->is_initialized ||
			time_after(jiffies, tdata->last_updated + HZ)) {
		ret = iio_read_channel_all_raw(tdata->iio_chan,
						tdata->cached_vals);
		if (ret == -ETIMEDOUT) {
			dev_err(&tzd->device,
				"ADC sampling failed:%d reading result regs\n",
				ret);
		}
		tdata->last_updated = jiffies;
		tdata->is_initialized = true;
	}

	ret = adc_to_temp(td_info->sensor->direct, tdata->cached_vals[indx],
								temp);
	if (ret)
		return ret;

	if (td_info->sensor->temp_correlation)
		ret = td_info->sensor->temp_correlation(td_info->sensor,
							*temp, temp);
	return ret;
}

static int show_temp(struct thermal_zone_device *tzd, long *temp)
{
	int ret;

	mutex_lock(&thrm_update_lock);

	ret = update_temp(tzd, temp);

	mutex_unlock(&thrm_update_lock);
	return ret;
}

#ifdef CONFIG_DEBUG_THERMAL
static int read_slope(struct thermal_zone_device *tzd, long *slope)
{
	struct thermal_device_info *td_info = tzd->devdata;

	*slope = td_info->sensor->slope;

	return 0;
}

static int update_slope(struct thermal_zone_device *tzd, long slope)
{
	struct thermal_device_info *td_info = tzd->devdata;

	td_info->sensor->slope = slope;

	return 0;
}

static int read_intercept(struct thermal_zone_device *tzd, long *intercept)
{
	struct thermal_device_info *td_info = tzd->devdata;

	*intercept = td_info->sensor->intercept;

	return 0;
}

static int update_intercept(struct thermal_zone_device *tzd, long intercept)
{
	struct thermal_device_info *td_info = tzd->devdata;

	td_info->sensor->intercept = intercept;

	return 0;
}
#endif

static int enable_tm(void)
{
	int ret;
	uint8_t data;

	mutex_lock(&thrm_update_lock);

	ret = intel_scu_ipc_ioread8(THRMMONCTL, &data);
	if (ret)
		goto ipc_fail;

	ret = intel_scu_ipc_iowrite8(THRMMONCTL, data |
					THERM_EN_ACTIVE_MODE |
					THERM_EN_STANDBY_MODE);
	if (ret)
		goto ipc_fail;

ipc_fail:
	mutex_unlock(&thrm_update_lock);
	return ret;
}

static struct thermal_device_info *initialize_sensor(
				struct intel_mid_thermal_sensor *sensor)
{
	struct thermal_device_info *td_info =
		kzalloc(sizeof(struct thermal_device_info), GFP_KERNEL);

	if (!td_info)
		return NULL;

	td_info->sensor = sensor;

	return td_info;
}

static void notify_thermal_event(struct thermal_event te)
{
	int ret;
	long cur_temp;
	char *thrm_event[4];
	struct thermal_zone_device *tzd = tdata->tzd[te.sensor];

	/*
	 * Read the current Temperature and send it to user land;
	 * so that the user space can avoid a sysfs read.
	 */
	ret = update_temp(tzd, &cur_temp);
	if (ret) {
		dev_err(&tzd->device, "Cannot update temperature\n");
		goto exit;
	}
	pr_info("Thermal Event: sensor: %s, cur_temp: %ld, event: %d\n",
				tzd->type, cur_temp, te.event);
	thrm_event[0] = kasprintf(GFP_KERNEL, "NAME=%s", tzd->type);
	thrm_event[1] = kasprintf(GFP_KERNEL, "TEMP=%ld", cur_temp);
	thrm_event[2] = kasprintf(GFP_KERNEL, "EVENT=%d", te.event);
	thrm_event[3] = NULL;

	kobject_uevent_env(&tzd->device.kobj, KOBJ_CHANGE, thrm_event);

	kfree(thrm_event[2]);
	kfree(thrm_event[1]);
	kfree(thrm_event[0]);

exit:
	return;
}

static irqreturn_t thermal_intrpt(int irq, void *dev_data)
{
	int ret;
	unsigned int irq_data;
	uint8_t irq_status;
	struct thermal_event te;
	struct thermal_data *tdata = (struct thermal_data *)dev_data;

	if (!tdata)
		return IRQ_HANDLED;

	mutex_lock(&thrm_update_lock);

	irq_data = ioread8(tdata->thrm_addr + PMIC_SRAM_THRM_OFFSET);

	ret = intel_scu_ipc_ioread8(STHRMIRQ, &irq_status);
	if (ret)
		goto ipc_fail;

	dev_dbg(&tdata->pdev->dev, "STHRMIRQ: %.2x\n", irq_status);

	/*
	 * -1 for invalid interrupt
	 * 1 for LOW to HIGH temperature alert
	 * 0 for HIGH to LOW temperature alert
	 */
	te.event = -1;

	/* Check which interrupt occured and for what event */
	if (irq_data & PMICALRT) {
		te.event = !!(irq_status & PMICALRT);
		te.sensor = PMIC_DIE;
	} else if (irq_data & SYS2ALRT) {
		te.event = !!(irq_status & SYS2ALRT);
		te.sensor = SYS2;
	} else if (irq_data & SYS1ALRT) {
		te.event = !!(irq_status & SYS1ALRT);
		te.sensor = SYS1;
	} else if (irq_data & SYS0ALRT) {
		te.event = !!(irq_status & SYS0ALRT);
		te.sensor = SYS0;
	} else {
		dev_err(&tdata->pdev->dev, "Invalid Interrupt\n");
		ret = IRQ_HANDLED;
		goto ipc_fail;
	}

	if (te.event != -1) {
		dev_info(&tdata->pdev->dev,
			"%s interrupt for thermal sensor %d\n",
			te.event ? "HIGH" : "LOW", te.sensor);

		/* Notify using UEvent */
		notify_thermal_event(te);
	}

	/* Unmask Thermal Interrupt bit:2 in the mask register */
	ret = intel_scu_ipc_iowrite8(MIRQLVL1, MTHERM_IRQ);
	if (ret)
		goto ipc_fail;

ipc_fail:
	mutex_unlock(&thrm_update_lock);
	/* In case of failure, returning IRQ_HANDLED to avoid
	repeated invoke of this function */
	return IRQ_HANDLED;
}

static struct thermal_zone_device_ops tzd_ops = {
	.get_temp = show_temp,
	.get_trip_type = show_trip_type,
	.get_trip_temp = show_trip_temp,
	.set_trip_temp = store_trip_temp,
	.get_trip_hyst = show_trip_hyst,
	.set_trip_hyst = store_trip_hyst,

#ifdef CONFIG_DEBUG_THERMAL
	.get_slope = read_slope,
	.set_slope = update_slope,
	.get_intercept = read_intercept,
	.set_intercept = update_intercept,
#endif
};

static irqreturn_t moor_thermal_intrpt_handler(int irq, void *dev_data)
{
	return IRQ_WAKE_THREAD;
}

static int moor_thermal_probe(struct platform_device *pdev)
{
	int i, size, ret;
	int trips = MOORE_THERMAL_TRIPS;
	int trips_rw = MOORE_TRIPS_RW;
	struct intel_mid_thermal_platform_data *pdata;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "platform data not found\n");
		return -EINVAL;
	}

	tdata = kzalloc(sizeof(struct thermal_data), GFP_KERNEL);
	if (!tdata) {
		dev_err(&pdev->dev, "kzalloc failed\n");
		return -ENOMEM;
	}

	tdata->pdev = pdev;
	tdata->num_sensors = pdata->num_sensors;
	tdata->sensors = pdata->sensors;
	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "platform_get_irq failed:%d\n", ret);
		goto exit_free;
	}
	tdata->irq = ret;

	platform_set_drvdata(pdev, tdata);

	size = sizeof(struct thermal_zone_device *) * tdata->num_sensors;
	tdata->tzd = kzalloc(size, GFP_KERNEL);
	if (!tdata->tzd) {
		dev_err(&pdev->dev, "kzalloc failed\n");
		ret = -ENOMEM;
		goto exit_free;
	}

	/* Program a default _max value for each sensor */
	ret = program_tmax(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Programming _max failed:%d\n", ret);
		goto exit_tzd;
	}

	/*
	 * Register with IIO to sample temperature values.
	 * Order of the channels obtained from adc:
	 * "SYSTHERM0", "SYSTHERM1", "SYSTHERM2", "PMICDIE"
	 */
	tdata->iio_chan = iio_channel_get_all(&pdev->dev);
	if (tdata->iio_chan == NULL) {
		dev_err(&pdev->dev, "tdata->iio_chan is null\n");
		ret = -EINVAL;
		goto exit_tzd;
	}

	/* Check whether we got all the four channels */
	ret = iio_channel_get_num(tdata->iio_chan);
	if (ret != PMIC_THERMAL_SENSORS) {
		dev_err(&pdev->dev, "incorrect number of channels:%d\n", ret);
		ret = -EFAULT;
		goto exit_iio;
	}

	/* Register each sensor with the generic thermal framework */
	for (i = 0; i < tdata->num_sensors; i++) {
		/* PMICDIE has only one trip point while other zones has two */
		if (i == PMIC_DIE)
			trips = trips_rw = 1;

		tdata->tzd[i] = thermal_zone_device_register(
				tdata->sensors[i].name, trips, trips_rw,
				initialize_sensor(&tdata->sensors[i]),
				&tzd_ops, NULL, 0, 0);

		if (IS_ERR(tdata->tzd[i])) {
			ret = PTR_ERR(tdata->tzd[i]);
			dev_err(&pdev->dev,
				"registering thermal sensor %s failed: %d\n",
				tdata->sensors[i].name, ret);
			goto exit_reg;
		}
	}

	tdata->thrm_addr = ioremap_nocache(PMIC_SRAM_BASE_ADDR, IOMAP_SIZE);
	if (!tdata->thrm_addr) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "ioremap_nocache failed\n");
		goto exit_reg;
	}

	/* Register for Interrupt Handler */
	ret = request_threaded_irq(tdata->irq, moor_thermal_intrpt_handler,
					thermal_intrpt, IRQF_TRIGGER_RISING,
					DRIVER_NAME, tdata);
	if (ret) {
		dev_err(&pdev->dev, "request_threaded_irq failed:%d\n", ret);
		goto exit_ioremap;
	}

	/* Enable Thermal Monitoring */
	ret = enable_tm();
	if (ret) {
		dev_err(&pdev->dev, "Enabling TM failed:%d\n", ret);
		goto exit_irq;
	}

	return 0;

exit_irq:
	free_irq(tdata->irq, tdata);
exit_ioremap:
	iounmap(tdata->thrm_addr);
exit_reg:
	while (--i >= 0)
		thermal_zone_device_unregister(tdata->tzd[i]);
exit_iio:
	iio_channel_release_all(tdata->iio_chan);
exit_tzd:
	kfree(tdata->tzd);
exit_free:
	kfree(tdata);
	return ret;
}

static int moor_thermal_resume(struct device *dev)
{
	dev_info(dev, "resume called.\n");
	return 0;
}

static int moor_thermal_suspend(struct device *dev)
{
	dev_info(dev, "suspend called.\n");
	return 0;
}

static int moor_thermal_remove(struct platform_device *pdev)
{
	int i;
	struct thermal_data *tdata = platform_get_drvdata(pdev);

	if (!tdata)
		return 0;

	for (i = 0; i < tdata->num_sensors; i++)
		thermal_zone_device_unregister(tdata->tzd[i]);

	free_irq(tdata->irq, tdata);
	iounmap(tdata->thrm_addr);
	iio_channel_release_all(tdata->iio_chan);
	kfree(tdata->tzd);
	kfree(tdata);
	return 0;
}

/* Driver initialization and finalization */
static const struct dev_pm_ops thermal_pm_ops = {
	.suspend = moor_thermal_suspend,
	.resume = moor_thermal_resume,
};

static struct platform_driver moor_thermal_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.pm = &thermal_pm_ops,
		},
	.probe = moor_thermal_probe,
	.remove = moor_thermal_remove,
};

static int moor_thermal_module_init(void)
{
	return platform_driver_register(&moor_thermal_driver);
}

static void moor_thermal_module_exit(void)
{
	platform_driver_unregister(&moor_thermal_driver);
}

/* RPMSG related functionality */
static int moor_thermal_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	if (!rpdev) {
		pr_err("rpmsg channel not created\n");
		dev_info(&rpdev->dev, "rpmsg channel not created for moor_thermal\n");
		return -ENODEV;
	}
	dev_info(&rpdev->dev, "Probed moor_thermal rpmsg device\n");

	return moor_thermal_module_init();
}

static void moor_thermal_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	moor_thermal_module_exit();
	dev_info(&rpdev->dev, "Removed moor_thermal rpmsg device\n");
}

static void moor_thermal_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
			int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
				data, len, true);
}

static struct rpmsg_device_id moor_thermal_id_table[] = {
	{ .name = "rpmsg_moor_thermal" },
	{ },
};

MODULE_DEVICE_TABLE(rpmsg, moor_thermal_id_table);

static struct rpmsg_driver moor_thermal_rpmsg = {
	.drv.name	= DRIVER_NAME,
	.drv.owner	= THIS_MODULE,
	.probe		= moor_thermal_rpmsg_probe,
	.callback	= moor_thermal_rpmsg_cb,
	.remove		= moor_thermal_rpmsg_remove,
	.id_table	= moor_thermal_id_table,
};

static int __init moor_thermal_rpmsg_init(void)
{
	return register_rpmsg_driver(&moor_thermal_rpmsg);
}

static void __exit moor_thermal_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&moor_thermal_rpmsg);
}

module_init(moor_thermal_rpmsg_init);
module_exit(moor_thermal_rpmsg_exit);

MODULE_AUTHOR("Sumeet Pawnikar<sumeet.r.pawnikar@intel.com>");
MODULE_DESCRIPTION("Intel Moorefield Platform Thermal Driver");
MODULE_LICENSE("GPL");
