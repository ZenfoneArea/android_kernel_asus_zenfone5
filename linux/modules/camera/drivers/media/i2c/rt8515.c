/*
 * LED flash driver for RT8515
 * This device is a pure GPIO control,  but for some reason.
 * I register this as a I2C device.
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <media/rt8515.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <linux/atomisp.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>

/*
static struct device* flash_dev;
static struct class* flash_class;
*/
static int set_light_record;
static int set_light_record_odd;
static void rt8515_torch_on(u8);
static void rt8515_flash_off(unsigned long );
u8 inline turn_percent_to_s2c(u8);


static ssize_t flash_show(struct file *dev, char *buffer, size_t count, loff_t *ppos)
{

   int len, report_light = 0;
   ssize_t ret = 0;
   char *buff;

   buff = kmalloc(100,GFP_KERNEL);
   if(!buff)
            return -ENOMEM;

    report_light = set_light_record*2 + set_light_record_odd;
    len += sprintf(buff+len, "%d\n", report_light);
    ret = simple_read_from_buffer(buffer,count,ppos,buff,len);
    kfree(buff);

    return ret;

}

static ssize_t flash_store(struct file *dev, const char *buf, size_t count, loff_t *loff)
{
    int set_light;
    set_light = -1;
    set_light_record_odd = 0;
    sscanf(buf, "%d", &set_light);
    printk(KERN_INFO "[Flash] Set light to %d \n", set_light);
    if (set_light == set_light_record) return count;

    if(set_light ==-1 || set_light >200) return -1;

    else if (set_light == 0 ){
        rt8515_flash_off((unsigned long) 0);
    }else{
        if(set_light%2 == 0){
                set_light_record_odd =0;
        }else{
                set_light_record_odd =1;
        }
        set_light /= 2;
        set_light_record = set_light;
        rt8515_flash_off((unsigned long) 0);
        u8 map_num = turn_percent_to_s2c((u8)set_light);
        printk(KERN_INFO "map_num is %x", map_num);
        rt8515_torch_on(map_num);
    }

    return count;
}

static const struct file_operations flash_proc_fops = {
     .read       = flash_show,
     .write      = flash_store,
};



DEVICE_ATTR(flash, 0664, flash_show, flash_store);

u8 inline turn_percent_to_s2c(u8 light_intensity_percentage){
		u8 ret;
        set_light_record = (int) light_intensity_percentage;

        ret = RT8515_TORCH_INT_0_PERCENT;
		if(light_intensity_percentage >= 20)
			ret = RT8515_TORCH_INT_20_PERCENT;
		if(light_intensity_percentage > 22)
			ret = RT8515_TORCH_INT_22_4_PERCENT;
		if(light_intensity_percentage > 25)
			ret = RT8515_TORCH_INT_25_1_PERCENT;
		if(light_intensity_percentage > 28)
			ret = RT8515_TORCH_INT_28_2_PERCENT;
		if(light_intensity_percentage > 31)
			ret = RT8515_TORCH_INT_31_6_PERCENT;
		if(light_intensity_percentage > 35)
			ret = RT8515_TORCH_INT_35_5_PERCENT;
		if(light_intensity_percentage > 39)
			ret = RT8515_TORCH_INT_39_8_PERCENT;
		if(light_intensity_percentage > 44)
			ret = RT8515_TORCH_INT_44_7_PERCENT;
		if(light_intensity_percentage >= 50)
			ret = RT8515_TORCH_INT_50_PERCENT;
		if(light_intensity_percentage >= 56)
			ret = RT8515_TORCH_INT_56_PERCENT;
		if(light_intensity_percentage >= 63)
			ret = RT8515_TORCH_INT_63_PERCENT;
		if(light_intensity_percentage >= 71)
			ret = RT8515_TORCH_INT_71_PERCENT;
		if(light_intensity_percentage >= 79)
			ret = RT8515_TORCH_INT_79_PERCENT;
		if(light_intensity_percentage >= 89)
			ret = RT8515_TORCH_INT_89_PERCENT;
		if(light_intensity_percentage >= 100)
			ret = RT8515_TORCH_INT_100_PERCENT;
		ret <<= 4;
		ret = ~ret;
		ret >>= 4;
		ret +=1;
		return ret;
}




