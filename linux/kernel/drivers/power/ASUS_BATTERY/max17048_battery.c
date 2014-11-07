/*
 *  max17048_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2012 Nvidia Cooperation
 *  Chandler Zhang <chazhang@nvidia.com>
 *  Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <asm/unaligned.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/max17048_battery.h>
#include <asm/intel_mid_gpadc.h>
#include <asm/intel_mid_thermal.h>

#include "asus_battery.h"

#define MAX17048_VCELL		0x02
#define MAX17048_SOC			0x04
#define MAX17048_MODE		0x06
#define MAX17048_VER			0x08
#define MAX17048_HIBRT		0x0A
#define MAX17048_CONFIG		0x0C
#define MAX17048_OCV			0x0E
#define MAX17048_VLRT		0x14
#define MAX17048_VRESET		0x18
#define MAX17048_STATUS		0x1A
#define MAX17048_UNLOCK		0x3E
#define MAX17048_TABLE		0x40
#define MAX17048_RCOMPSEG1	0x80
#define MAX17048_RCOMPSEG2	0x90
#define MAX17048_CMD			0xFF
#define MAX17048_UNLOCK_VALUE	0x4a57
#define MAX17048_RESET_VALUE	0x5400
#define MAX17048_DELAY		1000
#define MAX17048_BATTERY_FULL	100
#define MAX17048_BATTERY_LOW	15
#define MAX17048_VERSION_NO	0x12

#define NEW_OCV	1
#define FAKE_DEV	1
#define GAUGE_ERR(...)        printk(KERN_ERR "[MAX17048] " __VA_ARGS__);
#define GAUGE_INFO(...)       printk(KERN_INFO "[MAX17048] " __VA_ARGS__);

struct max17048_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		battery;
	struct power_supply		ac;
	struct power_supply		usb;
	struct max17048_battery_model	*model_data;
	/* State Of Connect */
	int ac_online;
	int usb_online;
	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;
	/* battery health */
	int health;
	/* battery capacity */
	int capacity_level;
	int lasttime_vcell;
	int lasttime_soc;
	int lasttime_status;
};

static int max17048_initialize(struct max17048_chip *chip);
static int g_temp=20, batt_id=0; //batt_id: 0=coslight, 1=ATL, 2=LG
static struct max17048_chip *g_max17048_chip;
static struct dev_func max17048_tbl;
static uint16_t vcell_1 = 0, vcell_2 = 0;
struct switch_dev max17048_batt_dev;
static int *bat_adc_value;
static void *bat_adc_handle;

uint8_t max17048_custom_data_1[] = {
	0x97, 0x00, 0xB6, 0x40, 0xB8, 0x40, 0xBA, 0x60,
	0xBB, 0xA0, 0xBC, 0x10, 0xBD, 0x70, 0xBE, 0x80,
	0xBF, 0xF0, 0xC1, 0x60, 0xC3, 0xA0, 0xC5, 0x40,
	0xC7, 0xD0, 0xCC, 0x50, 0xCE, 0x50, 0xD8, 0x30,
	0x00, 0x40, 0x1D, 0x60, 0x1C, 0x00, 0x25, 0x60,
	0x6B, 0xC0, 0x2B, 0xC0, 0x35, 0xE0, 0x21, 0xE0,
	0x21, 0xC0, 0x15, 0xE0, 0x1B, 0x40, 0x11, 0xE0,
	0x0C, 0x40, 0x19, 0x00, 0x0C, 0xE0, 0x0C, 0xE0};

