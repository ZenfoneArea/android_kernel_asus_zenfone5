/*
 * Support for Panasonic mn34130 13M camera sensor.
 *
 * Copyright (c) 2011 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <asm/intel-mid.h>
#include "mn34130.h"
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static u8 mn34130_otp_data[24];
static int currentFPS;

static int mn34130_read_reg(struct i2c_client *client, u16 len, u16 reg,
			    u16 *val)
{
	struct i2c_msg msg[2];
	u16 data[MN34130_SHORT_MAX];
	int err, i;

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (len > MN34130_BYTE_MAX) {
		v4l2_err(client, "%s error, invalid data length\n", __func__);
		return -EINVAL;
	}

	memset(msg, 0, sizeof(msg));
	memset(data, 0, sizeof(data));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = I2C_MSG_LENGTH;
	msg[0].buf = (u8 *)data;
	data[0] = cpu_to_be16(reg);

	msg[1].addr = client->addr;
	msg[1].len = len;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = (u8 *)data;

	err = i2c_transfer(client->adapter, msg, 2);
	if (err < 0) {
		dev_err(&client->dev, "read from offset 0x%x error %d", reg,
			err);
		return err;
	}

	if (len == MN34130_8BIT) {
		*val = (u8)data[0];
	} else {
		for (i = 0; i < (len >> 1); i++)
			val[i] = be16_to_cpu(data[i]);
	}

	return 0;
}

static int
mn34130_read_otp_data(struct i2c_client *client, u16 len, u16 reg, void *val)
{
        struct i2c_msg msg[2];
        u16 data[MN34130_SHORT_MAX] = { 0 };
        int err;

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (len > MN34130_BYTE_MAX) {
		v4l2_err(client, "%s error, invalid data length\n", __func__);
		return -EINVAL;
	}

	memset(msg, 0, sizeof(msg));
	memset(data, 0, sizeof(data));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = I2C_MSG_LENGTH;
	msg[0].buf = (u8 *)data;
	data[0] = cpu_to_be16(reg);

	msg[1].addr = client->addr;
	msg[1].len = len;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = (u8 *)data;

	err = i2c_transfer(client->adapter, msg, 2);
	if (err < 0) {
		dev_err(&client->dev, "read from offset 0x%x error %d", reg,
			err);
		return err;
	}

        memcpy(val, data, len);
        return 0;
}


static int mn34130_i2c_write(struct i2c_client *client, u16 len, u8 *data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;
	int retry = 0;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	do {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == num_msg)
			return 0;

		dev_err(&client->dev, "retrying i2c write transfer... %d",
			retry);
		retry++;
		usleep_range(I2C_RETRY_WAITUS, I2C_RETRY_WAITUS + 100);
	} while (retry <= I2C_RETRY_COUNT);

	return ret;
}

static int
mn34130_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	u16 *wreg = (u16 *)data;
	const u16 len = data_length + sizeof(u16); /* 16-bit address + data */

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != MN34130_8BIT && data_length != MN34130_16BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	*wreg = cpu_to_be16(reg);

	if (data_length == MN34130_8BIT) {
		data[2] = (u8)(val);
	} else {
		/* MN34130_16BIT */
		u16 *wdata = (u16 *)&data[2];
		*wdata = be16_to_cpu(val);
	}

	ret = mn34130_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}


/*
 * mn34130_write_reg_array - Initializes a list of MT9M114 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __mn34130_flush_reg_array, __mn34130_buf_reg_array() and
 * __mn34130_write_reg_is_consecutive() are internal functions to
 * mn34130_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __mn34130_flush_reg_array(struct i2c_client *client,
				     struct mn34130_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return mn34130_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __mn34130_buf_reg_array(struct i2c_client *client,
				   struct mn34130_write_ctrl *ctrl,
				   const struct mn34130_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case MN34130_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;

	case MN34130_16BIT:
		size = 2;
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = cpu_to_be16((u16)next->val);
		break;

	default:
		return -EINVAL;
	}

	/* When first item is added, we need to store its starting address */
	if (ctrl->index == 0)
		ctrl->buffer.addr = next->reg;

	ctrl->index += size;

	/*
	 * Buffer cannot guarantee free space for u32? Better flush it to avoid
	 * possible lack of memory for next item.
	 */
	if (ctrl->index + sizeof(u16) >= MN34130_MAX_WRITE_BUF_SIZE)
		__mn34130_flush_reg_array(client, ctrl);

	return 0;
}

static int
__mn34130_write_reg_is_consecutive(struct i2c_client *client,
				   struct mn34130_write_ctrl *ctrl,
				   const struct mn34130_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg;
}

static int mn34130_write_reg_array(struct i2c_client *client,
				   const struct mn34130_reg *reglist)
{
	const struct mn34130_reg *next = reglist;
	struct mn34130_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != MN34130_TOK_TERM; next++) {
		switch (next->type & MN34130_TOK_MASK) {
		case MN34130_TOK_DELAY:
			err = __mn34130_flush_reg_array(client, &ctrl);
			if (err)
				return err;

			usleep_range(next->val, next->val + 100);
			break;

		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__mn34130_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __mn34130_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}

			err = __mn34130_buf_reg_array(client, &ctrl, next);
			if (err) {
				v4l2_err(client, "%s: write error, aborted\n",
					 __func__);
				return err;
			}

			break;
		}
	}

	return __mn34130_flush_reg_array(client, &ctrl);
}

