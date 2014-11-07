/*
 * platform_mn34130.c: mn34130 platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
#include <asm/intel_scu_ipcutil.h>
#include <asm/intel-mid.h>
#include <media/v4l2-subdev.h>
#include <linux/regulator/consumer.h>
#include <linux/sfi.h>
#include "platform_camera.h"
#include "platform_mn34130.h"
#include <linux/lnw_gpio.h>

#define VPROG1_VAL 1800000
#define VPROG2_VAL 1200000
#define VEMMC1_VAL 2850000

static int camera_reset;
static int camera_vprog1_on;
static int camera_vprog2_on;
static int camera_vemmc1_on;

static struct regulator *vprog1_reg;
static struct regulator *vprog2_reg;
static struct regulator *vemmc1_reg;

/*
 * CLV primary camera sensor - MN34130 platform data
 */

static int mn34130_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;

	printk("%s: ++\n",__func__);

	if (camera_reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_0_RESET, GPIOF_DIR_OUT, 0);
		if (ret < 0){
			printk("camera_reset not available.\n");
			return ret;
		}
		camera_reset = ret;
	}

	if (flag) {
		gpio_set_value(camera_reset, 1);
		printk("<<< camera_reset = 1\n");
		usleep_range(100, 200);
	} else {
		gpio_set_value(camera_reset, 0);
		printk("<<< camera_reset = 0\n");
		usleep_range(250, 350);
	}

	return 0;
}

static int mn34130_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
	v4l2_err(sd, "%s: ++\n",__func__);
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM0, flag ? clock_khz : 0);
}

static int mn34130_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int reg_err;
	int ret;

	printk("%s: ++\n",__func__);

	if (camera_reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_0_RESET, GPIOF_DIR_OUT, 0);
		if (ret < 0){
			printk("camera_reset not available.\n");
			return ret;
		}
		camera_reset = ret;
	}

	printk("<< camera_reset:%d, flag:%d\n", camera_reset, flag);

	if (flag){

		if (camera_reset >= 0){
			gpio_set_value(camera_reset, 0);
			printk("<<< camera_reset = 0\n");
		}

		//turn on VCM power 2.85V
		if (!camera_vemmc1_on) {
			camera_vemmc1_on = 1;
			reg_err = regulator_enable(vemmc1_reg);
			if (reg_err) {
				printk(KERN_ALERT "Failed to enable regulator vemmc1\n");
				return reg_err;
			}
			printk("<<< VCM 2.85V = 1\n");
		}

		//turn on power 1.8V and 2.8V
		if (!camera_vprog1_on) {
			camera_vprog1_on = 1;
			reg_err = regulator_enable(vprog1_reg);
			if (reg_err) {
				printk(KERN_ALERT "Failed to enable regulator vprog1\n");
				return reg_err;
			}
			printk("<<< 1.8V and 2.8V = 1\n");
		}

		//turn on power 1.2V
		if (!camera_vprog2_on) {
			camera_vprog2_on = 1;
			reg_err = regulator_enable(vprog2_reg);
			if (reg_err) {
				printk(KERN_ALERT "Failed to enable regulator vprog2\n");
				return reg_err;
			}
			printk("<<< 1.2V = 1\n");
		}

		usleep_range(2000, 2100); //wait vprog1 and vprog2 from enable to 90% (max:2000us)

	}else{

		if (camera_reset >= 0){
			gpio_free(camera_reset);
			camera_reset = -1;
		}

		//turn off power 1.2V
		if (camera_vprog2_on) {
			camera_vprog2_on = 0;
			reg_err = regulator_disable(vprog2_reg);
			if (reg_err) {
				printk(KERN_ALERT "Failed to disable regulator vprog2\n");
				return reg_err;
			}
			printk("<<< 1.2V = 0\n");
		}

		//turn off power 1.8V and 2.8V
		if (camera_vprog1_on) {
			camera_vprog1_on = 0;
			reg_err = regulator_disable(vprog1_reg);
			if (reg_err) {
				printk(KERN_ALERT "Failed to disable regulator vprog1\n");
				return reg_err;
			}
			printk("<<< 1.8V and 2.8V = 0\n");
		}

		//turn off VCM power 2.85V
		if (camera_vemmc1_on) {
			camera_vemmc1_on = 0;
			reg_err = regulator_disable(vemmc1_reg);
			if (reg_err) {
				printk(KERN_ALERT "Failed to disable regulator vemmc1\n");
				return reg_err;
			}
			printk("<<< VCM 2.85V = 0\n");
		}
	}
	return 0;
}

static int mn34130_csi_configure(struct v4l2_subdev *sd, int flag)
{
	static const int lanes = 4;

	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_PRIMARY, lanes,
				 ATOMISP_INPUT_FORMAT_RAW_10,
				 atomisp_bayer_order_grbg, flag);
}

static int mn34130_platform_init(struct i2c_client *client)
{
	int ret;

	printk("%s: ++\n", __func__);

	//VPROG1 for 1.8V and 2.8V
	vprog1_reg = regulator_get(&client->dev, "vprog1");
	if (IS_ERR(vprog1_reg)) {
		dev_err(&client->dev, "vprog1 failed\n");
		return PTR_ERR(vprog1_reg);
	}
	ret = regulator_set_voltage(vprog1_reg, VPROG1_VAL, VPROG1_VAL);
	if (ret) {
		dev_err(&client->dev, "vprog1 set failed\n");
		regulator_put(vprog1_reg);
	}

	//VPROG2 for 1.2V
	vprog2_reg = regulator_get(&client->dev, "vprog2");
	if (IS_ERR(vprog2_reg)) {
		dev_err(&client->dev, "vprog2 failed\n");
		return PTR_ERR(vprog2_reg);
	}
	ret = regulator_set_voltage(vprog2_reg, VPROG2_VAL, VPROG2_VAL);
	if (ret) {
		dev_err(&client->dev, "vprog2 set failed\n");
		regulator_put(vprog2_reg);
	}

	//VEMMC1 for VCM, 2.85V
	vemmc1_reg = regulator_get(&client->dev, "vemmc1");
	if (IS_ERR(vemmc1_reg)) {
		dev_err(&client->dev, "vemmc1 failed\n");
		return PTR_ERR(vemmc1_reg);
	}

	return ret;
}

static int mn34130_platform_deinit(void)
{
	regulator_put(vprog1_reg);
	regulator_put(vprog2_reg);
	regulator_put(vemmc1_reg);

	return 0;
}
static struct camera_sensor_platform_data mn34130_sensor_platform_data = {
	.gpio_ctrl       = mn34130_gpio_ctrl,
	.flisclk_ctrl    = mn34130_flisclk_ctrl,
	.power_ctrl      = mn34130_power_ctrl,
	.csi_cfg         = mn34130_csi_configure,
	.platform_init   = mn34130_platform_init,
	.platform_deinit = mn34130_platform_deinit,
};

void *mn34130_platform_data(void *info)
{
	camera_reset = -1;

	return &mn34130_sensor_platform_data;
}
