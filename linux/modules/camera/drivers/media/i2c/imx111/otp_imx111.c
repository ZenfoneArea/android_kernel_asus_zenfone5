/*
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
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
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>
#include <asm/intel-mid.h>
#include "common.h"

/* Defines for OTP Data Registers */
#define IMX_OTP_START_ADDR		0x3500
#define IMX_OTP_DATA_SIZE		1280
#define IMX_OTP_PAGE_SIZE		8 //64
#define IMX_OTP_READY_REG		0x3B01
#define IMX_OTP_PAGE_REG		0x34C9 //0x3B02
#define IMX_OTP_MODE_REG		0x3B00
#define IMX_OTP_PAGE_MAX		20
#define IMX_OTP_READY_REG_DONE		1
#define IMX_OTP_READ_ONETIME		8 //32
#define IMX_OTP_MODE_READ		1

#define IMX111_DEFAULT_AF_10CM		370
#define IMX111_DEFAULT_AF_INF   	210
#define IMX111_DEFAULT_AF_START 	156
#define IMX111_DEFAULT_AF_END   	560

struct imx_af_data {
        u16 af_inf_pos;
        u16 af_1m_pos;
        u16 af_10cm_pos;
        u16 af_start_curr;
        u8 module_id;
        u8 vendor_id;
        u16 default_af_inf_pos;
        u16 default_af_10cm_pos;
        u16 default_af_start;
        u16 default_af_end;
};

u8 imx_otp_data[24];

static int
imx_read_otp_data(struct i2c_client *client, u16 len, u16 reg, void *val)
{
	struct i2c_msg msg[2];
	u16 data[IMX_SHORT_MAX] = { 0 };
	int err;

	if (len > IMX_BYTE_MAX) {
		dev_err(&client->dev, "%s error, invalid data length\n",
			__func__);
		return -EINVAL;
	}

	memset(msg, 0 , sizeof(msg));
	memset(data, 0 , sizeof(data));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = I2C_MSG_LENGTH;
	msg[0].buf = (u8 *)data;
	/* high byte goes first */
	data[0] = cpu_to_be16(reg);

	msg[1].addr = client->addr;
	msg[1].len = len;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = (u8 *)data;

	err = i2c_transfer(client->adapter, msg, 2);
	if (err != 2) {
		if (err >= 0)
			err = -EIO;
		goto error;
	}

	memcpy(val, data, len);
	return 0;

error:
	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

static int imx_read_otp_reg_array(struct i2c_client *client, u16 size, u16 addr,
				  u8 *buf)
{
	u16 index;
	int ret;

	for (index = 0; index + IMX_OTP_READ_ONETIME <= size;
					index += IMX_OTP_READ_ONETIME) {
		ret = imx_read_otp_data(client, IMX_OTP_READ_ONETIME,
					addr + index, &buf[index]);
		if (ret)
			return ret;
	}
	return 0;
}

static int __imx_otp_read(struct v4l2_subdev *sd, struct imx_af_data *buf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	int i=0, j=0, k=1;
	u8 read_value[24];

	for(i=6; i>=0; i=i-3) {

		/*set page NO.*/
		ret = imx_write_reg(client, IMX_8BIT, IMX_OTP_PAGE_REG, i & 0xff);
		if (ret) {
			dev_err(&client->dev, "failed to prepare OTP page\n");
			return ret;
		}

		/* Reading the OTP data array */
		ret = imx_read_otp_reg_array(client, IMX_OTP_PAGE_SIZE,
			IMX_OTP_START_ADDR + i * IMX_OTP_PAGE_SIZE, read_value);
		if (ret) {
			dev_err(&client->dev, "failed to read OTP data\n");
			return ret;
		}

	    	printk("%s Check bank %d 0x%X 0x%X\n", __func__, i, read_value[0], read_value[1]);
	    	if(((read_value[0] == 0) && (read_value[1] == 0)) || ((read_value[0] == 0xff) && (read_value[1] == 0xff))) {
			continue;
	    	}
		break;
	}

	for (j=i+1; j < i+3; j++) {

		/*set page NO.*/
		ret = imx_write_reg(client, IMX_8BIT, IMX_OTP_PAGE_REG, j & 0xff);
		if (ret) {
			dev_err(&client->dev, "failed to prepare OTP page\n");
			return ret;
		}

		/* Reading the OTP data array */
		ret = imx_read_otp_reg_array(client, IMX_OTP_PAGE_SIZE,
			IMX_OTP_START_ADDR + j * IMX_OTP_PAGE_SIZE, read_value + k * IMX_OTP_PAGE_SIZE);
		if (ret) {
			dev_err(&client->dev, "failed to read OTP data\n");
			return ret;
		}
		k++;
	}

	memcpy(imx_otp_data, read_value, 24);

	buf->af_inf_pos = read_value[0]<<8 | read_value[1];
	buf->af_1m_pos = read_value[2]<<8 | read_value[3];
	buf->af_10cm_pos = read_value[4]<<8 | read_value[5];
	buf->af_start_curr = read_value[6]<<8 | read_value[7];
	buf->module_id = read_value[8];
	buf->vendor_id = read_value[9];
	buf->default_af_inf_pos = IMX111_DEFAULT_AF_INF;
	buf->default_af_10cm_pos = IMX111_DEFAULT_AF_10CM;
	buf->default_af_start = IMX111_DEFAULT_AF_START;
	buf->default_af_end = IMX111_DEFAULT_AF_END;

	return 0;
}

void *imx_otp_read(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx_af_data *buf;
	int ret;

	buf = devm_kzalloc(&client->dev, IMX_OTP_DATA_SIZE, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ret = __imx_otp_read(sd, buf);

	/* Driver has failed to find valid data */
	if (ret) {
		dev_err(&client->dev, "sensor found no valid OTP data\n");
		return ERR_PTR(ret);
	}

	return buf;
}