//<Chung-Yi> Some default value for rt8515
#define RT8515_FLASH_TIMEOUT_MS    1000
#define RT8515_FLASH_TIMEOUT_US    300000
#define RT8515_FLASH_INTENSITY_DEFAULT    100
#define RT8515_FLASH_TORCH_INTENSITY_DEFAULT    100
#define RT8515_FLASH_TIME_OUT_DEFAULT    100
#define RT8515_FLASH_INDICATOR_INTENSITY 100
#define RT8515_MIN_PERCENT 0
#define RT8515_MAX_PERCENT 100
#define RT8515_FLASH_ENF 		161
#define RT8515_FLASH_ENT  		159


#define to_rt8515(p_sd)	container_of(p_sd, struct rt8515, sd)

struct timer_list flash_timer;
static void rt8515_time_off(unsigned long dummy);

static int rt8515_ENF = -1;
static int rt8515_ENT = -1;

////////////
static void rt8515_time_off(unsigned long dummy)
{
	del_timer(&flash_timer);
	gpio_direction_output(rt8515_ENF, 0);
	gpio_direction_output(rt8515_ENT, 0);
	printk(KERN_DEBUG "[FLASH] Timeout \n");
}


static void rt8515_flash_on(void)
{
	mod_timer(&flash_timer , jiffies + msecs_to_jiffies(RT8515_FLASH_TIMEOUT_MS) );
 	gpio_direction_output(rt8515_ENF, 1);
	gpio_direction_output(rt8515_ENT, 0);
	printk(KERN_DEBUG "[FLASH] RT8515_FLASH_ENF (%d) is set to 1 \n",rt8515_ENF);
}


static void rt8515_flash_off(unsigned long dummy)
{
    set_light_record = 0;
	del_timer(&flash_timer);
	gpio_direction_output(rt8515_ENF, 0);
	gpio_direction_output(rt8515_ENT, 0);
	printk(KERN_DEBUG "[FLASH] RT8515_FLASH_ENF (%d) is set to 0 \n",rt8515_ENF);
	//rt8515_hw_status = 0;
}

static void rt8515_torch_on(u8 data)
{
	// start with low state and leave high when done sending
	gpio_set_value(rt8515_ENF, 0);
	gpio_set_value(rt8515_ENT, 0);
	//usleep_range(500, 600);
	mdelay(4);
	printk(KERN_DEBUG "[FLASH] Settings RT8515_FLASH_ENT (%d) is set from 0 to 1 %d times for s2c communication \n",rt8515_ENT,data);
	if(data > 9){
		printk(KERN_DEBUG "[FLASH] change the step from %d to 9 \n",data);
		data = 9;
	}
	while(data--){
		gpio_set_value(rt8515_ENT, 0);
		udelay(4);
		gpio_set_value(rt8515_ENT, 1);
		udelay(4);
	}
	//rt8515_hw_status = (RT8515_HW_FLASH_IS_ON);
}


/////////////
#if 0
static int rt8515_delay_strobe(struct rt8515 *flash)
{
	int ret;
	unsigned long dummy = 1;
	void (*flash_off)(unsigned long);
	flash_off = rt8515_flash_off;
	u32 timeout = flash->flash_timeout;
	del_timer_sync(&flash->flash_off_delay);
	setup_timer(&flash->flash_off_delay,flash_off(dummy),1);
	ret = mod_timer(&flash->flash_off_delay,jiffies + usecs_to_jiffies(timeout));
	if(ret)
		printk(

KERN_ERR "[FLASH] mod_timer \n");
	return ret;

}
#endif
static int rt8515_apply(struct v4l2_subdev *sd, int is_flash)
{
	struct rt8515 *flash = to_rt8515(sd);
	u8 s2c_data;
	unsigned long flags;

	if(is_flash){ // full intensity flash the flash
		rt8515_flash_on();
		return 0;//rt8515_delay_strobe(flash); // with full flash we need SW watchedog defined by the timeout being sent
	} else {
		s2c_data = turn_percent_to_s2c(flash->torch_intensity);

		// the s2c transfer have to be done under 75us
		// otherwise the sent 16bit data won't be read as 16bit
		spin_lock_irqsave(&flash->lock, flags);
		rt8515_torch_on(s2c_data);
		spin_unlock_irqrestore(&flash->lock, flags);

		return 0;
	}
}



