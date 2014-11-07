/*
 * Support for Omnivision MN34130 camera sensor.
 * Based on Aptina mt9e013 driver.
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

#ifndef __MN34130_H__
#define __MN34130_H__
#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/types.h>

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define to_mn34130_sensor(sd) container_of(sd, struct mn34130_device, sd)

#define I2C_MSG_LENGTH		0x2
#define V4L2_IDENT_MN34130	34130
#define MN34130_BYTE_MAX	32
#define MN34130_SHORT_MAX	16
#define I2C_RETRY_COUNT		5
#define I2C_RETRY_WAITUS	5000
#define MN34130_TOK_MASK	0xfff0

#define MAX_FPS_OPTIONS_SUPPORTED	3

#define MN34130_READ_MODE_BINNING_ON	0x0400
#define MN34130_READ_MODE_BINNING_OFF	0x0000

#define MN34130_INTEGRATION_TIME_MARGIN	4
#define MN34130_INTEGRATION_TIME_MAX	52170
#define MN34130_EXPOSURE_MIN		1
#define MN34130_EXPOSURE_MAX \
		(MN34130_INTEGRATION_TIME_MAX - MN34130_INTEGRATION_TIME_MARGIN)

#define MN34130_GAIN_MIN		256
#define MN34130_GAIN_MAX		512
#define MN34130_MWB_GAIN_MIN		256
#define MN34130_MWB_GAIN_MAX		384
#define MN34130_MWB_RED_GAIN		0x0210
#define MN34130_MWB_GREEN_RED_GAIN	0x020E
#define MN34130_MWB_GREEN_BLUE_GAIN	0x0214
#define MN34130_MWB_BLUE_GAIN		0x0212

#define	MN34130_NAME			"mn34130"
#define MN34130_CHIP_ID_ADDR		0x0000
#define MN34130_CHIP_ID_VAL		0x0001

#define MN34130_MCLK				19200000
#define MN34130_PRE_PLL_CLK_DIV			0x0304
#define MN34130_PLL_MULTIPLIER			0x0306
#define MN34130_INTEGRATION_TIME_MIN		0x1004
#define MN34130_INTEGRATION_TIME_MAX_MARGIN	0x1006
#define MN34130_FRAME_LENGTH_LINES		0x0340
#define MN34130_COARSE_INTEGRATION_TIME		0x0202
#define MN34130_GAIN				0x0204
#define MN34130_STREAM_MODE			0x0100

#define MN34130_HORIZONTAL_START		0x0344
#define MN34130_VERTICAL_START			0x0346
#define MN34130_HORIZONTAL_SIZE			0x0348
#define MN34130_VERTICAL_SIZE			0x034A
#define MN34130_OUTPUT_SIZE_X			0x034C
#define MN34130_OUTPUT_SIZE_Y			0x034E

/* Defines for OTP Data Registers */
#define MN34130_OTP_START_ADDR          0x3500
#define MN34130_OTP_DATA_SIZE		512
#define MN34130_OTP_READ_ONETIME	24

#define MN34130_DEFAULT_AF_10CM           370
#define MN34130_DEFAULT_AF_INF           235
#define MN34130_DEFAULT_AF_START         156
#define MN34130_DEFAULT_AF_END           660