static int mn34130_read_otp_reg_array(struct i2c_client *client, u16 size, u16 addr,
                                  u8 *buf)
{
        u16 index;
        int ret;

        for (index = 0; index + MN34130_OTP_READ_ONETIME <= size;
                                        index += MN34130_OTP_READ_ONETIME) {
                ret = mn34130_read_otp_data(client, MN34130_OTP_READ_ONETIME,
                                        addr + index, &buf[index]);
                if (ret)
                        return ret;
        }
        return 0;
}

static int __mn34130_otp_read(struct v4l2_subdev *sd, struct mn34130_af_data *buf)
{
        struct i2c_client *client = v4l2_get_subdevdata(sd);
        int ret;
	int i;
	u8 read_value[24];

	for(i=24*2 ; i>=0 ; i=i-24)
	{
	    ret = mn34130_read_otp_reg_array(client, MN34130_OTP_READ_ONETIME, MN34130_OTP_START_ADDR + i, read_value);
	    printk("%s Check bank %d 0x%X 0x%X\n", __func__, i/24, read_value[0], read_value[1]);
	    if((read_value[0]!=0 || read_value[1]!=0) && (read_value[0]!=0xff || read_value[1]!=0xff))
	        break;
	}
	memcpy(mn34130_otp_data, read_value, 24);

	buf->af_inf_pos = read_value[0]<<8 | read_value[1];
	buf->af_1m_pos = read_value[2]<<8 | read_value[3];
	buf->af_10cm_pos = read_value[4]<<8 | read_value[5];
	buf->af_start_curr = read_value[6]<<8 | read_value[7];
	buf->module_id = read_value[8];
	buf->vendor_id = read_value[9];
	buf->default_af_inf_pos = MN34130_DEFAULT_AF_INF;
	buf->default_af_10cm_pos = MN34130_DEFAULT_AF_10CM;
	buf->default_af_start = MN34130_DEFAULT_AF_START;
	buf->default_af_end = MN34130_DEFAULT_AF_END;

	return 0;
}

static void *mn34130_otp_read(struct v4l2_subdev *sd)
{
        struct i2c_client *client = v4l2_get_subdevdata(sd);
        struct mn34130_af_data *buf;
        int ret;

        buf = devm_kzalloc(&client->dev, MN34130_OTP_DATA_SIZE, GFP_KERNEL);
        if (!buf)
                return ERR_PTR(-ENOMEM);

        ret = __mn34130_otp_read(sd, buf);

        /* Driver has failed to find valid data */
        if (ret) {
                dev_err(&client->dev, "sensor found no valid OTP data\n");
                return ERR_PTR(ret);
        }

        return buf;
}

static int
__mn34130_get_max_fps_index(const struct mn34130_fps_setting *fps_settings)
{
	int i;

	for (i = 0; i < MAX_FPS_OPTIONS_SUPPORTED; i++) {
		if (fps_settings[i].fps == 0)
			break;
	}

	return i - 1;
}

static int __mn34130_set_exposure(struct v4l2_subdev *sd, int integration_time,
				  int gain, int dig_gain, u16 *ppl, u16 *lpf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	/* Update frame timings. Expsure must be minimum < Frame Length - 4 */
	/* Increase the Frame Length to match integration time + 4 */
	if (integration_time > *lpf - MN34130_INTEGRATION_TIME_MARGIN)
		*lpf = (u16) integration_time + MN34130_INTEGRATION_TIME_MARGIN;

	ret = mn34130_write_reg(client, MN34130_16BIT,
				MN34130_FRAME_LENGTH_LINES, *lpf);
	if (ret)
		return ret;

	ret = mn34130_write_reg(client, MN34130_16BIT,
				MN34130_COARSE_INTEGRATION_TIME,
				integration_time);
	if (ret)
		return ret;

	/* Digital gain : to all MWB channel gains */
	ret = mn34130_write_reg(client, MN34130_16BIT, MN34130_MWB_RED_GAIN,
	                        dig_gain);
	if (ret)
	        return ret;

	ret = mn34130_write_reg(client, MN34130_16BIT,
	                        MN34130_MWB_GREEN_RED_GAIN, dig_gain);
	if (ret)
	        return ret;

	ret = mn34130_write_reg(client, MN34130_16BIT,
	                        MN34130_MWB_GREEN_BLUE_GAIN, dig_gain);
	if (ret)
	        return ret;

	ret = mn34130_write_reg(client, MN34130_16BIT,
				MN34130_MWB_BLUE_GAIN, dig_gain);
	if (ret)
	        return ret;

	/* set global gain */
	ret = mn34130_write_reg(client, MN34130_16BIT, MN34130_GAIN, gain);

	return ret;
}

static int mn34130_set_exposure(struct v4l2_subdev *sd, int integration_time,
				int gain, int dig_gain)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	const struct mn34130_resolution *res;
	u16 ppl, lpf;
	int ret;

	mutex_lock(&dev->input_lock);

	/* Validate integration time */
	integration_time = clamp_t(int, integration_time, MN34130_EXPOSURE_MIN,
				   MN34130_EXPOSURE_MAX);

	/* Validate gain */
	gain += 256;
	gain = clamp_t(int, gain, MN34130_GAIN_MIN, MN34130_GAIN_MAX);

	/* Validate digital gain */
	dig_gain = clamp_t(int, dig_gain, MN34130_MWB_GAIN_MIN,
	                   MN34130_MWB_GAIN_MAX);

	//dig_gain = digital_gain_tbl[dig_gain - 32];

	res = &dev->curr_res_table[dev->fmt_idx];
	ppl = res->fps_options[dev->fps_index].pixels_per_line;
	lpf = res->fps_options[dev->fps_index].lines_per_frame;
	ret = __mn34130_set_exposure(sd, integration_time, gain, dig_gain, &ppl,
				     &lpf);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		return ret;
	}

	/* Updated the device variable. These are the current values. */
	dev->gain = gain;
	dev->integration_time = integration_time;
	dev->digital_gain = dig_gain;
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int mn34130_s_exposure(struct v4l2_subdev *sd,
			      struct atomisp_exposure *exposure)
{
	return mn34130_set_exposure(sd, exposure->integration_time[0],
				    exposure->gain[0], exposure->gain[1]);
}

