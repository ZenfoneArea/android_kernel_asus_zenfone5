/*
 * include/media/rt8515.h
 *
 * Chung-Yi (chungy-yi_chou@asus.com)
 */ 

#ifndef _RT8515_H_
#define _RT8515_H_

#include <linux/videodev2.h>
#include <linux/atomisp.h>
#include <media/v4l2-int-device.h>
#include <media/v4l2-subdev.h>


#define RT8515_NAME				"rt8515"

#define RT8515_HW_FLASH_IS_ON 1
#define RT8515_HW_FLASH_MODE  1<<1


#define	v4l2_queryctrl_entry_integer(_id, _name,\
		_minimum, _maximum, _step, \
		_default_value, _flags)	\
	{\
		.id = (_id), \
		.type = V4L2_CTRL_TYPE_INTEGER, \
		.name = _name, \
		.minimum = (_minimum), \
		.maximum = (_maximum), \
		.step = (_step), \
		.default_value = (_default_value),\
		.flags = (_flags),\
	}
#define	v4l2_queryctrl_entry_boolean(_id, _name,\
		_default_value, _flags)	\
	{\
		.id = (_id), \
		.type = V4L2_CTRL_TYPE_BOOLEAN, \
		.name = _name, \
		.minimum = 0, \
		.maximum = 1, \
		.step = 1, \
		.default_value = (_default_value),\
		.flags = (_flags),\
	}

#define	s_ctrl_id_entry_integer(_id, _name, \
		_minimum, _maximum, _step, \
		_default_value, _flags, \
		_s_ctrl, _g_ctrl)	\
	{\
		.qc = v4l2_queryctrl_entry_integer(_id, _name,\
				_minimum, _maximum, _step,\
				_default_value, _flags), \
		.s_ctrl = _s_ctrl, \
		.g_ctrl = _g_ctrl, \
	}

#define	s_ctrl_id_entry_boolean(_id, _name, \
		_default_value, _flags, \
		_s_ctrl, _g_ctrl)	\
	{\
		.qc = v4l2_queryctrl_entry_boolean(_id, _name,\
				_default_value, _flags), \
		.s_ctrl = _s_ctrl, \
		.g_ctrl = _g_ctrl, \
	}


struct rt8515_ctrl_id {
	struct v4l2_queryctrl qc;
	int (*s_ctrl) (struct v4l2_subdev *sd, __u32 val);
	int (*g_ctrl) (struct v4l2_subdev *sd, __s32 *val);
};

struct rt8515 {
	const struct rt8515_platform_data *pdata;
	u32 flash_timeout;
	u8	turned_on;
	u8	torch_intensity;
	u8	flash_intensity;	
	bool dev_init_done;
	spinlock_t lock; // needed for the c2s transaction
	enum v4l2_power power;		       /* Requested power state */
//<Chung-Yi>	 other add-on information for v4l2 command
	struct v4l2_subdev sd;	
	enum atomisp_flash_mode led_mode;  /* Requested flash state */
	enum atomisp_flash_status led_status;  /* Requested flash state */
	u8 flash_indicator_intensity;
//</Chung-Yi>	
};



/**
 * struct rt8515_platform_data - platform data values and access functions
 *
**/

enum rt8515_intensity_values{
	RT8515_TORCH_INT_0_PERCENT,	
	RT8515_TORCH_INT_20_PERCENT,
	RT8515_TORCH_INT_22_4_PERCENT,
	RT8515_TORCH_INT_25_1_PERCENT,
	RT8515_TORCH_INT_28_2_PERCENT,
	RT8515_TORCH_INT_31_6_PERCENT,
	RT8515_TORCH_INT_35_5_PERCENT,
	RT8515_TORCH_INT_39_8_PERCENT,
	RT8515_TORCH_INT_44_7_PERCENT,
	RT8515_TORCH_INT_50_PERCENT,
	RT8515_TORCH_INT_56_PERCENT,
	RT8515_TORCH_INT_63_PERCENT,
	RT8515_TORCH_INT_71_PERCENT,
	RT8515_TORCH_INT_79_PERCENT,
	RT8515_TORCH_INT_89_PERCENT,
	RT8515_TORCH_INT_100_PERCENT,	
};

/**
 * struct rt8515_platform_data - platform data values and access functions
 * @power_set: Power state access function, zero is off, non-zero is on.
 * @flash_on: Turn on the flash.
 * @flash_off: Turn off the flash.
 * @update_hw: Depending on the torch intensity, turn on/off torch.
 * @priv_data_set: device private data (pointer) access function
 */
struct rt8515_platform_data {
	int (*init)(void);
	int (*exit)(void);
	void (*flash_on)(void);
	void (*torch_on)(u8 data);	
	void (*flash_off)(unsigned long dummy);
	int (*update_hw)(struct v4l2_int_device *s);
};

#endif /* _RT8515_H_ */