/////////////
static int rt8515_s_flash_timeout(struct v4l2_subdev *sd, u32 val)
{
	struct rt8515 *flash = to_rt8515(sd);
	flash->flash_timeout= val;
	return 0;
}

static int rt8515_g_flash_timeout(struct v4l2_subdev *sd, s32 *val)
{
	struct rt8515 *flash = to_rt8515(sd);
	*val = flash->flash_timeout;
	return 0;
}

static int rt8515_s_flash_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	struct rt8515 *flash = to_rt8515(sd);
	flash->flash_intensity = intensity;
	return 0;
}

static int rt8515_g_flash_intensity(struct v4l2_subdev *sd, s32 *val)
{
	struct rt8515 *flash = to_rt8515(sd);
	*val = flash->flash_intensity;
	return 0;
}

static int rt8515_s_torch_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	struct rt8515 *flash = to_rt8515(sd);
	flash->torch_intensity = intensity;
	return 0;
}

static int rt8515_g_torch_intensity(struct v4l2_subdev *sd, s32 *val)
{
	struct rt8515 *flash = to_rt8515(sd);
	*val = flash->torch_intensity;
	printk("[FLASH] get torch val = %d\n",*val);
	return 0;
}

static int rt8515_s_indicator_intensity(struct v4l2_subdev *sd, u32 intensity)
{
	struct rt8515 *flash = to_rt8515(sd);
	flash->flash_indicator_intensity = intensity;
	return 0;
}

static int rt8515_g_indicator_intensity(struct v4l2_subdev *sd, s32 *val)
{
	struct rt8515 *flash = to_rt8515(sd);
	*val = flash->flash_indicator_intensity;
	return 0;
}

static int rt8515_s_flash_strobe(struct v4l2_subdev *sd, u32 val)
{
	struct rt8515 *flash = to_rt8515(sd);
	flash->turned_on = val;

	if(val){
		return rt8515_apply(sd,1);
	}
	return 0;
}

static int rt8515_s_flash_mode(struct v4l2_subdev *sd, u32 new_mode)
{
	struct rt8515 *flash = to_rt8515(sd);
	int ret = 0;
	flash->led_mode = new_mode;
	switch(new_mode){
		case ATOMISP_FLASH_MODE_OFF:
			rt8515_flash_off(1);
			break;
		case ATOMISP_FLASH_MODE_FLASH:
			ret = rt8515_apply(sd,1);
			break;
		case ATOMISP_FLASH_MODE_TORCH:
			ret = rt8515_apply(sd,0);
			break;
		//Need knowhow here
		case ATOMISP_FLASH_MODE_INDICATOR:
			ret = rt8515_apply(sd,0);
			break;
		default:{
			printk("[FLASH] Unknown command \n");
		}
	}
	return ret;
}


static int rt8515_g_flash_mode(struct v4l2_subdev *sd, s32 * val)
{
	struct rt8515 *flash = to_rt8515(sd);
	*val = flash->led_mode;
	return 0;
}

//Need knowhow here
static int rt8515_g_flash_status(struct v4l2_subdev *sd, s32 *val)
{
	struct rt8515 *flash = to_rt8515(sd);
	*val = flash->led_status;
	return 0;
}



/* -----------------------------------------------------------------------------
 * V4L2 controls
 */


