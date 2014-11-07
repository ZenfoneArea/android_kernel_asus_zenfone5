/* drivers/input/misc/CM36283.c - CM36283 optical sensors driver
 *    
 * Copyright (C) 2012 Capella Microsystems Inc.
 *                                       
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/lightsensor.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

//#include <asm/mach-types.h>

#include <linux/cm36283.h>
#include <linux/capella_cm3602.h>
#include <asm/setup.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/HWVersion.h>
extern int Read_PROJ_ID(void);


#define D(x...) pr_info(x)
#define sensitivity_20  20
#define sensitivity_25  25

static int sensitivity_x = 20;
static bool newold = 0 ; //20140523 Eve_Wen default = 0 means cm36283

//<-- ASUS-Bevis_Chen + -->

bool enLSensorConfig_flag =0 ;
bool enPSensorConfig_flag =0 ;



int LSensor_CALIDATA[2] = {0}; //input calibration data . Format : "200 lux -->lux value ; 1000 lux -->lux value"
int PSensor_CALIDATA[2] = {0}; //input calibration data . Format : "near 3cm :--> value ; far  5cm :--> value"
struct proc_dir_entry *lpsensor_entry = NULL;

static int asus_lpsensor_status_read(char *buffer, char **buffer_location,

							off_t offset, int buffer_length, int *eof, void *data)

{	

	if(!lpsensor_entry)
      	return sprintf(buffer, "-1\n");

	else
    	return sprintf(buffer, "1\n");;

}

/***************

int create_asusproc_lpsensor_status_entry( void )

{   

   lpsensor_entry = create_proc_entry("asus_lpsensor_status", S_IWUGO| S_IRUGO, NULL);

    if (!lpsensor_entry)

        return -ENOMEM;

   	lpsensor_entry->read_proc = asus_lpsensor_status_read;

    	return 0;

}

**********************/

//<-- ASUS-Bevis_Chen - -->

//=========================================

//     Calibration Formula:

//     y = f(x) 

//  -> ax - by = constant_k

//     a is f(x2) - f(x1) , b is x2 - x1

////=========================================            

int static calibration_light(int x_big, int x_small, int report_lux){        

                    int y_1000 = 1000;
                    int y_200 = 200;
                    int constant_k = (y_1000 - y_200)*x_small - (x_big - x_small)*y_200; 

					    if ( report_lux*(y_1000 - y_200) < constant_k){
                              return 0; 
                        }else {   
                             return ((report_lux*(y_1000 - y_200) - constant_k) / (x_big - x_small));			
                        }
}

#define I2C_RETRY_COUNT 10
#define NEAR_DELAY_TIME ((100 * HZ) / 1000)
#define CONTROL_INT_ISR_REPORT        0x00
#define CONTROL_ALS                   0x01
#define CONTROL_PS                    0x02
#define LS_RAWDATA_WINDOW             (10)  // 0.05 step/lux

static int record_init_fail = 0;
static void sensor_irq_do_work(struct work_struct *work);
static void light_sensor_initial_value_work_routine(struct work_struct *work);
static void proximity_initial_value_work_routine(struct work_struct *work);
static DECLARE_WORK(sensor_irq_work, sensor_irq_do_work);
static DECLARE_DELAYED_WORK(light_sensor_initial_value_work, light_sensor_initial_value_work_routine);
static DECLARE_DELAYED_WORK(proximity_initial_value_work, proximity_initial_value_work_routine);

struct CM36283_info {
	struct class *CM36283_class;
	struct device *ls_dev;
	struct device *ps_dev;
	struct input_dev *ls_input_dev;
	struct input_dev *ps_input_dev;
	struct early_suspend early_suspend;
	struct i2c_client *i2c_client;
	struct workqueue_struct *lp_wq;
	int intr_pin;
	int als_enable;
	int ps_enable;
	int ps_irq_flag;
	uint16_t *adc_table;
	uint16_t cali_table[10];
	int irq;
	int ls_calibrate;
	int (*power)(int, uint8_t); /* power to the chip */
	uint32_t als_kadc_cm36283;
	uint32_t als_gadc;
	uint16_t golden_adc;
	struct wake_lock ps_wake_lock;
	int psensor_opened;
	int lightsensor_opened;
	uint8_t slave_addr;
	uint16_t ps_close_thd_set;
	uint16_t ps_away_thd_set;	
	int current_level;
	uint16_t current_adc;
	uint16_t ps_conf1_val;
	uint16_t ps_conf3_val;
	uint16_t ls_cmd;
	uint8_t record_clear_int_fail;
	uint8_t psensor_sleep_becuz_suspend;
	uint8_t lsensor_sleep_becuz_early_suspend;
	uint8_t status_calling;
	int last_initial_report_lux;

};

struct CM36283_info *lp_info_cm36283;
#define DEBUG_VEMMC2
#ifdef DEBUG_VEMMC2
#include <linux/regulator/driver.h>
#include <linux/regulator/intel_pmic.h> 
#include <linux/mfd/intel_msic.h>

static struct device* vemmc2_userCtrl_dev;
static struct class* vemmc2_userCtrl_class;
static struct class* gpio_userCtrl_class;
static struct device* gpio_userCtrl_dev;

static struct regulator *vemmc2_reg;
static ssize_t vemmc2_reg_show(struct device *dev,
            struct device_attribute *attr, char *buf){

/*	

    int reg_err;	

	reg_err = reg_err = intel_msic_reg_write(VEMMC2CNT_ADDR, 0x07);

	if(!reg_err)

	     D("VEMMC2_cm32863 ---regulator write success !!\n");

    else 

     	D("VEMMC2_cm32863 ---regulator write fail !!\n");	

*/

/*



     struct CM36283_info *lpi = lp_info_cm36283;

     struct input_dev* find_ps_input_dev_ptr = lpi->ls_input_dev;

	 struct list_head node_start = find_ps_input_dev_ptr->node;

	 char* self_name = find_ps_input_dev_ptr->name;	 

	 list_for_each_entry_continue( find_ps_input_dev_ptr, 

	                              &node_start,

								   node ){ 

        D("input_dev is %s!!\n",find_ps_input_dev_ptr->name);								   

         if (find_ps_input_dev_ptr->name =="proximit"){

               D("find ps_input_dev success !!!\n");		

               break;   			   

		}

		

		if (find_ps_input_dev_ptr->name == self_name){

	           D("find ps_input_dev fail !!\n");	

               break;			   

	    }

	}/config/wifi

*/     

/*



        int projid;

        projid = Read_PROJ_ID();

        D("projid = %d\n", projid);

*/



/*

	int res=0;

    FILE  *sysfsfp;

    int* var;

	char *filename = "/config/wifi/psensor.txt";

    sysfsfp = fopen(filename, "r");

    if (sysfsfp!=NULL) {

        fscanf(sysfsfp, "%d\n", var);

        fclose(sysfsfp);

    } else {

        ALOGE("open file %s to read with error %d", filename, res);

    }

    D(" read test value is %d\n", var);

*/

return 0;
}

static ssize_t vemmc2_reg_store(struct device *dev,
                   struct device_attribute *attr,
                   const char *buf, size_t count){
	struct CM36283_info *lpi = lp_info_cm36283;			    
    int vemmc2_en, reg_err; 
	vemmc2_en = -1;
	sscanf(buf, "%d", &vemmc2_en);

	if (vemmc2_en != 0 && vemmc2_en != 1
		&& vemmc2_en != 2 && vemmc2_en != 3 && vemmc2_en != 4)
		return -EINVAL;


	if (vemmc2_en==0) {
		D("[proximity-Touch test] %s: report =%d\n",
			__func__, vemmc2_en);

		 input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, 0);   
		 input_sync(lpi->ps_input_dev);  

	}else if(vemmc2_en==1){

 		D("[proximity-Touch test] %s: report =%d\n",
			__func__, vemmc2_en);

		 input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, 1);   
		 input_sync(lpi->ps_input_dev);  

    }else if(vemmc2_en==2){

 		D("[proximity-Touch test] %s: report =%d\n",
			__func__, vemmc2_en);

		 input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, 2);   
		 input_sync(lpi->ps_input_dev);  

    }else if(vemmc2_en==3){

 		D("[proximity-Touch test] %s: report =%d\n",
			__func__, vemmc2_en);

		 input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, 3);   
		 input_sync(lpi->ps_input_dev);  
    }

return count;
}
DEVICE_ATTR(vemmc2_ctrl, 0664, vemmc2_reg_show, vemmc2_reg_store);


static ssize_t gpio_test_tool_show(struct device *dev,
              struct device_attribute *attr, char *buf){

     return 0;
}


static ssize_t gpio_test_tool_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count){

     int gpio_num, reg_err; 
         gpio_num = -1;
         sscanf(buf, "%d", &gpio_num);
         gpio_free(gpio_num);

return count;
}


DEVICE_ATTR(gpio_ctrl, 0664, gpio_test_tool_show, gpio_test_tool_store);

#endif // DEBUG_VEMMC2


int fLevel_cm36283=-1;

