/*
 * platform_compass.c: compass platform data initilization file
 *
 * (C) Copyright 2013
 * Author : cheng_kao
 *
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/lnw_gpio.h>
#include <asm/intel-mid.h>
#include "platform_akm09911.h"
#include <linux/akm09911.h>
void *akm09911_platform_data(void *info)
{

	static struct akm09911_platform_data akm09911_pdata;
//	akm09911_pdata.gpio_DRDY	= get_gpio_by_name("COMP_DRDY");
	akm09911_pdata.gpio_RSTN	= get_gpio_by_name("ECOMP_RST#");
	return &akm09911_pdata;	
	
}