static const struct rt8515_ctrl_id rt8515_ctrls[] = {
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_TIMEOUT,
				"Flash Timeout",
				0,
				RT8515_FLASH_TIMEOUT_US,
				1,
				RT8515_FLASH_TIMEOUT_US,
				0,
				rt8515_s_flash_timeout,
				rt8515_g_flash_timeout),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_INTENSITY,
				"Flash Intensity",
				RT8515_MIN_PERCENT,
				RT8515_MAX_PERCENT,
				1,
				RT8515_FLASH_INTENSITY_DEFAULT,
				0,
				rt8515_s_flash_intensity,
				rt8515_g_flash_intensity),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_TORCH_INTENSITY,
				"Torch Intensity",
				RT8515_MIN_PERCENT,
				RT8515_MAX_PERCENT,
				1,
				RT8515_FLASH_TORCH_INTENSITY_DEFAULT,
				0,
				rt8515_s_torch_intensity,
				rt8515_g_torch_intensity),
/*
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_INDICATOR_INTENSITY,
				"Indicator Intensity",
				RT8515_MIN_PERCENT,
				RT8515_MAX_PERCENT,
				1,
				RT8515_FLASH_INDICATOR_INTENSITY,
				0,
				rt8515_s_indicator_intensity,
				rt8515_g_indicator_intensity),
*/
	s_ctrl_id_entry_boolean(V4L2_CID_FLASH_STROBE,
				"Flash Strobe",
				0,
				0,
				rt8515_s_flash_strobe,
				NULL),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_MODE,
				"Flash Mode",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				ATOMISP_FLASH_MODE_OFF,
				0,
				rt8515_s_flash_mode,
				rt8515_g_flash_mode),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_STATUS,
				"Flash Status",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				ATOMISP_FLASH_STATUS_OK,
				0,
				NULL,
				rt8515_g_flash_status),
};

#if 0
static int rt8515_s_test_function(struct v4l2_subdev *sd, u32 intensity)
{
	gpio_direction_output(159, 1);
	return 0;
}

static int rt8515_g_test_function(struct v4l2_subdev *sd, s32 *val)
{
	gpio_direction_output(159, 1);
	return 0;
}
static const struct rt8515_ctrl_id rt8515_ctrls[] = {
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_TIMEOUT,
				"Flash Timeout",
				0,
				0,
				1,
				0,
				0,
				rt8515_s_test_function,
				rt8515_g_test_function),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_INTENSITY,
				"Flash Intensity",
				0,
				0,
				1,
				0,
				0,
				rt8515_s_test_function,
				rt8515_g_test_function),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_TORCH_INTENSITY,
				"Torch Intensity",
				0,
				0,
				1,
				0,
				0,
				rt8515_s_test_function,
				rt8515_g_test_function),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_INDICATOR_INTENSITY,
				"Indicator Intensity",
				0,
				0,
				1,
				0,
				0,
				rt8515_s_test_function,
				rt8515_g_test_function),
	s_ctrl_id_entry_boolean(V4L2_CID_FLASH_STROBE,
				"Flash Strobe",
				0,
				0,
				rt8515_s_test_function,
				NULL),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_MODE,
				"Flash Mode",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				0,
				0,
				rt8515_s_test_function,
				rt8515_g_test_function),
	s_ctrl_id_entry_integer(V4L2_CID_FLASH_STATUS,
				"Flash Status",
				0,   /* don't assume any enum ID is first */
				100, /* enum value, may get extended */
				1,
				0,
				0,
				NULL,
				rt8515_g_test_function),
};
#endif
static const struct rt8515_ctrl_id *find_ctrl_id(unsigned int id)
{
	int i;
	int num;

	num = ARRAY_SIZE(rt8515_ctrls);
	for (i = 0; i < num; i++) {
		if (rt8515_ctrls[i].qc.id == id)
			return &rt8515_ctrls[i];
	}

	return NULL;
}

static int rt8515_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int num;

	if (!qc)
		return -EINVAL;

	num = ARRAY_SIZE(rt8515_ctrls);
	if (qc->id >= num)
		return -EINVAL;

	*qc = rt8515_ctrls[qc->id].qc;
	return 0;
}

static int rt8515_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	const struct rt8515_ctrl_id *s_ctrl;

	if (!ctrl)
		return -EINVAL;
	s_ctrl = find_ctrl_id(ctrl->id);
	if (!s_ctrl)
		return -EINVAL;
	return s_ctrl->s_ctrl(sd, ctrl->value);
}

