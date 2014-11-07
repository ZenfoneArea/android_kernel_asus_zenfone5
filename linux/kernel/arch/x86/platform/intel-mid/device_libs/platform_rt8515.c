/*
 * platform_rt8515c: rt8515 platform data initilization file
 *
 * Chung-Yi (chung-yi_chou@asus.com)
 */

#include <linux/init.h>
#include <linux/types.h>
#include <asm/intel-mid.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <media/rt8515.h>

#include "platform_rt8515.h"

#define RT8515_FLASH_ENF 		161
#define RT8515_FLASH_ENT  		159


static u8 rt8515_hw_status;

static int rt8515_ENF;
static int rt8515_ENT;

static int rt8515_on(void)
{
	rt8515_ENF = get_gpio_by_name("FLED_DRIVER_ENF");
	rt8515_ENT = get_gpio_by_name("FLED_DRIVER_ENT");
	if (rt8515_ENF== -1) {
		pr_err("[FLASH] %s: Unable to find RT8515_FLASH_ENF\n", __func__);
		rt8515_ENF = RT8515_FLASH_ENF;
		pr_err("[FLASH] rt8515_ENF = %d \n", RT8515_FLASH_ENF);		
	}
	if (rt8515_ENT == -1) {
		pr_err("[FLASH] %s: Unable to find RT8515_FLASH_ENT\n", __func__);
		rt8515_ENT = RT8515_FLASH_ENT;
		pr_err("[FLASH] rt8515_ENT = %d \n", RT8515_FLASH_ENT);		
	}
	return 0;
}


static int rt8515_off(void)
{

	gpio_direction_output(rt8515_ENF, 0);
	gpio_direction_output(rt8515_ENT, 0);
	
 	gpio_free(rt8515_ENF);
	gpio_free(rt8515_ENT);

	return 0;
}

static void rt8515_flash_on(void)
{
 	gpio_direction_output(rt8515_ENF, 1);
	gpio_direction_output(rt8515_ENT, 0);
	printk(KERN_DEBUG "[FLASH] RT8515_FLASH_ENF (%d) is set to 1 \n",rt8515_ENF);
	rt8515_hw_status = (RT8515_HW_FLASH_MODE | RT8515_HW_FLASH_IS_ON);
}

static void rt8515_flash_off(unsigned long dummy)
{
	gpio_direction_output(rt8515_ENF, 0);
	gpio_direction_output(rt8515_ENT, 0);
	printk(KERN_DEBUG "[FLASH] RT8515_FLEN_GPIO (%d) is set to 0 \n",rt8515_ENF);
	//rt8515_hw_status = 0;
}

static void rt8515_torch_on(u8 data)
{
	// start with low state and leave high when done sending
	gpio_set_value(rt8515_ENF, 0);
	gpio_set_value(rt8515_ENT, 0);
	udelay(5); 
	

	printk(KERN_DEBUG "[FLASH] Settings RT8515_FLASH_ENT (%d) is set from 0 to 1 %d times for s2c communication \n",rt8515_ENT,data);
	while(data){
		gpio_set_value(rt8515_ENT, 0);
		udelay(1);
		gpio_set_value(rt8515_ENT, 1);
		udelay(1);
		data--;
	}
    	udelay(5);
	
	//rt8515_hw_status = (RT8515_HW_FLASH_IS_ON);
}




static int rt8515_update_hw(struct v4l2_int_device *s)
{
	return rt8515_hw_status;
}



struct rt8515_platform_data platform_rt8515_data = {
	.init		= rt8515_on,
	.exit		= rt8515_off,
	.flash_on	= rt8515_flash_on,
	.torch_on   = rt8515_torch_on,
	.flash_off	= rt8515_flash_off,
	.update_hw	= rt8515_update_hw,
};


void *rt8515_platform_data_func(void *info)
{
	printk("[FLASH] rt8515_platform_data_func\n");
	return &platform_rt8515_data;
}