static int mn34130_g_priv_int_data(struct v4l2_subdev *sd,
                                   struct v4l2_private_int_data *priv)
{
        struct i2c_client *client = v4l2_get_subdevdata(sd);
        struct mn34130_device *dev = to_mn34130_sensor(sd);
        u8 __user *to = priv->data;
        u32 read_size = priv->size;
        int ret;

        /* No need to copy data if size is 0 */
        if (!read_size)
                goto out;

        if (IS_ERR(dev->otp_data)) {
                dev_err(&client->dev, "OTP data not available");
                return PTR_ERR(dev->otp_data);
        }
        /* Correct read_size value only if bigger than maximum */
        if (read_size > MN34130_OTP_DATA_SIZE)
                read_size = MN34130_OTP_DATA_SIZE;

        ret = copy_to_user(to, dev->otp_data, read_size);
        if (ret) {
                dev_err(&client->dev, "%s: failed to copy OTP data to user\n",
                         __func__);
                return -EFAULT;
        }
out:
        /* Return correct size */
        priv->size = MN34130_OTP_DATA_SIZE;

        return 0;
}


static long mn34130_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret;
	int input_arg;
	s32 value;
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return mn34130_s_exposure(sd, (struct atomisp_exposure *)arg);
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
		return mn34130_g_priv_int_data(sd, arg);
	case ATOMISP_TEST_CMD_SET_VCM_POS:
		input_arg = *(int *)arg;
		ret = dev->vcm_driver->t_vcm_slew(sd, input_arg);
		ret = dev->vcm_driver->t_focus_abs(sd,input_arg);
		printk("[AsusVCM] Set postion to %d\n",input_arg);
		return 0;
	case ATOMISP_TEST_CMD_GET_VCM_POS:
		ret = dev->vcm_driver->q_focus_abs(sd, &value);
		*(int*) arg = value;
		printk("[AsusVCM] Get VCM postion %d\n",*(int*) arg);
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mn34130_init_registers(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return mn34130_write_reg_array(client, mn34130_basic_settings);
}

static void mn34130_uninit(struct v4l2_subdev *sd)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	dev->integration_time = 0;
	dev->gain = 0;
	dev->digital_gain = 0;
}

static int mn34130_power_up(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	int ret;

	/* Enable clock */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
	        goto fail_clk;

	/* Enable power */
	ret = dev->platform_data->power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* Release reset */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret) {
		dev_err(&client->dev, "gpio failed\n");
		goto fail_power;
	}

	return 0;

fail_power:
	dev->platform_data->power_ctrl(sd, 0);
fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
	mn34130_uninit(sd);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static void mn34130_power_down(struct v4l2_subdev *sd)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	mn34130_uninit(sd);
	/* gpio ctrl */
	if (dev->platform_data->gpio_ctrl(sd, 0))
		dev_err(&client->dev, "gpio failed\n");

	/* power control */
	if (dev->platform_data->power_ctrl(sd, 0))
		dev_err(&client->dev, "vprog failed.\n");

	if (dev->platform_data->flisclk_ctrl(sd, 0))
		dev_err(&client->dev, "flisclk failed\n");
}

static int mn34130_s_power(struct v4l2_subdev *sd, int on)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	int ret = 0;

	mutex_lock(&dev->input_lock);
	if (on) {
		mn34130_power_up(sd);
		ret = mn34130_init_registers(sd);
	} else {
		mn34130_power_down(sd);
	}

	mutex_unlock(&dev->input_lock);

	return ret;
}

static int mn34130_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_MN34130, 0);

	return 0;
}