static int rt8515_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	const struct rt8515_ctrl_id *s_ctrl;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = find_ctrl_id(ctrl->id);
	if (s_ctrl == NULL)
		return -EINVAL;
	return s_ctrl->g_ctrl(sd, &ctrl->value);
}



/* -----------------------------------------------------------------------------
 * V4L2 subdev core operations
 */

/* Put device into known state. */
static int rt8515_setup(struct rt8515 *flash)
{
	printk("[Flash] initial device information \n");

	return 0;
}


static int __rt8515_s_power(struct rt8515 *flash, int power)
{
	return 0;
}

static int rt8515_s_power(struct v4l2_subdev *sd, int power)
{
	return 0;
}

static long rt8515_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int input_arg = 0;
	switch (cmd) {
	case ATOMISP_TEST_CMD_SET_TORCH:
		input_arg = *(int *)arg;
		if(input_arg == 0){
			printk("[AsusFlash] Set Torch off \n");
			rt8515_flash_off((unsigned long) 0);
		}else{
			printk("[AsusFlash] Set Torch on \n");
			rt8515_torch_on((u8)9);
		}
		return 0;
	case ATOMISP_TEST_CMD_SET_FLASH:
		input_arg = *(int *)arg;
		if(input_arg == 0){
			printk("[AsusFlash] Set Flash off \n");
			rt8515_flash_off((unsigned long) 0);
		}else{
			printk("[AsusFlash] Set Flash on \n");
			rt8515_flash_on();
		}
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_subdev_core_ops rt8515_core_ops = {
	.queryctrl = rt8515_queryctrl,
	.g_ctrl = rt8515_g_ctrl,
	.s_ctrl = rt8515_s_ctrl,
	.s_power = rt8515_s_power,
	.ioctl = rt8515_ioctl,
};

static const struct v4l2_subdev_ops rt8515_ops = {
	.core = &rt8515_core_ops,
};

static int rt8515_detect(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_adapter *adapter = client->adapter;
	struct rt8515 *flash = to_rt8515(sd);
	int ret;

	printk("[Flash] rt8515_detect\n");
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, " [Flash] rt8515 detect i2c error !?\n");
		return -ENODEV;
	}

	/* Power up the flash driver and reset it */
	ret = rt8515_s_power(&flash->sd, 1);
	if (ret < 0)
		return ret;

	/* Setup default values. This makes sure that the chip is in a known
	 * state.
	 */
	ret = rt8515_setup(flash);
	if (ret < 0)
		goto fail;

	dev_dbg(&client->dev, " [Flash] Successfully detected rt8515 LED flash\n");
	rt8515_s_power(&flash->sd, 0);

	printk("[Flash] rt8515_detect success\n");
	return 0;
fail:
	rt8515_s_power(&flash->sd, 0);
	return ret;
}

static int rt8515_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return 0;
}

static int rt8515_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	return 0;
}

static const struct v4l2_subdev_internal_ops rt8515_internal_ops = {
	.registered = rt8515_detect,
	.open = rt8515_open,
	.close = rt8515_close,
};



static int rt8515_suspend(struct device *dev)
{
	return 0;
}

static int rt8515_resume(struct device *dev)
{
	return 0;
}