static struct mutex als_enable_mutex, als_disable_mutex, als_get_adc_mutex;
static struct mutex ps_enable_mutex, ps_disable_mutex, ps_get_adc_mutex;
static struct mutex CM36283_control_mutex;
static int lightsensor_disable(struct CM36283_info *lpi);
static int initial_CM36283(struct CM36283_info *lpi);
static void psensor_initial_cmd(struct CM36283_info *lpi);
int32_t als_kadc_cm36283;
static int control_and_report(struct CM36283_info *lpi, uint8_t mode, uint16_t param);
static int I2C_RxData(uint16_t slaveAddr, uint8_t cmd, uint8_t *rxData, int length)
{
	uint8_t loop_i;
	int val;
	struct CM36283_info *lpi = lp_info_cm36283;
    uint8_t subaddr[1];
  subaddr[0] = cmd;	

	struct i2c_msg msgs[] = {
		{
		 .addr = slaveAddr,
		 .flags = 0,
		 .len = 1,
		 .buf = subaddr,
		 },

		{
		 .addr = slaveAddr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
		 },		 
	};

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {
		if (i2c_transfer(lp_info_cm36283->i2c_client->adapter, msgs, 2) > 0)
			break;

		val = gpio_get_value(lpi->intr_pin);
		/*check intr GPIO when i2c error*/
		if (loop_i == 0 || loop_i == I2C_RETRY_COUNT -1)
			D("[PS][CM36283 error] %s, i2c err, slaveAddr 0x%x ISR gpio %d  = %d, record_init_fail %d \n",
				__func__, slaveAddr, lpi->intr_pin, val, record_init_fail);
		msleep(10);
	}

	if (loop_i >= I2C_RETRY_COUNT) {
		printk(KERN_ERR "[PS_ERR][CM36283 error] %s retry over %d\n",
			__func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}



static int I2C_TxData(uint16_t slaveAddr, uint8_t *txData, int length)

{
	uint8_t loop_i;
	int val;
	struct CM36283_info *lpi = lp_info_cm36283;
	struct i2c_msg msg[] = {
		{
		 .addr = slaveAddr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
		 },
	};

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {
		if (i2c_transfer(lp_info_cm36283->i2c_client->adapter, msg, 1) > 0)
			break;

		val = gpio_get_value(lpi->intr_pin);
		/*check intr GPIO when i2c error*/
		if (loop_i == 0 || loop_i == I2C_RETRY_COUNT -1)
			D("[PS][CM36283 error] %s, i2c err, slaveAddr 0x%x, value 0x%x, ISR gpio%d  = %d, record_init_fail %d\n",
				__func__, slaveAddr, txData[0], lpi->intr_pin, val, record_init_fail);


		msleep(10);
	}


	if (loop_i >= I2C_RETRY_COUNT) {
		printk(KERN_ERR "[PS_ERR][CM36283 error] %s retry over %d\n",
			__func__, I2C_RETRY_COUNT);

		return -EIO;
	}

	return 0;
}

static int _CM36283_I2C_Read_Word(uint16_t slaveAddr, uint8_t cmd, uint16_t *pdata)
{
	uint8_t buffer[2];
	int ret = 0;

	if (pdata == NULL)
		return -EFAULT;

	ret = I2C_RxData(slaveAddr, cmd, buffer, 2);

	if (ret < 0) {
		pr_err(
			"[PS_ERR][CM36283 error]%s: I2C_RxData fail [0x%x, 0x%x]\n",
			__func__, slaveAddr, cmd);

		return ret;
	}

	*pdata = (buffer[1]<<8)|buffer[0];
#if 0
	/* Debug use */
	printk(KERN_DEBUG "[CM36283] %s: I2C_RxData[0x%x, 0x%x] = 0x%x\n",

		__func__, slaveAddr, cmd, *pdata);
#endif
	return ret;
}


static int _CM36283_I2C_Write_Word(uint16_t SlaveAddress, uint8_t cmd, uint16_t data)
{
	char buffer[3];
	int ret = 0;
#if 0
	/* Debug use */
	printk(KERN_DEBUG
	"[CM36283] %s: _CM36283_I2C_Write_Word[0x%x, 0x%x, 0x%x]\n",
		__func__, SlaveAddress, cmd, data);
#endif

	buffer[0] = cmd;
	buffer[1] = (uint8_t)(data&0xff);
	buffer[2] = (uint8_t)((data&0xff00)>>8);	

	ret = I2C_TxData(SlaveAddress, buffer, 3);
	if (ret < 0) {
		pr_err("[PS_ERR][CM36283 error]%s: I2C_TxData fail\n", __func__);
		return -EIO;
	}

	return ret;
}

static int get_ls_adc_value(uint16_t *als_step, bool resume)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	uint32_t tmpResult;
	int ret = 0;

	if (als_step == NULL)
		return -EFAULT;

	/* Read ALS data: */
	ret = _CM36283_I2C_Read_Word(lpi->slave_addr, ALS_DATA, als_step);
	if (ret < 0) {
		pr_err(
			"[LS][CM36283 error]%s: _CM36283_I2C_Read_Word fail\n",
			__func__);
		return -EIO;
	}

    if (!lpi->ls_calibrate) {
		tmpResult = (uint32_t)(*als_step) * lpi->als_gadc / lpi->als_kadc_cm36283;
		
        if (tmpResult > 0xFFFF)
			*als_step = 0xFFFF;
		else
		  *als_step = tmpResult;  			


	}
	D("[LS][CM36283] %s: raw adc = 0x%X, ls_calibrate = %d\n",
		__func__, *als_step, lpi->ls_calibrate);

	return ret;
}

static int set_lsensor_range(uint16_t low_thd, uint16_t high_thd)
{
	int ret = 0;
	struct CM36283_info *lpi = lp_info_cm36283;
	_CM36283_I2C_Write_Word(lpi->slave_addr, ALS_THDH, high_thd);
	_CM36283_I2C_Write_Word(lpi->slave_addr, ALS_THDL, low_thd);
	return ret;
}

static int get_ps_adc_value(uint16_t *data)
{
	int ret = 0;
	struct CM36283_info *lpi = lp_info_cm36283;

	if (data == NULL)
		return -EFAULT;	

	ret = _CM36283_I2C_Read_Word(lpi->slave_addr, PS_DATA, data);
        if(newold == 0)//20140522Eve
        {
                (*data) &= 0xFF;
                printk("Eve_Wen Psensor in cm36283\n");
        }
                if (ret < 0) {
		pr_err(
			"[PS][CM36283 error]%s: _CM36283_I2C_Read_Word fail\n",
			__func__);
		return -EIO;

	}else{
		pr_err(
			"[PS][CM36283 OK]%s: _CM36283_I2C_Read_Word OK 0x%x\n",
			__func__, *data);
	}

	return ret;
}

static uint16_t mid_value(uint16_t value[], uint8_t size)
{
	int i = 0, j = 0;
	uint16_t temp = 0;

	if (size < 3)
		return 0;


	for (i = 0; i < (size - 1); i++)
		for (j = (i + 1); j < size; j++)
			if (value[i] > value[j]) {
				temp = value[i];
				value[i] = value[j];
				value[j] = temp;
			}

	return value[((size - 1) / 2)];
}

static int get_stable_ps_adc_value(uint16_t *ps_adc)
{
	uint16_t value[3] = {0, 0, 0}, mid_val = 0;
	int ret = 0;
	int i = 0;
	int wait_count = 0;
	struct CM36283_info *lpi = lp_info_cm36283;

	for (i = 0; i < 3; i++) {
		/*wait interrupt GPIO high*/
		while (gpio_get_value(lpi->intr_pin) == 0) {
			msleep(10);
			wait_count++;

			if (wait_count > 12) {
				pr_err("[PS_ERR][CM36283 error]%s: interrupt GPIO low,"
					" get_ps_adc_value\n", __func__);
				return -EIO;
			}
		}

		ret = get_ps_adc_value(&value[i]);
		if (ret < 0) {
			pr_err("[PS_ERR][CM36283 error]%s: get_ps_adc_value\n",
				__func__);
			return -EIO;
		}

		if (wait_count < 60/10) {/*wait gpio less than 60ms*/
			msleep(60 - (10*wait_count));
		}

		wait_count = 0;
	}
	/*D("Sta_ps: Before sort, value[0, 1, 2] = [0x%x, 0x%x, 0x%x]",
		value[0], value[1], value[2]);*/
	mid_val = mid_value(value, 3);
	D("Sta_ps: After sort, value[0, 1, 2] = [0x%x, 0x%x, 0x%x]",
		value[0], value[1], value[2]);
	*ps_adc = (mid_val & 0xFF);

	return 0;
}

static void proximity_initial_value_work_routine(struct work_struct *work){
        struct CM36283_info *lpi = lp_info_cm36283;
		uint16_t ps_adc_value_init;
        int ret;
        ret = get_ps_adc_value(&ps_adc_value_init);
        if(!ret){
             if(ps_adc_value_init > lpi->ps_close_thd_set){
			       input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, 0);     
	               input_sync(lpi->ps_input_dev);    
			       D("[PS][CM36283] proximity initial NEAR\n");
			 }else{
			       input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, 1);     
	               input_sync(lpi->ps_input_dev);    
			       D("[PS][CM36283] proximity initial FAR\n");  
			 }
        }

         enable_irq(lpi->irq);		
}

static void light_sensor_initial_value_work_routine(struct work_struct *work){
         int ret;
		 uint16_t adc_value;
         uint16_t ls_low_thd, ls_high_thd;
		 struct CM36283_info *lpi = lp_info_cm36283;
         get_ls_adc_value(&adc_value, 0);

		 if (adc_value > LS_RAWDATA_WINDOW){
		      ls_low_thd = adc_value - LS_RAWDATA_WINDOW;
		  }else{
		      ls_low_thd = 0;
		  }

		  if (adc_value < 0xFFFF - LS_RAWDATA_WINDOW){
		      ls_high_thd = adc_value + LS_RAWDATA_WINDOW;    
		  }else{
		      ls_high_thd = 0xFFFF -1 ;
		  }

/*		  
    	  ret = set_lsensor_range(((i == 0) || (adc_value == 0)) ? 0 :
    		   	*(lpi->cali_table + (i - 1)) + 1,
    		    *(lpi->cali_table + i));
 */
//////// modified to increase sensitivity of lux interrupt 
           ret = set_lsensor_range(ls_low_thd,ls_high_thd);
		   //<-- ASUS-Bevis_Chen + -->
		   int report_lux = adc_value;

           if(enLSensorConfig_flag == 1 ){//calibration enable 
					if(LSensor_CALIDATA[0] > 0&&LSensor_CALIDATA[1] > 0 
					&&LSensor_CALIDATA[1] >LSensor_CALIDATA[0] ){ //in case of zero divisor error

					D("CM36283---Before 1st calibration, report_lux is %d\n", report_lux);
                    report_lux = calibration_light(LSensor_CALIDATA[1], LSensor_CALIDATA[0], report_lux);
                    D("CM36283---After 1st calibration, report_lux is %d\n", report_lux);
					report_lux = report_lux/sensitivity_x;
			        D("CM36283---After 1st devided by 20, report_lux is %d\n", report_lux);	
					}else{
       					printk("%s:ASUS input LSensor_CALIDATA was invalid .error !!!!!\n",__func__);
				    }

			}else{	
#ifndef CONFIG_ASUS_FACTORY_MODE // in user build and no calibration data			   
                   report_lux = report_lux*15;
				   D("CM36283--- NO calibration data, use default config\n"); 
#else
	               D("CM36283--- NO calibration data, Factory branch , NO default config\n"); 				  
#endif // end of CONFIG_ASUS_FACTORY_MODE

                   report_lux = report_lux/sensitivity_x;
			       D("CM36283---After devided by 20, report_lux is %d\n", report_lux);	

		//<-- ASUS-Bevis_Chen - -->

                 }			

            if(report_lux %2 == 1)
			       report_lux -= 1;


            if(report_lux == lpi->last_initial_report_lux){
                  report_lux += 2;			  
      		  }	  

			input_report_abs(lpi->ls_input_dev, ABS_MISC,report_lux);
            input_sync(lpi->ls_input_dev);
			lpi->last_initial_report_lux = report_lux;
}

static void sensor_irq_do_work(struct work_struct *work)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	uint16_t intFlag;
    _CM36283_I2C_Read_Word(lpi->slave_addr, INT_FLAG, &intFlag);
 //  D("CM36283-----sensor_irq_do_work, intFlag = 0x%X\n", intFlag);
	control_and_report(lpi, CONTROL_INT_ISR_REPORT, intFlag);  
//	D("CM36283 --- before enable_irq \n");  
	enable_irq(lpi->irq);
//	D("CM36283 --- before wake_unlock \n");
	wake_unlock(&(lpi->ps_wake_lock)); 
