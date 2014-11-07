/*
 * platform_gc0339.c: gc0339 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
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
#include <linux/mfd/intel_mid_pmic.h>
#include <linux/regulator/consumer.h>
#include "platform_camera.h"
#include "platform_gc0339.h"

#define VPROG1_VAL 1800000

static int camera_reset;
static int camera_power_down;
static int camera_vprog1_on;

static struct regulator *vprog1_reg;

static int gc0339_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret = 0;

	printk("%s: ++\n",__func__);

	if (camera_reset < 0) {
		ret = camera_sensor_gpio(GP_CORE_077, "SUB_CAM_RST#", GPIOF_DIR_OUT, 0);
		if (ret < 0){
		    printk("camera_reset not available.\n");
		    return ret;
		}
		camera_reset = ret;
	}
	if (camera_power_down < 0) {
		ret = camera_sensor_gpio(GP_CORE_080, "SUB_CAM_PWDN", GPIOF_DIR_OUT, 0);
		if (ret < 0){
		    printk("camera_power_down not available.\n");
		    return ret;
		}
		camera_power_down = ret;
	}

	if (flag) {
		gpio_set_value(camera_reset, 1);
		usleep_range(150, 200);
	} else {
		gpio_set_value(camera_reset, 1);
		msleep(1);
		gpio_set_value(camera_power_down, 0);
		msleep(1);
	}

	if (camera_reset >= 0){
		gpio_free(camera_reset);
		camera_reset = -1;
	}

	if (camera_power_down >= 0){
		gpio_free(camera_power_down);
		camera_power_down = -1;
	}

	return 0;
}

static int gc0339_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
	int ret = 0;
	printk("%s: ++\n",__func__);
	ret = intel_scu_ipc_osc_clk(OSC_CLK_CAM1, flag ? clock_khz : 0);
	msleep(5);
	return ret;
}

static int gc0339_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret = 0;
	int reg_err = 0;

	printk("%s: ++\n",__func__);

	if (camera_reset < 0) {
		ret = camera_sensor_gpio(GP_CORE_077, "SUB_CAM_RST#", GPIOF_DIR_OUT, 0);
		if (ret < 0){
		    printk("camera_reset not available.\n");
		    return ret;
		}
		camera_reset = ret;
	}

	if (camera_power_down < 0) {
		ret = camera_sensor_gpio(GP_CORE_080, "SUB_CAM_PWDN", GPIOF_DIR_OUT, 0);
		if (ret < 0){
		    printk("camera_power_down not available.\n");
		    return ret;
		}
		camera_power_down = ret;
	}

	printk("<< camera_reset:%d, camera_power_down:%d, flag:%d\n", camera_reset, camera_power_down, flag);

	if (flag) {

		if (camera_reset >= 0){
			gpio_set_value(camera_reset, 0);
			printk("<<< camera_reset = 0\n");
			msleep(1);
		}
		if (camera_power_down >= 0){
			gpio_set_value(camera_power_down, 0);
			printk("<<< camera_power_down = 0\n");
			msleep(1);
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
			msleep(1);
		}

		msleep(2); //wait vprog1 from enable to 90% (max:2000us)

	} else {

		//turn off power 1.8V and 2.8V
		if (camera_vprog1_on) {
			camera_vprog1_on = 0;
			reg_err = regulator_disable(vprog1_reg);
			if (reg_err) {
				printk(KERN_ALERT "Failed to disable regulator vprog1\n");
				return reg_err;
			}
			printk("<<< 1.8V and 2.8V = 0\n");
			msleep(1);
		}


	}

	if (camera_reset >= 0){
		gpio_free(camera_reset);
		camera_reset = -1;
	}

	if (camera_power_down >= 0){
		gpio_free(camera_power_down);
		camera_power_down = -1;
	}

	return 0;
}

static int gc0339_csi_configure(struct v4l2_subdev *sd, int flag)
{
	static const int LANES = 1;
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_SECONDARY, LANES,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_rggb, flag);
}

static int gc0339_platform_init(struct i2c_client *client)
{
        int ret = 0;

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

        return ret;
}

static int gc0339_platform_deinit(void)
{
	regulator_put(vprog1_reg);
}

static struct camera_sensor_platform_data gc0339_sensor_platform_data = {
	.gpio_ctrl       = gc0339_gpio_ctrl,
	.flisclk_ctrl    = gc0339_flisclk_ctrl,
	.power_ctrl      = gc0339_power_ctrl,
	.csi_cfg         = gc0339_csi_configure,
	.platform_init   = gc0339_platform_init,
	.platform_deinit = gc0339_platform_deinit,
};

void *gc0339_platform_data(void *info)
{
	camera_reset = -1;
	camera_power_down = -1;

	return &gc0339_sensor_platform_data;
}
