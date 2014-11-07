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
#define IMX_OTP_START_ADDR		0x3204
#define IMX_OTP_DATA_SIZE		512
#define IMX_OTP_PAGE_SIZE		24
#define IMX_OTP_PAGE_REG		0x3202
#define IMX_OTP_MODE_REG		0x3200
#define IMX_OTP_PAGE_MAX		12
#define IMX_OTP_READ_ONETIME		8
#define IMX_OTP_MODE_READ		1
#define IMX_OTP_WRITE_CLOCK		0x3302
#define IMX_OTP_ECC			0x3300

#define IMX219_DEFAULT_AF_10CM		370
#define IMX219_DEFAULT_AF_INF   	210
#define IMX219_DEFAULT_AF_START 	156
#define IMX219_DEFAULT_AF_END   	560

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
	int i=0;
	u8 read_value[24];

	ret = imx_write_reg_array(client, imx_soft_standby);
	if (ret) {
		dev_err(&client->dev, "failed to write standby mode.\n");
		return ret;
	}

	ret = imx_write_reg(client, IMX_16BIT, IMX_OTP_WRITE_CLOCK, 0x01E0);
	if (ret) {
		dev_err(&client->dev, "failed to write OTP write clock.\n");
		return ret;
	}

	ret = imx_write_reg(client, IMX_16BIT, 0x012A, 0x1334);
	if (ret) {
		dev_err(&client->dev, "failed to write INCK.\n");
		return ret;
	}

	ret = imx_write_reg(client, IMX_8BIT, IMX_OTP_ECC, 0x08);
	if (ret) {
		dev_err(&client->dev, "failed to write ECC.\n");
		return ret;
	}

	ret = imx_write_reg(client, IMX_8BIT, IMX_OTP_MODE_REG, IMX_OTP_MODE_READ);
	if (ret) {
		dev_err(&client->dev, "failed to write read mode.\n");
		return ret;
	}

	for(i=2; i>=0; i--) {

		/*set page NO.*/
		ret = imx_write_reg(client, IMX_8BIT, IMX_OTP_PAGE_REG, i);
		if (ret) {
			dev_err(&client->dev, "failed to prepare OTP page\n");
			return ret;
		}

		/* Reading the OTP data array */
		ret = imx_read_otp_reg_array(client, IMX_OTP_PAGE_SIZE,
			IMX_OTP_START_ADDR, read_value);
		if (ret) {
			dev_err(&client->dev, "failed to read OTP data\n");
			return ret;
		}

	    	printk("%s Check bank %d 0x%X 0x%X\n", __func__, i, read_value[0], read_value[1]);
		if((read_value[0]!=0 || read_value[1]!=0) && (read_value[0]!=0xff || read_value[1]!=0xff))
			break;

		/* For Chicony bank 3(Page0/0x3234 to Page1/0x320b) */
		if(i == 2) {
			/*set page NO.*/
			ret = imx_write_reg(client, IMX_8BIT, IMX_OTP_PAGE_REG, 0);
			if (ret) {
				dev_err(&client->dev, "failed to prepare OTP page\n");
				return ret;
			}

			/* Reading the OTP data array */
			ret = imx_read_otp_reg_array(client, 16,
				IMX_OTP_START_ADDR + 2*IMX_OTP_PAGE_SIZE, read_value);
			if (ret) {
				dev_err(&client->dev, "failed to read OTP data\n");
				return ret;
			}

			/*set page NO.*/
			ret = imx_write_reg(client, IMX_8BIT, IMX_OTP_PAGE_REG, 1);
			if (ret) {
				dev_err(&client->dev, "failed to prepare OTP page\n");
				return ret;
			}

			/* Reading the OTP data array */
			ret = imx_read_otp_reg_array(client, 8,
				IMX_OTP_START_ADDR, read_value + 16);
			if (ret) {
				dev_err(&client->dev, "failed to read OTP data\n");
				return ret;
			}

		    	printk("%s Check bank %d 0x%X 0x%X for Chicony\n", __func__, i, read_value[0], read_value[1]);
			if((read_value[0]!=0 || read_value[1]!=0) && (read_value[0]!=0xff || read_value[1]!=0xff))
				break;
		}
		/* For Chicony bank 2(Page0/0x321c to Page0/0x3233) */
		else if(i == 1) {
			/*set page NO.*/
			ret = imx_write_reg(client, IMX_8BIT, IMX_OTP_PAGE_REG, 0);
			if (ret) {
				dev_err(&client->dev, "failed to prepare OTP page\n");
				return ret;
			}

			/* Reading the OTP data array */
			ret = imx_read_otp_reg_array(client, IMX_OTP_PAGE_SIZE,
				IMX_OTP_START_ADDR + IMX_OTP_PAGE_SIZE, read_value);
			if (ret) {
				dev_err(&client->dev, "failed to read OTP data\n");
				return ret;
			}

		    	printk("%s Check bank %d 0x%X 0x%X for Chicony\n", __func__, i, read_value[0], read_value[1]);
			if((read_value[0]!=0 || read_value[1]!=0) && (read_value[0]!=0xff || read_value[1]!=0xff))
				break;
		}

	}

	memcpy(imx_otp_data, read_value, 24);

	buf->af_inf_pos = read_value[0]<<8 | read_value[1];
	buf->af_1m_pos = read_value[2]<<8 | read_value[3];
	buf->af_10cm_pos = read_value[4]<<8 | read_value[5];
	buf->af_start_curr = read_value[6]<<8 | read_value[7];
	buf->module_id = read_value[8];
	buf->vendor_id = read_value[9];
	buf->default_af_inf_pos = IMX219_DEFAULT_AF_INF;
	buf->default_af_10cm_pos = IMX219_DEFAULT_AF_10CM;
	buf->default_af_start = IMX219_DEFAULT_AF_START;
	buf->default_af_end = IMX219_DEFAULT_AF_END;

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


