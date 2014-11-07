/*
 * platform_hm2056.c: hm2056 platform data initilization file
 *
 * (C) Copyright 2013 Intel
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipcutil.h>
#include <media/v4l2-subdev.h>
#include <linux/regulator/consumer.h>
#include <linux/sfi.h>
#include <linux/lnw_gpio.h>
#include "platform_camera.h"
#include "platform_hm2056.h"
#include "platform_mt9e013.h"

#define VPROG1_VAL 1800000
static int camera_reset;
static int camera_power_down;
static int camera_vprog1_on;

static struct regulator *vprog1_reg;

static int hm2056_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
    printk("%s: ++\n",__func__);

	if (flag){
		gpio_set_value(camera_reset, 1);
        printk("<<< camera_reset = 1\n");
        mdelay(10);
		gpio_set_value(camera_reset, 0);
        mdelay(10);
		gpio_set_value(camera_reset, 1);
    }
    else{
        gpio_set_value(camera_power_down, 1);
        printk("<<< camera_power_down = 1\n");
        mdelay(10);
		gpio_set_value(camera_reset, 0);
        printk("<<< camera_reset = 0\n");
    }
    mdelay(1);

	return 0;
}

static int hm2056_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
    v4l2_err(sd, "%s: ++\n",__func__);
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM1, flag ? clock_khz : 0);
}

static int hm2056_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int reg_err, ret;

    printk("%s: ++\n",__func__);

    if (camera_reset < 0) {
        ret = camera_sensor_gpio(GP_CORE_077, "SUB_CAM_RST#", GPIOF_DIR_OUT, 0);
        if (ret < 0){
            printk("camera_reset not available.\n");
            return ret;
        }
        camera_reset = ret;
    }
    printk("<< camera_reset:%d, flag:%d\n", camera_reset, flag);

    if (camera_power_down < 0) {
        ret = camera_sensor_gpio(GP_CORE_080, "SUB_CAM_PWDN", GPIOF_DIR_OUT, 0);
        if (ret < 0){
            printk("camera_power_down not available.\n");
            return ret;
        }
        camera_power_down = ret;
    }
    printk("<< camera_power_down:%d, flag:%d\n", camera_power_down, flag);

    if (flag){
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

        msleep(2);

    }else{
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
        if (camera_power_down >= 0){
            gpio_set_value(camera_power_down, 0);
            printk("<<< camera_power_down = 0\n");
            msleep(1);
        }
    }
    return 0;
}

static int hm2056_csi_configure(struct v4l2_subdev *sd, int flag)
{
    static const int LANES = 1;
    return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_SECONDARY, LANES,
            ATOMISP_INPUT_FORMAT_RAW_8, atomisp_bayer_order_grbg, flag);
}

static int hm2056_platform_init(struct i2c_client *client)
{
    int ret;

    printk("%s: ++\n", __func__);

    //VPROG1 for 1.8V and 2.8V
    vprog1_reg = regulator_get(&client->dev, "vprog1");
    if (IS_ERR(vprog1_reg)) {
        dev_err(&client->dev, "regulator_get failed\n");
        return PTR_ERR(vprog1_reg);
    }
    ret = regulator_set_voltage(vprog1_reg, VPROG1_VAL, VPROG1_VAL);
    if (ret) {
        dev_err(&client->dev, "regulator voltage set failed\n");
        regulator_put(vprog1_reg);
    }
    return ret;
}

static int hm2056_platform_deinit(void)
{
    regulator_put(vprog1_reg);
    //regulator_put(vemmc1_reg);
    return 0;
}

static struct camera_sensor_platform_data hm2056_sensor_platform_data = {
    .gpio_ctrl	 = hm2056_gpio_ctrl,
    .flisclk_ctrl	 = hm2056_flisclk_ctrl,
    .power_ctrl	 = hm2056_power_ctrl,
    .csi_cfg	 = hm2056_csi_configure,
    .platform_init   = hm2056_platform_init,
    .platform_deinit = hm2056_platform_deinit,
};

void *hm2056_platform_data(void *info)
{
    camera_reset = -1;
    camera_power_down = -1;
    return &hm2056_sensor_platform_data;
}