//	D("CM36283 --- End of sensor_irq_do_work\n");
}

static irqreturn_t CM36283_irq_handler(int irq, void *data)
{
    D("CM36283 --- Enter into CM36283_irq_handler\n");
	struct CM36283_info *lpi = lp_info_cm36283;
    wake_lock(&(lpi->ps_wake_lock));
	disable_irq_nosync(lpi->irq);
	queue_work(lpi->lp_wq, &sensor_irq_work);

    D("CM36283 --- End of CM36283_irq_handler\n");
	return IRQ_HANDLED;
}

static int als_power(int enable)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	if (lpi->power)
		lpi->power(LS_PWR_ON, 1);

	return 0;
}

static void ls_initial_cmd(struct CM36283_info *lpi)
{	
	/*must disable l-sensor interrupt befrore IST create*//*disable ALS func*/
	lpi->ls_cmd &= CM36283_ALS_INT_MASK;
    lpi->ls_cmd |= CM36283_ALS_SD;
   _CM36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);  
}

static void psensor_initial_cmd(struct CM36283_info *lpi)
{
	/*must disable p-sensor interrupt befrore IST create*//*disable ALS func*/		
	if(newold == 0)
	{
		lpi->ps_conf1_val |= CM36283_PS_SD;
		lpi->ps_conf1_val &= CM36283_PS_INT_MASK;
		
		_CM36283_I2C_Write_Word(lpi->slave_addr, PS_THD, (lpi->ps_close_thd_set <<8)| lpi->ps_away_thd_set);
	}
	else
	{
		/* Modify for only to impact A502cg */
		if (Read_PROJ_ID() == PROJ_ID_A502CG)
		{
			// change initial settings according to vendor's suggestion.
			lpi->ps_conf1_val = 0x03d7;
			lpi->ps_conf3_val = 0x0210;
			
			_CM36283_I2C_Write_Word(lpi->slave_addr, PS_THDL, 130);
			_CM36283_I2C_Write_Word(lpi->slave_addr, PS_THDH, 160);
		}
		else
		{
			lpi->ps_conf1_val |= CM36283_PS_SD;
			lpi->ps_conf1_val &= CM36283_PS_INT_MASK;
		
			_CM36283_I2C_Write_Word(lpi->slave_addr, PS_THDL, lpi->ps_away_thd_set);
			_CM36283_I2C_Write_Word(lpi->slave_addr, PS_THDH, lpi->ps_close_thd_set);
		}
	}
	_CM36283_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val);   
    _CM36283_I2C_Write_Word(lpi->slave_addr, PS_CONF3, lpi->ps_conf3_val);
	
	D("[PS][CM36283] %s, finish\n", __func__);	
}

static int psensor_enable(struct CM36283_info *lpi)
{
	int ret = -EIO;
	mutex_lock(&ps_enable_mutex);
	D("[PS][CM36283] %s\n", __func__);

	if ( lpi->ps_enable ) {
		D("[PS][CM36283] %s: already enabled\n", __func__);
		ret = 0;
	}else{
	// +++	
//     psensor_initial_cmd(lpi);
/*
	_CM36283_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val);   
    _CM36283_I2C_Write_Word(lpi->slave_addr, PS_CONF3, lpi->ps_conf3_val);
	_CM36283_I2C_Write_Word(lpi->slave_addr, PS_THD, (lpi->ps_close_thd_set <<8)| lpi->ps_away_thd_set);
*/		
//    enable_irq(lpi->irq);
// ---	
  	disable_irq_nosync(lpi->irq);
	ret = control_and_report(lpi, CONTROL_PS, 1);
// +++	
/*     
	 input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, 1);     
	 input_sync(lpi->ps_input_dev);  
*/
// ---
    }

	mutex_unlock(&ps_enable_mutex);
	if (ret!=0){
         D("[PS][CM36283] psensor_enable--fail!!!\n");    		
     }else{
	       D("[PS][CM36283] psensor_enable--success!!!\n");
	 }
    
     queue_delayed_work(lpi->lp_wq, &proximity_initial_value_work, 10);  
	 return ret;
}

static int psensor_disable(struct CM36283_info *lpi)
{
	int ret = -EIO;
	mutex_lock(&ps_disable_mutex);
	D("[PS][CM36283] %s\n", __func__);
	if ( lpi->ps_enable == 0 ) {
		D("[PS][CM36283] %s: already disabled\n", __func__);
		ret = 0;
	} else{
  	    ret = control_and_report(lpi, CONTROL_PS,0);
   }
//	disable_irq(lpi->irq);
	mutex_unlock(&ps_disable_mutex); //For next time event be guaranteed to be sent! 
	input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, 4);     
	input_sync(lpi->ps_input_dev);    
	return ret;
}

static int psensor_open(struct inode *inode, struct file *file)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	D("[PS][CM36283] %s\n", __func__);
	if (lpi->psensor_opened)
		return -EBUSY;

	lpi->psensor_opened = 1;
	return 0;
}

static int psensor_release(struct inode *inode, struct file *file)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	D("[PS][CM36283] %s\n", __func__);
	lpi->psensor_opened = 0;

	return psensor_disable(lpi);
	//return 0;
}

// Add to modify xtalk value when enabling p-sensor.
static int psensor_modi_xtalk(struct CM36283_info *lpi, int set)
{
	int ret = 0;
	uint16_t ps_raw;
	
	D("[PS][CM36283] %s, set = %d\n", __func__, set);
	if (set)
	{
		msleep(100);
		ret = get_ps_adc_value(&ps_raw);
		if (!ret)
		{
			D("[PS][CM36283] ps_raw = %d\n", ps_raw);
			_CM36283_I2C_Write_Word(lpi->slave_addr, PS_CANC, ps_raw);
		}
	}
	else
	{
		D("[PS][CM36283] clean PS_CANC reg.\n");
		_CM36283_I2C_Write_Word(lpi->slave_addr, PS_CANC, 0);
	}
	
	return ret;
}

static long psensor_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int val;
	int rc ;
	int ret;
	struct CM36283_info *lpi = lp_info_cm36283;
	char enPcalibration_flag = 0 ;
	void __user *argp = (void __user *)arg;
	D("[PS][CM36283] %s cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case CAPELLA_CM3602_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg))
			return -EFAULT;

		if (val){
			ret = psensor_enable(lpi);
			if (ret)
				return ret;
			ret = psensor_modi_xtalk(lpi, 1);
			return ret; 
		}else{
			ret = psensor_disable(lpi);
			if (ret)
				return ret;
			ret = psensor_modi_xtalk(lpi, 0);
			return ret;
		}

		break;
	case CAPELLA_CM3602_IOCTL_GET_ENABLED:
		return put_user(lpi->ps_enable, (unsigned long __user *)arg);
		break;
	//<----------- ASUS-Bevis_Chen + --------------->
	/*case ASUS_PSENSOR_IOCTL_START:	
	        printk("%s:ASUS ASUS_PSENSOR_IOCTL_START  \n", __func__);
	        break;

      case ASUS_PSENSOR_IOCTL_CLOSE:				
	 	  printk("%s:ASUS ASUS_PSENSOR_IOCTL_CLOSE \n", __func__);
	        break;*/
	case ASUS_PSENSOR_IOCTL_GETDATA:
		printk("%s:ASUS ASUS_PSENSOR_IOCTL_GETDATA \n", __func__);
		rc = 0 ;
		uint16_t ps_adc_value = 0;	
		int ret=0;
		ret = get_ps_adc_value(&ps_adc_value);
		if (ret < 0) {
				printk("%s:ASUS failed to get_ps_adc_value. \n",__func__);
				rc = -EIO;
				goto pend;
		}

	    if ( copy_to_user(argp, &ps_adc_value, sizeof(ps_adc_value) ) ) {
	            printk("%s:ASUS failed to copy psense data to user space.\n",__func__);
				rc = -EFAULT;			
				goto pend;
	    }		

		printk("%s:ASUS_PSENSOR_IOCTL_GETDATA end\n", __func__);
		break;
	case ASUS_PSENSOR_SETCALI_DATA:
		printk("%s:ASUS ASUS_PSENSOR_SETCALI_DATA \n", __func__);
		rc = 0 ;
		memset(PSensor_CALIDATA, 0, 2*sizeof(int));
		if (copy_from_user(PSensor_CALIDATA, argp, sizeof(PSensor_CALIDATA))){
			rc = -EFAULT;
			goto pend;
		}	

		printk("%s:ASUS_PSENSOR SETCALI_DATA : PSensor_CALIDATA[0] :  %d ,PSensor_CALIDATA[1]:  %d \n", 
			__func__, PSensor_CALIDATA[0],PSensor_CALIDATA[1]);

		if(PSensor_CALIDATA[0] <= 0||PSensor_CALIDATA[1] <= 0 
			||PSensor_CALIDATA[0] <= PSensor_CALIDATA[1] )
			rc =  -EINVAL;
			

	 	lpi->ps_away_thd_set = PSensor_CALIDATA[1] ;
		lpi->ps_close_thd_set = PSensor_CALIDATA[0];
		D("enPSensorConfig_flag is 1, ps_close_thd_set = 0x%x, ps_away_thd_set = 0x%x\n",lpi->ps_close_thd_set, lpi->ps_away_thd_set);
		if(newold == 0)
		{
   	    	        _CM36283_I2C_Write_Word(lpi->slave_addr, PS_THD, (lpi->ps_close_thd_set <<8)| lpi->ps_away_thd_set);
		}
		else
		{
			_CM36283_I2C_Write_Word(lpi->slave_addr, PS_THDL, lpi->ps_away_thd_set);
                        _CM36283_I2C_Write_Word(lpi->slave_addr, PS_THDH, lpi->ps_close_thd_set); 
		}
		break;

	case ASUS_PSENSOR_EN_CALIBRATION:
		printk("%s:ASUS ASUS_PSENSOR_EN_CALIBRATION \n", __func__);
		rc = 0 ;
		if (copy_from_user(&enPcalibration_flag , argp, sizeof(enPcalibration_flag ))){
			rc = -EFAULT;
			goto pend;
		}	
		enPSensorConfig_flag =  enPcalibration_flag ;
		printk("%s: ASUS_PSENSOR_EN_CALIBRATION : enPSensorConfig_flag is : %d  \n",__func__,enPSensorConfig_flag); 
		break;		
	//<----------- ASUS-Bevis_Chen - ------------->
	default:
		pr_err("[PS][CM36283 error]%s: invalid cmd %d\n",
			__func__, _IOC_NR(cmd));
		return -EINVAL;
	}

	pend:
 		return rc;
}

static const struct file_operations psensor_fops = {
	.owner = THIS_MODULE,
	.open = psensor_open,
	.release = psensor_release,
	.unlocked_ioctl = psensor_ioctl
};

struct miscdevice psensor_misc_cm36283 = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "cm3602",
	.fops = &psensor_fops
};