static int mn34130_get_intg_factor(struct v4l2_subdev *sd,
				   struct camera_mipi_info *info)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct atomisp_sensor_mode_data *m = &info->data;
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	const struct mn34130_resolution *res =
		&dev->curr_res_table[dev->fmt_idx];
	u16 pre_pll_clk_div, pll_multiplier, read_data;
	int ret;

	if (info == NULL)
		return -EINVAL;

	memset(&info->data, 0, sizeof(info->data));
	ret = mn34130_read_reg(client, MN34130_16BIT, MN34130_PRE_PLL_CLK_DIV,
			       &pre_pll_clk_div);
	if (ret)
		return -EIO;

	ret = mn34130_read_reg(client, MN34130_16BIT, MN34130_PLL_MULTIPLIER,
			       &pll_multiplier);
	if (ret)
		return -EIO;

	m->vt_pix_clk_freq_mhz = MN34130_MCLK / ++pre_pll_clk_div *
		pll_multiplier / ((res->bin_factor_x) ? res->bin_factor_x : 1) /
		10 * 4;
	ret = mn34130_read_reg(client, MN34130_16BIT,
			       MN34130_INTEGRATION_TIME_MIN,
			       &read_data);
	if (ret)
		return -EIO;

	m->coarse_integration_time_min = read_data;
	ret = mn34130_read_reg(client, MN34130_16BIT,
			       MN34130_INTEGRATION_TIME_MAX_MARGIN,
			       &read_data);
	if (ret)
		return -EIO;

	m->coarse_integration_time_max_margin = read_data;

	/*
	 * read_mode indicate whether binning is used for calculating
	 * the correct exposure value from the user side. So adapt the
	 * read mode values accordingly.
	 */
	m->read_mode = res->bin_factor_x ?
		MN34130_READ_MODE_BINNING_ON : MN34130_READ_MODE_BINNING_OFF;
	m->binning_factor_x = res->bin_factor_x ? res->bin_factor_x : 1;
	m->binning_factor_y = res->bin_factor_y ? res->bin_factor_y : 1;

	/* Get the cropping and output resolution to ISP for this mode. */
	// Crop X Start: The read_data from 0x0344 will be the size after binning. So we need to multiple the binning factor back
	ret = mn34130_read_reg(client, MN34130_16BIT, MN34130_HORIZONTAL_START,
			       &read_data);
	if (ret)
		return -EIO;

	m->crop_horizontal_start = read_data * m->binning_factor_x;
	// Crop X End: The read_data from 0x0348 is incorrect. Need the (output_size_x + crop_x_start) * binning factor -1
	m->crop_horizontal_end = (res->width + read_data) * m->binning_factor_x - 1;

	// Crop Y Start
	ret = mn34130_read_reg(client, MN34130_16BIT, MN34130_VERTICAL_START,
			       &read_data);
	if (ret)
		return -EIO;

	m->crop_vertical_start = read_data;

	// Crop Y End
	ret = mn34130_read_reg(client, MN34130_16BIT, MN34130_VERTICAL_SIZE,
			       &read_data);
	if (ret)
		return -EIO;

	m->crop_vertical_end = read_data;

	m->output_width = res->width;
	m->output_height = res->height;
	m->line_length_pck = res->fps_options[dev->fps_index].pixels_per_line;
	m->frame_length_lines =
		res->fps_options[dev->fps_index].lines_per_frame;

	currentFPS = res->fps_options[dev->fps_index].fps;

	return 0;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int mn34130_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 coarse;
	int ret;

	/* the fine integration time is currently not calculated */
	ret = mn34130_read_reg(client, MN34130_16BIT,
			       MN34130_COARSE_INTEGRATION_TIME, &coarse);
	*value = coarse;

	return ret;
}

int mn34130_q_focus_abs(struct v4l2_subdev *sd, s32 *value)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	if (dev->vcm_driver && dev->vcm_driver->q_focus_abs)
		return dev->vcm_driver->q_focus_abs(sd, value);
	return 0;
}

int mn34130_q_focus_status(struct v4l2_subdev *sd, s32 *value)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	if (dev->vcm_driver && dev->vcm_driver->q_focus_status)
		return dev->vcm_driver->q_focus_status(sd, value);
	return 0;
}

static int mn34130_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (MN34130_FOCAL_LENGTH_NUM << 16) | MN34130_FOCAL_LENGTH_DEM;
	return 0;
}

static int mn34130_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	*val = (MN34130_F_NUMBER_DEFAULT_NUM << 16) | MN34130_F_NUMBER_DEM;
	return 0;
}

static int mn34130_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (MN34130_F_NUMBER_DEFAULT_NUM << 24) |
		(MN34130_F_NUMBER_DEM << 16) |
		(MN34130_F_NUMBER_DEFAULT_NUM << 8) | MN34130_F_NUMBER_DEM;
	return 0;
}

static int mn34130_g_bin_factor_x(struct v4l2_subdev *sd, s32 *val)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	*val = dev->curr_res_table[dev->fmt_idx].bin_factor_x;

	return 0;
}

static int mn34130_g_bin_factor_y(struct v4l2_subdev *sd, s32 *val)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	*val = dev->curr_res_table[dev->fmt_idx].bin_factor_y;

	return 0;
}

int mn34130_t_focus_abs(struct v4l2_subdev *sd, s32 value)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	if (dev->vcm_driver && dev->vcm_driver->t_focus_abs)
		return dev->vcm_driver->t_focus_abs(sd, value);
	return 0;
}

int mn34130_t_focus_rel(struct v4l2_subdev *sd, s32 value)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	if (dev->vcm_driver && dev->vcm_driver->t_focus_rel)
		return dev->vcm_driver->t_focus_rel(sd, value);
	return 0;
}

int mn34130_t_vcm_slew(struct v4l2_subdev *sd, s32 value)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	if (dev->vcm_driver && dev->vcm_driver->t_vcm_slew)
		return dev->vcm_driver->t_vcm_slew(sd, value);
	return 0;
}

int mn34130_t_vcm_timing(struct v4l2_subdev *sd, s32 value)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	if (dev->vcm_driver && dev->vcm_driver->t_vcm_timing)
		return dev->vcm_driver->t_vcm_timing(sd, value);
	return 0;
}

/*
 * distance - calculate the distance
 * @res: resolution
 * @w: width
 * @h: height
 *
 * Get the gap between resolution and w/h.
 * res->width/height smaller than w/h wouldn't be considered.
 * Returns the value of gap or -1 if fail.
 */
/* tune this value so that the DVS resolutions get selected properly,
 * but make sure 16:9 does not match 4:3.
 */
#define LARGEST_ALLOWED_RATIO_MISMATCH 600
static int distance(struct mn34130_resolution const *res, const u32 w,
				const u32 h)
{
	unsigned int w_ratio = ((res->width << 13) / w);
	unsigned int h_ratio = ((res->height << 13) / h);
	int match = abs(((w_ratio << 13) / h_ratio) - ((int) 8192));

	if ((w_ratio < (int) 8192) || (h_ratio < (int) 8192)
		|| (match > LARGEST_ALLOWED_RATIO_MISMATCH))
		return -1;

	return w_ratio + h_ratio;
}

/*
 * Returns the nearest higher resolution index.
 * @w: width
 * @h: height
 * matching is done based on enveloping resolution and
 * aspect ratio. If the aspect ratio cannot be matched
 * to any index, -1 is returned.
 */