uint8_t max17048_custom_data_2[] = {
	0x9F, 0xD0, 0xB5, 0x70, 0xB7, 0xA0, 0xB9, 0xB0,
	0xBA, 0xA0, 0xBC, 0x40, 0xBC, 0xA0, 0xBD, 0x70,
	0xBF, 0x80, 0xC0, 0xD0, 0xC3, 0x80, 0xC4, 0xF0,
	0xC8, 0x90, 0xCB, 0xD0, 0xD1, 0x90, 0xD6, 0xF0,
	0x00, 0x50, 0x19, 0x00, 0x1D, 0x10, 0x2E, 0x40,
	0x24, 0x30, 0x41, 0x00, 0x56, 0x60, 0x19, 0x10,
	0x38, 0xB0, 0x0D, 0xF0, 0x17, 0xB0, 0x12, 0x80,
	0x11, 0x10, 0x0F, 0x60, 0x0D, 0x00, 0x0D, 0x00};

uint8_t max17048_custom_data_3[] = {
	0xB1, 0x90, 0xB7, 0x20, 0xB8, 0x20, 0xB8, 0xE0,
	0xBA, 0xE0, 0xBB, 0xF0, 0xBC, 0xF0, 0xBD, 0xC0,
	0xBE, 0x90, 0xBF, 0x10, 0xC0, 0xE0, 0xC3, 0xE0,
	0xC6, 0xA0, 0xCA, 0xC0, 0xD0, 0x00, 0xD5, 0xC0,
	0x05, 0x00, 0x16, 0x30, 0x15, 0x00, 0x09, 0x10,
	0x17, 0x00, 0x23, 0x10, 0x19, 0x10, 0x19, 0x10,
	0x14, 0x50, 0x10, 0x50, 0x08, 0xD0, 0x09, 0xE0,
	0x09, 0xF0, 0x07, 0xF0, 0x06, 0xF0, 0x06, 0xF0};

int max17048_adc_volt_table[] = {
	1442, 1439, 1435, 1432, 1428, 1424, 1420, 1416,
	1412, 1407, 1402, 1397, 1392, 1387, 1381, 1375,
	1369, 1363, 1357, 1350, 1343, 1336, 1329, 1321,
	1313, 1305, 1297, 1288, 1279, 1270, 1261, 1251,
	1242, 1232, 1221, 1211, 1200, 1189, 1178, 1167,
	1156, 1144, 1132, 1120, 1108, 1095, 1083, 1070,
	1057, 1044, 1030, 1017, 1004, 990, 976, 962,
	949, 	935, 	921, 	907, 	893, 	878, 	864, 	850,
	836, 	822, 	808, 	794, 	780, 	766, 	752, 	738,
	724, 	711, 	697, 	684, 	671, 	658, 	645, 	632,
	619, 	606, 	594, 	582, 	570, 	558, 	546, 	534,
	523, 	512, 	501, 	490, 	479, 	469, 	458, 	448,
	438, 	429, 	419, 	410, 	400, 	391, 	383, 	374,
	365, 	357, 	349, 	341, 	333, 	326, 	318, 	311,
	304, 	297, 	290, 	283, 	277, 	270, 	264, 	258,
	252, 	246, 	241, 	235, 	230, 	224, 	219, 	214,
	209, 	204, 	200, 	195, 	191, 	186, 	182, 	178,
	174, 	170, 	166, 	162, 	159, 	155, 	152, 	148,
	145, 	142, 	139, 	135, 	132, 	130, 	127,	124,
	121, 	119, 	116, 	114, 	111, 	109, 	106, 	104,
	102, 	100, 	98,  	96,  	94, 92};

int max17048_temp_table[] = {
	-40, -39, -38, -37, -36, -35, -34, -33, -32, -31,
	-30, -29, -28, -27, -26, -25, -24, -23, -22, -21,
	-20, -19, -18, -17, -16, -15, -14, -13, -12, -11,
	-10, -9, -8, -7, -6, -5, -4, -3, -2, -1,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
	60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
	70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
	90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
	100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
	110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
	120, 121, 122, 123, 124, 125};