void lightsensor_set_kvalue_cm36283(struct CM36283_info *lpi)
{
	if (!lpi) {
		pr_err("[LS][CM36283 error]%s: ls_info is empty\n", __func__);
		return;
	}

	D("[LS][CM36283] %s: ALS calibrated als_kadc_cm36283=0x%x\n",
			__func__, als_kadc_cm36283);

	if (als_kadc_cm36283 >> 16 == ALS_CALIBRATED)
		lpi->als_kadc_cm36283 = als_kadc_cm36283 & 0xFFFF;
	else{
		lpi->als_kadc_cm36283 = 0;
		D("[LS][CM36283] %s: no ALS calibrated\n", __func__);
	}

	if (lpi->als_kadc_cm36283 && lpi->golden_adc > 0) {
		lpi->als_kadc_cm36283 = (lpi->als_kadc_cm36283 > 0 && lpi->als_kadc_cm36283 < 0x1000) ?
				lpi->als_kadc_cm36283 : lpi->golden_adc;
		lpi->als_gadc = lpi->golden_adc;
	}else{
		lpi->als_kadc_cm36283 = 1;
		lpi->als_gadc = 1;
	}

	D("[LS][CM36283] %s: als_kadc_cm36283=0x%x, als_gadc=0x%x\n",
		__func__, lpi->als_kadc_cm36283, lpi->als_gadc);
}

static int lightsensor_update_table(struct CM36283_info *lpi)
{
	uint32_t tmpData[10];
	int i;
	for (i = 0; i < 10; i++) {
		tmpData[i] = (uint32_t)(*(lpi->adc_table + i))
				* lpi->als_kadc_cm36283 / lpi->als_gadc ;

		if( tmpData[i] <= 0xFFFF ){
               lpi->cali_table[i] = (uint16_t) tmpData[i];		
         }else{
               lpi->cali_table[i] = 0xFFFF;    
          }         

		D("[LS][CM36283] %s: Calibrated adc_table: data[%d], %x\n",
			__func__, i, lpi->cali_table[i]);

	}

	return 0;
}

static int lightsensor_enable(struct CM36283_info *lpi)
{
	int ret = -EIO;
	mutex_lock(&als_enable_mutex);
	D("[LS][CM36283] %s\n", __func__);

	if (lpi->als_enable) {
		D("[LS][CM36283] %s: already enabled\n", __func__);
		ret = 0;
	}else{
       ret = control_and_report(lpi, CONTROL_ALS, 1);
    }

	mutex_unlock(&als_enable_mutex);
	return ret;
}

static int lightsensor_disable(struct CM36283_info *lpi)
{
	int ret = -EIO;
	mutex_lock(&als_disable_mutex);
	D("[LS][CM36283] %s\n", __func__);

	if ( lpi->als_enable == 0 ) {
		D("[LS][CM36283] %s: already disabled\n", __func__);
		ret = 0;
	}else{
        ret = control_and_report(lpi, CONTROL_ALS, 0);
    }

	mutex_unlock(&als_disable_mutex);
	return ret;
}

static int lightsensor_open(struct inode *inode, struct file *file)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	int rc = 0;
	D("[LS][CM36283] %s\n", __func__);

	if (lpi->lightsensor_opened) {
		pr_err("[LS][CM36283 error]%s: already opened\n", __func__);
		rc = -EBUSY;
	}

	lpi->lightsensor_opened = 1;
	return rc;
}

static int lightsensor_release(struct inode *inode, struct file *file)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	D("[LS][CM36283] %s\n", __func__);
	lpi->lightsensor_opened = 0;
	return 0;
}

static long lightsensor_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
	int rc, val;
	struct CM36283_info *lpi = lp_info_cm36283;
	char encalibration_flag = 0 ;
	void __user *argp = (void __user *)arg;
	/*D("[CM36283] %s cmd %d\n", __func__, _IOC_NR(cmd));*/
	switch (cmd) {
	case LIGHTSENSOR_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg)) {
			rc = -EFAULT;
			break;
		}
		D("[LS][CM36283] %s LIGHTSENSOR_IOCTL_ENABLE, value = %d\n",
			__func__, val);

		rc = val ? lightsensor_enable(lpi) : lightsensor_disable(lpi);
		break;
	case LIGHTSENSOR_IOCTL_GET_ENABLED:
		 val = lpi->als_enable;
	   	 D("[LS][CM36283] %s LIGHTSENSOR_IOCTL_GET_ENABLED, enabled %d\n",
			__func__, val);

		rc = put_user(val, (unsigned long __user *)arg);
		break;
	//<----------- ASUS-Bevis_Chen + --------------->
	/*case ASUS_LIGHTSENSOR_IOCTL_START:	
	        printk("%s:ASUS ASUS_LIGHTSENSOR_IOCTL_START  \n", __func__);
	        break;
	case ASUS_LIGHTSENSOR_IOCTL_CLOSE:				
	 	  printk("%s:ASUS ASUS_LIGHTSENSOR_IOCTL_CLOSE \n", __func__);
	        break;*/
	case ASUS_LIGHTSENSOR_IOCTL_GETDATA:
         printk("%s:ASUS ASUS_LIGHTSENSOR_IOCTL_GETDATA \n", __func__);
		 rc = 0 ;
		 uint16_t adc_value = 0;	
		 mutex_lock(&CM36283_control_mutex);
         get_ls_adc_value(&adc_value, 0);
		 mutex_unlock(&CM36283_control_mutex);

		 int report_lux = adc_value;		
				if(enLSensorConfig_flag == 1 ){//calibration enable 
			  		  if(LSensor_CALIDATA[0] > 0&&LSensor_CALIDATA[1] > 0 
  	                     &&LSensor_CALIDATA[1] >LSensor_CALIDATA[0] ){ //in case of zero divisor error
                            
                              D("CM36283---Before calibration, ASUS_LIGHTSENSOR_IOCTL_GETDATA,  report_lux is %d\n", report_lux);
                              report_lux = calibration_light(LSensor_CALIDATA[1], LSensor_CALIDATA[0], report_lux);
                              D("CM36283---After calibration, ASUS_LIGHTSENSOR_IOCTL_GETDATA, report_lux is %d\n", report_lux);
                              report_lux = report_lux/sensitivity_x;
                              D("CM36283---After devided by 20, ASUS_LIGHTSENSOR_IOCTL_GETDATA, report_lux is %d\n", report_lux);	

                      }else{
					        rc = -EINVAL;
					        printk("%s:ASUS input LSensor_CALIDATA was invalid .error !!!!!\n",__func__);
					       }
              }else{
#ifndef CONFIG_ASUS_FACTORY_MODE // in user build and no calibration data			   
                     report_lux = report_lux*15;
				     D("CM36283--- NO calibration data, use default config\n"); 
#else
	                 D("CM36283--- NO calibration data, Factory branch , NO default config\n"); 	
#endif // end of CONFIG_ASUS_FACTORY_MODE				
				     report_lux = report_lux/sensitivity_x;
                     D("CM36283---After devided by 20, ASUS_LIGHTSENSOR_IOCTL_GETDATA, report_lux is %d\n", report_lux);	
                   }

                if ( copy_to_user(argp, &report_lux, sizeof(report_lux) ) ) {
                       printk("%s:ASUS failed to copy lightsense data to user space.\n",__func__);
                       rc = -EFAULT;			
                       goto end;
	           }		
		printk("%s:ASUS_LIGHTSENSOR_IOCTL_GETDATA end\n", __func__);
		break;
	case ASUS_LIGHTSENSOR_SETCALI_DATA:

		printk("%s:ASUS ASUS_LIGHTSENSOR_SETCALI_DATA \n", __func__);
		rc = 0 ;
		memset(LSensor_CALIDATA, 0, 2*sizeof(int));

		if (copy_from_user(LSensor_CALIDATA, argp, sizeof(LSensor_CALIDATA)))
		{
			rc = -EFAULT;
			goto end;
		}	

		printk("%s:ASUS_LIGHTSENSOR SETCALI_DATA : LSensor_CALIDATA[0] :  %d ,LSensor_CALIDATA[1]:  %d \n", 
			__func__, LSensor_CALIDATA[0],LSensor_CALIDATA[1]);

		if(LSensor_CALIDATA[0] <= 0||LSensor_CALIDATA[1] <= 0 
			||LSensor_CALIDATA[0] >= LSensor_CALIDATA[1] )
			rc =  -EINVAL;


		break;
	case ASUS_LIGHTSENSOR_EN_CALIBRATION:
		printk("%s:ASUS ASUS_LIGHTSENSOR_EN_CALIBRATION \n", __func__);
		rc = 0 ;
		if (copy_from_user(&encalibration_flag , argp, sizeof(encalibration_flag )))
		{
			rc = -EFAULT;
			goto end;
		}	
		enLSensorConfig_flag =  encalibration_flag ;

		printk("%s: ASUS_LIGHTSENSOR_EN_CALIBRATION : enLSensorConfig_flag is : %d  \n",__func__,enLSensorConfig_flag); 
		break;		
	//<----------- ASUS-Bevis_Chen - ------------->

	default:
		pr_err("[LS][CM36283 error]%s: invalid cmd %d\n", __func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}

end:
	
    return rc;
}

static const struct file_operations lightsensor_fops = {
	.owner = THIS_MODULE,
	.open = lightsensor_open,
	.release = lightsensor_release,
	.unlocked_ioctl = lightsensor_ioctl
};

static struct miscdevice lightsensor_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lightsensor",
	.fops = &lightsensor_fops
};

static ssize_t ps_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t value;
	int ret;
	struct CM36283_info *lpi = lp_info_cm36283;
	int intr_val;
	intr_val = gpio_get_value(lpi->intr_pin);
	get_ps_adc_value(&value);
	
    ret = sprintf(buf, "ADC[0x%04X], ENABLE = %d, intr_pin = %d\n", value, lpi->ps_enable, intr_val);
/*
	input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, 1);      
    input_sync(lpi->ps_input_dev);   
*/	 
	return ret;
}

static ssize_t ps_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct CM36283_info *lpi = lp_info_cm36283;
    int ps_en; 
	ps_en = -1;
	sscanf(buf, "%d", &ps_en);

	if (ps_en != 0 && ps_en != 1
		&& ps_en != 10 && ps_en != 13 && ps_en != 16)
		return -EINVAL;


	if (ps_en) {
		D("[PS][CM36283] %s: ps_en=%d\n", __func__, ps_en);
		psensor_enable(lpi);
	}else{
		psensor_disable(lpi);
    }

	D("[PS][CM36283] %s\n", __func__);

	return count;
}
static DEVICE_ATTR(ps_adc, 0664, ps_adc_show, ps_enable_store);
unsigned PS_cmd_test_value_cm36283;

static ssize_t ps_parameters_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	struct CM36283_info *lpi = lp_info_cm36283;

	ret = sprintf(buf, "PS_close_thd_set = 0x%x, PS_away_thd_set = 0x%x, PS_cmd_cmd:value = 0x%x\n",
		lpi->ps_close_thd_set, lpi->ps_away_thd_set, PS_cmd_test_value_cm36283);

	return ret;
}