static int __devinit rt8515_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int err;
	struct rt8515 *flash;
	//rt8515_ENF = get_gpio_by_name("FLED_DRIVER_ENF");
	//rt8515_ENT = get_gpio_by_name("FLED_DRIVER_ENT");
	if (rt8515_ENF== -1) {
		printk("[FLASH] %s: Unable to find RT8515_FLASH_ENF\n", __func__);
		rt8515_ENF = RT8515_FLASH_ENF;
		gpio_free(rt8515_ENF);
        	gpio_request(rt8515_ENF, "rt8515_ENF");
		printk("[FLASH] rt8515_ENF = %d \n", RT8515_FLASH_ENF);
	}
	if (rt8515_ENT == -1) {
		printk("[FLASH] %s: Unable to find RT8515_FLASH_ENT\n", __func__);
		rt8515_ENT = RT8515_FLASH_ENT;
		gpio_free(rt8515_ENT);
        	gpio_request(rt8515_ENT, "rt8515_ENT");
		printk("[FLASH] rt8515_ENT = %d \n", RT8515_FLASH_ENT);
	}


	if (client->dev.platform_data == NULL) {
		printk(&client->dev, "[Flash] rt8515 no platform data\n");
		return -ENODEV;
	}

	flash = kzalloc(sizeof(*flash), GFP_KERNEL);
	if (!flash) {
		dev_err(&client->dev, "[Flash] out of memory\n");
		return -ENOMEM;
	}

	flash->pdata = client->dev.platform_data;

	v4l2_i2c_subdev_init(&flash->sd, client, &rt8515_ops);
	flash->sd.internal_ops = &rt8515_internal_ops;
	flash->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	flash->led_mode = ATOMISP_FLASH_MODE_OFF;
	//flash->timeout = LM3554_MAX_TIMEOUT / LM3554_TIMEOUT_STEPSIZE - 1;

	err = media_entity_init(&flash->sd.entity, 0, NULL, 0);
	if (err) {
		dev_err(&client->dev, "[Flash] error initialize a media entity.\n");
		goto fail1;
	}

	spin_lock_init(&flash->lock);

	flash->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_FLASH;

	init_timer(&flash_timer);
	flash_timer.expires = jiffies + msecs_to_jiffies(RT8515_FLASH_TIMEOUT_MS);
	flash_timer.function = &rt8515_time_off;
	flash_timer.data = (unsigned long) 10;
	add_timer(&flash_timer);
/*
    flash_class = class_create(THIS_MODULE, "flash_dev");
    flash_dev = device_create(flash_class, NULL, 0, "%s", "flash_ctrl");
    device_create_file(flash_dev, &dev_attr_flash);
*/
    void* dummy;
    struct proc_dir_entry* proc_entry_flash;
    proc_entry_flash = proc_create_data("driver/asus_flash_brightness", 0664, NULL, &flash_proc_fops, dummy);
    proc_set_user(proc_entry_flash, 1000, 1000);

#if 0


	err = lm3554_gpio_init(client);
	if (err) {
		dev_err(&client->dev, "gpio request/direction_output fail");
		goto fail2;
	}
#endif
	printk("[Flash] probe success\n");
	return 0;
fail1:
	v4l2_device_unregister_subdev(&flash->sd);
	kfree(flash);
	printk("[Flash] probe failed\n");
	return err;
}


static int __devexit rt8515_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct rt8515 *flash = to_rt8515(sd);
	int ret=0;

	media_entity_cleanup(&flash->sd.entity);
	v4l2_device_unregister_subdev(sd);


	if (ret < 0)
		goto fail;

	kfree(flash);

	return 0;
fail:
	dev_err(&client->dev, "[Flash] gpio request/direction_output fail");
	return ret;
}

static const struct i2c_device_id rt8515_id[] = {
	{RT8515_NAME, 0},
	{},
};


MODULE_DEVICE_TABLE(i2c, rt8515_id);

static const struct dev_pm_ops rt8515_pm_ops = {
	.suspend = rt8515_suspend,
	.resume = rt8515_resume,
};

static struct i2c_driver rt8515_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = RT8515_NAME,
		.pm   = &rt8515_pm_ops,
	},
	.probe = rt8515_probe,
	.remove = __devexit_p(rt8515_remove),
	.id_table = rt8515_id,
};
/**
 * rt8515_init - Module initialisation.
 *
 * Returns 0 if successful, or -ENODEV if device couldn't be initialized, or
 * added as a character device.
 **/
static int __init rt8515_init(void)
{
	int r ;
	printk("[Flash] rt8515_init\n");
	r = i2c_add_driver(&rt8515_driver);
	return r;
}

/**
 * rt8515_exit - Module cleanup.
 **/
static void __exit rt8515_exit(void)
{
	i2c_del_driver(&rt8515_driver);
	printk("[Flash] rt8515_exit\n");
}


module_init(rt8515_init);
module_exit(rt8515_exit);

MODULE_DESCRIPTION("LED flash driver for RT8515");
MODULE_LICENSE("GPL");