static int max17048_write_word(struct i2c_client *client, int reg, u16 value)
{
	int ret;
	ret = i2c_smbus_write_word_data(client, reg, swab16(value));
	if (ret < 0)
		dev_err(&client->dev, "%s(): Failed in writing register "
					"0x%02x err %d\n", __func__, reg, ret);
	return ret;
}
static int max17048_read_word(struct i2c_client *client, int reg)
{
	int ret;
	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in reading register "
					"0x%02x err %d\n", __func__, reg, ret);
		return ret;
	} else {
		ret = (int)swab16((uint16_t)(ret & 0x0000ffff));
		return ret;
	}
}

static void max17048_get_vcell(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	int vcell;
	vcell = max17048_read_word(client, MAX17048_VCELL);
	if (vcell < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, vcell);
	else
		chip->vcell = (uint16_t)vcell;

	g_max17048_chip->vcell = chip->vcell;
}
static void max17048_get_soc(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	struct max17048_battery_model *mdata = chip->model_data;
	int soc;
	soc = max17048_read_word(client, MAX17048_SOC);
	if (soc < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, soc);
	else {
		if(batt_id==2)
			mdata->bits = 18;
		if (mdata->bits == 19)
			chip->soc = (uint16_t)soc >> 9;
		else if (mdata->bits == 18)
			chip->soc = (uint16_t)soc >> 8;
	}
	if (chip->soc > MAX17048_BATTERY_FULL) {
		chip->soc = MAX17048_BATTERY_FULL;
		chip->status = POWER_SUPPLY_STATUS_FULL;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (chip->soc < MAX17048_BATTERY_LOW) {
		chip->health = POWER_SUPPLY_HEALTH_DEAD;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	} else {
		chip->health = POWER_SUPPLY_HEALTH_GOOD;
		chip->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}

	g_max17048_chip->soc = chip->soc;
}
static uint16_t max17048_get_version(struct i2c_client *client)
{
	return swab16(i2c_smbus_read_word_data(client, MAX17048_VER));
}

int max17048_read_percentage(void)
{
	int soc=0;

	//GAUGE_INFO("%s start\n", __func__);
	max17048_get_soc(g_max17048_chip->client);
	soc = g_max17048_chip->soc;
	return soc;
}
int max17048_read_current(void)
{
	int curr=0;

	curr += 0x10000;
	return curr;
}
int max17048_read_volt(void)
{
	uint16_t volt;
	
	//GAUGE_INFO("%s start\n", __func__);
	max17048_get_vcell(g_max17048_chip->client);
	volt = g_max17048_chip->vcell;
	volt = ((long)volt * 5120) >> 16 ;
	GAUGE_INFO("%s, volt = %d\n", __func__, volt);
	return volt;
}
int max17048_read_temp(void)
{
	return g_temp*10;
}

static int max17048_update_rcomp(struct max17048_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct max17048_battery_model *mdata = chip->model_data;
	int i = 0, ret = 0, adc_volt=0, tempcoup = -1325, tempcodown = -2925;
	uint8_t new_rcomp = 0, config=0;
	//uint32_t bat_adc_value[1], bat_adc_channel[1];
	//void *bat_adc_handle;
	int vol_temp=0;

	/*read SYSTHERM3 ADC value*/
	ret = get_gpadc_sample(bat_adc_handle, 1, bat_adc_value);
	if(ret) {
		GAUGE_ERR("get_gpadc_sample fail at channel 7: %d\n", ret);
	}else {
		vol_temp = (int)bat_adc_value[1];
		adc_volt = vol_temp*1500/1023;
		GAUGE_INFO("SYSTHERM3 pin ADC value=0x%x, decimal value=%d, voltage=%dmV\n", bat_adc_value[1], vol_temp, adc_volt);

		/* use adc voltage to map temperature*/
		if(adc_volt>max17048_adc_volt_table[0]) {
			GAUGE_ERR("adc voltage too high: %d > %d\n", adc_volt, max17048_adc_volt_table[0]);
			g_temp = max17048_temp_table[0];
		}else if(adc_volt<max17048_adc_volt_table[165]) {
			GAUGE_ERR("adc voltage too low: %d < %d\n", adc_volt, max17048_adc_volt_table[165]);
			g_temp = max17048_temp_table[165];
		}else {
			for(i=0;i<166;i++) {
				if(adc_volt<max17048_adc_volt_table[i])
					g_temp = max17048_temp_table[i];
				else
					break;
			}
		}
	}
	GAUGE_INFO("final temp = %d, in table[%d]\n", g_temp, i);

	if(batt_id==1) {
		mdata->rcomp = 86;
		tempcoup = -825;
		tempcodown = -2950;
	}else if(batt_id==2) {
		mdata->rcomp = 252;
		tempcoup = -3900;
		tempcodown = 0;
	}

	if(batt_id==2)
		mdata->bits = 18;
	if (mdata->bits == 19)
		config = 32 - (mdata->alert_threshold * 2);
	else if (mdata->bits == 18)
		config = 32 - mdata->alert_threshold;
	//config = mdata->one_percent_alerts | config;  //need to confirm
	new_rcomp = (g_temp > 20)?(mdata->rcomp+(g_temp-20)*tempcoup/1000):(mdata->rcomp+(g_temp-20)*tempcodown/1000);
	if (new_rcomp > 0xFF)
		new_rcomp = 0xFF;
	else if (new_rcomp < 0)
		new_rcomp = 0;

	GAUGE_INFO("%s, old_rcomp=0x%x, new_rcomp=0x%x\n", __func__, mdata->rcomp, new_rcomp);
	ret = max17048_write_word(client, MAX17048_CONFIG,
			((new_rcomp << 8) | config));
	if (ret < 0)
		return ret;

	return ret;
}

static int max17048_dump_register(struct max17048_chip *chip)
{
	struct i2c_client *client = chip->client;
	int i = 0;

	GAUGE_INFO("+++++ %s +++++\n", __func__);
	for(i=0;i<16;i++) {
		GAUGE_INFO("0x%x = 0x%x\n", 0x00+i*0x02, max17048_read_word(client, 0x00+i*0x02))
	}
	GAUGE_INFO("0x40 = 0x%x\n", max17048_read_word(client, 0x40))
	GAUGE_INFO("----- %s -----\n", __func__);

	return 0;
}

static void max17048_work(struct work_struct *work)
{
	struct max17048_chip *chip;
	int ret = 0;
	chip = container_of(work, struct max17048_chip, work.work);
	struct i2c_client *client = chip->client;

	max17048_get_vcell(chip->client);
	max17048_get_soc(chip->client);
	if (chip->vcell != chip->lasttime_vcell ||
		chip->soc != chip->lasttime_soc ||
		chip->status !=	chip->lasttime_status) {
		chip->lasttime_vcell = chip->vcell;
		chip->lasttime_soc = chip->soc;
		//power_supply_changed(&chip->battery);
	}
	/* Update RCOMP*/
	ret = max17048_update_rcomp(chip);
	if (ret < 0) {
		GAUGE_ERR("max17048_update_rcomp: err %d\n", ret);
	}
	GAUGE_INFO("%s, original soc=%d, voltage=%d\n", __func__, chip->soc, chip->vcell);
	/*dump registers for debug*/
	max17048_dump_register(chip);
	/* check reset indicator bit*/
	if( (max17048_read_word(client, MAX17048_STATUS)&0x0100)>0 ) {
		GAUGE_INFO("rest bit is 1, someone reset IC, init again\n");
		ret = max17048_initialize(chip);
		if (ret < 0)
			GAUGE_ERR("Error: Initializing fuel-gauge\n");
	}	
	schedule_delayed_work(&chip->work, MAX17048_DELAY);
}

static int max17048_write_rcomp_seg(struct i2c_client *client,
						uint16_t rcomp_seg)
{
	uint8_t rs1, rs2;
	int ret;
	rs2 = rcomp_seg & 0x00FF;
	rs1 = (rcomp_seg >> 8) & 0x00FF;
	uint8_t rcomp_seg_table[16] = { rs1, rs2, rs1, rs2,
					rs1, rs2, rs1, rs2,
					rs1, rs2, rs1, rs2,
					rs1, rs2, rs1, rs2};
	GAUGE_INFO("%s: rs1:0x%x, rs2:0x%x\n", __func__, rs1, rs2);
	ret = i2c_smbus_write_i2c_block_data(client, MAX17048_RCOMPSEG1,
				16, (uint8_t *)rcomp_seg_table);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	ret = i2c_smbus_write_i2c_block_data(client, MAX17048_RCOMPSEG2,
				16, (uint8_t *)rcomp_seg_table);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	return 0;
}
static int max17048_load_model_data(struct max17048_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct max17048_battery_model *mdata = chip->model_data;
	uint16_t soc_tst, ocv;
	uint8_t config=0, *max17048_custom_data;
	int ret = 0, i = 0;

	if(batt_id == 1) {
		GAUGE_INFO("%s: use max17048_custom_data_2\n", __func__);
		max17048_custom_data = max17048_custom_data_2;
	}else if(batt_id == 2) {
		GAUGE_INFO("%s: use max17048_custom_data_3\n", __func__);
		max17048_custom_data = max17048_custom_data_3;
	}else{
		GAUGE_INFO("%s: max17048_custom_data_1\n", __func__);
		max17048_custom_data = max17048_custom_data_1;
	}

	/* C2: read OCV */
	ret = max17048_read_word(client, MAX17048_OCV);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}
	ocv = (uint16_t)ret;
	GAUGE_INFO("line:%d, ocv = 0x%x\n", __LINE__, ocv);
	if (ocv == 0xffff) {
		dev_err(&client->dev, "%s: Failed in unlocking"
					"max17048 err: %d\n", __func__, ocv);
		return -1;
	}
#ifdef NEW_OCV
	if( (vcell_1 > ocv)&&(vcell_1 >= vcell_2) )
		ocv = vcell_1;
	else if( (vcell_2 > ocv)&&(vcell_2 >= vcell_1) )
		ocv = vcell_2;
	GAUGE_INFO("line:%d, final ocv = 0x%x\n", __LINE__, ocv);
#endif
	/* C5: write custom model data */
	GAUGE_INFO("for debug, max17048_custom_data[%d] = 0x%x\n", i, max17048_custom_data[i]);
	for (i = 0; i < 4; i += 1) {
		if (i2c_smbus_write_i2c_block_data(client,
			(MAX17048_TABLE+i*16), 16,
				&max17048_custom_data[i*0x10]) < 0) {
			dev_err(&client->dev, "%s: error writing model data:\n",
								__func__);
			return -1;
		}
	}
	//mdelay(200);
	/* C5.1: Write RCOMPSeg */
	//ret = max17048_write_rcomp_seg(client, mdata->rcomp_seg);
	//if (ret < 0)
	//	return ret;
	//mdelay(200);
	/* C7: Write OCV to Test value */
	if(batt_id==1)
		mdata->ocvtest = 57584;
	else if(batt_id==2)
		mdata->ocvtest = 57280;
	GAUGE_INFO("line:%d, ocvtest = %d\n", __LINE__, mdata->ocvtest);
	ret = max17048_write_word(client, MAX17048_OCV, mdata->ocvtest);
	if (ret < 0)
		return ret;
	/* C7.1: Disable hibernate */
	ret = max17048_write_word(client, MAX17048_HIBRT, 0x0000);
	if (ret < 0)
		return ret;
	/* C7.2: Lock model access */
	ret = max17048_write_word(client, MAX17048_UNLOCK, 0x0000);
	if (ret < 0)
		return ret;
	/* C8: Delay between 150ms to 600ms */
	mdelay(200);
	/* C9: Read SOC Register and Comfirm Value */
	ret = max17048_read_word(client, MAX17048_SOC);
	GAUGE_INFO("line:%d, ret=0x%x\n", __LINE__,  ret);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}
	soc_tst = (uint16_t)ret;
	if(batt_id==1) {
		mdata->soccheck_A = 230;
		mdata->soccheck_B = 232;
	}else if(batt_id==2) {
		mdata->soccheck_A = 116;
		mdata->soccheck_B = 118;
	}
	GAUGE_INFO("line:%d, soccheck_A=%d, soccheck_B=%d, ret=0x%x\n", __LINE__, mdata->soccheck_A, mdata->soccheck_B, ret);
	if (!((soc_tst >> 8) >= mdata->soccheck_A &&
				(soc_tst >> 8) <=  mdata->soccheck_B)) {
		dev_err(&client->dev, "%s: soc comparison failed %d\n",
					__func__, ret);
		//return ret;
	} else {
		dev_info(&client->dev, "MAX17048 Custom data"
						" loading successfull\n");
	}
	/* C9.1: unlock model access */
	ret = max17048_write_word(client, MAX17048_UNLOCK,
					MAX17048_UNLOCK_VALUE);
	if (ret < 0)
		return ret;
	/* C10: Restore CONFIG and OCV */
	if(batt_id==2)
		mdata->bits = 18;
	if (mdata->bits == 19)
		config = 32 - (mdata->alert_threshold * 2);
	else if (mdata->bits == 18)
		config = 32 - mdata->alert_threshold;
	//config = mdata->one_percent_alerts | config;  //need to confirm
	ret = max17048_write_word(client, MAX17048_CONFIG,
			((mdata->rcomp << 8) | config));
	if (ret < 0)
		return ret;
	GAUGE_INFO("line:%d, ocv = 0x%x\n", __LINE__, ocv);
	ret = max17048_write_word(client, MAX17048_OCV, ocv);
	if (ret < 0)
		return ret;
	/* C11: Lock model access */
	ret = max17048_write_word(client, MAX17048_UNLOCK, 0x0000);
	if (ret < 0)
		return ret;
	/* C12: Add delay */
	mdelay(200);
	return ret;
}
static int max17048_initialize(struct max17048_chip *chip)
{
	int ret;
	uint16_t value = 0;
	struct i2c_client *client = chip->client;
	//struct max17048_battery_model *mdata = chip->model_data;

	GAUGE_INFO("%s start\n", __func__);
#if 0
	/* reset IC */
	ret = max17048_write_word(client, 0xFE, MAX17048_RESET_VALUE);
	if (ret < 0)
		GAUGE_ERR("ret<0 when reseting IC = %d\n", ret);
	GAUGE_INFO("reset IC done\n");
#endif
#ifdef NEW_OCV
	vcell_1 = max17048_read_word(client, MAX17048_VCELL);
	if (vcell_1 < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, vcell_1);
	GAUGE_INFO("line:%d, vcell_1 = 0x%x\n", __LINE__, vcell_1);
	mdelay(500);
	vcell_2 = max17048_read_word(client, MAX17048_VCELL);
	if (vcell_2 < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, vcell_2);
	GAUGE_INFO("line:%d, vcell_2 = 0x%x\n", __LINE__, vcell_2);
#endif
	/* check RI bit to determine if load data is needed */
	if( (max17048_read_word(client, MAX17048_STATUS)&0x0100)>0 ) {
		GAUGE_INFO("rest bit is 1, so load model data again\n");
		/* C1: unlock model access */
		ret = max17048_write_word(client, MAX17048_UNLOCK,
			MAX17048_UNLOCK_VALUE);
		if (ret < 0)
			return ret;
		/* C2-10: load model data */
		ret = max17048_load_model_data(chip);
		if (ret < 0) {
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
			return ret;
		}
	}else
		GAUGE_INFO("rest bit is 0, no need to load model data\n");
	/* C10.1 Restore your desired value of HIBRT*/
#if 0
	/* Voltage Alert configuration */
	ret = max17048_write_word(client, MAX17048_VLRT, mdata->valert);
	if (ret < 0)
		return ret;
	/* Hibernate configuration */
	ret = max17048_write_word(client, MAX17048_HIBRT, mdata->hibernate);
	if (ret < 0)
		return ret;
	ret = max17048_write_word(client, MAX17048_VRESET, mdata->vreset);
	if (ret < 0)
		return ret;
#endif
	/* clears the reset indicator */
	value = max17048_read_word(client, MAX17048_STATUS);
	if (value < 0)
		GAUGE_ERR("read reset indicator registr fail");
	/* Sets the EnVR bit if selected */
	value = value & 0xFEFF;
	ret = max17048_write_word(client, MAX17048_STATUS, value);
	if (ret < 0)
		GAUGE_ERR("write reset indicator registr fail");
	GAUGE_INFO("%s done\n", __func__);
	return 0;
}