static ssize_t ps_parameters_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	char *token[10];
	int i;
	printk(KERN_INFO "[PS][CM36283] %s\n", buf);
	for (i = 0; i < 3; i++)
		token[i] = strsep((char **)&buf, " ");


	lpi->ps_close_thd_set = simple_strtoul(token[0], NULL, 16);
	lpi->ps_away_thd_set = simple_strtoul(token[1], NULL, 16);	
	PS_cmd_test_value_cm36283 = simple_strtoul(token[2], NULL, 16);

	printk(KERN_INFO "[PS][CM36283]Set PS_close_thd_set = 0x%x, PS_away_thd_set = 0x%x, PS_cmd_cmd:value = 0x%x\n",
		lpi->ps_close_thd_set, lpi->ps_away_thd_set, PS_cmd_test_value_cm36283);

	D("[PS][CM36283] %s\n", __func__);

	return count;
}

static DEVICE_ATTR(ps_parameters, 0664, ps_parameters_show, ps_parameters_store);

static ssize_t ps_conf_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	return sprintf(buf, "PS_CONF1 = 0x%x, PS_CONF3 = 0x%x\n", lpi->ps_conf1_val, lpi->ps_conf3_val);
}

static ssize_t ps_conf_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t count)
{
	int code1, code2;
	struct CM36283_info *lpi = lp_info_cm36283;

	sscanf(buf, "0x%x 0x%x", &code1, &code2);
	D("[PS]%s: store value PS conf1 reg = 0x%x PS conf3 reg = 0x%x\n", __func__, code1, code2);
    lpi->ps_conf1_val = code1;
    lpi->ps_conf3_val = code2;

	_CM36283_I2C_Write_Word(lpi->slave_addr, PS_CONF3, lpi->ps_conf3_val );  
	_CM36283_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val );

	return count;
}

static DEVICE_ATTR(ps_conf, 0664, ps_conf_show, ps_conf_store);

static ssize_t ps_thd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	struct CM36283_info *lpi = lp_info_cm36283;
    if(newold==0)
    {
	uint16_t thd;

        ret = _CM36283_I2C_Read_Word(lpi->slave_addr, PS_THD, &thd); 

        if (!ret){
        D("[PS][CM36283]threshold = 0x%x\n", thd);
        }else{
        D("[PS][CM36283] --- fail to read threshold  \n");
        }	

        int dummy;
        dummy = sprintf(buf, "%s ps_close_thd_set = 0x%x, ps_away_thd_set = 0x%x\n", __func__, lpi->ps_close_thd_set, lpi->ps_away_thd_set);

        return dummy;
    }
    else
    {
  	ret = sprintf(buf, "[PS][CM36686]PS Hi/Low THD ps_close_thd_set = 0x%04x(%d), ps_away_thd_set = 0x%04x(%d)\n", lpi->ps_close_thd_set, lpi->ps_close_thd_set, lpi->ps_away_thd_set, lpi->ps_away_thd_set);
    return ret;

    }
}

static ssize_t ps_thd_store(struct device *dev,	struct device_attribute *attr, const char *buf, size_t count)
{
	if(newold ==0)
	{
	int code;
	struct CM36283_info *lpi = lp_info_cm36283;

	sscanf(buf, "0x%x", &code);
	D("[PS]%s: store value = 0x%x\n", __func__, code);

	lpi->ps_away_thd_set = code &0xFF;
	lpi->ps_close_thd_set = (code &0xFF00)>>8;	
    _CM36283_I2C_Write_Word(lpi->slave_addr, PS_THD, (lpi->ps_close_thd_set <<8)| lpi->ps_away_thd_set);
	D("[PS]%s: ps_close_thd_set = 0x%x, ps_away_thd_set = 0x%x\n", __func__, lpi->ps_close_thd_set, lpi->ps_away_thd_set);

    int dummy;
	dummy = sprintf(buf,"[PS]%s: ps_close_thd_set = 0x%x, ps_away_thd_set = 0x%x\n", __func__, lpi->ps_close_thd_set, lpi->ps_away_thd_set );

	return dummy;
	}
	else
	{
		int code1, code2;
		struct CM36283_info *lpi = lp_info_cm36283;

        sscanf(buf, "0x%x 0x%x", &code1, &code2);
        lpi->ps_close_thd_set = code1;  
        lpi->ps_away_thd_set = code2;
        _CM36283_I2C_Write_Word(lpi->slave_addr, PS_THDH, lpi->ps_close_thd_set );
        _CM36283_I2C_Write_Word(lpi->slave_addr, PS_THDL, lpi->ps_away_thd_set );
        D("[PS][CM36686]%s: ps_close_thd_set = 0x%04x(%d), ps_away_thd_set = 0x%04x(%d)\n", __func__, lpi->ps_close_thd_set, lpi->ps_close_thd_set, lpi->ps_away_thd_set, lpi->ps_away_thd_set);
        int dummy;
        dummy = sprintf(buf,"[PS][CM36686]%s: ps_close_thd_set = 0x%04x(%d), ps_away_thd_set = 0x%04x(%d)\n", __func__, lpi->ps_close_thd_set, lpi->ps_close_thd_set, lpi->ps_away_thd_set, lpi->ps_away_thd_set);
        return dummy;
	}
}

static DEVICE_ATTR(ps_thd, 0664, ps_thd_show, ps_thd_store);

static ssize_t ps_hw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct CM36283_info *lpi = lp_info_cm36283;

	ret = sprintf(buf, "PS1: reg = 0x%x, PS3: reg = 0x%x, ps_close_thd_set = 0x%x, ps_away_thd_set = 0x%x\n",
	lpi->ps_conf1_val, lpi->ps_conf3_val, lpi->ps_close_thd_set, lpi->ps_away_thd_set);

	return ret;
}

static ssize_t ps_hw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int code;
//	struct CM36283_info *lpi = lp_info_cm36283;

	sscanf(buf, "0x%x", &code);
	D("[PS]%s: store value = 0x%x\n", __func__, code);

	return count;
}

static DEVICE_ATTR(ps_hw, 0664, ps_hw_show, ps_hw_store);

static ssize_t ps_calling_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct CM36283_info *lpi = lp_info_cm36283;
    int ret = 0;
	ret = sprintf(buf,"[PS][CM36283]%s: calling status is %d\n", __func__, lpi->status_calling);

	return ret;
}

static ssize_t ps_calling_store(struct device *dev, 
                                struct device_attribute *attr,
                                const char *buf, 
                                size_t count)
{

	struct CM36283_info *lpi = lp_info_cm36283;			    
    int status_calling; 
	status_calling = -1;

	sscanf(buf, "%d", &status_calling);
    lpi->status_calling = status_calling;
	D("[PS][CM36283]%s: calling status is %d\n", __func__, lpi->status_calling);

	return count;
}

static DEVICE_ATTR(ps_calling, 0664, ps_calling_show, ps_calling_store);

static ssize_t ls_adc_show(struct device *dev,
				           struct device_attribute *attr, 
                           char *buf)
{
	int ret;
	uint16_t als_step;
	struct CM36283_info *lpi = lp_info_cm36283;

    get_ls_adc_value(&als_step, 0);
	D("[LS][CM36283] %s: ADC = 0x%x !!!\n",	__func__, als_step);

	ret = sprintf(buf, "ADC[0x%x] !!!\n", als_step);

	return ret;
}

static ssize_t ls_adc_store(struct device *dev,
 				            struct device_attribute *attr,
				            const char *buf, size_t count)
{   // NOP
	return count;
}

static DEVICE_ATTR(ls_adc, 0664, ls_adc_show, ls_adc_store);

static ssize_t ls_enable_show(struct device *dev,
				              struct device_attribute *attr, 
                              char *buf)
{
	int ret = 0;
	struct CM36283_info *lpi = lp_info_cm36283;

	ret = sprintf(buf, "Light sensor Auto Enable = %d\n", lpi->als_enable);

	return ret;
}


static ssize_t ls_enable_store(struct device *dev,
				               struct device_attribute *attr,
				               const char *buf, 
                               size_t count)
{
	int ret = 0;
	int ls_auto;
	struct CM36283_info *lpi = lp_info_cm36283;

	ls_auto = -1;
	sscanf(buf, "%d", &ls_auto);

	if (ls_auto != 0 && ls_auto != 1 && ls_auto != 147)
		return -EINVAL;


	if (ls_auto) {
		lpi->ls_calibrate = (ls_auto == 147) ? 1 : 0;
		ret = lightsensor_enable(lpi);
	}else{
		lpi->ls_calibrate = 0;
		ret = lightsensor_disable(lpi);
	}

	D("[LS][CM36283] %s: lpi->als_enable = %d, lpi->ls_calibrate = %d, ls_auto=%d\n",
		__func__, lpi->als_enable, lpi->ls_calibrate, ls_auto);

	if (ret < 0)
		pr_err("[LS][CM36283 error]%s: set auto light sensor fail\n",__func__);


	return count;
}

static DEVICE_ATTR(ls_auto, 0664, ls_enable_show, ls_enable_store);

static ssize_t ls_kadc_show(struct device *dev,
                            struct device_attribute *attr, 
                            char *buf)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	int ret;

	ret = sprintf(buf, "kadc = 0x%x", lpi->als_kadc_cm36283);

	return ret;
}

static ssize_t ls_kadc_store(struct device *dev,
        				     struct device_attribute *attr,
				             const char *buf, 
                             size_t count)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	int kadc_temp = 0;
	sscanf(buf, "%d", &kadc_temp);

	mutex_lock(&als_get_adc_mutex);

  if(kadc_temp != 0) {
		lpi->als_kadc_cm36283 = kadc_temp;

            if(  lpi->als_gadc != 0){
                        if (lightsensor_update_table(lpi) < 0)
			    	        printk(KERN_ERR "[LS][CM36283 error] %s: update ls table fail\n", __func__);

            }else{
			       printk(KERN_INFO "[LS]%s: als_gadc =0x%x wait to be set\n",	__func__, lpi->als_gadc);
            }		

  }else{
		printk(KERN_INFO "[LS]%s: als_kadc_cm36283 can't be set to zero\n",	__func__);

       }

	mutex_unlock(&als_get_adc_mutex);
	return count;
}

static DEVICE_ATTR(ls_kadc, 0664, ls_kadc_show, ls_kadc_store);

static ssize_t ls_gadc_show(struct device *dev,
				            struct device_attribute *attr, 
                            char *buf)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	int ret;

	ret = sprintf(buf, "gadc = 0x%x\n", lpi->als_gadc);
	return ret;
}