static int nearest_resolution_index(struct v4l2_subdev *sd, int w, int h)
{
	int i;
	int idx = -1;
	int dist;
	int min_dist = INT_MAX;
	const struct mn34130_resolution *tmp_res = NULL;
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	for (i = 0; i < dev->entries_curr_table; i++) {
		tmp_res = &dev->curr_res_table[i];
		dist = distance(tmp_res, w, h);
		if (dist == -1)
			continue;

		if (dist < min_dist) {
			min_dist = dist;
			idx = i;
		}
	}

	return idx;
}

static int get_resolution_index(struct v4l2_subdev *sd, int w, int h)
{
	int i;
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	for (i = 0; i < dev->entries_curr_table; i++) {
		if (w != dev->curr_res_table[i].width)
			continue;

		if (h != dev->curr_res_table[i].height)
			continue;

		/* Found it */
		return i;
	}

	return -1;
}

static int __mn34130_try_mbus_fmt(struct v4l2_subdev *sd,
				 struct v4l2_mbus_framefmt *fmt)
{
	int idx;
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	if (!fmt)
		return -EINVAL;

	if ((fmt->width > MN34130_RES_WIDTH_MAX) ||
		(fmt->height > MN34130_RES_HEIGHT_MAX)) {
		fmt->width = MN34130_RES_WIDTH_MAX;
		fmt->height = MN34130_RES_HEIGHT_MAX;
	} else {
		idx = nearest_resolution_index(sd, fmt->width, fmt->height);

		/*
		 * nearest_resolution_index() doesn't return smaller resolutions.
		 * If it fails, it means the requested resolution is higher than we
		 * can support. Fallback to highest possible resolution in this case.
		 */

		if (idx == -1 && ((fmt->width == 368 &&  fmt->height == 304) || ( fmt->width == 192 &&  fmt->height == 160))) {
			idx = 0;
		}

		if (idx == -1)
			idx = dev->entries_curr_table - 1;

		fmt->width = dev->curr_res_table[idx].width;
		fmt->height = dev->curr_res_table[idx].height;
	}

	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int mn34130_try_mbus_fmt(struct v4l2_subdev *sd,
			       struct v4l2_mbus_framefmt *fmt)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	int r;

	mutex_lock(&dev->input_lock);
	r = __mn34130_try_mbus_fmt(sd, fmt);
	mutex_unlock(&dev->input_lock);

	return r;
}

static int mn34130_s_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	struct camera_mipi_info *mn34130_info = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	const struct mn34130_resolution *res;

	mn34130_info = v4l2_get_subdev_hostdata(sd);
	if (mn34130_info == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = __mn34130_try_mbus_fmt(sd, fmt);
	if (ret)
		goto out;

	dev->fmt_idx = get_resolution_index(sd, fmt->width, fmt->height);

	/* Sanity check */
	if (unlikely(dev->fmt_idx == -1)) {
		ret = -EINVAL;
		goto out;
	}

	/* Sets the default FPS */
	dev->fps_index = 0;

	/* Get the current resolution setting */
	res = &dev->curr_res_table[dev->fmt_idx];
	printk("%s RES %s selected\n", __func__, dev->curr_res_table[dev->fmt_idx].desc);

	/* Write the selected resolution table values to the registers */
	ret = mn34130_write_reg_array(client, res->regs);
	if (ret)
		goto out;

	ret = mn34130_get_intg_factor(sd, mn34130_info);
out:
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int mn34130_g_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	if (!fmt)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fmt->width = dev->curr_res_table[dev->fmt_idx].width;
	fmt->height = dev->curr_res_table[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int mn34130_detect(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	u16 id = 0;
	int ret;

	/* i2c check */
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	ret = mn34130_read_reg(client, MN34130_16BIT, MN34130_CHIP_ID_ADDR,
			       &id);
	if (ret)
		return ret;

	dev_info(&client->dev, "Sensor Chip ID = 0x%4.4x\n", id);
	if (id != MN34130_CHIP_ID_VAL)
		return -ENODEV;

	return ret;
}

/*
 * mn34130 stream on/off
 */
static int mn34130_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	unsigned int rvalue = 0, tmp = 0, delayTime = 0;

	mutex_lock(&dev->input_lock);
	if (enable) {
		printk("<ASUS> Set Stream on\n");
	        // Set Stream on
	        ret = mn34130_write_reg(client, MN34130_8BIT, MN34130_STREAM_MODE, 1);
	        if (ret) {
			mutex_unlock(&dev->input_lock);
			v4l2_err(client, "failed to set streaming\n");
			return ret;
		}

		printk("<ASUS> Detect first frame coming\n");
		// Detect first frame coming
		delayTime = 0;
		ret = mn34130_read_reg(client, MN34130_8BIT, 0x0005, &rvalue);
		tmp = rvalue;
		while(tmp == rvalue){
			ret = mn34130_read_reg(client, MN34130_8BIT, 0x0005, &rvalue);
			mdelay(1);
			delayTime++;
			printk("<ASUS> rvalue = 0x%x\n", rvalue);
		}
		printk("<ASUS> check SOF delay = %d, rvalue = %d, tmp = %d\n", delayTime, rvalue, tmp);

		printk("<ASUS> Set LP00 off\n");
	    	// Set LP00 off
		ret = mn34130_write_reg(client, MN34130_8BIT, 0x3009, 0x00);
		if (ret) {
			mutex_unlock(&dev->input_lock);
			v4l2_err(client, "failed to set streaming\n");
			return ret;
		}
	} else {
		printk("<ASUS> Set LP00 on\n");
	        // Set LP00 on
	        ret = mn34130_write_reg(client, MN34130_8BIT, 0x3009, 0xf8);
	        if (ret) {
	                mutex_unlock(&dev->input_lock);
	                v4l2_err(client, "failed to set streaming\n");
	                return ret;
	        }

		msleep(10);

		printk("<ASUS> Stream off\n");
		// Stream off
		ret = mn34130_write_reg(client, MN34130_8BIT, MN34130_STREAM_MODE, 0);
		if (ret) {
			mutex_unlock(&dev->input_lock);
			v4l2_err(client, "failed to set streaming\n");
			return ret;
		}
	}

	dev->streaming = enable;
	mutex_unlock(&dev->input_lock);

	return 0;
}

/*
 * mn34130 enum frame size, frame intervals
 */
static int mn34130_enum_framesizes(struct v4l2_subdev *sd,
				   struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (index >= dev->entries_curr_table) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = dev->curr_res_table[index].width;
	fsize->discrete.height = dev->curr_res_table[index].height;
	fsize->reserved[0] = dev->curr_res_table[index].used;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int mn34130_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;
	int fmt_index;
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	const struct mn34130_resolution *res;

	mutex_lock(&dev->input_lock);

	/*
	 * since the isp will donwscale the resolution to the right size,
	 * find the nearest one that will allow the isp to do so important to
	 * ensure that the resolution requested is padded correctly by the
	 * requester, which is the atomisp driver in this case.
	 */
	fmt_index = nearest_resolution_index(sd, fival->width, fival->height);
	if (-1 == fmt_index)
		fmt_index = dev->entries_curr_table - 1;

	res = &dev->curr_res_table[fmt_index];

	/* Check if this index is supported */
	if (index > __mn34130_get_max_fps_index(res->fps_options)) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = dev->curr_res_table[index].width;
	fival->height = dev->curr_res_table[index].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = res->fps_options[index].fps;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int mn34130_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				 enum v4l2_mbus_pixelcode *code)
{
	*code = V4L2_MBUS_FMT_SGRBG10_1X10;
	return 0;
}

static int mn34130_s_config(struct v4l2_subdev *sd, int irq, void *pdata)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (pdata == NULL)
		return -ENODEV;

	dev->platform_data = pdata;

	mutex_lock(&dev->input_lock);
	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			mutex_unlock(&dev->input_lock);
			v4l2_err(client, "mn34130 platform init err\n");
			return ret;
		}
	}

	ret = mn34130_power_up(sd);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		v4l2_err(client, "mn34130 power-up err.\n");
		return ret;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = mn34130_detect(client);
	if (ret) {
		v4l2_err(client, "mn34130_detect err.\n");
		goto fail_detect;
	}

	/* Read sensor's OTP data */
	dev->otp_data = mn34130_otp_read(sd);

	/* power off sensor */
	mn34130_power_down(sd);
	mutex_unlock(&dev->input_lock);

	return ret;

