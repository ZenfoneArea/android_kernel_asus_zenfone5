/*
 * Support for Sony IMX camera sensor.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
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

#ifndef __IMX_H__
#define __IMX_H__
#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "imx219.h"


#define IMX_MCLK		192

/* TODO - This should be added into include/linux/videodev2.h */
#ifndef V4L2_IDENT_IMX
#define V4L2_IDENT_IMX	8245
#endif

/*
 * imx System control registers
 */
#define IMX_VT_PIX_CLK_DIV 0x0301
#define IMX_VT_SYS_CLK_DIV 0x0303
#define IMX_OP_PIX_DIV 0x0309
#define IMX_OP_SYS_DIV 0x030B

#define IMX_MASK_5BIT	0x1F
#define IMX_MASK_4BIT	0xF
#define IMX_MASK_2BIT	0x3
#define IMX_MASK_11BIT	0x7FF
#define IMX_INTG_BUF_COUNT		2

#define IMX_FINE_INTG_TIME		0x1E8

#define IMX_PRE_PLL_CLK_DIV			0x0305
#define IMX_PLL_MULTIPLIER			0x0306
#define IMX_RGPLTD                      0x30A4
#define IMX_CCP2_DATA_FORMAT            0x0113
#define IMX_FRAME_LENGTH_LINES		0x0160
#define IMX_LINE_LENGTH_PIXELS		0x0162
#define IMX_COARSE_INTG_TIME_MIN	0x1004
#define IMX_COARSE_INTG_TIME_MAX	0x1006
#define IMX_BINNING_ENABLE		0x0390
#define IMX_BINNING_TYPE		0x0391

#define IMX_READ_MODE				0x0390

#define IMX_HORIZONTAL_START_H 0x0164
#define IMX_VERTICAL_START_H 0x0168
#define IMX_HORIZONTAL_END_H 0x0166
#define IMX_VERTICAL_END_H 0x016A
#define IMX_HORIZONTAL_OUTPUT_SIZE_H 0x016c
#define IMX_VERTICAL_OUTPUT_SIZE_H 0x016e

#define IMX_COARSE_INTEGRATION_TIME		0x015A
#define IMX_TEST_PATTERN_MODE			0x0600
#define IMX_IMG_ORIENTATION			0x0101
#define IMX_VFLIP_BIT			2
#define IMX_HFLIP_BIT			1
#define IMX_GLOBAL_GAIN			0x0157
#define IMX_SHORT_AGC_GAIN		0x0233
#define IMX_DGC_ADJ		0x0158
#define IMX_DGC_LEN		4
#define IMX_MAX_EXPOSURE_SUPPORTED 0xff0b
#define IMX_MAX_GLOBAL_GAIN_SUPPORTED 0x00ff
#define IMX_MAX_DIGITAL_GAIN_SUPPORTED 0x0fff

/* Defines for register writes and register array processing */
#define I2C_RETRY_COUNT		5
#define IMX_TOK_MASK	0xfff0

#define MAX_FMTS 1

#define IMX_SUBDEV_PREFIX "imx"
#define IMX_DRIVER	"imx219"
#define IMX_NAME_219	"imx219"
#define IMX219_ID	0x0219

#define DEFAULT_OTP_SIZE 512
#define IMX219 0x219

#define IMX_ID_DEFAULT	0x0000
#define IMX219_CHIP_ID	0x0000

#define IMX219_RES_WIDTH_MAX	3280
#define IMX219_RES_HEIGHT_MAX	2464

/* Defines for lens/VCM */
#define IMX_FOCAL_LENGTH_NUM	269	/*2.69mm*/
#define IMX_FOCAL_LENGTH_DEM	100
#define IMX_F_NUMBER_DEFAULT_NUM	20
#define IMX_F_NUMBER_DEM	10
#define IMX_INVALID_CONFIG	0xffffffff
#define IMX_MAX_FOCUS_POS	1023
#define IMX_MAX_FOCUS_NEG	(-1023)
#define IMX_VCM_SLEW_STEP_MAX	0x3f
#define IMX_VCM_SLEW_TIME_MAX	0x1f

#define IMX_BIN_FACTOR_MAX			4
#define IMX_INTEGRATION_TIME_MARGIN	4
/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define IMX_FOCAL_LENGTH_DEFAULT 0x1710064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define IMX_F_NUMBER_DEFAULT 0x16000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define IMX_F_NUMBER_RANGE 0x160a160a

struct imx_vcm {
	int (*power_up)(struct v4l2_subdev *sd);
	int (*power_down)(struct v4l2_subdev *sd);
	int (*init)(struct v4l2_subdev *sd);
	int (*t_focus_vcm)(struct v4l2_subdev *sd, u16 val);
	int (*t_focus_abs)(struct v4l2_subdev *sd, s32 value);
	int (*t_focus_rel)(struct v4l2_subdev *sd, s32 value);
	int (*q_focus_status)(struct v4l2_subdev *sd, s32 *value);
	int (*q_focus_abs)(struct v4l2_subdev *sd, s32 *value);
	int (*t_vcm_slew)(struct v4l2_subdev *sd, s32 value);
	int (*t_vcm_timing)(struct v4l2_subdev *sd, s32 value);
};

struct imx_otp {
	void *(*otp_read)(struct v4l2_subdev *sd);
	u32 size;
};