static ssize_t ls_gadc_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf, 
                             size_t count)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	int gadc_temp = 0;

	sscanf(buf, "%d", &gadc_temp);
	mutex_lock(&als_get_adc_mutex);

  if(gadc_temp != 0) {
		lpi->als_gadc = gadc_temp;

                if(  lpi->als_kadc_cm36283 != 0){
  		               if (lightsensor_update_table(lpi) < 0)
                            printk(KERN_ERR "[LS][CM36283 error] %s: update ls table fail\n", __func__);
  	   
               }else{
			            printk(KERN_INFO "[LS]%s: als_kadc_cm36283 =0x%x wait to be set\n",	__func__, lpi->als_kadc_cm36283);
  	                }		

	}else{
		        printk(KERN_INFO "[LS]%s: als_gadc can't be set to zero\n", __func__);

	}

	mutex_unlock(&als_get_adc_mutex);

	return count;
}


static DEVICE_ATTR(ls_gadc, 0664, ls_gadc_show, ls_gadc_store);

static ssize_t ls_adc_table_show(struct device *dev,
			                     struct device_attribute *attr, 
                                 char *buf)
{
	unsigned length = 0;
	int i;

	for (i = 0; i < 10; i++) {
		length += sprintf(buf + length,
			"[CM36283]Get adc_table[%d] =  0x%x ; %d, Get cali_table[%d] =  0x%x ; %d, \n",
			i, *(lp_info_cm36283->adc_table + i),
			*(lp_info_cm36283->adc_table + i),
			i, 
            *(lp_info_cm36283->cali_table + i),
			*(lp_info_cm36283->cali_table + i));
	}

	return length;
}



static ssize_t ls_adc_table_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, 
                                  size_t count)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	char *token[10];
	uint16_t tempdata[10];
	int i;

	printk(KERN_INFO "[LS][CM36283]%s\n", buf);

	for (i = 0; i < 10; i++) {
		token[i] = strsep((char **)&buf, " ");
		tempdata[i] = simple_strtoul(token[i], NULL, 16);

		if (tempdata[i] < 1 || tempdata[i] > 0xffff) {
			printk(KERN_ERR "[LS][CM36283 error] adc_table[%d] =  0x%x Err\n", i, tempdata[i]);

			return count;
		}

	}

	mutex_lock(&als_get_adc_mutex);

	for (i = 0; i < 10; i++) {
		lpi->adc_table[i] = tempdata[i];

		printk(KERN_INFO

		"[LS][CM36283]Set lpi->adc_table[%d] =  0x%x\n", i, *(lp_info_cm36283->adc_table + i));

	}

	if (lightsensor_update_table(lpi) < 0)
		printk(KERN_ERR "[LS][CM36283 error] %s: update ls table fail\n", __func__);


	mutex_unlock(&als_get_adc_mutex);
	D("[LS][CM36283] %s\n", __func__);

	return count;
}

static DEVICE_ATTR(ls_adc_table, 0664, ls_adc_table_show, ls_adc_table_store);

static ssize_t ls_conf_show(struct device *dev,
				            struct device_attribute *attr, 
                            char *buf)
{
	struct CM36283_info *lpi = lp_info_cm36283;

	return sprintf(buf, "ALS_CONF = 0x%x\n", lpi->ls_cmd);
}

static ssize_t ls_conf_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf, 
                             size_t count)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	int value = 0;

	sscanf(buf, "0x%x", &value);

	lpi->ls_cmd = value;
	printk(KERN_INFO "[LS]set ALS_CONF = %x\n", lpi->ls_cmd);

	_CM36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);

	return count;
}

static DEVICE_ATTR(ls_conf, 0664, ls_conf_show, ls_conf_store);



static ssize_t ls_fLevel_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
	return sprintf(buf, "fLevel_cm36283 = %d\n", fLevel_cm36283);
}

static ssize_t ls_fLevel_store(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t count)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	int value=0;

	sscanf(buf, "%d", &value);
	(value>=0)?(value=min(value,10)):(value=max(value,-1));

	fLevel_cm36283=value;

	input_report_abs(lpi->ls_input_dev, ABS_MISC, fLevel_cm36283);
	input_sync(lpi->ls_input_dev);

	printk(KERN_INFO "[LS]set fLevel_cm36283 = %d\n", fLevel_cm36283);

	msleep(1000);

	fLevel_cm36283=-1;

	return count;
}

static DEVICE_ATTR(ls_flevel, 0664, ls_fLevel_show, ls_fLevel_store);

static int lightsensor_setup(struct CM36283_info *lpi)
{
	int ret;
	lpi->ls_input_dev = input_allocate_device();

	if (!lpi->ls_input_dev) {
		pr_err(	"[LS][CM36283 error]%s: could not allocate ls input device\n", __func__);

		return -ENOMEM;
	}

	lpi->ls_input_dev->name = "lightsensor-level";
	set_bit(EV_ABS, lpi->ls_input_dev->evbit);

	input_set_abs_params(lpi->ls_input_dev, ABS_MISC, 0, 9, 0, 0);

	ret = input_register_device(lpi->ls_input_dev);

	if (ret < 0) {
		pr_err("[LS][CM36283 error]%s: can not register ls input device\n", __func__);

		goto err_free_ls_input_device;
	}

	ret = misc_register(&lightsensor_misc);

	if (ret < 0) {
		pr_err("[LS][CM36283 error]%s: can not register ls misc device\n", __func__);

		goto err_unregister_ls_input_device;
	}

	return ret;

err_unregister_ls_input_device:

	input_unregister_device(lpi->ls_input_dev);

err_free_ls_input_device:

	input_free_device(lpi->ls_input_dev);

	return ret;
}

static int psensor_setup(struct CM36283_info *lpi)
{
	int ret;
	lpi->ps_input_dev = input_allocate_device();

	if (!lpi->ps_input_dev) {
		pr_err( "[PS][CM36283 error]%s: could not allocate ps input device\n", __func__);
		return -ENOMEM;
	}

	lpi->ps_input_dev->name = "proximity";
	
    set_bit(EV_ABS, lpi->ps_input_dev->evbit);
	input_set_abs_params(lpi->ps_input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(lpi->ps_input_dev);

	if (ret < 0) {
               pr_err( "[PS][CM36283 error]%s: could not register ps input device\n", __func__);
               goto err_free_ps_input_device;
	}

	ret = misc_register(&psensor_misc_cm36283);

	if (ret < 0) {
             pr_err( "[PS][CM36283 error]%s: could not register ps misc device\n", __func__);
 		     goto err_unregister_ps_input_device;
	}


	return ret;

err_unregister_ps_input_device:
	input_unregister_device(lpi->ps_input_dev);

err_free_ps_input_device:
	input_free_device(lpi->ps_input_dev);


	return ret;
}

static int I2C_RxData_to_judge_cm3628(unsigned short slaveAddr, unsigned char *rxData, int length)
{
    unsigned char loop_i;
    int val;

    struct i2c_msg msgs[] = {
        {
            .addr = slaveAddr,
            .flags = I2C_M_RD,
            .len = length,
            .buf = rxData,
        },
    };

    for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {

           if (i2c_transfer(lp_info_cm36283->i2c_client->adapter, msgs, 1) > 0)
               break;

        val = gpio_get_value(lp_info_cm36283->intr_pin);
        /*check intr GPIO when i2c error*/
           if (loop_i == 0 || loop_i == I2C_RETRY_COUNT -1)
                 D("[ALS+PS][CM3628-AD3 error] %s, i2c err, slaveAddr 0x%x ISR gpio %d  = %d, record_init_fail %d \n",
                    __func__, slaveAddr, lp_info_cm36283->intr_pin, val, record_init_fail);


        msleep(10);
    }

    if (loop_i >= I2C_RETRY_COUNT) {
            printk(KERN_ERR "[ALS+PS_ERR][CM3628-AD3 error] %s retry over %d\n",  __func__, I2C_RETRY_COUNT);
            return -EIO;
    }


    return 0;
}

static int initial_CM36283(struct CM36283_info *lpi)
{
	int val, ret_cm3628_or_cm36283;
	int ret;
	uint8_t dummy;
	uint16_t idReg;

    val = gpio_get_value(lpi->intr_pin);
	D("[PS][CM36283] %s, INTERRUPT GPIO val = %d\n", __func__, val);

	ret = _CM36283_I2C_Read_Word(lpi->slave_addr, ID_REG, &idReg);
        idReg = idReg&0x00FF;
        printk("P-sensor idReg : %d \n", idReg);
	if ((ret < 0) || ((idReg != 0x0083) && (idReg != 0x0086))) {
  		    if (record_init_fail == 0)
  		   	      record_init_fail = 1;   
        printk("ret = %d \n", ret);			    
        printk("p-sensor init fail \n");
        return -ENOMEM;/*If devices without CM36283/CM36686 chip and did not probe driver*/	
     }
        if(idReg == 0x0083)
        {
                sensitivity_x = sensitivity_20;
                newold = 0; // if newold = 1 means cm36686
        }
        else
        {
                sensitivity_x = sensitivity_25;
                newold = 1;
        }
                printk("p-sensor init OK!\n");
        printk("p-sensor sensitivity = %d", sensitivity_x);
	return 0;
}

static int CM36283_setup(struct CM36283_info *lpi)
{
	int ret = 0;
	als_power(1);
	msleep(5);

	ret = gpio_request(lpi->intr_pin, "gpio_CM36283_intr");

	if (ret < 0) {
		pr_err("[PS][CM36283 error]%s: gpio %d request failed (%d)\n", __func__, lpi->intr_pin, ret);
		return ret;
	}

	ret = gpio_direction_input(lpi->intr_pin);

	if (ret < 0) {
   		   pr_err( "[PS][CM36283 error]%s: fail to set gpio %d as input (%d)\n", __func__, lpi->intr_pin, ret);
		   goto fail_free_intr_pin;
	}

	ret = initial_CM36283(lpi);

	if (ret < 0) {
		   pr_err( "[PS_ERR][CM36283 error]%s: fail to initial CM36283 (%d)\n", __func__, ret);
           goto fail_free_intr_pin;
	}

	/*Default disable P sensor and L sensor*/
    ls_initial_cmd(lpi);
	psensor_initial_cmd(lpi);

	ret = request_irq (lpi->irq,
                       CM36283_irq_handler,
                       IRQF_TRIGGER_FALLING ,
             			"cm3628",
            			lpi);

	if (ret < 0) {
   		   pr_err( "[PS][CM36283 error]%s: req_irq(%d) fail for gpio %d (%d)\n", __func__, lpi->irq, lpi->intr_pin, ret);
           goto fail_free_intr_pin;
	}

	ret = enable_irq_wake(lpi->irq);

	if (ret < 0) {
           pr_err( "[PS][CM36283 error]%s: req_irq(%d) fail for gpio %d (%d)\n", __func__, lpi->irq, lpi->intr_pin, ret);
           goto fail_free_intr_pin;
	}


	return ret;

fail_free_intr_pin:
	gpio_free(lpi->intr_pin);

	return ret;
}

static void CM36283_early_suspend(struct early_suspend *h)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	D("[LS][CM36283] %s\n", __func__);

	if (lpi->als_enable){
            lightsensor_disable(lpi);
            lpi->lsensor_sleep_becuz_early_suspend = 1;
	}
}