fail_detect:
	dev->platform_data->csi_cfg(sd, 0);
fail_csi_cfg:
	mn34130_power_down(sd);
	mutex_unlock(&dev->input_lock);
	dev_err(&client->dev, "sensor power-gating failed\n");

	return ret;
}

static int
mn34130_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;

	code->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int
mn34130_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (index >= dev->entries_curr_table) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fse->min_width = dev->curr_res_table[index].width;
	fse->min_height = dev->curr_res_table[index].height;
	fse->max_width = dev->curr_res_table[index].width;
	fse->max_height = dev->curr_res_table[index].height;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int
mn34130_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	struct v4l2_mbus_framefmt *format = NULL;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		format = v4l2_subdev_get_try_format(fh, fmt->pad);
	else if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		format = &dev->format;

	if (format == NULL)
		return -EINVAL;

	fmt->format = *format;

	return 0;
}

static int
mn34130_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;

	return 0;
}

static int mn34130_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mn34130_device *dev =
		container_of(ctrl->handler, struct mn34130_device,
			     ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE_ABSOLUTE:
		return mn34130_q_exposure(&dev->sd, &ctrl->val);
	case V4L2_CID_FOCUS_ABSOLUTE:
		return mn34130_q_focus_abs(&dev->sd, &ctrl->val);
	case V4L2_CID_FOCUS_STATUS:
		return mn34130_q_focus_status(&dev->sd, &ctrl->val);
	case V4L2_CID_FOCAL_ABSOLUTE:
		return mn34130_g_focal(&dev->sd, &ctrl->val);
	case V4L2_CID_FNUMBER_ABSOLUTE:
		return mn34130_g_fnumber(&dev->sd, &ctrl->val);
	case V4L2_CID_FNUMBER_RANGE:
		return mn34130_g_fnumber_range(&dev->sd, &ctrl->val);
	case V4L2_CID_BIN_FACTOR_HORZ:
		return mn34130_g_bin_factor_x(&dev->sd, &ctrl->val);
	case V4L2_CID_BIN_FACTOR_VERT:
		return mn34130_g_bin_factor_y(&dev->sd, &ctrl->val);
	}
	return -EINVAL; /* Should not happen. */
}

