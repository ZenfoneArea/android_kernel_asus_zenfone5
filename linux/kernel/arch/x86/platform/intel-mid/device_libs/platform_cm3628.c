
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/lnw_gpio.h>
#include <asm/intel-mid.h>
#include "platform_cm3628.h"
#include <linux/cm36283.h>
#include <linux/HWVersion.h>
extern int Read_PROJ_ID(void);

void *accel_cm3628_platform_data(void *info)
{
                static struct CM36283_platform_data accel_data_cm3628 = {

                    //2. Assign the interrupt pin number
                    //modify the element in cm3628_pdata structure
                    // .intr = get_gpio_by_name("accel_int"),
                    .levels = { 0x0A, 0xA0, 0xE1, 0x140, 0x280,
                        0x500, 0xA28, 0x16A8, 0x1F40, 0x2800},                  
                    .power = NULL,
                    .slave_addr = CM36283_slave_add,
					// Modify threshold settings for new PS initial setting.
					.ps_close_thd_set = 0x55,        
                    .ps_away_thd_set = 0x49,
                    .ls_cmd = CM36283_ALS_IT_160ms | CM36283_ALS_GAIN_2,     
                   
                    
                    .ps_conf1_val = CM36283_PS_ITB_1 | CM36283_PS_DR_1_320 | CM36283_PS_IT_1_6T | 
					                CM36283_PS_PERS_2 | CM36283_PS_RES_1 |CM36283_PS_INT_IN_AND_OUT,
              		
					.ps_conf3_val = CM36283_PS_MS_NORMAL | CM36283_PS_PROL_255 | CM36283_PS_SMART_PERS_ENABLE,      
                };


                accel_data_cm3628.intr = 77;//get_gpio_by_name("als_int");

                return &accel_data_cm3628;
       
}