static void CM36283_late_resume(struct early_suspend *h)
{
	struct CM36283_info *lpi = lp_info_cm36283;
	D("[ALS][CM36283] %s\n", __func__);

	if (lpi->lsensor_sleep_becuz_early_suspend){
		    lightsensor_enable(lpi);
            lpi->lsensor_sleep_becuz_early_suspend = 0;
            D("[LS][CM36283] lightsensor late_resume\n");    
	}
}

static int CM36283_probe (struct i2c_client *client,
	                      const struct i2c_device_id *id)
{   
	int ret = 0;
	struct CM36283_info *lpi;
	struct CM36283_platform_data *pdata;
	D("[PS][CM36283] %s\n", __func__);

	lpi = kzalloc(sizeof(struct CM36283_info), GFP_KERNEL);

	if (!lpi)
		return -ENOMEM;

	lpi->i2c_client = client;
	pdata = client->dev.platform_data;

	if (!pdata) {
		pr_err("[PS][CM36283 error]%s: Assign platform_data error!!\n", __func__);
 		ret = -EBUSY;
		goto err_platform_data_null;
	}

/******* modified start  *******/
    // Instead of set in intel-mid.c, irq value is set here.   
    lpi->irq =  gpio_to_irq(pdata->intr);
/******* modified end *******/

	i2c_set_clientdata(client, lpi);
    lpi->intr_pin = pdata->intr;
	lpi->adc_table = pdata->levels;
	lpi->power = pdata->power;
	lpi->slave_addr = pdata->slave_addr;
	lpi->ps_away_thd_set = pdata->ps_away_thd_set;
	lpi->ps_close_thd_set = pdata->ps_close_thd_set;	
	lpi->ps_conf1_val = pdata->ps_conf1_val;
    if (Read_PROJ_ID() == PROJ_ID_A502CG){
        lpi->ps_conf1_val = CM36283_PS_ITB_1  | CM36283_PS_DR_1_320 | CM36283_PS_IT_1T | 
                            CM36283_PS_PERS_2 | CM36283_PS_RES_1    | CM36283_PS_INT_IN_AND_OUT ;
    }
	lpi->ps_conf3_val = pdata->ps_conf3_val;
	lpi->status_calling = 0;
	lpi->ls_cmd  = pdata->ls_cmd;
	lpi->record_clear_int_fail=0;

	D("[PS][CM36283] %s: ls_cmd 0x%x\n", __func__, lpi->ls_cmd);

	if (pdata->ls_cmd == 0) {
		lpi->ls_cmd  = CM36283_ALS_IT_160ms | CM36283_ALS_GAIN_2;

	}

	lp_info_cm36283 = lpi;

	mutex_init(&CM36283_control_mutex);
	mutex_init(&als_enable_mutex);
	mutex_init(&als_disable_mutex);
	mutex_init(&als_get_adc_mutex);

	ret = lightsensor_setup(lpi);

	if (ret < 0) {
		pr_err("[LS][CM36283 error]%s: lightsensor_setup error!!\n", __func__);
		goto err_lightsensor_setup;
	}

	mutex_init(&ps_enable_mutex);
	mutex_init(&ps_disable_mutex);
	mutex_init(&ps_get_adc_mutex);

	ret = psensor_setup(lpi);

	if (ret < 0) {
		pr_err("[PS][CM36283 error]%s: psensor_setup error!!\n", __func__);
		goto err_psensor_setup;
	}


  //SET LUX STEP FACTOR HERE
  // if adc raw value one step = 5/100 = 1/20 = 0.05 lux
  // the following will set the factor 0.05 = 1/20
  // and lpi->golden_adc = 1;  
  // set als_kadc_cm36283 = (ALS_CALIBRATED <<16) | 20;

   als_kadc_cm36283 = (ALS_CALIBRATED <<16) | 20;
   lpi->golden_adc = 1;
  //ls calibrate always set to 1 
   lpi->ls_calibrate = 1;
   lightsensor_set_kvalue_cm36283(lpi);

   ret = lightsensor_update_table(lpi);

	if (ret < 0) {
 		pr_err("[LS][CM36283 error]%s: update ls table fail\n", __func__);
		goto err_lightsensor_update_table;
	}

	lpi->lp_wq = create_singlethread_workqueue("CM36283_wq");

	if (!lpi->lp_wq) {
		pr_err("[PS][CM36283 error]%s: can't create workqueue\n", __func__);
		ret = -ENOMEM;
		goto err_create_singlethread_workqueue;
	}

	wake_lock_init(&(lpi->ps_wake_lock), WAKE_LOCK_SUSPEND, "proximity");

	ret = CM36283_setup(lpi);

	if (ret < 0) {
		pr_err("[PS_ERR][CM36283 error]%s: CM36283_setup error!\n", __func__);
		goto err_CM36283_setup;
	}

	lpi->CM36283_class = class_create(THIS_MODULE, "optical_sensors");

	if (IS_ERR(lpi->CM36283_class)) {
		ret = PTR_ERR(lpi->CM36283_class);
		lpi->CM36283_class = NULL;
		goto err_create_class;
	}

	lpi->ls_dev = device_create (lpi->CM36283_class,
                                  NULL,
                                  0,
                                  "%s", 
                                  "lightsensor");

	if (unlikely(IS_ERR(lpi->ls_dev))) {
		ret = PTR_ERR(lpi->ls_dev);
		lpi->ls_dev = NULL;
		goto err_create_ls_device;
	}

	/* register the attributes */
	ret = device_create_file(lpi->ls_dev, &dev_attr_ls_adc);

	if (ret)
		goto err_create_ls_device_file;

	/* register the attributes */
	ret = device_create_file(lpi->ls_dev, &dev_attr_ls_auto);

	if (ret)
		goto err_create_ls_device_file;

	/* register the attributes */
	ret = device_create_file(lpi->ls_dev, &dev_attr_ls_kadc);

	if (ret)
		goto err_create_ls_device_file;

	ret = device_create_file(lpi->ls_dev, &dev_attr_ls_gadc);

	if (ret)
		goto err_create_ls_device_file;

	ret = device_create_file(lpi->ls_dev, &dev_attr_ls_adc_table);

	if (ret)
		goto err_create_ls_device_file;

	ret = device_create_file(lpi->ls_dev, &dev_attr_ls_conf);

	if (ret)
		goto err_create_ls_device_file;

	ret = device_create_file(lpi->ls_dev, &dev_attr_ls_flevel);

	if (ret)
		goto err_create_ls_device_file;

	lpi->ps_dev = device_create( lpi->CM36283_class,
                                 NULL,
                                  0, 
                                  "%s", 
                                  "proximity");

	if (unlikely(IS_ERR(lpi->ps_dev))) {
 		  ret = PTR_ERR(lpi->ps_dev);
          lpi->ps_dev = NULL;
          goto err_create_ls_device_file;
	}

	/* register the attributes */
	ret = device_create_file(lpi->ps_dev, &dev_attr_ps_adc);

	if (ret)
		goto err_create_ps_device;

	ret = device_create_file( lpi->ps_dev, &dev_attr_ps_parameters);

	if (ret)
		goto err_create_ps_device;

	/* register the attributes */
	ret = device_create_file(lpi->ps_dev, &dev_attr_ps_conf);

	if (ret)
		goto err_create_ps_device;

	/* register the attributes */
	ret = device_create_file(lpi->ps_dev, &dev_attr_ps_thd);

	if (ret)
		goto err_create_ps_device;

	ret = device_create_file(lpi->ps_dev, &dev_attr_ps_hw);

	if (ret)
  	    goto err_create_ps_device;

	ret = device_create_file(lpi->ps_dev, &dev_attr_ps_calling);

	if (ret)
		goto err_create_ps_device;

#ifdef DEBUG_VEMMC2
    vemmc2_userCtrl_class = class_create(THIS_MODULE, "vemmc2_userCtrl_dev");
    vemmc2_userCtrl_dev = device_create(vemmc2_userCtrl_class, NULL, 0, "%s", "vemmc2_userCtrl");
 	ret = device_create_file(vemmc2_userCtrl_dev, &dev_attr_vemmc2_ctrl);

    if(ret){
         device_unregister(vemmc2_userCtrl_dev);
    }

    gpio_userCtrl_class = class_create(THIS_MODULE, "gpio_userCtrl_dev");
    gpio_userCtrl_dev = device_create(gpio_userCtrl_class, NULL, 0, "%s", "gpio_userCtrl");
        
    ret = device_create_file(gpio_userCtrl_dev, &dev_attr_gpio_ctrl);

    if(ret){
         device_unregister(gpio_userCtrl_dev);
    }
/*	
	vemmc2_reg = regulator_get(vemmc2_userCtrl_dev, "vemmc2");     
	if (IS_ERR(vemmc2_reg)) { 
	    D(" VEMMC2_cm32863 ---regulator get fail !!!\n");
	}
	int reg_err;
	uint8_t reg;
*/
/*	
    reg_err = intel_msic_reg_read(VEMMC2CNT_ADDR, &reg);
	if (reg_err){
              D(" VEMMC2_cm32863 ---regulator read VEMMC2CNT_ADDR fail !!!\n");
	}else{
	     D(" VEMMC2_cm32863 ---regulator read VEMMC2CNT_ADDR = %d\n", reg);
	}

	reg_err = intel_msic_reg_write(VEMMC2CNT_ADDR, (reg|0x07));

   	if (reg_err){
	     D(" VEMMC2_cm32863 ---regulator write VEMMC2CNT_ADDR to set NORMAL mode fail !!!\n");
	}

	reg = 0;
    reg_err = intel_msic_reg_read(VEMMC2CNT_ADDR, &reg);

	if (!reg_err){
	     D(" VEMMC2_cm32863 ---regulator read VEMMC2CNT_ADDR final = %d 2!!!\n",reg);
	}
*/
/*
    reg_err = regulator_set_mode(vemmc2_reg, 0x02);
	
    if(!reg_err)
	     D("VEMMC2_cm32863 ---regulator setmode success !!\n");
    else 
     	D("VEMMC2_cm32863 ---regulator setmode fail !!\n");
*/
#endif //DEBUG_VEMMC2
	lpi->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	lpi->early_suspend.suspend = CM36283_early_suspend;
	lpi->early_suspend.resume = CM36283_late_resume;
	register_early_suspend(&lpi->early_suspend);

	D("[PS][CM36283] %s: Probe success!\n", __func__);
	//<-- ASUS-Bevis_chen + -->
#ifdef CONFIG_ASUS_FACTORY_MODE
	if(create_asusproc_lpsensor_status_entry( ))
		printk("[%s] : ERROR to create lpsensor proc entry\n",__func__);
#endif
	//<-- ASUS-Bevis_chen - -->

	return ret;

err_create_ps_device:
	device_unregister(lpi->ps_dev);

err_create_ls_device_file:
	device_unregister(lpi->ls_dev);

err_create_ls_device:
	class_destroy(lpi->CM36283_class);

err_create_class:

err_CM36283_setup:
	destroy_workqueue(lpi->lp_wq);
	wake_lock_destroy(&(lpi->ps_wake_lock));

	input_unregister_device(lpi->ls_input_dev);
	input_free_device(lpi->ls_input_dev);
	input_unregister_device(lpi->ps_input_dev);
	input_free_device(lpi->ps_input_dev);

err_create_singlethread_workqueue:

err_lightsensor_update_table:
	misc_deregister(&psensor_misc_cm36283);

err_psensor_setup:
	mutex_destroy(&CM36283_control_mutex);
	mutex_destroy(&ps_enable_mutex);
	mutex_destroy(&ps_disable_mutex);
	mutex_destroy(&ps_get_adc_mutex);
	misc_deregister(&lightsensor_misc);

err_lightsensor_setup:
	mutex_destroy(&als_enable_mutex);
    mutex_destroy(&als_disable_mutex);
	mutex_destroy(&als_get_adc_mutex);

err_platform_data_null:
	kfree(lpi);


	return ret;
}