static int mn34130_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mn34130_device *dev =
		container_of(ctrl->handler, struct mn34130_device,
			     ctrl_handler);

	/* input_lock is taken by the control framework, so it
	 * doesn't need to be taken here.
	 */
	/* We only handle V4L2_CID_RUN_MODE for now. */
	switch (ctrl->id) {
	case V4L2_CID_RUN_MODE:
		switch (ctrl->val) {
		case ATOMISP_RUN_MODE_VIDEO:
			dev->curr_res_table = mn34130_res_video;
			dev->entries_curr_table = ARRAY_SIZE(mn34130_res_video);
			break;

		case ATOMISP_RUN_MODE_STILL_CAPTURE:
			dev->curr_res_table = mn34130_res_still;
			dev->entries_curr_table = ARRAY_SIZE(mn34130_res_still);
			break;

		default:
			dev->curr_res_table = mn34130_res_preview;
			dev->entries_curr_table =
				ARRAY_SIZE(mn34130_res_preview);
		}

		dev->fmt_idx = 0;
		dev->fps_index = 0;

		return 0;
	case V4L2_CID_FOCUS_ABSOLUTE:
		return mn34130_t_focus_abs(&dev->sd, ctrl->val);
	case V4L2_CID_FOCUS_RELATIVE:
		return mn34130_t_focus_rel(&dev->sd, ctrl->val);
	case V4L2_CID_VCM_SLEW:
		return mn34130_t_vcm_slew(&dev->sd, ctrl->val);
	case V4L2_CID_VCM_TIMEING:
		return mn34130_t_vcm_timing(&dev->sd, ctrl->val);
	}

	return -EINVAL; /* Should not happen. */
}

static int mn34130_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *interval)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	const struct mn34130_resolution *res;

	mutex_lock(&dev->input_lock);

	/* Return the currently selected settings' maximum frame interval */
	res = &dev->curr_res_table[dev->fmt_idx];

	interval->interval.numerator = 1;
	interval->interval.denominator = res->fps_options[dev->fps_index].fps;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int mn34130_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *interval)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);
	struct camera_mipi_info *info = v4l2_get_subdev_hostdata(sd);
	const struct mn34130_resolution *res =
		&dev->curr_res_table[dev->fmt_idx];
	int i, fps, ret;
	u16 ppl, lpf;

	mutex_lock(&dev->input_lock);
	if (!interval->interval.numerator)
		interval->interval.numerator = 1;

	fps = interval->interval.denominator / interval->interval.numerator;

	/* Ignore if we are already using the required FPS. */
	if (fps == res->fps_options[dev->fps_index].fps)
		return 0;

	dev->fps_index = 0;

	/* Go through the supported FPS list */
	for (i = 0; i < MAX_FPS_OPTIONS_SUPPORTED; i++) {
		if (!res->fps_options[i].fps)
			break;
		if (abs(res->fps_options[i].fps - fps) <
			abs(res->fps_options[dev->fps_index].fps - fps))
			dev->fps_index = i;
	}

	/* Get the new Frame timing values for new exposure */
	ppl = res->fps_options[dev->fps_index].pixels_per_line;
	lpf = res->fps_options[dev->fps_index].lines_per_frame;

	/* update frametiming. Conside the curren exposure/gain as well */
	ret = __mn34130_set_exposure(sd, dev->integration_time, dev->gain,
				     dev->digital_gain, &ppl, &lpf);
	if (ret)
		return ret;

	/* Update the new values so that user side knows the current settings */
	ret = mn34130_get_intg_factor(sd, info);
	if (ret)
		return ret;

	interval->interval.denominator = res->fps_options[dev->fps_index].fps;
	interval->interval.numerator = 1;
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int mn34130_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	mutex_lock(&dev->input_lock);
	*frames = dev->curr_res_table[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static const struct v4l2_subdev_video_ops mn34130_video_ops = {
	.s_stream = mn34130_s_stream,
	.enum_framesizes = mn34130_enum_framesizes,
	.enum_frameintervals = mn34130_enum_frameintervals,
	.enum_mbus_fmt = mn34130_enum_mbus_fmt,
	.try_mbus_fmt = mn34130_try_mbus_fmt,
	.g_mbus_fmt = mn34130_g_mbus_fmt,
	.s_mbus_fmt = mn34130_s_mbus_fmt,
	.g_frame_interval = mn34130_g_frame_interval,
	.s_frame_interval = mn34130_s_frame_interval,
};

static const struct v4l2_subdev_sensor_ops mn34130_sensor_ops = {
	.g_skip_frames	= mn34130_g_skip_frames,
};

static const struct v4l2_subdev_core_ops mn34130_core_ops = {
	.g_chip_ident = mn34130_g_chip_ident,
	.queryctrl = v4l2_subdev_queryctrl,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.s_power = mn34130_s_power,
	.ioctl = mn34130_ioctl,
};

static const struct v4l2_subdev_pad_ops mn34130_pad_ops = {
	.enum_mbus_code = mn34130_enum_mbus_code,
	.enum_frame_size = mn34130_enum_frame_size,
	.get_fmt = mn34130_get_pad_format,
	.set_fmt = mn34130_set_pad_format,
};

static const struct v4l2_subdev_ops mn34130_ops = {
	.core = &mn34130_core_ops,
	.video = &mn34130_video_ops,
	.pad = &mn34130_pad_ops,
	.sensor = &mn34130_sensor_ops,
};

static int mn34130_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct mn34130_device *dev = to_mn34130_sensor(sd);

	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();

	media_entity_cleanup(&dev->sd.entity);
	v4l2_ctrl_handler_free(&dev->ctrl_handler);
	dev->platform_data->csi_cfg(sd, 0);
	v4l2_device_unregister_subdev(sd);
	kfree(dev);

	remove_proc_entry("otp", NULL);

	return 0;
}

static void mn34130_shutdown(struct i2c_client *client)
{
	mn34130_remove(client);
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.s_ctrl = mn34130_s_ctrl,
	.g_volatile_ctrl = mn34130_g_ctrl,
};

static const char * const ctrl_run_mode_menu[] = {
	NULL,
	"Video",
	"Still capture",
	"Continuous capture",
	"Preview",
};