struct mn34130_af_data {
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

/* Defines for lens/VCM */
#define MN34130_FOCAL_LENGTH_NUM	369	/*3.69mm*/
#define MN34130_FOCAL_LENGTH_DEM	100
#define MN34130_F_NUMBER_DEFAULT_NUM	20
#define MN34130_F_NUMBER_DEM	10
#define MN34130_MAX_FOCUS_POS	1023
#define MN34130_MAX_FOCUS_NEG	(-1023)
#define MN34130_VCM_SLEW_STEP_MAX	0x3f
#define MN34130_VCM_SLEW_TIME_MAX	0x1f

#define MN34130_BIN_FACTOR_MAX		6

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define MN34130_FOCAL_LENGTH_DEFAULT 0x1710064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define MN34130_F_NUMBER_DEFAULT 0x16000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define MN34130_F_NUMBER_RANGE 0x160a160a

struct mn34130_vcm {
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

enum mn34130_tok_type {
	MN34130_8BIT = 0x0001,
	MN34130_16BIT = 0x0002,
	MN34130_TOK_TERM = 0xf000,	/* terminating token for reg list */
	MN34130_TOK_DELAY = 0xfe00	/* delay token for reg list */
};

/**
 * struct mn34130_reg - sensor register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct mn34130_reg {
	enum mn34130_tok_type type;
	u16 reg;
	u32 val;	/* @set value for read/mod/write, @mask */
};

struct mn34130_fps_setting {
	int fps;
	unsigned short pixels_per_line;
	unsigned short lines_per_frame;
};

struct mn34130_resolution {
	u8 *desc;
	int res;
	int width;
	int height;
	bool used;
	const struct mn34130_reg *regs;
	u8 bin_factor_x;
	u8 bin_factor_y;
	unsigned short skip_frames;
	const struct mn34130_fps_setting fps_options[MAX_FPS_OPTIONS_SUPPORTED];
};

/* mn34130 device structure */
struct mn34130_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct camera_sensor_platform_data *platform_data;
	int fmt_idx;
	int streaming;
	int integration_time;
	int gain;
	int digital_gain;
	struct mutex input_lock; /* serialize sensor's ioctl */
	const struct mn34130_resolution *curr_res_table;
	int entries_curr_table;
	int fps_index;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *run_mode;
	u8 *otp_data;
	struct mn34130_vcm *vcm_driver;
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

struct mn34130_vcm mn34130_vcms = {
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

/*
 * table of digital gain value
 * digital gain(dB) = 20 * log10(gain) / 0.09375 + 256
 */
static const int digital_gain_tbl[] = {
	256, 259, 262, 264, 267, 269, 272, 274,
	277, 279, 281, 283, 286, 288, 290, 292,
	294, 295, 297, 299, 301, 303, 304, 306,
	308, 309, 311, 313, 314, 316, 317, 319,
	320
};

/*
 * The i2c adapter on Intel Medfield can transfer 32 bytes maximum
 * at a time. In burst mode we require that the buffer is transferred
 * in one shot, so limit the buffer size to 32 bytes minus a safety.
 */
#define MN34130_MAX_WRITE_BUF_SIZE	30
struct mn34130_write_buffer {
	u16 addr;
	u8 data[MN34130_MAX_WRITE_BUF_SIZE];
};

struct mn34130_write_ctrl {
	int index;
	struct mn34130_write_buffer buffer;
};

#define MN34130_RES_WIDTH_MAX	4224
#define MN34130_RES_HEIGHT_MAX	3168

static const struct mn34130_reg mn34130_basic_settings[] = {
	{MN34130_8BIT, 0x3009, 0xf8},
	{MN34130_8BIT, 0x0304, 0x00},
	{MN34130_8BIT, 0x0305, 0x02},
	{MN34130_8BIT, 0x0306, 0x00},
	{MN34130_8BIT, 0x0307, 0x65},
	{MN34130_8BIT, 0x3000, 0x01},
	{MN34130_8BIT, 0x3004, 0x02},
	{MN34130_8BIT, 0x3005, 0xA3},
	{MN34130_8BIT, 0x3000, 0x03},
	{MN34130_8BIT, 0x300A, 0x01},
	{MN34130_8BIT, 0x3006, 0x80},
	{MN34130_8BIT, 0x300A, 0x00},
	{MN34130_8BIT, 0x3000, 0x43},
	{MN34130_TOK_DELAY, 0, 100},
	{MN34130_8BIT, 0x0202, 0x06},
	{MN34130_8BIT, 0x0203, 0xC3},
	{MN34130_8BIT, 0x3006, 0x00},
	{MN34130_8BIT, 0x3016, 0x23},
	{MN34130_8BIT, 0x3018, 0x2C},
	{MN34130_8BIT, 0x301C, 0x57},
	{MN34130_8BIT, 0x301D, 0x42},
	{MN34130_8BIT, 0x301E, 0x54},
	{MN34130_8BIT, 0x3020, 0x00},
	{MN34130_8BIT, 0x3021, 0x00},
	{MN34130_8BIT, 0x3022, 0x00},
	{MN34130_8BIT, 0x3023, 0x00},
	{MN34130_8BIT, 0x3028, 0xF1},
	{MN34130_8BIT, 0x302C, 0x09},
	{MN34130_8BIT, 0x3031, 0x20},
	{MN34130_8BIT, 0x3034, 0x0E},
	{MN34130_8BIT, 0x303D, 0x01},
	{MN34130_8BIT, 0x303E, 0x02},
	{MN34130_8BIT, 0x304F, 0xFF},
	{MN34130_8BIT, 0x3070, 0xC7},
	{MN34130_8BIT, 0x308A, 0x8E},
	{MN34130_8BIT, 0x308D, 0x00},
	{MN34130_8BIT, 0x308E, 0x18},
	{MN34130_8BIT, 0x3091, 0x00},
	{MN34130_8BIT, 0x3092, 0x18},
	{MN34130_8BIT, 0x3095, 0x10},
	{MN34130_8BIT, 0x3096, 0x50},
	{MN34130_8BIT, 0x30A4, 0x63},
	{MN34130_8BIT, 0x30A7, 0x00},
	{MN34130_8BIT, 0x30A8, 0x06},
	{MN34130_8BIT, 0x30A9, 0x01},
	{MN34130_8BIT, 0x30AA, 0x9A},
	{MN34130_8BIT, 0x30AB, 0x00},
	{MN34130_8BIT, 0x30AC, 0x06},
	{MN34130_8BIT, 0x30AD, 0x0A},
	{MN34130_8BIT, 0x30AE, 0xC5},
	{MN34130_8BIT, 0x30AF, 0x05},
	{MN34130_8BIT, 0x30B0, 0x70},
	{MN34130_8BIT, 0x30B1, 0x03},
	{MN34130_8BIT, 0x30B2, 0x10},
	{MN34130_8BIT, 0x30BC, 0x01},
	{MN34130_8BIT, 0x30BE, 0x04},
	{MN34130_8BIT, 0x30C2, 0x42},
	{MN34130_8BIT, 0x30C5, 0x01},
	{MN34130_8BIT, 0x30CB, 0x40},
	{MN34130_8BIT, 0x30CE, 0x01},
	{MN34130_8BIT, 0x30D0, 0x18},
	{MN34130_8BIT, 0x30D4, 0x46},
	{MN34130_8BIT, 0x3272, 0x1F},
	{MN34130_8BIT, 0x3276, 0x22},
	{MN34130_8BIT, 0x3278, 0x2B},
	{MN34130_8BIT, 0x3280, 0x0B},
	{MN34130_8BIT, 0x329A, 0x78},
	{MN34130_8BIT, 0x3319, 0x14},
	{MN34130_8BIT, 0x3348, 0x0B},
	{MN34130_8BIT, 0x334B, 0x19},
	{MN34130_8BIT, 0x334D, 0x19},
	{MN34130_8BIT, 0x334F, 0x19},
	{MN34130_8BIT, 0x33C2, 0x23},
	{MN34130_8BIT, 0x3490, 0x38},
	{MN34130_8BIT, 0x3491, 0xED},
	{MN34130_8BIT, 0x3497, 0x70},
	{MN34130_8BIT, 0x3498, 0xED},
	{MN34130_8BIT, 0x349E, 0xB4},
	{MN34130_8BIT, 0x349F, 0xED},
	{MN34130_8BIT, 0x3721, 0x38},
	{MN34130_8BIT, 0x3723, 0x62},
	{MN34130_8BIT, 0x372D, 0x31},
	{MN34130_8BIT, 0x372E, 0x08},
	{MN34130_8BIT, 0x375E, 0x0F},
	{MN34130_8BIT, 0x375F, 0x18},
	{MN34130_8BIT, 0x376C, 0x0D},
	{MN34130_8BIT, 0x3000, 0xF3},
	{MN34130_TOK_DELAY, 0, 1000},
	{MN34130_TOK_TERM, 0, 0}
};

static const struct mn34130_reg mn34130_608x496[] = {
	{MN34130_8BIT, 0x0104, 0x01},	// Hold on
	{MN34130_8BIT, 0x3319, 0x09},	// Output V[line] setting
	{MN34130_8BIT, 0x30A7, 0x00},	// H Size setting
	{MN34130_8BIT, 0x30A8, 0x2E},
	{MN34130_8BIT, 0x30AB, 0x00},
	{MN34130_8BIT, 0x30AC, 0x2E},
	{MN34130_8BIT, 0x30AF, 0x02},
	{MN34130_8BIT, 0x30B0, 0x60},
	{MN34130_8BIT, 0x30B3, 0x00},
	{MN34130_8BIT, 0x30B4, 0x05},
	{MN34130_8BIT, 0x30B5, 0x00},
	{MN34130_8BIT, 0x30B6, 0x07},
	{MN34130_8BIT, 0x30A3, 0x03},	// V Size setting
	{MN34130_8BIT, 0x30A4, 0x63},
	{MN34130_8BIT, 0x30A5, 0x13},	// Line Length PCK
	{MN34130_8BIT, 0x30A6, 0xBC},
	{MN34130_8BIT, 0x3748, 0x4D},
	{MN34130_8BIT, 0x3749, 0x01},
	{MN34130_8BIT, 0x30A9, 0x00},
	{MN34130_8BIT, 0x30AA, 0x60},
	{MN34130_8BIT, 0x30AD, 0x0B},
	{MN34130_8BIT, 0x30AE, 0xFD},
	{MN34130_8BIT, 0x30B1, 0x01},
	{MN34130_8BIT, 0x30B2, 0xF0},
	{MN34130_8BIT, 0x30B7, 0x00},
	{MN34130_8BIT, 0x30B8, 0x05},
	{MN34130_8BIT, 0x30B9, 0x00},
	{MN34130_8BIT, 0x30BA, 0x07},
	{MN34130_8BIT, 0x30D0, 0x0C},	// output setting
	{MN34130_8BIT, 0x30D2, 0x40},
	{MN34130_8BIT, 0x30D3, 0x07},
	{MN34130_8BIT, 0x30D4, 0x47},
	{MN34130_8BIT, 0x30D5, 0x08},
	{MN34130_8BIT, 0x326E, 0x67},
	{MN34130_8BIT, 0x3270, 0x62},
	{MN34130_8BIT, 0x3708, 0xF4},
	{MN34130_8BIT, 0x370A, 0xA7},
	{MN34130_8BIT, 0x370C, 0xAC},
	{MN34130_8BIT, 0x370F, 0x05},
	{MN34130_8BIT, 0x3747, 0x07},
	{MN34130_8BIT, 0x374A, 0x0D},
	{MN34130_8BIT, 0x377A, 0x36},
	{MN34130_8BIT, 0x30BB, 0x02},	// Mode change
	{MN34130_8BIT, 0x30BB, 0x22},	// MIPI-IF update
	{MN34130_8BIT, 0x3057, 0x00},
	{MN34130_8BIT, 0x3058, 0x02},
	{MN34130_8BIT, 0x0104, 0x00},	// Hold off
	{MN34130_TOK_TERM, 0, 0}
};

static const struct mn34130_reg mn34130_656x496[] = {
	{MN34130_8BIT, 0x0104, 0x01},
	{MN34130_8BIT, 0x3319, 0x09},
	{MN34130_8BIT, 0x30A7, 0x00},
	{MN34130_8BIT, 0x30A8, 0x16},
	{MN34130_8BIT, 0x30AB, 0x00},
	{MN34130_8BIT, 0x30AC, 0x16},
	{MN34130_8BIT, 0x30AF, 0x02},
	{MN34130_8BIT, 0x30B0, 0x90},
	{MN34130_8BIT, 0x30B3, 0x00},
	{MN34130_8BIT, 0x30B4, 0x05},
	{MN34130_8BIT, 0x30B5, 0x00},
	{MN34130_8BIT, 0x30B6, 0x07},
	{MN34130_8BIT, 0x30A3, 0x02},
	{MN34130_8BIT, 0x30A4, 0xB9},
	{MN34130_8BIT, 0x30A5, 0x13},
	{MN34130_8BIT, 0x30A6, 0xBC},
	{MN34130_8BIT, 0x3748, 0x4D},
	{MN34130_8BIT, 0x3749, 0x01},
	{MN34130_8BIT, 0x30A9, 0x00},
	{MN34130_8BIT, 0x30AA, 0x60},
	{MN34130_8BIT, 0x30AD, 0x0B},
	{MN34130_8BIT, 0x30AE, 0xFD},
	{MN34130_8BIT, 0x30B1, 0x01},
	{MN34130_8BIT, 0x30B2, 0xF0},
	{MN34130_8BIT, 0x30B7, 0x00},
	{MN34130_8BIT, 0x30B8, 0x05},
	{MN34130_8BIT, 0x30B9, 0x00},
	{MN34130_8BIT, 0x30BA, 0x07},
	{MN34130_8BIT, 0x30D0, 0x0C},
	{MN34130_8BIT, 0x30D2, 0x40},
	{MN34130_8BIT, 0x30D3, 0x07},
	{MN34130_8BIT, 0x30D4, 0x47},
	{MN34130_8BIT, 0x30D5, 0x08},
	{MN34130_8BIT, 0x326E, 0x67},
	{MN34130_8BIT, 0x3270, 0x62},
	{MN34130_8BIT, 0x3708, 0xF4},
	{MN34130_8BIT, 0x370A, 0xA7},
	{MN34130_8BIT, 0x370C, 0xAC},
	{MN34130_8BIT, 0x370F, 0x05},
	{MN34130_8BIT, 0x3747, 0x07},
	{MN34130_8BIT, 0x374A, 0x0D},
	{MN34130_8BIT, 0x377A, 0x36},
	{MN34130_8BIT, 0x30BB, 0x02},
	{MN34130_8BIT, 0x30BB, 0x22},
	{MN34130_8BIT, 0x3057, 0x00},
	{MN34130_8BIT, 0x3058, 0x02},
	{MN34130_8BIT, 0x0104, 0x00},
	{MN34130_TOK_TERM, 0, 0}
};

static const struct mn34130_reg mn34130_1296x736[] = {
	{MN34130_8BIT, 0x0104, 0x01},
	{MN34130_8BIT, 0x3319, 0x10},
	{MN34130_8BIT, 0x30A7, 0x00},
	{MN34130_8BIT, 0x30A8, 0x36},
	{MN34130_8BIT, 0x30AB, 0x00},
	{MN34130_8BIT, 0x30AC, 0x36},
	{MN34130_8BIT, 0x30AF, 0x05},
	{MN34130_8BIT, 0x30B0, 0x10},
	{MN34130_8BIT, 0x30B3, 0x00},
	{MN34130_8BIT, 0x30B4, 0x03},
	{MN34130_8BIT, 0x30B5, 0x00},
	{MN34130_8BIT, 0x30B6, 0x03},
	{MN34130_8BIT, 0x30A3, 0x03},
	{MN34130_8BIT, 0x30A4, 0xE8},
	{MN34130_8BIT, 0x30A5, 0x13},
	{MN34130_8BIT, 0x30A6, 0x68},
	{MN34130_8BIT, 0x3748, 0x47},
	{MN34130_8BIT, 0x3749, 0x01},
	{MN34130_8BIT, 0x30A9, 0x01},
	{MN34130_8BIT, 0x30AA, 0xE2},
	{MN34130_8BIT, 0x30AD, 0x0A},
	{MN34130_8BIT, 0x30AE, 0x7D},
	{MN34130_8BIT, 0x30B1, 0x02},
	{MN34130_8BIT, 0x30B2, 0xE0},
	{MN34130_8BIT, 0x30B7, 0x00},
	{MN34130_8BIT, 0x30B8, 0x03},
	{MN34130_8BIT, 0x30B9, 0x00},
	{MN34130_8BIT, 0x30BA, 0x03},
	{MN34130_8BIT, 0x30D0, 0x18},
	{MN34130_8BIT, 0x30D2, 0x80},
	{MN34130_8BIT, 0x30D3, 0x07},
	{MN34130_8BIT, 0x30D4, 0x46},
	{MN34130_8BIT, 0x30D5, 0x19},
	{MN34130_8BIT, 0x326E, 0x2C},
	{MN34130_8BIT, 0x3270, 0x2F},
	{MN34130_8BIT, 0x3708, 0xD8},
	{MN34130_8BIT, 0x370A, 0xFB},
	{MN34130_8BIT, 0x370C, 0xF9},
	{MN34130_8BIT, 0x370F, 0x09},
	{MN34130_8BIT, 0x3747, 0x05},
	{MN34130_8BIT, 0x374A, 0x1C},
	{MN34130_8BIT, 0x377A, 0x32},
	{MN34130_8BIT, 0x30BB, 0x02},
	{MN34130_8BIT, 0x30BB, 0x22},
	{MN34130_8BIT, 0x3057, 0x02},
	{MN34130_8BIT, 0x0104, 0x00},
	{MN34130_TOK_TERM, 0, 0}
};

static const struct mn34130_reg mn34130_1936x1096[] = {
	{MN34130_8BIT, 0x0104, 0x01},	// Hold on
	{MN34130_8BIT, 0x3319, 0x14},	// Output V[line] setting
	{MN34130_8BIT, 0x306F, 0x06},
	{MN34130_8BIT, 0x3070, 0xC7},
	{MN34130_8BIT, 0x3073, 0x00},	// H Size setting
	{MN34130_8BIT, 0x3074, 0x58},
 	{MN34130_8BIT, 0x3077, 0x00},
	{MN34130_8BIT, 0x3078, 0x58},
	{MN34130_8BIT, 0x307B, 0x07},
	{MN34130_8BIT, 0x307C, 0x90},
	{MN34130_8BIT, 0x3075, 0x01},	// V Size setting
	{MN34130_8BIT, 0x3076, 0xE8},
	{MN34130_8BIT, 0x3079, 0x0A},
	{MN34130_8BIT, 0x307A, 0x77},
	{MN34130_8BIT, 0x307D, 0x04},
	{MN34130_8BIT, 0x307E, 0x48},
	{MN34130_8BIT, 0x3071, 0x15},	// Line Length PCK
	{MN34130_8BIT, 0x3072, 0x18},
	{MN34130_8BIT, 0x3345, 0x23},
	{MN34130_8BIT, 0x3346, 0x00},
	{MN34130_8BIT, 0x30BB, 0x00},	// Mode Change
	{MN34130_8BIT, 0x30BB, 0x20},	// Mode Change for prepare
	{MN34130_8BIT, 0x3057, 0x02},	// Mode Change for prepare
	{MN34130_8BIT, 0x0104, 0x00},	// Hold off
	{MN34130_TOK_TERM, 0, 0}
};

static const struct mn34130_reg mn34130_2064x1168[] = {
	{MN34130_8BIT, 0x0104, 0x01},	// Hold on
	{MN34130_8BIT, 0x3319, 0x14},	// Output V[line] setting
	{MN34130_8BIT, 0x306F, 0x06},
	{MN34130_8BIT, 0x3070, 0xC7},
	{MN34130_8BIT, 0x3073, 0x00},	// H Size setting
	{MN34130_8BIT, 0x3074, 0x18},
	{MN34130_8BIT, 0x3077, 0x00},
	{MN34130_8BIT, 0x3078, 0x18},
 	{MN34130_8BIT, 0x307B, 0x08},
	{MN34130_8BIT, 0x307C, 0x10},
	{MN34130_8BIT, 0x3075, 0x01},	// V Size setting
	{MN34130_8BIT, 0x3076, 0xA0},
 	{MN34130_8BIT, 0x3079, 0x0A},
	{MN34130_8BIT, 0x307A, 0xBF},
 	{MN34130_8BIT, 0x307D, 0x04},
	{MN34130_8BIT, 0x307E, 0x90},
	{MN34130_8BIT, 0x30BB, 0x00},	// Mode Change
	{MN34130_8BIT, 0x30BB, 0x20},	// Mode Change for prepare
	{MN34130_8BIT, 0x3057, 0x02},	// Mode Change for prepare
	{MN34130_8BIT, 0x0104, 0x00},	// Hold off
 	{MN34130_TOK_TERM, 0, 0}
};

static const struct mn34130_reg mn34130_2064x1552[] = {
	{MN34130_8BIT, 0x0104, 0x01},	// Hold on
	{MN34130_8BIT, 0x3319, 0x14},	// Output V[line] setting
	{MN34130_8BIT, 0x306F, 0x07},
	{MN34130_8BIT, 0x3070, 0x94},
	{MN34130_8BIT, 0x3073, 0x00},	// H Size setting
	{MN34130_8BIT, 0x3074, 0x18},
	{MN34130_8BIT, 0x3077, 0x00},
	{MN34130_8BIT, 0x3078, 0x18},
	{MN34130_8BIT, 0x307B, 0x08},
	{MN34130_8BIT, 0x307C, 0x10},
	{MN34130_8BIT, 0x3075, 0x00},	// V Size setting
	{MN34130_8BIT, 0x3076, 0x20},
	{MN34130_8BIT, 0x3079, 0x0C},
	{MN34130_8BIT, 0x307A, 0x3F},
	{MN34130_8BIT, 0x307D, 0x06},
	{MN34130_8BIT, 0x307E, 0x10},
	{MN34130_8BIT, 0x30BB, 0x00},	// Mode Change
	{MN34130_8BIT, 0x30BB, 0x20},	// Mode Change for prepare
	{MN34130_8BIT, 0x3057, 0x02},	// Mode Change for prepare
	{MN34130_8BIT, 0x0104, 0x00},	// Hold off
	{MN34130_TOK_TERM, 0, 0}
};

static const struct mn34130_reg mn34130_zsl_4112x2320[] = {
	{MN34130_8BIT, 0x0104, 0x01},	// Hold on
	{MN34130_8BIT, 0x3319, 0x28},	// Output V[Line] setting
	{MN34130_8BIT, 0x308D, 0x00},	// H Size setting
	{MN34130_8BIT, 0x308E, 0x38},
	{MN34130_8BIT, 0x3091, 0x00},
	{MN34130_8BIT, 0x3092, 0x38},
	{MN34130_8BIT, 0x3095, 0x10},
	{MN34130_8BIT, 0x3096, 0x10},
	{MN34130_8BIT, 0x3089, 0x0D},
	{MN34130_8BIT, 0x308A, 0x8E},
	{MN34130_8BIT, 0x308F, 0x01},	// V Size setting
	{MN34130_8BIT, 0x3090, 0xA8},
 	{MN34130_8BIT, 0x3093, 0x0A},
	{MN34130_8BIT, 0x3094, 0xB7},
 	{MN34130_8BIT, 0x3097, 0x09},
	{MN34130_8BIT, 0x3098, 0x10},
	{MN34130_8BIT, 0x308B, 0x13},	// Line Length PCK
 	{MN34130_8BIT, 0x308C, 0x68},
 	{MN34130_8BIT, 0x3627, 0x1C},
 	{MN34130_8BIT, 0x3628, 0x00},
	{MN34130_8BIT, 0x30BB, 0x01},	// Mode Change
	{MN34130_8BIT, 0x30BB, 0x21},	// MIPI-IF update
	{MN34130_8BIT, 0x3057, 0x04},
	{MN34130_8BIT, 0x0104, 0x00},	// Hold off
 	{MN34130_TOK_TERM, 0, 0}
};

static const struct mn34130_reg mn34130_zsl_4112x3088[] = {
	{MN34130_8BIT, 0x0104, 0x01},	// Hold on
	{MN34130_8BIT, 0x3319, 0x28},	// Output V[Line] setting
	{MN34130_8BIT, 0x308D, 0x00},	// H Size setting
	{MN34130_8BIT, 0x308E, 0x38},
	{MN34130_8BIT, 0x3091, 0x00},
	{MN34130_8BIT, 0x3092, 0x38},
	{MN34130_8BIT, 0x3095, 0x10},
	{MN34130_8BIT, 0x3096, 0x10},
	{MN34130_8BIT, 0x3089, 0x0D},
	{MN34130_8BIT, 0x308A, 0x8E},
	{MN34130_8BIT, 0x308F, 0x00},	// V Size setting
	{MN34130_8BIT, 0x3090, 0x28},
 	{MN34130_8BIT, 0x3093, 0x0C},
	{MN34130_8BIT, 0x3094, 0x37},
 	{MN34130_8BIT, 0x3097, 0x0C},
	{MN34130_8BIT, 0x3098, 0x10},
	{MN34130_8BIT, 0x308B, 0x13},	// Line Length PCK
 	{MN34130_8BIT, 0x308C, 0x68},
 	{MN34130_8BIT, 0x3627, 0x1C},
 	{MN34130_8BIT, 0x3628, 0x00},
	{MN34130_8BIT, 0x30BB, 0x01},	// Mode Change
	{MN34130_8BIT, 0x30BB, 0x21},	// MIPI-IF update
	{MN34130_8BIT, 0x3057, 0x04},
	{MN34130_8BIT, 0x0104, 0x00},	// Hold off
 	{MN34130_TOK_TERM, 0, 0}
};

static const struct mn34130_reg mn34130_4112x2320[] = {
	{MN34130_8BIT, 0x0104, 0x01},	// Hold on
	{MN34130_8BIT, 0x3319, 0x28},	// Output V[Line] setting
	{MN34130_8BIT, 0x308D, 0x00},	// H Size setting
	{MN34130_8BIT, 0x308E, 0x38},
	{MN34130_8BIT, 0x3091, 0x00},
	{MN34130_8BIT, 0x3092, 0x38},
	{MN34130_8BIT, 0x3095, 0x10},
	{MN34130_8BIT, 0x3096, 0x10},
	{MN34130_8BIT, 0x3089, 0x0D},
	{MN34130_8BIT, 0x308A, 0x8E},
	{MN34130_8BIT, 0x308F, 0x01},	// V Size setting
	{MN34130_8BIT, 0x3090, 0xA8},
 	{MN34130_8BIT, 0x3093, 0x0A},
	{MN34130_8BIT, 0x3094, 0xB7},
 	{MN34130_8BIT, 0x3097, 0x09},
	{MN34130_8BIT, 0x3098, 0x10},
	{MN34130_8BIT, 0x308B, 0x21},	// Line Length PCK
 	{MN34130_8BIT, 0x308C, 0x00},
 	{MN34130_8BIT, 0x3627, 0xA5},
 	{MN34130_8BIT, 0x3628, 0x01},
	{MN34130_8BIT, 0x30BB, 0x01},	// Mode Change
	{MN34130_8BIT, 0x30BB, 0x21},	// MIPI-IF update
	{MN34130_8BIT, 0x3057, 0x04},
	{MN34130_8BIT, 0x0104, 0x00},	// Hold off
 	{MN34130_TOK_TERM, 0, 0}
};

static const struct mn34130_reg mn34130_4112x3088[] = {
	{MN34130_8BIT, 0x0104, 0x01},	// Hold on
	{MN34130_8BIT, 0x3319, 0x28},	// Output V[Line] setting
	{MN34130_8BIT, 0x308D, 0x00},	// H Size setting
	{MN34130_8BIT, 0x308E, 0x38},
	{MN34130_8BIT, 0x3091, 0x00},
	{MN34130_8BIT, 0x3092, 0x38},
	{MN34130_8BIT, 0x3095, 0x10},
	{MN34130_8BIT, 0x3096, 0x10},
	{MN34130_8BIT, 0x3089, 0x0D},
	{MN34130_8BIT, 0x308A, 0x8E},
	{MN34130_8BIT, 0x308F, 0x00},	// V Size setting
	{MN34130_8BIT, 0x3090, 0x28},
 	{MN34130_8BIT, 0x3093, 0x0C},
	{MN34130_8BIT, 0x3094, 0x37},
 	{MN34130_8BIT, 0x3097, 0x0C},
	{MN34130_8BIT, 0x3098, 0x10},
	{MN34130_8BIT, 0x308B, 0x21},	// Line Length PCK
 	{MN34130_8BIT, 0x308C, 0x00},
 	{MN34130_8BIT, 0x3627, 0xA5},
 	{MN34130_8BIT, 0x3628, 0x01},
	{MN34130_8BIT, 0x30BB, 0x01},	// Mode Change
	{MN34130_8BIT, 0x30BB, 0x21},	// MIPI-IF update
	{MN34130_8BIT, 0x3057, 0x04},
 	{MN34130_8BIT, 0x0104, 0x00},
 	{MN34130_TOK_TERM, 0, 0}
};

static struct mn34130_resolution mn34130_res_preview[] = {
	{
		.desc = "mn34130_608x496",
		.width = 608,
		.height = 496,
		.used = 0,
		.regs = mn34130_608x496,
		.bin_factor_x = 6,
		.bin_factor_y = 6,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 30,
				.pixels_per_line = 867,
				.lines_per_frame = 640,
			},
			{
			}
		}
	},
	{
		.desc = "mn34130_2064x1168",
		.width = 2064,
		.height = 1168,
		.used = 0,
		.regs = mn34130_2064x1168,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 30,
				.pixels_per_line = 2484,
				.lines_per_frame = 1735,
			},
			{
			}
		}
	},
	{
		.desc = "mn34130_2064x1552",
		.width = 2064,
		.height = 1552,
		.used = 0,
		.regs = mn34130_2064x1552,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 26,
				.pixels_per_line = 2484,
				.lines_per_frame = 1940,
			},
			{
			}
		}
	},
	{
		.desc = "mn34130_4112x2320",
		.width = 4112,
		.height = 2320,
 		.used = 0,
		.regs = mn34130_4112x2320,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 8,
				.pixels_per_line = 8448,
				.lines_per_frame = 3470,
			},
			{
			}
		},
	},
	{
		.desc = "mn34130_4112x3088",
		.width = 4112,
		.height = 3088,
 		.used = 0,
		.regs = mn34130_4112x3088,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 8,
				.pixels_per_line = 8448,
				.lines_per_frame = 3470,
			},
			{
			}
		},
	},
	//{
		//.desc = "mn34130_zsl_4112x2320",
		//.width = 4112,
		//.height = 2320,
		//.used = 0,
		//.regs = mn34130_zsl_4112x2320,
		//.bin_factor_x = 1,
		//.bin_factor_y = 1,
		//.skip_frames = 1,
		//.fps_options = {
			//{
				//.fps = 15,
				//.pixels_per_line = 4968,
				//.lines_per_frame = 3470,
			//},
			//{
			//}
		//},
	//},
	//{
		//.desc = "mn34130_zsl_4112x3088",
		//.width = 4112,
		//.height = 3088,
		//.used = 0,
		//.regs = mn34130_zsl_4112x3088,
		//.bin_factor_x = 1,
		//.bin_factor_y = 1,
		//.skip_frames = 1,
		//.fps_options = {
			//{
				//.fps = 15,
				//.pixels_per_line = 4968,
				//.lines_per_frame = 3470,
			//},
			//{
			//}
		//},
	//},
};