struct imx_settings {
	struct imx_reg const *init_settings;
	struct imx_resolution *res_preview;
	struct imx_resolution *res_still;
	struct imx_resolution *res_video;
	int n_res_preview;
	int n_res_still;
	int n_res_video;
};

struct imx_settings imx219_set = {
	.init_settings = imx219_init_settings,
	.res_preview = imx219_res_preview,
	.res_still = imx219_res_still,
	.res_video = imx219_res_video,
	.n_res_preview = ARRAY_SIZE(imx219_res_preview),
	.n_res_still = ARRAY_SIZE(imx219_res_still),
	.n_res_video = ARRAY_SIZE(imx219_res_video),
};

#define	v4l2_format_capture_type_entry(_width, _height, \
		_pixelformat, _bytesperline, _colorspace) \
	{\
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,\
		.fmt.pix.width = (_width),\
		.fmt.pix.height = (_height),\
		.fmt.pix.pixelformat = (_pixelformat),\
		.fmt.pix.bytesperline = (_bytesperline),\
		.fmt.pix.colorspace = (_colorspace),\
		.fmt.pix.sizeimage = (_height)*(_bytesperline),\
	}

#define	s_output_format_entry(_width, _height, _pixelformat, \
		_bytesperline, _colorspace, _fps) \
	{\
		.v4l2_fmt = v4l2_format_capture_type_entry(_width, \
			_height, _pixelformat, _bytesperline, \
				_colorspace),\
		.fps = (_fps),\
	}

#define	s_output_format_reg_entry(_width, _height, _pixelformat, \
		_bytesperline, _colorspace, _fps, _reg_setting) \
	{\
		.s_fmt = s_output_format_entry(_width, _height,\
				_pixelformat, _bytesperline, \
				_colorspace, _fps),\
		.reg_setting = (_reg_setting),\
	}

struct s_ctrl_id {
	struct v4l2_queryctrl qc;
	int (*s_ctrl)(struct v4l2_subdev *sd, u32 val);
	int (*g_ctrl)(struct v4l2_subdev *sd, u32 *val);
};

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


struct imx_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

/* imx device structure */
struct imx_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct camera_sensor_platform_data *platform_data;
	struct mutex input_lock; /* serialize sensor's ioctl */
	int fmt_idx;
	int status;
	int streaming;
	int power;
	int run_mode;
	int vt_pix_clk_freq_mhz;
	int fps_index;
	u32 focus;
	u16 sensor_id;
	u16 coarse_itg;
	u16 fine_itg;
	u16 digital_gain;
	u16 gain;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 fps;
	u8 res;
	u8 type;
	u8 sensor_revision;
	u8 *otp_data;
	struct imx_settings *mode_tables;
	struct imx_vcm *vcm_driver;
	struct imx_otp *otp_driver;
	const struct imx_resolution *curr_res_table;
	int entries_curr_table;
};

#define to_imx_sensor(x) container_of(x, struct imx_device, sd)

#define IMX_MAX_WRITE_BUF_SIZE	32
struct imx_write_buffer {
	u16 addr;
	u8 data[IMX_MAX_WRITE_BUF_SIZE];
};

struct imx_write_ctrl {
	int index;
	struct imx_write_buffer buffer;
};


static const struct imx_reg imx_streaming[] = {
	{IMX_8BIT, 0x0100, 0x01},
	{IMX_TOK_TERM, 0, 0}
};

extern int dw9714_vcm_power_up(struct v4l2_subdev *sd);
extern int dw9714_vcm_power_down(struct v4l2_subdev *sd);
extern int dw9714_vcm_init(struct v4l2_subdev *sd);

extern int dw9714_t_focus_vcm(struct v4l2_subdev *sd, u16 val);
extern int dw9714_t_focus_abs(struct v4l2_subdev *sd, s32 value);
extern int dw9714_t_focus_rel(struct v4l2_subdev *sd, s32 value);
extern int dw9714_q_focus_status(struct v4l2_subdev *sd, s32 *value);
extern int dw9714_q_focus_abs(struct v4l2_subdev *sd, s32 *value);
extern int dw9714_t_vcm_slew(struct v4l2_subdev *sd, s32 value);
extern int dw9714_t_vcm_timing(struct v4l2_subdev *sd, s32 value);

extern int vcm_power_up(struct v4l2_subdev *sd);
extern int vcm_power_down(struct v4l2_subdev *sd);

struct imx_vcm imx219_vcm = {
	.power_up = dw9714_vcm_power_up,
	.power_down = dw9714_vcm_power_down,
	.init = dw9714_vcm_init,
	.t_focus_vcm = dw9714_t_focus_vcm,
	.t_focus_abs = dw9714_t_focus_abs,
	.t_focus_rel = dw9714_t_focus_rel,
	.q_focus_status = dw9714_q_focus_status,
	.q_focus_abs = dw9714_q_focus_abs,
	.t_vcm_slew = dw9714_t_vcm_slew,
	.t_vcm_timing = dw9714_t_vcm_timing,
};

extern void *imx_otp_read(struct v4l2_subdev *sd);
struct imx_otp imx219_otps = {
		.otp_read = imx_otp_read,
		.size = DEFAULT_OTP_SIZE,
};

#endif

