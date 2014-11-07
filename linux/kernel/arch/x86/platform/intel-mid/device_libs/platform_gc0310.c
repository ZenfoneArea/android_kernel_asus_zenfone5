/*
 * platform_gc0310.c: gc0310 platform data initilization file
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
#include "platform_gc0310.h"

#define VPROG1_VAL 1800000
static int camera_reset;
static int camera_power_down;
static int camera_vprog1_on;

static struct regulator *vprog1_reg;

static int gc0310_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret = 0;

	printk("%s: ++\n",__func__);

	int camera_power_down_local = camera_power_down;
    printk(KERN_INFO "camera_power_down_local is %d\n", camera_power_down_local);
   	if (camera_power_down_local < 0) {
		ret = camera_sensor_gpio(GP_CORE_080, "SUB_CAM_PWDN", GPIOF_DIR_OUT, 0);
		if (ret < 0){
		    printk("camera_power_down not available.\n");
		    return ret;
		}
        camera_power_down = camera_power_down_local = ret;
	}

	if (flag) {
		  gpio_set_value(camera_power_down_local, 0);
		  msleep(1);
          gpio_set_value(camera_power_down_local, 1);
          usleep_range(10, 20);	
          gpio_set_value(camera_power_down_local, 0);
	} else {
	       gpio_set_value(camera_power_down, 1);
		   usleep_range(10, 20);	
	}
    return 0;
}

static int gc0310_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
	int ret = 0;
	printk("%s: ++\n",__func__);
	ret = intel_scu_ipc_osc_clk(OSC_CLK_CAM1, flag ? clock_khz : 0);
	msleep(1);

	return ret;
}

static int gc0310_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret = 0;
	int reg_err = 0;

	printk("%s: ++\n",__func__);

	if (flag) {
       
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

	return 0;
}

static int gc0310_csi_configure(struct v4l2_subdev *sd, int flag)
{
	static const int LANES = 1;
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_SECONDARY, LANES,
		ATOMISP_INPUT_FORMAT_RAW_8, atomisp_bayer_order_rggb, flag);
}

static int gc0310_platform_init(struct i2c_client *client)
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

static int gc0310_platform_deinit(void)
{
	regulator_put(vprog1_reg);
}

static struct camera_sensor_platform_data gc0310_sensor_platform_data = {
	.gpio_ctrl       = gc0310_gpio_ctrl,
	.flisclk_ctrl    = gc0310_flisclk_ctrl,
	.power_ctrl      = gc0310_power_ctrl,
	.csi_cfg         = gc0310_csi_configure,
	.platform_init   = gc0310_platform_init,
	.platform_deinit = gc0310_platform_deinit,
};

void *gc0310_platform_data(void *info)
{
	camera_reset = -1;
	camera_power_down = -1;

	return &gc0310_sensor_platform_data;
}