static int control_and_report( struct CM36283_info *lpi, uint8_t mode, uint16_t param ) {
     int ret=0;
	 uint16_t adc_value = 0;
	 uint16_t ps_data = 0;	
 	 int level = 0, i, val;

     mutex_lock(&CM36283_control_mutex);

    if( mode == CONTROL_ALS ){

            if(param){
                lpi->ls_cmd &= CM36283_ALS_SD_MASK;      
            }else{
                lpi->ls_cmd |= CM36283_ALS_SD;
            }

	        int als_wr_result;
            als_wr_result =_CM36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);
        
            if (!als_wr_result){
                 lpi->als_enable=param;
	        }
  }else if( mode == CONTROL_PS ){
            if(param){ 
                  lpi->ps_conf1_val &= CM36283_PS_SD_MASK;
                  lpi->ps_conf1_val |= CM36283_PS_INT_IN_AND_OUT;      
            }else{
                  lpi->ps_conf1_val |= CM36283_PS_SD;
                  lpi->ps_conf1_val &= CM36283_PS_INT_MASK;
            }

            int ps_wr_result;
	        ps_wr_result = _CM36283_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val);    

            if (!ps_wr_result){ 
                    lpi->ps_enable=param;
	        }
  }

  if((mode == CONTROL_ALS)||(mode == CONTROL_PS)){  
        if( param==1 ){
		     msleep(100);  
         }
  }

  if(lpi->als_enable){
        if( mode == CONTROL_ALS 
            ||( mode == CONTROL_INT_ISR_REPORT 
               && ((param&INT_FLAG_ALS_IF_L)||(param&INT_FLAG_ALS_IF_H)))){
 
    	  lpi->ls_cmd &= CM36283_ALS_INT_MASK;
    	  ret = _CM36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);  
          get_ls_adc_value(&adc_value, 0);

        if( lpi->ls_calibrate ) {
        	for (i = 0; i < 10; i++) {
      	  	     if (adc_value <= (*(lpi->cali_table + i))) {
      		     	level = i;

      			     if (*(lpi->cali_table + i))
      				      break;
      		      }

      		  if ( i == 9) {/*avoid  i = 10, because 'cali_table' of size is 10 */
      			  level = i;
      			  break;
      		  }
      	  }
        }else{
        	  for (i = 0; i < 10; i++) {
            		  if (adc_value <= (*(lpi->adc_table + i))) {
      		              level = i;

      			          if (*(lpi->adc_table + i))
               				  break;
      		          }

            		  if ( i == 9) {/*avoid  i = 10, because 'cali_table' of size is 10 */
               			  level = i;
            			  break;
     		           }
      	       }

    	}

          uint16_t ls_low_thd, ls_high_thd;

		  if (adc_value > LS_RAWDATA_WINDOW){
       	      ls_low_thd = adc_value - LS_RAWDATA_WINDOW;
		  }else{
		      ls_low_thd = 0;
		  }

		  if (adc_value < 0xFFFF - LS_RAWDATA_WINDOW){
		      ls_high_thd = adc_value + LS_RAWDATA_WINDOW;    
		  }else{
		      ls_high_thd = 0xFFFF -1 ;
		  }
/*		  
    	  ret = set_lsensor_range(((i == 0) || (adc_value == 0)) ? 0 :
    		   	*(lpi->cali_table + (i - 1)) + 1,
    		    *(lpi->cali_table + i));
 */
//////// modified to increase sensitivity of lux interrupt 
           ret = set_lsensor_range(ls_low_thd,ls_high_thd);
/////////////////////end modified ///////////////
           lpi->ls_cmd |= CM36283_ALS_INT_EN;
           ret = _CM36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);  
	  
           if ((i == 0) || (adc_value == 0))
    			D("[LS][CM36283] %s: ADC=0x%03X, Level=%d, l_thd equal 0, h_thd = 0x%x \n", __func__, adc_value, level, *(lpi->cali_table + i));
    		else
    			D("[LS][CM36283] %s: ADC=0x%03X, Level=%d, l_thd = 0x%x, h_thd = 0x%x \n", __func__, adc_value, level, *(lpi->cali_table + (i - 1)) + 1, *(lpi->cali_table + i));


    		lpi->current_level = level;
    		lpi->current_adc = adc_value;    
		//<-- ASUS-Bevis_Chen + -->
		    int report_lux = adc_value;

			if(enLSensorConfig_flag == 1 ){//calibration enable 

					if( LSensor_CALIDATA[0] > 0&&LSensor_CALIDATA[1] > 0 
                       &&LSensor_CALIDATA[1] >LSensor_CALIDATA[0] ){ //in case of zero divisor error

                          D("CM36283---Before calibration, report_lux is %d\n", report_lux);
                          report_lux = calibration_light(LSensor_CALIDATA[1], LSensor_CALIDATA[0], report_lux);
                          D("CM36283---After calibration, report_lux is %d\n", report_lux);
                          report_lux = report_lux/sensitivity_x;
			              D("CM36283---After devided by 20, report_lux is %d\n", report_lux);
                	}else{
                          printk("%s:ASUS input LSensor_CALIDATA was invalid .error !!!!!\n",__func__);
				    }
         	}else{	
#ifndef CONFIG_ASUS_FACTORY_MODE // in user build and no calibration data			   
                  report_lux = report_lux*15;
                  D("CM36283--- NO calibration data, use default config\n"); 
#else
                  D("CM36283--- NO calibration data, Factory branch , NO default config\n"); 				  
#endif // end of CONFIG_ASUS_FACTORY_MODE
                  report_lux = report_lux/sensitivity_x;
  		          D("CM36283---After devided by 20, report_lux is %d\n", report_lux);	
		//<-- ASUS-Bevis_Chen - -->
             }			

        if(mode == CONTROL_ALS){
		    queue_delayed_work(lpi->lp_wq, &light_sensor_initial_value_work, 0);
		}else{
		    if(report_lux %2 == 0) 
			        report_lux += 1;
	
			input_report_abs(lpi->ls_input_dev, ABS_MISC,report_lux);
            input_sync(lpi->ls_input_dev);
		}
    }
  }

#define PS_CLOSE 1
#define PS_AWAY  (1<<1)
#define PS_CLOSE_AND_AWAY PS_CLOSE+PS_AWAY

  if(lpi->ps_enable){
        int ps_status = 0;

  if( mode == CONTROL_PS ){
      ps_status = PS_CLOSE_AND_AWAY;   

  }else if(mode == CONTROL_INT_ISR_REPORT ){  
           if ( param & INT_FLAG_PS_IF_CLOSE )
                 ps_status |= PS_CLOSE;      

           if ( param & INT_FLAG_PS_IF_AWAY )
                 ps_status |= PS_AWAY;
  }

   if (ps_status!=0){
         switch(ps_status){
               case PS_CLOSE_AND_AWAY:
                        get_stable_ps_adc_value(&ps_data);
                        val = (ps_data >= lpi->ps_close_thd_set) ? 0 : 1;
                        D("[PS][CM36283] proximity --ps_status = PS_CLOSE_AND_AWAY\n");
		                mutex_unlock(&CM36283_control_mutex);

                        return ret;
                        break;
           
               case PS_AWAY:
                       val = 1;
                       D("[PS][CM36283] proximity --ps_status = PS_AWAY\n");
                       break;

               case PS_CLOSE:
                       val = 0;
             		   D("[PS][CM36283] proximity --ps_status = PS_CLOSE\n");
                       break;
        };

	  D("[PS][CM36283] proximity %s, val=%d\n", val ? "FAR" : "NEAR", val);	
    
      input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, val);     
	  input_sync(lpi->ps_input_dev);        
   }
 }

  mutex_unlock(&CM36283_control_mutex);
  return ret;
}

static const struct i2c_device_id CM36283_i2c_id[] = {
	{CM36283_I2C_NAME, 0},
	{}
};

static int CM36283_suspend(struct i2c_client *client , pm_message_t mesg)
{   
    D("CM32683:  CM36283_suspend\n");
    struct CM36283_info *lpi = lp_info_cm36283;
	int status_calling = lpi->status_calling;
    D("CM32683:  CM36283_suspend  no.1 \n");

	if (lpi->ps_enable && !status_calling){
		  D("CM32683:  CM36283_suspend  no.2 \n");
  		  psensor_disable(lpi);
  	      lpi->psensor_sleep_becuz_suspend =1;
		  D("CM32683:  CM36283_suspend  no.3 \n");
//		disable_irq_nosync(lp_info_cm36283->irq);
	}

    D("CM32683:  CM36283_suspend  no.4 \n");
	return 0;
}

static int CM32683_resume(struct i2c_client *client)
{
     D("CM36283:  CM32683_resume\n");
	 struct CM36283_info *lpi = lp_info_cm36283;
	 if (!lpi->ps_enable && lpi->psensor_sleep_becuz_suspend){
		 psensor_enable(lpi);
         lpi->psensor_sleep_becuz_suspend =0;
//		enable_irq(lp_info_cm36283->irq);
    }
	return 0;
}

static struct i2c_driver CM36283_driver = {
	.id_table = CM36283_i2c_id,
	.probe = CM36283_probe,
	.suspend = CM36283_suspend,
	.resume = CM32683_resume,
	.driver = {
		.name = CM36283_I2C_NAME,
		.owner = THIS_MODULE,
	},
};    

static int __init CM36283_init(void)
{  
    return i2c_add_driver(&CM36283_driver);      
}

static void __exit CM36283_exit(void)
{
//<-- ASUS-Bevis_Chen + -->
#ifdef CONFIG_ASUS_FACTORY_MODE
	if (lpsensor_entry)
		remove_proc_entry("asus_lpsensor_status", NULL);
#endif
//<-- ASUS-Bevis_Chen - -->

	i2c_del_driver(&CM36283_driver);
}

module_init(CM36283_init);
module_exit(CM36283_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CM36283 Driver");