static const struct v4l2_ctrl_config ctrl_run_mode = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_RUN_MODE,
	.name = "run mode",
	.type = V4L2_CTRL_TYPE_MENU,
	.min = 1,
	.def = 4,
	.max = 4,
	.qmenu = ctrl_run_mode_menu,
};

static const struct v4l2_ctrl_config ctrls[] = {
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_EXPOSURE_ABSOLUTE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "exposure",
		.min = 0x0,
		.max = 0xffff,
		.step = 0x01,
		.def = 0x00,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCUS_ABSOLUTE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "focus move absolute",
		.min = 0,
		.max = MN34130_MAX_FOCUS_POS,
		.step = 1,
		.def = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCUS_RELATIVE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "focus move relative",
		.min = MN34130_MAX_FOCUS_NEG,
		.max = MN34130_MAX_FOCUS_POS,
		.step = 1,
		.def = 0,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCUS_STATUS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "focus status",
		.min = 0,
		.max = 100, /* allow enum to grow in the future */
		.step = 1,
		.def = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_VCM_SLEW,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "vcm slew",
		.min = 0,
		.max = MN34130_VCM_SLEW_STEP_MAX,
		.step = 1,
		.def = 0,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_VCM_TIMEING,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "vcm step time",
		.min = 0,
		.max = MN34130_VCM_SLEW_TIME_MAX,
		.step = 1,
		.def = 0,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCAL_ABSOLUTE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "focal length",
		.min = MN34130_FOCAL_LENGTH_DEFAULT,
		.max = MN34130_FOCAL_LENGTH_DEFAULT,
		.step = 0x01,
		.def = MN34130_FOCAL_LENGTH_DEFAULT,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FNUMBER_ABSOLUTE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "f-number",
		.min = MN34130_F_NUMBER_DEFAULT,
		.max = MN34130_F_NUMBER_DEFAULT,
		.step = 0x01,
		.def = MN34130_F_NUMBER_DEFAULT,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FNUMBER_RANGE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "f-number range",
		.min = MN34130_F_NUMBER_RANGE,
		.max = MN34130_F_NUMBER_RANGE,
		.step = 0x01,
		.def = MN34130_F_NUMBER_RANGE,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_BIN_FACTOR_HORZ,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "horizontal binning factor",
		.min = 0,
		.max = MN34130_BIN_FACTOR_MAX,
		.step = 1,
		.def = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_BIN_FACTOR_VERT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "vertical binning factor",
		.min = 0,
		.max = MN34130_BIN_FACTOR_MAX,
		.step = 1,
		.def = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	}
};

static int imx_otp_proc_show(struct seq_file *s, void *v)
{
       int i;

        for(i=0 ; i<20 ; i++)
        {
                seq_printf(s, "0x%X", mn34130_otp_data[i]);
                if((i+1) % 8 != 0 && (i+1) != 20)
                        seq_printf(s, " ");
                else
                        seq_printf(s, "\n");
        }

       return 0;
}

static int imx_proc_open(struct inode *inode, struct  file *file)
{
       return single_open(file, imx_otp_proc_show, NULL);
}

static const struct file_operations otp_proc_fops = {
       .owner = THIS_MODULE,
       .open = imx_proc_open,
       .read = seq_read,
       .llseek = seq_lseek,
       .release = single_release,
};

static int mn34130_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct mn34130_device *dev;
	unsigned int i;
	int ret;

	/* allocate sensor device & init sub device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		v4l2_err(client, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);
	dev->fmt_idx = 0;
	v4l2_i2c_subdev_init(&(dev->sd), client, &mn34130_ops);
	if (client->dev.platform_data) {
		ret = mn34130_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret) {
			v4l2_device_unregister_subdev(&dev->sd);
			kfree(dev);
			return ret;
		}
	}

	dev->vcm_driver = &mn34130_vcms;
	dev->vcm_driver->init(&dev->sd);

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	dev->format.code = V4L2_MBUS_FMT_SGRBG10_1X10;
	ret = v4l2_ctrl_handler_init(&dev->ctrl_handler, ARRAY_SIZE(ctrls) + 1);
	if (ret) {
		mn34130_remove(client);
		return ret;
	}

	dev->run_mode = v4l2_ctrl_new_custom(&dev->ctrl_handler,
					     &ctrl_run_mode, NULL);
	for (i = 0; i < ARRAY_SIZE(ctrls); i++)
		v4l2_ctrl_new_custom(&dev->ctrl_handler, &ctrls[i], NULL);

	if (dev->ctrl_handler.error) {
		mn34130_remove(client);
		return dev->ctrl_handler.error;
	}

	/* Use same lock for controls as for everything else. */
	dev->ctrl_handler.lock = &dev->input_lock;
	dev->sd.ctrl_handler = &dev->ctrl_handler;
	v4l2_ctrl_handler_setup(&dev->ctrl_handler);
	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		mn34130_remove(client);
		return ret;
	}

	proc_create("otp", 0, NULL, &otp_proc_fops);

	return 0;
}

static const struct i2c_device_id mn34130_id[] = {
	{MN34130_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, mn34130_id);

static struct i2c_driver mn34130_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = MN34130_NAME,
	},
	.probe = mn34130_probe,
	.remove = mn34130_remove,
	.shutdown = mn34130_shutdown,
	.id_table = mn34130_id,
};

static __init int mn34130_init_mod(void)
{
	return i2c_add_driver(&mn34130_driver);
}

static __exit void mn34130_exit_mod(void)
{
	i2c_del_driver(&mn34130_driver);
}

module_init(mn34130_init_mod);
module_exit(mn34130_exit_mod);

MODULE_DESCRIPTION("A low-level driver for Panasonic MN34130 sensors");
MODULE_LICENSE("GPL");
