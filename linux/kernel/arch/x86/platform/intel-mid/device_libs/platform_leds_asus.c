/*
 * setup of GPIO control of LEDs and buttons on Asus 500CG.
 *
 *
 * Copyright (C) 2013 Asus
 * Written by Chih-Hsuan Chang <chih-hsuan_chang@asus.com>

 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>

static struct led_info a500cg_leds[] = {
	{
		.name = "red",
	},
	{
		.name = "green",
	},
};

static struct led_platform_data a500cg_leds_data = {
	.num_leds = ARRAY_SIZE(a500cg_leds),
	.leds = a500cg_leds,
};

static struct platform_device a500cg_leds_dev = {
	.name = "leds-asus",
	.id = -1,
	.dev.platform_data = &a500cg_leds_data,
};

static struct __initdata platform_device *a500cg_devs[] = {
	&a500cg_leds_dev,
};

static void __init register_a500cg(void)
{
	/* Setup LED control through leds-gpio driver */
	platform_add_devices(a500cg_devs, ARRAY_SIZE(a500cg_devs));
}

static int __init a500cg_init(void)
{
	register_a500cg();

	return 0;
}

module_init(a500cg_init);

MODULE_AUTHOR("Chih-Hsuan Chang <chih-hsuan_chang@asus.com>");
MODULE_LICENSE("GPL");