static struct mn34130_resolution mn34130_res_still[] = {
	{
		.desc = "mn34130_2064x1168",
		.width = 2064,
		.height = 1168,
		.used = 0,
		.regs = mn34130_2064x1168,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 30,
				.pixels_per_line = 2484,
				.lines_per_frame = 1735,
			},
			{
			}
		}
	},
	{
		.desc = "mn34130_2064x1552",
		.width = 2064,
		.height = 1552,
		.used = 0,
		.regs = mn34130_2064x1552,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 26,
				.pixels_per_line = 2484,
				.lines_per_frame = 1940,
			},
			{
			}
		}
	},
	{
		.desc = "mn34130_4112x2320",
		.width = 4112,
		.height = 2320,
 		.used = 0,
		.regs = mn34130_4112x2320,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 8,
				.pixels_per_line = 8448,
				.lines_per_frame = 3470,
			},
			{
			}
		},
	},
	{
		.desc = "mn34130_4112x3088",
		.width = 4112,
		.height = 3088,
 		.used = 0,
		.regs = mn34130_4112x3088,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.skip_frames = 1,
		.fps_options = {
			{
				.fps = 8,
				.pixels_per_line = 8448,
				.lines_per_frame = 3470,
			},
			{
			}
		},
	},
};

static struct mn34130_resolution mn34130_res_video[] = {
	{
		.desc = "mn34130_608x496",
		.width = 608,
		.height = 496,
		.used = 0,
		.regs = mn34130_608x496,
		.bin_factor_x = 6,
		.bin_factor_y = 6,
		.skip_frames = 3,
		.fps_options = {
			{
				.fps = 60,
				.pixels_per_line = 867,
				.lines_per_frame = 640,
			},
			{
			}
		}
	},
	{
		.desc = "mn34130_656x496",
		.width = 656,
		.height = 496,
		.used = 0,
		.regs = mn34130_656x496,
		.bin_factor_x = 6,
		.bin_factor_y = 6,
		.skip_frames = 3,
		.fps_options = {
			{
				.fps = 69,
				.pixels_per_line = 842,
				.lines_per_frame = 697,
			},
			{
			}
		}
	},
	{
		.desc = "mn34130_1296x736",
		.width = 1296,
		.height = 736,
		.used = 0,
		.regs = mn34130_1296x736,
		.bin_factor_x = 3,
		.bin_factor_y = 3,
		.skip_frames = 3,
		.fps_options = {
			{
				.fps = 52,
				.pixels_per_line = 1656,
				.lines_per_frame = 1000,
			},
			{
			}
		}
	},
	{
		.desc = "mn34130_1936x1096",
		.width = 1936,
		.height = 1096,
		.used = 0,
		.regs = mn34130_1936x1096,
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.skip_frames = 5,
		.fps_options = {
			{
				.fps = 27,
				.pixels_per_line = 2700,
				.lines_per_frame = 1735,
			},
			{
			}
		}
	},
};

#endif