static ssize_t batt_switch_name(struct switch_dev *sdev, char *buf)
{
	if(batt_id==1)
		return sprintf(buf, "%s\n", "110:645:3-448a-Cg:P011-110:645:0-S");
	else if(batt_id==2)
		return sprintf(buf, "%s\n", "110:645:3-448a-9g:P016-110:645:0-S");
	else
		return sprintf(buf, "%s\n", "110:645:3-448a-Jg:P010-110:645:0-S");
}
#ifdef FAKE_DEV
static int max17048_misc_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int max17048_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long max17048_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static struct file_operations max17048_fops = {
  .owner = THIS_MODULE,
  .open = max17048_misc_open,
  .release = max17048_misc_release,
  .unlocked_ioctl = max17048_misc_ioctl
};

struct miscdevice max17048_misc = {
  .minor = MISC_DYNAMIC_MINOR,
  .name = "ug31xx",
  .fops = &max17048_fops
};
#endif
static int __devinit max17048_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct max17048_chip *chip;
	int ret;
	uint16_t version;
	u32 test_major_flag=0;
	struct asus_bat_config bat_cfg;
	int *bat_adc_channel;
	int vol_temp=0;

	//turn to jiffeys
	bat_cfg.polling_time = 0;
	bat_cfg.critical_polling_time = 0;
	bat_cfg.polling_time *= HZ;
	bat_cfg.critical_polling_time *= HZ;

	GAUGE_INFO("%s start\n", __func__);
	ret = i2c_smbus_read_word_data(client, 0x08);
	GAUGE_INFO("%s and ret = %d\n", __func__, ret);
	if(ret<0)
		return ret;
	else {
		//init battery info & work queue
		ret = asus_battery_init(bat_cfg.polling_time, bat_cfg.critical_polling_time, test_major_flag);
		if (ret)
			GAUGE_ERR("asus_battery_init fail\n");
	}

	bat_adc_channel = kzalloc((sizeof(int) * 2),GFP_KERNEL);
	bat_adc_value = kzalloc((sizeof(int) * 2),GFP_KERNEL);
	
	bat_adc_channel[0] = 0x06 | CH_NEED_VREF | CH_NEED_VCALIB;
	bat_adc_channel[1] = 0x07 | CH_NEED_VREF | CH_NEED_VCALIB;
	bat_adc_handle = gpadc_alloc_channels(2, bat_adc_channel);
	if(!bat_adc_handle) {
		GAUGE_ERR("gpadc_alloc_channels fail\n");
	}
	else {
		/*read battery ID*/
		ret = get_gpadc_sample(bat_adc_handle, 1, bat_adc_value);
		if(ret) {
			GAUGE_ERR("get_gpadc_sample fail at channel 6: %d\n", ret);
		}else {
			vol_temp = (int)bat_adc_value[0];
			GAUGE_INFO("battery ID pin ADC value=0x%x, decimal value=%d, voltage=%dmV\n", bat_adc_value[0], vol_temp, vol_temp*1500/1023);
		}
	}
	if(vol_temp > 915) { //1.343V
		GAUGE_INFO("%s: battery = COSLIGHT\n", __func__);
		batt_id = 0;
	}else if(vol_temp > 778) { //1.141V
		GAUGE_INFO("%s: battery = ATL \n", __func__);
		batt_id = 1;
	}else{
		GAUGE_INFO("%s: battery = LG \n", __func__);
		batt_id = 2;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->client = client;
	chip->model_data = client->dev.platform_data;
	chip->ac_online = 0;
	chip->usb_online = 0;
	i2c_set_clientdata(client, chip);
	version = max17048_get_version(client);
	//if (version != MAX17048_VERSION_NO) {
	//	GAUGE_ERR("%s fail in read max17048 version: %d\n", __func__, version);
	//	ret = -ENODEV;
	//	goto error2;
	//}
	dev_info(&client->dev, "MAX17048 Fuel-Gauge Ver 0x%x\n", version);
	ret = max17048_initialize(chip);
	if (ret < 0) {
		dev_err(&client->dev, "Error: Initializing fuel-gauge\n");
		goto error2;
	}
	g_max17048_chip = chip;
	INIT_DELAYED_WORK(&chip->work, max17048_work);
	schedule_delayed_work(&chip->work, MAX17048_DELAY);

	max17048_tbl.read_percentage = max17048_read_percentage;
	max17048_tbl.read_current = max17048_read_current;
	max17048_tbl.read_volt = max17048_read_volt;
	max17048_tbl.read_temp = max17048_read_temp;
	ret = asus_register_power_supply(&client->dev, &max17048_tbl);
	if (ret)
                GAUGE_ERR("asus_register_power_supply fail\n");

	/* register switch device for battery information versions report */
	max17048_batt_dev.name = "battery";
	max17048_batt_dev.print_name = batt_switch_name;
	if (switch_dev_register(&max17048_batt_dev) < 0)
		GAUGE_ERR("%s: fail to register battery switch\n", __func__);

	kfree(bat_adc_channel);
#ifdef FAKE_DEV
	/*make dev/ug31xx node*/
	if(misc_register(&max17048_misc) < 0)
	{
		GAUGE_ERR("[%s] Unable to register misc deive\n", __func__);
		misc_deregister(&max17048_misc);
	}
#endif
	GAUGE_INFO("%s done\n", __func__);
	return 0;

error2:
	kfree(chip);
	return ret;
}
static int __devexit max17048_remove(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	cancel_delayed_work(&chip->work);
	kfree(bat_adc_value);
	intel_mid_gpadc_free(bat_adc_handle);
	kfree(chip);
	misc_deregister(&max17048_misc);
	return 0;
}
#ifdef CONFIG_PM
static int max17048_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	cancel_delayed_work(&chip->work);
	return 0;
}
static int max17048_resume(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	schedule_delayed_work(&chip->work, MAX17048_DELAY);
	return 0;
}
#else
#define max17048_suspend NULL
#define max17048_resume NULL
#endif /* CONFIG_PM */
static const struct i2c_device_id max17048_id[] = {
	{ "max17048", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17048_id);
static struct i2c_driver max17048_i2c_driver = {
	.driver	= {
		.name	= "max17048",
	},
	.probe		= max17048_probe,
	.remove		= __devexit_p(max17048_remove),
	.suspend		= max17048_suspend,
	.resume		= max17048_resume,
	.id_table		= max17048_id,
};
static int __init max17048_init(void)
{
	return i2c_add_driver(&max17048_i2c_driver);
}
module_init(max17048_init);
static void __exit max17048_exit(void)
{
	i2c_del_driver(&max17048_i2c_driver);
}
module_exit(max17048_exit);
MODULE_AUTHOR("Chandler Zhang <chazhang@nvidia.com>");
MODULE_DESCRIPTION("MAX17048 Fuel Gauge");
MODULE_LICENSE("GPL");

