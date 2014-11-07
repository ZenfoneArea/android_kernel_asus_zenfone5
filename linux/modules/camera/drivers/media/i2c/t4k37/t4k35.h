#ifndef __T4K37_H__
#define __T4K37_H__
#include "common.h"

/************************** settings for tsb *************************/
static struct tsb_reg const tsb_STILL_8M_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{TSB_8BIT, 0x0113, 0x0A},	// CSI_DATA_FORMAT[7:0];
	{TSB_8BIT, 0x0301, 0x01},	// -/-/-/-/VT_PIX_CLK_DIV[3:0];
	{TSB_8BIT, 0x0303, 0x06},	// -/-/-/-/VT_SYS_CLK_DIV[3:0];
	{TSB_8BIT, 0x0307, 0xDC},	// PLL_MULTIPLIER[7:0];
	{TSB_8BIT, 0x030B, 0x01},	// -/-/-/-/OP_SYS_CLK_DIV[3:0];
	{TSB_8BIT, 0x0340, 0x0C},	// FR_LENGTH_LINES[15:8];
	{TSB_8BIT, 0x0341, 0x48},	// FR_LENGTH_LINES[7:0];
	{TSB_8BIT, 0x0342, 0x11},	// LINE_LENGTH_PCK[15:8];
	{TSB_8BIT, 0x0343, 0xE8},	// LINE_LENGTH_PCK[7:0];
	{TSB_8BIT, 0x0346, 0x00},	// Y_ADDR_START[15:8];
	{TSB_8BIT, 0x0347, 0x00},	// Y_ADDR_START[7:0];
	{TSB_8BIT, 0x034A, 0x0C},	// Y_ADDR_END[15:8];
	{TSB_8BIT, 0x034B, 0x2F},	// Y_ADDR_END[7:0];
	{TSB_8BIT, 0x034C, 0x10},	// X_OUTPUT_SIZE[15:8];
	{TSB_8BIT, 0x034D, 0x70},	// X_OUTPUT_SIZE[7:0];
	{TSB_8BIT, 0x034E, 0x0C},	// Y_OUTPUT_SIZE[15:8];
	{TSB_8BIT, 0x034F, 0x30},	// Y_OUTPUT_SIZE[7:0];
	{TSB_8BIT, 0x0401, 0x00},	// -/-/-/-/-/-/SCALING_MODE[1:0];
	{TSB_8BIT, 0x0404, 0x10},	// SCALE_M[7:0];
	{TSB_8BIT, 0x0409, 0x00},	// DCROP_XOFS[7:0];
	{TSB_8BIT, 0x040C, 0x10},	// DCROP_WIDTH[15:8];
	{TSB_8BIT, 0x040D, 0x70},	// DCROP_WIDTH[7:0];
	{TSB_8BIT, 0x040E, 0x0C},	// DCROP_HIGT[15:8];
	{TSB_8BIT, 0x040F, 0x30},	// DCROP_HIGT[7:0];
	{TSB_8BIT, 0x0820, 0x10},	// MSB_LBRATE[31:24];
	{TSB_8BIT, 0x0821, 0x80},	// MSB_LBRATE[23:16];
	{TSB_8BIT, 0x0900, 0x00},	// -/-/-/-/-/-/H_BIN[1:0];
	{TSB_8BIT, 0x0901, 0x00},	// -/-/-/-/-/-/V_BIN_MODE[1:0];
	{TSB_8BIT, 0x32F7, 0x00},	// -/-/-/-/-/-/-/PP_DCROP_SW;
	{TSB_TOK_TERM, 0, 0}

};

static struct tsb_reg const tsb_STILL_8M_15fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{TSB_8BIT, 0x0113, 0x0A},	// CSI_DATA_FORMAT[7:0];
	{TSB_8BIT, 0x0301, 0x01},	// -/-/-/-/VT_PIX_CLK_DIV[3:0];
	{TSB_8BIT, 0x0303, 0x0C},	// -/-/-/-/VT_SYS_CLK_DIV[3:0];
	{TSB_8BIT, 0x0307, 0x6E},	// PLL_MULTIPLIER[7:0];
	{TSB_8BIT, 0x030B, 0x01},	// -/-/-/-/OP_SYS_CLK_DIV[3:0];
	{TSB_8BIT, 0x0340, 0x0C},	// FR_LENGTH_LINES[15:8];
	{TSB_8BIT, 0x0341, 0x48},	// FR_LENGTH_LINES[7:0];
	{TSB_8BIT, 0x0342, 0x11},	// LINE_LENGTH_PCK[15:8];
	{TSB_8BIT, 0x0343, 0xE8},	// LINE_LENGTH_PCK[7:0];
	{TSB_8BIT, 0x0346, 0x00},	// Y_ADDR_START[15:8];
	{TSB_8BIT, 0x0347, 0x00},	// Y_ADDR_START[7:0];
	{TSB_8BIT, 0x034A, 0x0C},	// Y_ADDR_END[15:8];
	{TSB_8BIT, 0x034B, 0x2F},	// Y_ADDR_END[7:0];
	{TSB_8BIT, 0x034C, 0x10},	// X_OUTPUT_SIZE[15:8];
	{TSB_8BIT, 0x034D, 0x70},	// X_OUTPUT_SIZE[7:0];
	{TSB_8BIT, 0x034E, 0x0C},	// Y_OUTPUT_SIZE[15:8];
	{TSB_8BIT, 0x034F, 0x30},	// Y_OUTPUT_SIZE[7:0];
	{TSB_8BIT, 0x0401, 0x00},	// -/-/-/-/-/-/SCALING_MODE[1:0];
	{TSB_8BIT, 0x0404, 0x10},	// SCALE_M[7:0];
	{TSB_8BIT, 0x0409, 0x00},	// DCROP_XOFS[7:0];
	{TSB_8BIT, 0x040C, 0x10},	// DCROP_WIDTH[15:8];
	{TSB_8BIT, 0x040D, 0x70},	// DCROP_WIDTH[7:0];
	{TSB_8BIT, 0x040E, 0x0C},	// DCROP_HIGT[15:8];
	{TSB_8BIT, 0x040F, 0x30},	// DCROP_HIGT[7:0];
	{TSB_8BIT, 0x0820, 0x08},	// MSB_LBRATE[31:24];
	{TSB_8BIT, 0x0821, 0x40},	// MSB_LBRATE[23:16];
	{TSB_8BIT, 0x0900, 0x00},	// -/-/-/-/-/-/H_BIN[1:0];
	{TSB_8BIT, 0x0901, 0x00},	// -/-/-/-/-/-/V_BIN_MODE[1:0];
	{TSB_8BIT, 0x32F7, 0x00},	// -/-/-/-/-/-/-/PP_DCROP_SW;
	{TSB_TOK_TERM, 0, 0}
};

static struct tsb_reg const tsb_PREVIEW_30fps[] = {	// 3.25M 30fps
	GROUPED_PARAMETER_HOLD_ENABLE,
	{TSB_8BIT, 0x0113, 0x0A},	// CSI_DATA_FORMAT[7:0];
	{TSB_8BIT, 0x0301, 0x02},	// -/-/-/-/VT_PIX_CLK_DIV[3:0];
	{TSB_8BIT, 0x0303, 0x08},	// -/-/-/-/VT_SYS_CLK_DIV[3:0];
	{TSB_8BIT, 0x0307, 0x6E},	// PLL_MULTIPLIER[7:0];
	{TSB_8BIT, 0x030B, 0x02},	// -/-/-/-/OP_SYS_CLK_DIV[3:0];
	{TSB_8BIT, 0x0340, 0x06},	// FR_LENGTH_LINES[15:8];
	{TSB_8BIT, 0x0341, 0x30},	// FR_LENGTH_LINES[7:0];
	{TSB_8BIT, 0x0342, 0x0D},	// LINE_LENGTH_PCK[15:8];
	{TSB_8BIT, 0x0343, 0x58},	// LINE_LENGTH_PCK[7:0];
	{TSB_8BIT, 0x0346, 0x00},	// Y_ADDR_START[15:8];
	{TSB_8BIT, 0x0347, 0x00},	// Y_ADDR_START[7:0];
	{TSB_8BIT, 0x034A, 0x0C},	// Y_ADDR_END[15:8];
	{TSB_8BIT, 0x034B, 0x2F},	// Y_ADDR_END[7:0];
	{TSB_8BIT, 0x034C, 0x08},	// X_OUTPUT_SIZE[15:8];
	{TSB_8BIT, 0x034D, 0x38},	// X_OUTPUT_SIZE[7:0];
	{TSB_8BIT, 0x034E, 0x06},	// Y_OUTPUT_SIZE[15:8];
	{TSB_8BIT, 0x034F, 0x18},	// Y_OUTPUT_SIZE[7:0];
	{TSB_8BIT, 0x0401, 0x00},	// -/-/-/-/-/-/SCALING_MODE[1:0];
	{TSB_8BIT, 0x0404, 0x10},	// SCALE_M[7:0];
	{TSB_8BIT, 0x0409, 0x00},	// DCROP_XOFS[7:0];
	{TSB_8BIT, 0x040C, 0x10},	// DCROP_WIDTH[15:8];
	{TSB_8BIT, 0x040D, 0x70},	// DCROP_WIDTH[7:0];
	{TSB_8BIT, 0x040E, 0x0C},	// DCROP_HIGT[15:8];
	{TSB_8BIT, 0x040F, 0x30},	// DCROP_HIGT[7:0];
	{TSB_8BIT, 0x0820, 0x04},	// MSB_LBRATE[31:24];
	{TSB_8BIT, 0x0821, 0x20},	// MSB_LBRATE[23:16];
	{TSB_8BIT, 0x0900, 0x01},	// -/-/-/-/-/-/H_BIN[1:0];
	{TSB_8BIT, 0x0901, 0x01},	// -/-/-/-/-/-/V_BIN_MODE[1:0];
	{TSB_8BIT, 0x32F7, 0x00},	// -/-/-/-/-/-/-/PP_DCROP_SW;
	{TSB_TOK_TERM, 0, 0}
};

#if 0	// TSB
/*****************************video************************/
static struct imx_reg const imx_1080p_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x06},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x4C},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x09},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x01},
	{IMX_8BIT, 0x030D, 0x12},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x06},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0xA4},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x11},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0xC6},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x01},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0xDB},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x02},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x42},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0A},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xEA},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x07},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x61},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x09},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x10},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x05},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x20},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x00}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x00}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x10}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x57},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x6F},
	{IMX_8BIT, 0x3371, 0x27},
	{IMX_8BIT, 0x3372, 0x4F},
	{IMX_8BIT, 0x3373, 0x2F},
	{IMX_8BIT, 0x3374, 0x27},
	{IMX_8BIT, 0x3375, 0x2F},
	{IMX_8BIT, 0x3376, 0x97},
	{IMX_8BIT, 0x3377, 0x37},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x0C},
	{IMX_8BIT, 0x33D5, 0xD0},
	{IMX_8BIT, 0x33D6, 0x07},
	{IMX_8BIT, 0x33D7, 0x38},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_1080p_no_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x08},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0xD5},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x09},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x01},
	{IMX_8BIT, 0x030D, 0x12},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x07},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0xD0},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0F},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x3C},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x01},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x34},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x08},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x6B},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x07},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x94},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x04},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x44},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x00}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x02}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x1B}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x57},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x6F},
	{IMX_8BIT, 0x3371, 0x27},
	{IMX_8BIT, 0x3372, 0x4F},
	{IMX_8BIT, 0x3373, 0x2F},
	{IMX_8BIT, 0x3374, 0x27},
	{IMX_8BIT, 0x3375, 0x2F},
	{IMX_8BIT, 0x3376, 0x97},
	{IMX_8BIT, 0x3377, 0x37},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x0C},
	{IMX_8BIT, 0x33D5, 0xD0},
	{IMX_8BIT, 0x33D6, 0x07},
	{IMX_8BIT, 0x33D7, 0x38},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_1080p_no_dvs_15fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x08},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0xD5},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x09},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x01},
	{IMX_8BIT, 0x030D, 0x12},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x09},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0xA6},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x18},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x9C},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x01},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x34},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x08},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x6B},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x07},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x94},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x04},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x44},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x00}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x02}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x1B}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x57},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x6F},
	{IMX_8BIT, 0x3371, 0x27},
	{IMX_8BIT, 0x3372, 0x4F},
	{IMX_8BIT, 0x3373, 0x2F},
	{IMX_8BIT, 0x3374, 0x27},
	{IMX_8BIT, 0x3375, 0x2F},
	{IMX_8BIT, 0x3376, 0x97},
	{IMX_8BIT, 0x3377, 0x37},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x0C},
	{IMX_8BIT, 0x33D5, 0xD0},
	{IMX_8BIT, 0x33D6, 0x07},
	{IMX_8BIT, 0x33D7, 0x38},
	{IMX_TOK_TERM, 0, 0}
};
/*****************************video************************/
static struct imx_reg const imx_720p_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0xFC},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x09},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x01},
	{IMX_8BIT, 0x030D, 0x12},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x06},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x00},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x13},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x9C},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x01},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0xD7},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x02},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x3E},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0A},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xEE},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x07},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x65},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x06},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x10},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x03},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x70},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x00}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x02}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x18}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x57},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x6F},
	{IMX_8BIT, 0x3371, 0x27},
	{IMX_8BIT, 0x3372, 0x4F},
	{IMX_8BIT, 0x3373, 0x2F},
	{IMX_8BIT, 0x3374, 0x27},
	{IMX_8BIT, 0x3375, 0x2F},
	{IMX_8BIT, 0x3376, 0x97},
	{IMX_8BIT, 0x3377, 0x37},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x0C},
	{IMX_8BIT, 0x33D5, 0xD0},
	{IMX_8BIT, 0x33D6, 0x07},
	{IMX_8BIT, 0x33D7, 0x38},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_480p_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0xFC},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x09},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x01},
	{IMX_8BIT, 0x030D, 0x12},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x06},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x00},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x13},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x9C},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x01},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0xD4},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x01},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0xC8},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0A},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xF1},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x07},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0xDB},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x03},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x70},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x02},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x50},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x01}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x02}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x15}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x57},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x6F},
	{IMX_8BIT, 0x3371, 0x27},
	{IMX_8BIT, 0x3372, 0x4F},
	{IMX_8BIT, 0x3373, 0x2F},
	{IMX_8BIT, 0x3374, 0x27},
	{IMX_8BIT, 0x3375, 0x2F},
	{IMX_8BIT, 0x3376, 0x97},
	{IMX_8BIT, 0x3377, 0x37},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x0C},
	{IMX_8BIT, 0x33D5, 0xD0},
	{IMX_8BIT, 0x33D6, 0x07},
	{IMX_8BIT, 0x33D7, 0x38},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_STILL_720p_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x04},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x48},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x14},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x28},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x48},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x01},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x64},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0x87},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x08},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x3B},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x06},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x20},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x03},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x6C},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x01}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x00}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x10}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x06},
	{IMX_8BIT, 0x33D5, 0x20},
	{IMX_8BIT, 0x33D6, 0x03},
	{IMX_8BIT, 0x33D7, 0x6C},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_STILL_720p_15fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x04},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x08},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0xCA},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x18},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x38},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x48},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x01},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x64},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0x87},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x08},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x3B},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x06},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x20},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x03},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x6C},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x01}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x00}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x10}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x06},
	{IMX_8BIT, 0x33D5, 0x20},
	{IMX_8BIT, 0x33D6, 0x03},
	{IMX_8BIT, 0x33D7, 0x6C},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_WVGA_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0xEC},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x09},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x01},
	{IMX_8BIT, 0x030D, 0x12},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x06},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x00},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x13},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x9C},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x00},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0xD0},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x08},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0xCF},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x06},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x68},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x04},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x00},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x01}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x00}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x10}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x57},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x6F},
	{IMX_8BIT, 0x3371, 0x27},
	{IMX_8BIT, 0x3372, 0x4F},
	{IMX_8BIT, 0x3373, 0x2F},
	{IMX_8BIT, 0x3374, 0x27},
	{IMX_8BIT, 0x3375, 0x2F},
	{IMX_8BIT, 0x3376, 0x97},
	{IMX_8BIT, 0x3377, 0x37},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x0C},
	{IMX_8BIT, 0x33D5, 0xD0},
	{IMX_8BIT, 0x33D6, 0x07},
	{IMX_8BIT, 0x33D7, 0x38},
	{IMX_TOK_TERM, 0, 0}
};
static struct imx_reg const imx_CIF_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0xFC},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x04},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x06},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x00},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x11},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0xDB},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x00},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x00},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x09},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x9F},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x01},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x70},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x01},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x30},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x02}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x00}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x10}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x06},
	{IMX_8BIT, 0x33D5, 0x20},
	{IMX_8BIT, 0x33D6, 0x03},
	{IMX_8BIT, 0x33D7, 0x6C},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_VGA_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0xFC},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x04},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x06},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x00},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x11},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x94},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x00},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x00},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x09},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x9F},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x03},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x34},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x02},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x68},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x02}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x00}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x10}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x06},
	{IMX_8BIT, 0x33D5, 0x20},
	{IMX_8BIT, 0x33D6, 0x03},
	{IMX_8BIT, 0x33D7, 0x6C},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_VGA_strong_dvs_15fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0xFC},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x04},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x07},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x9E},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x1C},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0xB6},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x00},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x00},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x00},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x00},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x0C},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0xCF},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x09},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x9F},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x03},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x34},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x02},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x68},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x02}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x00}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x10}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x06},
	{IMX_8BIT, 0x33D5, 0x20},
	{IMX_8BIT, 0x33D6, 0x03},
	{IMX_8BIT, 0x33D7, 0x6C},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_QVGA_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x48},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x03},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0x38},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x02},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x68},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x09},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0x97},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x07},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x37},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x01},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0x98},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x01},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0x34},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x02}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x00}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x10}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x01},
	{IMX_8BIT, 0x33D5, 0x98},
	{IMX_8BIT, 0x33D6, 0x01},
	{IMX_8BIT, 0x33D7, 0x34},
	{IMX_TOK_TERM, 0, 0}
};

static struct imx_reg const imx_QCIF_strong_dvs_30fps[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{IMX_8BIT, 0x0100, 0x00},  /*	mode_select	*/
	/* shutter */
	{IMX_8BIT, 0x0202, 0x05},  /* coarse _integration_time[15:8] */
	{IMX_8BIT, 0x0203, 0x44},  /* coarse _integration_time[7:0] */
	/* pll */
	{IMX_8BIT, 0x0301, 0x05},  /*	vt_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x0303, 0x01},  /*	vt_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x0305, 0x06},  /*	pre_pll_clk_div[7:0]	*/
	{IMX_8BIT, 0x0309, 0x05},  /*	op_pix_clk_div[7:0]	*/
	{IMX_8BIT, 0x030B, 0x01},  /*	op_sys_clk_div[7:0]	*/
	{IMX_8BIT, 0x030C, 0x00},
	{IMX_8BIT, 0x030D, 0x6D},
	/* image sizing */
	{IMX_8BIT, 0x0340, 0x05},  /* frame_length_lines[15:8] */
	{IMX_8BIT, 0x0341, 0x48},  /*	frame_length_lines[7:0]	*/
	{IMX_8BIT, 0x0342, 0x0D},  /*	line_length_pck[15:8]	*/
	{IMX_8BIT, 0x0343, 0x70},  /*	line_length_pck[7:0]	*/
	{IMX_8BIT, 0x0344, 0x04},  /*	x_addr_start[15:8]	*/
	{IMX_8BIT, 0x0345, 0xB8},  /*	x_addr_start[7:0]	*/
	{IMX_8BIT, 0x0346, 0x03},  /*	y_addr_start[15:8]	*/
	{IMX_8BIT, 0x0347, 0x70},  /*	y_addr_start[7:0]	*/
	{IMX_8BIT, 0x0348, 0x08},  /*	x_addr_end[15:8]	*/
	{IMX_8BIT, 0x0349, 0x17},  /*	x_addr_end[7:0]	*/
	{IMX_8BIT, 0x034A, 0x06},  /*	y_addr_end[15:8]	*/
	{IMX_8BIT, 0x034B, 0x2F},  /*	y_addr_end[7:0]	*/
	{IMX_8BIT, 0x034C, 0x00},  /*	x_output_size[15:8]	*/
	{IMX_8BIT, 0x034D, 0xD8},  /*	x_output_size[7:0]	*/
	{IMX_8BIT, 0x034E, 0x00},  /*	y_output_size[15:8]	*/
	{IMX_8BIT, 0x034F, 0xB0},  /*	y_output_size[7:0]	*/
	/* binning & scaling */
	{IMX_8BIT, 0x0390, 0x02}, /* binning mode */
	{IMX_8BIT, 0x0401, 0x00}, /* scaling mode*/
	{IMX_8BIT, 0x0405, 0x10}, /* scale_m[7:0] */
	/* timer */
	{IMX_8BIT, 0x3344, 0x37},
	{IMX_8BIT, 0x3345, 0x1F},
	/* timing */
	{IMX_8BIT, 0x3370, 0x5F},
	{IMX_8BIT, 0x3371, 0x17},
	{IMX_8BIT, 0x3372, 0x37},
	{IMX_8BIT, 0x3373, 0x17},
	{IMX_8BIT, 0x3374, 0x17},
	{IMX_8BIT, 0x3375, 0x0F},
	{IMX_8BIT, 0x3376, 0x57},
	{IMX_8BIT, 0x3377, 0x27},
	{IMX_8BIT, 0x33C8, 0x01},
	{IMX_8BIT, 0x33D4, 0x00},
	{IMX_8BIT, 0x33D5, 0xD8},
	{IMX_8BIT, 0x33D6, 0x00},
	{IMX_8BIT, 0x33D7, 0xB0},
	{IMX_TOK_TERM, 0, 0}
};
#endif	// TSB

static struct tsb_reg const tsb_init_settings[] = {
	GROUPED_PARAMETER_HOLD_ENABLE,
	{TSB_8BIT, 0x0101, 0x00},	// /-/-/-/-/-/IMAGE_ORIENT[1:0];
	{TSB_8BIT, 0x0103, 0x00},	// /-/-/-/-/-/MIPI_RST/SOFTWARE_RESET;
	{TSB_8BIT, 0x0104, 0x00},	// /-/-/-/-/-/-/GROUP_PARA_HOLD;
	{TSB_8BIT, 0x0105, 0x00},	// /-/-/-/-/-/-/MSK_CORRUPT_FR;
	{TSB_8BIT, 0x0110, 0x00},	// /-/-/-/-/CSI_CHAN_IDNTF[2:0];
	{TSB_8BIT, 0x0111, 0x02},	// /-/-/-/-/-/CSI_SIGNAL_MOD[1:0];
	{TSB_8BIT, 0x0112, 0x0A},	// SI_DATA_FORMAT[15:8];
	{TSB_8BIT, 0x0113, 0x0A},	// SI_DATA_FORMAT[7:0];
	{TSB_8BIT, 0x0114, 0x03},	// /-/-/-/-/-/CSI_LANE_MODE[1:0];
	{TSB_8BIT, 0x0115, 0x30},	// /-/CSI_10TO8_DT[5:0];
	{TSB_8BIT, 0x0117, 0x32},	// /-/CSI_10TO6_DT[5:0];
	{TSB_8BIT, 0x0130, 0x13},	// XTCLK_FRQ_MHZ[15:8];
	{TSB_8BIT, 0x0131, 0x33},	// XTCLK_FRQ_MHZ[7:0];
	{TSB_8BIT, 0x0141, 0x00},	// /-/-/-/-/CTX_SW_CTL[2:0];
	{TSB_8BIT, 0x0142, 0x00},	// /-/-/-/CONT_MDSEL_FRVAL[1:0]/CONT_FRCNT_MSK/CONT_GRHOLD_MSK;
	{TSB_8BIT, 0x0143, 0x00},	// _FRAME_COUNT[7:0];
	{TSB_8BIT, 0x0202, 0x0C},	// OAR_INTEGR_TIM[15:8];
	{TSB_8BIT, 0x0203, 0x42},	// OAR_INTEGR_TIM[7:0];
	{TSB_8BIT, 0x0204, 0x00},	// /-/-/-/ANA_GA_CODE_GL[11:8];
	{TSB_8BIT, 0x0205, 0x37},	// NA_GA_CODE_GL[7:0];
	{TSB_8BIT, 0x0210, 0x01},	// /-/-/-/-/-/DG_GA_GREENR[9:8];
	{TSB_8BIT, 0x0211, 0x00},	// G_GA_GREENR[7:0];
	{TSB_8BIT, 0x0212, 0x01},	// /-/-/-/-/-/DG_GA_RED[9:8];
	{TSB_8BIT, 0x0213, 0x00},	// G_GA_RED[7:0];
	{TSB_8BIT, 0x0214, 0x01},	// /-/-/-/-/-/DG_GA_BLUE[9:8];
	{TSB_8BIT, 0x0215, 0x00},	// G_GA_BLUE[7:0];
	{TSB_8BIT, 0x0216, 0x01},	// /-/-/-/-/-/DG_GA_GREENB[9:8];
	{TSB_8BIT, 0x0217, 0x00},	// G_GA_GREENB[7:0];
	{TSB_8BIT, 0x0230, 0x00},	// /-/-/HDR_MODE[4:0];
	{TSB_8BIT, 0x0232, 0x04},	// DR_RATIO_1[7:0];
	{TSB_8BIT, 0x0234, 0x00},	// DR_SHT_INTEGR_TIM[15:8];
	{TSB_8BIT, 0x0235, 0x19},	// DR_SHT_INTEGR_TIM[7:0];
	{TSB_8BIT, 0x0301, 0x01},	// /-/-/-/VT_PIX_CLK_DIV[3:0];
	{TSB_8BIT, 0x0303, 0x0C},	// /-/-/-/VT_SYS_CLK_DIV[3:0];
	{TSB_8BIT, 0x0305, 0x03},	// /-/-/-/-/PRE_PLL_CLK_DIV[2:0];
	{TSB_8BIT, 0x0306, 0x00},	// /-/-/-/-/-/-/PLL_MULTIPLIER[8];
	{TSB_8BIT, 0x0307, 0x6E},	// LL_MULTIPLIER[7:0];
	{TSB_8BIT, 0x030B, 0x01},	// /-/-/-/OP_SYS_CLK_DIV[3:0];
	{TSB_8BIT, 0x030D, 0x03},	// /-/-/-/-/PRE_PLL_ST_CLK_DIV[2:0];
	{TSB_8BIT, 0x030E, 0x00},	// /-/-/-/-/-/-/PLL_MULT_ST[8];
	{TSB_8BIT, 0x030F, 0x87},	// LL_MULT_ST[7:0];
	{TSB_8BIT, 0x0310, 0x00},	// /-/-/-/-/-/-/OPCK_PLLSEL;
	{TSB_8BIT, 0x0340, 0x0C},	// R_LENGTH_LINES[15:8];
	{TSB_8BIT, 0x0341, 0x48},	// R_LENGTH_LINES[7:0];
	{TSB_8BIT, 0x0342, 0x11},	// INE_LENGTH_PCK[15:8];
	{TSB_8BIT, 0x0343, 0xE8},	// INE_LENGTH_PCK[7:0];
	{TSB_8BIT, 0x0344, 0x00},	// /-/-/-/H_CROP[3:0];
	{TSB_8BIT, 0x0346, 0x00},	// _ADDR_START[15:8];
	{TSB_8BIT, 0x0347, 0x00},	// _ADDR_START[7:0];
	{TSB_8BIT, 0x034A, 0x0C},	// _ADDR_END[15:8];
	{TSB_8BIT, 0x034B, 0x2F},	// _ADDR_END[7:0];
	{TSB_8BIT, 0x034C, 0x10},	// _OUTPUT_SIZE[15:8];
	{TSB_8BIT, 0x034D, 0x70},	// _OUTPUT_SIZE[7:0];
	{TSB_8BIT, 0x034E, 0x0C},	// _OUTPUT_SIZE[15:8];
	{TSB_8BIT, 0x034F, 0x30},	// _OUTPUT_SIZE[7:0];
	{TSB_8BIT, 0x0401, 0x00},	// /-/-/-/-/-/SCALING_MODE[1:0];
	{TSB_8BIT, 0x0403, 0x00},	// /-/-/-/-/-/SPATIAL_SAMPLING[1:0];
	{TSB_8BIT, 0x0404, 0x10},	// CALE_M[7:0];
	{TSB_8BIT, 0x0408, 0x00},	// CROP_XOFS[15:8];
	{TSB_8BIT, 0x0409, 0x00},	// CROP_XOFS[7:0];
	{TSB_8BIT, 0x040A, 0x00},	// CROP_YOFS[15:8];
	{TSB_8BIT, 0x040B, 0x00},	// CROP_YOFS[7:0];
	{TSB_8BIT, 0x040C, 0x10},	// CROP_WIDTH[15:8];
	{TSB_8BIT, 0x040D, 0x70},	// CROP_WIDTH[7:0];
	{TSB_8BIT, 0x040E, 0x0C},	// CROP_HIGT[15:8];
	{TSB_8BIT, 0x040F, 0x30},	// CROP_HIGT[7:0];
	{TSB_8BIT, 0x0601, 0x00},	// EST_PATT_MODE[7:0];
	{TSB_8BIT, 0x0602, 0x02},	// /-/-/-/-/-/TEST_DATA_RED[9:8];
	{TSB_8BIT, 0x0603, 0xC0},	// EST_DATA_RED[7:0];
	{TSB_8BIT, 0x0604, 0x02},	// /-/-/-/-/-/TEST_DATA_GREENR[9:8];
	{TSB_8BIT, 0x0605, 0xC0},	// EST_DATA_GREENR[7:0];
	{TSB_8BIT, 0x0606, 0x02},	// /-/-/-/-/-/TEST_DATA_BLUE[9:8];
	{TSB_8BIT, 0x0607, 0xC0},	// EST_DATA_BLUE[7:0];
	{TSB_8BIT, 0x0608, 0x02},	// /-/-/-/-/-/TEST_DATA_GREENB[9:8];
	{TSB_8BIT, 0x0609, 0xC0},	// EST_DATA_GREENB[7:0];
	{TSB_8BIT, 0x060A, 0x00},	// O_CURS_WIDTH[15:8];
	{TSB_8BIT, 0x060B, 0x00},	// O_CURS_WIDTH[7:0];
	{TSB_8BIT, 0x060C, 0x00},	// O_CURS_POSITION[15:8];
	{TSB_8BIT, 0x060D, 0x00},	// O_CURS_POSITION[7:0];
	{TSB_8BIT, 0x060E, 0x00},	// E_CURS_WIDTH[15:8];
	{TSB_8BIT, 0x060F, 0x00},	// E_CURS_WIDTH[7:0];
	{TSB_8BIT, 0x0610, 0x00},	// E_CURS_POSITION[15:8];
	{TSB_8BIT, 0x0611, 0x00},	// E_CURS_POSITION[7:0];
	{TSB_8BIT, 0x0800, 0x88},	// CLK_POST[7:3]/-/-/-;
	{TSB_8BIT, 0x0801, 0x38},	// HS_PREPARE[7:3]/-/-/-;
	{TSB_8BIT, 0x0802, 0x78},	// HS_ZERO[7:3]/-/-/-;
	{TSB_8BIT, 0x0803, 0x48},	// HS_TRAIL[7:3]/-/-/-;
	{TSB_8BIT, 0x0804, 0x48},	// CLK_TRAIL[7:3]/-/-/-;
	{TSB_8BIT, 0x0805, 0x40},	// CLK_PREPARE[7:3]/-/-/-;
	{TSB_8BIT, 0x0806, 0x00},	// CLK_ZERO[7:3]/-/-/-;
	{TSB_8BIT, 0x0807, 0x48},	// LPX[7:3]/-/-/-;
	{TSB_8BIT, 0x0808, 0x01},	// /-/-/-/-/-/DPHY_CTRL[1:0];
	{TSB_8BIT, 0x0820, 0x08},	// SB_LBRATE[31:24];
	{TSB_8BIT, 0x0821, 0x40},	// SB_LBRATE[23:16];
	{TSB_8BIT, 0x0822, 0x00},	// SB_LBRATE[15:8];
	{TSB_8BIT, 0x0823, 0x00},	// SB_LBRATE[7:0];
	{TSB_8BIT, 0x0900, 0x00},	// /-/-/-/-/-/H_BIN[1:0];
	{TSB_8BIT, 0x0901, 0x00},	// /-/-/-/-/-/V_BIN_MODE[1:0];
	{TSB_8BIT, 0x0902, 0x01},	// /-/-/-/-/-/BINNING_WEIGHTING[1:0];
	{TSB_8BIT, 0x0A05, 0x01},	// /-/-/-/-/-/-/MAP_DEF_EN;
	{TSB_8BIT, 0x0A06, 0x01},	// /-/-/-/-/-/-/SGL_DEF_EN;
	{TSB_8BIT, 0x0A07, 0x98},	// GL_DEF_W[7:0];
	{TSB_8BIT, 0x0A0A, 0x01},	// /-/-/-/-/-/-/COMB_CPLT_SGL_DEF_EN;
	{TSB_8BIT, 0x0A0B, 0x98},	// OMB_CPLT_SGL_DEF_W[7:0];
	{TSB_8BIT, 0x0C00, 0x00},	// /-/-/-/-/-/GLBL_RST_CTRL1[1:0];
	{TSB_8BIT, 0x0C02, 0x00},	// LBL_RST_CFG_1[7:0];
	{TSB_8BIT, 0x0C04, 0x00},	// RDY_CTRL[15:8];
	{TSB_8BIT, 0x0C05, 0x32},	// RDY_CTRL[7:0];
	{TSB_8BIT, 0x0C06, 0x00},	// RDOUT_CTRL[15:8];
	{TSB_8BIT, 0x0C07, 0x10},	// RDOUT_CTRL[7:0];
	{TSB_8BIT, 0x0C08, 0x00},	// SHT_STB_DLY_CTRL[15:8];
	{TSB_8BIT, 0x0C09, 0x49},	// SHT_STB_DLY_CTRL[7:0];
	{TSB_8BIT, 0x0C0A, 0x01},	// SHT_STB_WDTH_CTRL[15:8];
	{TSB_8BIT, 0x0C0B, 0x68},	// SHT_STB_WDTH_CTRL[7:0];
	{TSB_8BIT, 0x0C0C, 0x00},	// FLSH_STB_DLY_CTRL[15:8];
	{TSB_8BIT, 0x0C0D, 0x34},	// FLSH_STB_DLY_CTRL[7:0];
	{TSB_8BIT, 0x0C0E, 0x00},	// FLSH_STB_WDTH_CTRL[15:8];
	{TSB_8BIT, 0x0C0F, 0x40},	// FLSH_STB_WDTH_CTRL[7:0];
	{TSB_8BIT, 0x0C12, 0x01},	// LASH_ADJ[7:0];
	{TSB_8BIT, 0x0C14, 0x00},	// LASH_LINE[15:8];
	{TSB_8BIT, 0x0C15, 0x01},	// LASH_LINE[7:0];
	{TSB_8BIT, 0x0C16, 0x00},	// LASH_DELAY[15:8];
	{TSB_8BIT, 0x0C17, 0x20},	// LASH_DELAY[7:0];
	{TSB_8BIT, 0x0C18, 0x00},	// LASH_WIDTH[15:8];
	{TSB_8BIT, 0x0C19, 0x40},	// LASH_WIDTH[7:0];
	{TSB_8BIT, 0x0C1A, 0x00},	// /-/FLASH_MODE[5:0];
	{TSB_8BIT, 0x0C1B, 0x00},	// /-/-/-/-/-/-/FLASH_TRG;
	{TSB_8BIT, 0x0F00, 0x00},	// /-/-/-/-/ABF_LUT_CTL[2:0];
	{TSB_8BIT, 0x0F01, 0x01},	// /-/-/-/-/-/ABF_LUT_MODE[1:0];
	{TSB_8BIT, 0x0F02, 0x01},	// BF_ES_A[15:8];
	{TSB_8BIT, 0x0F03, 0x40},	// BF_ES_A[7:0];
	{TSB_8BIT, 0x0F04, 0x00},	// /-/-/-/ABF_AG_A[11:8];
	{TSB_8BIT, 0x0F05, 0x40},	// BF_AG_A[7:0];
	{TSB_8BIT, 0x0F06, 0x01},	// /-/-/-/-/-/-/ABF_DG_GR_A[8];
	{TSB_8BIT, 0x0F07, 0x00},	// BF_DG_GR_A[7:0];
	{TSB_8BIT, 0x0F08, 0x01},	// /-/-/-/-/-/-/ABF_DG_R_A[8];
	{TSB_8BIT, 0x0F09, 0x00},	// BF_DG_R_A[7:0];
	{TSB_8BIT, 0x0F0A, 0x01},	// /-/-/-/-/-/-/ABF_DG_B_A[8];
	{TSB_8BIT, 0x0F0B, 0x00},	// BF_DG_B_A[7:0];
	{TSB_8BIT, 0x0F0C, 0x01},	// /-/-/-/-/-/-/ABF_DG_GB_A[8];
	{TSB_8BIT, 0x0F0D, 0x00},	// BF_DG_GB_A[7:0];
	{TSB_8BIT, 0x0F0E, 0x00},	// /-/-/-/-/-/-/F_ENTRY_A;
	{TSB_8BIT, 0x0F0F, 0x01},	// BF_ES_B[15:8];
	{TSB_8BIT, 0x0F10, 0x50},	// BF_ES_B[7:0];
	{TSB_8BIT, 0x0F11, 0x00},	// /-/-/-/ABF_AG_B[11:8];
	{TSB_8BIT, 0x0F12, 0x50},	// BF_AG_B[7:0];
	{TSB_8BIT, 0x0F13, 0x01},	// /-/-/-/-/-/-/ABF_DG_GR_B[8];
	{TSB_8BIT, 0x0F14, 0x00},	// BF_DG_GR_B[7:0];
	{TSB_8BIT, 0x0F15, 0x01},	// /-/-/-/-/-/-/ABF_DG_R_B[8];
	{TSB_8BIT, 0x0F16, 0x00},	// BF_DG_R_B[7:0];
	{TSB_8BIT, 0x0F17, 0x01},	// /-/-/-/-/-/-/ABF_DG_B_B[8];
	{TSB_8BIT, 0x0F18, 0x00},	// BF_DG_B_B[7:0];
	{TSB_8BIT, 0x0F19, 0x01},	// /-/-/-/-/-/-/ABF_DG_GB_B[8];
	{TSB_8BIT, 0x0F1A, 0x00},	// BF_DG_GB_B[7:0];
	{TSB_8BIT, 0x0F1B, 0x00},	// /-/-/-/-/-/-/F_ENTRY_B;
	{TSB_8BIT, 0x0F1C, 0x01},	// BF_ES_C[15:8];
	{TSB_8BIT, 0x0F1D, 0x60},	// BF_ES_C[7:0];
	{TSB_8BIT, 0x0F1E, 0x00},	// /-/-/-/ABF_AG_C[11:8];
	{TSB_8BIT, 0x0F1F, 0x60},	// BF_AG_C[7:0];
	{TSB_8BIT, 0x0F20, 0x01},	// /-/-/-/-/-/-/ABF_DG_GR_C[8];
	{TSB_8BIT, 0x0F21, 0x00},	// BF_DG_GR_C[7:0];
	{TSB_8BIT, 0x0F22, 0x01},	// /-/-/-/-/-/-/ABF_DG_R_C[8];
	{TSB_8BIT, 0x0F23, 0x00},	// BF_DG_R_C[7:0];
	{TSB_8BIT, 0x0F24, 0x01},	// /-/-/-/-/-/-/ABF_DG_B_C[8];
	{TSB_8BIT, 0x0F25, 0x00},	// BF_DG_B_C[7:0];
	{TSB_8BIT, 0x0F26, 0x01},	// /-/-/-/-/-/-/ABF_DG_GB_C[8];
	{TSB_8BIT, 0x0F27, 0x00},	// BF_DG_GB_C[7:0];
	{TSB_8BIT, 0x0F28, 0x00},	// /-/-/-/-/-/-/F_ENTRY_C;
	{TSB_8BIT, 0x1101, 0x00},	// /-/-/-/-/-/IMAGE_ORIENT_1B[1:0];
	{TSB_8BIT, 0x1143, 0x00},	// _FRAME_COUNT_1B[7:0];
	{TSB_8BIT, 0x1202, 0x00},	// OAR_INTEGR_TIM_1B[15:8];
	{TSB_8BIT, 0x1203, 0x19},	// OAR_INTEGR_TIM_1B[7:0];
	{TSB_8BIT, 0x1204, 0x00},	// /-/-/-/ANA_GA_CODE_GL_1B[11:8];
	{TSB_8BIT, 0x1205, 0x40},	// NA_GA_CODE_GL_1B[7:0];
	{TSB_8BIT, 0x1210, 0x01},	// /-/-/-/-/-/DG_GA_GREENR_1B[9:8];
	{TSB_8BIT, 0x1211, 0x00},	// G_GA_GREENR_1B[7:0];
	{TSB_8BIT, 0x1212, 0x01},	// /-/-/-/-/-/DG_GA_RED_1B[9:8];
	{TSB_8BIT, 0x1213, 0x00},	// G_GA_RED_1B[7:0];
	{TSB_8BIT, 0x1214, 0x01},	// /-/-/-/-/-/DG_GA_BLUE_1B[9:8];
	{TSB_8BIT, 0x1215, 0x00},	// G_GA_BLUE_1B[7:0];
	{TSB_8BIT, 0x1216, 0x01},	// /-/-/-/-/-/DG_GA_GREENB_1B[9:8];
	{TSB_8BIT, 0x1217, 0x00},	// G_GA_GREENB_1B[7:0];
	{TSB_8BIT, 0x1230, 0x00},	// /-/-/HDR_MODE_1B[4:0];
	{TSB_8BIT, 0x1232, 0x04},	// DR_RATIO_1_1B[7:0];
	{TSB_8BIT, 0x1234, 0x00},	// DR_SHT_INTEGR_TIM_1B[15:8];
	{TSB_8BIT, 0x1235, 0x19},	// DR_SHT_INTEGR_TIM_1B[7:0];
	{TSB_8BIT, 0x1340, 0x0C},	// R_LENGTH_LINES_1B[15:8];
	{TSB_8BIT, 0x1341, 0x80},	// R_LENGTH_LINES_1B[7:0];
	{TSB_8BIT, 0x1342, 0x15},	// INE_LENGTH_PCK_1B[15:8];
	{TSB_8BIT, 0x1343, 0xE0},	// INE_LENGTH_PCK_1B[7:0];
	{TSB_8BIT, 0x1344, 0x00},	// /-/-/-/H_CROP_1B[3:0];
	{TSB_8BIT, 0x1346, 0x00},	// _ADDR_START_1B[15:8];
	{TSB_8BIT, 0x1347, 0x00},	// _ADDR_START_1B[7:0];
	{TSB_8BIT, 0x134A, 0x0C},	// _ADDR_END_1B[15:8];
	{TSB_8BIT, 0x134B, 0x2F},	// _ADDR_END_1B[7:0];
	{TSB_8BIT, 0x134C, 0x10},	// _OUTPUT_SIZE_1B[15:8];
	{TSB_8BIT, 0x134D, 0x70},	// _OUTPUT_SIZE_1B[7:0];
	{TSB_8BIT, 0x134E, 0x0C},	// _OUTPUT_SIZE_1B[15:8];
	{TSB_8BIT, 0x134F, 0x30},	// _OUTPUT_SIZE_1B[7:0];
	{TSB_8BIT, 0x1401, 0x00},	// /-/-/-/-/-/SCALING_MODE_1B[1:0];
	{TSB_8BIT, 0x1403, 0x00},	// /-/-/-/-/-/SPATIAL_SAMPLING_1B[1:0];
	{TSB_8BIT, 0x1404, 0x10},	// CALE_M_1B[7:0];
	{TSB_8BIT, 0x1408, 0x00},	// CROP_XOFS_1B[15:8];
	{TSB_8BIT, 0x1409, 0x00},	// CROP_XOFS_1B[7:0];
	{TSB_8BIT, 0x140A, 0x00},	// CROP_YOFS_1B[15:8];
	{TSB_8BIT, 0x140B, 0x00},	// CROP_YOFS_1B[7:0];
	{TSB_8BIT, 0x140C, 0x10},	// CROP_WIDTH_1B[15:8];
	{TSB_8BIT, 0x140D, 0x70},	// CROP_WIDTH_1B[7:0];
	{TSB_8BIT, 0x140E, 0x0C},	// CROP_HIGT_1B[15:8];
	{TSB_8BIT, 0x140F, 0x30},	// CROP_HIGT_1B[7:0];
	{TSB_8BIT, 0x1601, 0x00},	// EST_PATT_MODE_1B[7:0];
	{TSB_8BIT, 0x1602, 0x02},	// /-/-/-/-/-/TEST_DATA_RED_1B[9:8];
	{TSB_8BIT, 0x1603, 0xC0},	// EST_DATA_RED_1B[7:0];
	{TSB_8BIT, 0x1604, 0x02},	// /-/-/-/-/-/TEST_DATA_GREENR_1B[9:8];
	{TSB_8BIT, 0x1605, 0xC0},	// EST_DATA_GREENR_1B[7:0];
	{TSB_8BIT, 0x1606, 0x02},	// /-/-/-/-/-/TEST_DATA_BLUE_1B[9:8];
	{TSB_8BIT, 0x1607, 0xC0},	// EST_DATA_BLUE_1B[7:0];
	{TSB_8BIT, 0x1608, 0x02},	// /-/-/-/-/-/TEST_DATA_GREENB_1B[9:8];
	{TSB_8BIT, 0x1609, 0xC0},	// EST_DATA_GREENB_1B[7:0];
	{TSB_8BIT, 0x160A, 0x00},	// O_CURS_WIDTH_1B[15:8];
	{TSB_8BIT, 0x160B, 0x00},	// O_CURS_WIDTH_1B[7:0];
	{TSB_8BIT, 0x160C, 0x00},	// O_CURS_POSITION_1B[15:8];
	{TSB_8BIT, 0x160D, 0x00},	// O_CURS_POSITION_1B[7:0];
	{TSB_8BIT, 0x160E, 0x00},	// E_CURS_WIDTH_1B[15:8];
	{TSB_8BIT, 0x160F, 0x00},	// E_CURS_WIDTH_1B[7:0];
	{TSB_8BIT, 0x1610, 0x00},	// E_CURS_POSITION_1B[15:8];
	{TSB_8BIT, 0x1611, 0x00},	// E_CURS_POSITION_1B[7:0];
	{TSB_8BIT, 0x1900, 0x00},	// /-/-/-/-/-/H_BIN_1B[1:0];
	{TSB_8BIT, 0x1901, 0x00},	// /-/-/-/-/-/V_BIN_MODE_1B[1:0];
	{TSB_8BIT, 0x1902, 0x00},	// /-/-/-/-/-/BINNING_WEIGHTING_1B[1:0];
	{TSB_8BIT, 0x3002, 0x0E},	// eserved ;
	{TSB_8BIT, 0x301A, 0x66},	// eserved ;
	{TSB_8BIT, 0x301B, 0x66},	// eserved ;
	{TSB_8BIT, 0x3024, 0x00},	// eserved ;
	{TSB_8BIT, 0x3025, 0x7C},	// eserved ;
	{TSB_8BIT, 0x3053, 0xE0},	// eserved ;
	{TSB_8BIT, 0x305D, 0x10},	// eserved ;
	{TSB_8BIT, 0x305E, 0x06},	// eserved ;
	{TSB_8BIT, 0x306B, 0x08},	// eserved ;
	{TSB_8BIT, 0x3073, 0x26},	// eserved ;
	{TSB_8BIT, 0x3074, 0x1A},	// eserved ;
	{TSB_8BIT, 0x3075, 0x0F},	// eserved ;
	{TSB_8BIT, 0x3076, 0x03},	// eserved ;
	{TSB_8BIT, 0x307E, 0x02},	// eserved ;
	{TSB_8BIT, 0x308D, 0x03},	// eserved ;
	{TSB_8BIT, 0x308E, 0x20},	// eserved ;
	{TSB_8BIT, 0x3091, 0x04},	// eserved ;
	{TSB_8BIT, 0x3096, 0x75},	// eserved ;
	{TSB_8BIT, 0x3097, 0x7E},	// eserved ;
	{TSB_8BIT, 0x3098, 0x20},	// eserved ;
	{TSB_8BIT, 0x30A0, 0x82},	// eserved ;
	{TSB_8BIT, 0x30AB, 0x30},	// eserved ;
	{TSB_8BIT, 0x30B0, 0x3E},	// eserved ;
	{TSB_8BIT, 0x30B2, 0x1F},	// eserved ;
	{TSB_8BIT, 0x30B4, 0x3E},	// eserved ;
	{TSB_8BIT, 0x30B6, 0x1F},	// eserved ;
	{TSB_8BIT, 0x30CC, 0xC0},	// eserved ;
	{TSB_8BIT, 0x30CF, 0x75},	// eserved ;
	{TSB_8BIT, 0x30D2, 0xB3},	// eserved ;
	{TSB_8BIT, 0x30D5, 0x09},	// eserved ;
	{TSB_8BIT, 0x30E5, 0x80},	// eserved ;
	{TSB_8BIT, 0x3134, 0x01},	// eserved ;
	{TSB_8BIT, 0x314D, 0x80},	// eserved ;
	{TSB_8BIT, 0x3165, 0x67},	// eserved ;
	{TSB_8BIT, 0x3169, 0x77},	// eserved ;
	{TSB_8BIT, 0x316A, 0x77},	// eserved ;
	{TSB_8BIT, 0x3173, 0x30},	// eserved ;
	{TSB_8BIT, 0x31B1, 0x40},	// eserved ;
	{TSB_8BIT, 0x31C1, 0x27},	// eserved ;
	{TSB_8BIT, 0x31DB, 0x15},	// eserved ;
	{TSB_8BIT, 0x31DC, 0xE0},	// eserved ;
	{TSB_8BIT, 0x3204, 0x00},	// eserved ;
	{TSB_8BIT, 0x3231, 0x00},	// WB_RG[7:0];
	{TSB_8BIT, 0x3232, 0x00},	// WB_GRG[7:0];
	{TSB_8BIT, 0x3233, 0x00},	// WB_GBG[7:0];
	{TSB_8BIT, 0x3234, 0x00},	// WB_BG[7:0];
	{TSB_8BIT, 0x3282, 0xC0},	// BPC_EN/ABPC_CK_EN/-/-/-/-/-/-;
	{TSB_8BIT, 0x3284, 0x06},	// eserved ;
	{TSB_8BIT, 0x3285, 0x03},	// eserved ;
	{TSB_8BIT, 0x3286, 0x02},	// eserved ;
	{TSB_8BIT, 0x328A, 0x03},	// eserved ;
	{TSB_8BIT, 0x328B, 0x02},	// eserved ;
	{TSB_8BIT, 0x3290, 0x20},	// eserved ;
	{TSB_8BIT, 0x3294, 0x10},	// eserved ;
	{TSB_8BIT, 0x32A8, 0x84},	// eserved ;
	{TSB_8BIT, 0x32B3, 0x10},	// eserved ;
	{TSB_8BIT, 0x32B4, 0x1F},	// eserved ;
	{TSB_8BIT, 0x32B7, 0x3B},	// eserved ;
	{TSB_8BIT, 0x32BB, 0x0F},	// eserved ;
	{TSB_8BIT, 0x32BC, 0x0F},	// eserved ;
	{TSB_8BIT, 0x32BE, 0x04},	// eserved ;
	{TSB_8BIT, 0x32BF, 0x0F},	// eserved ;
	{TSB_8BIT, 0x32C0, 0x0F},	// eserved ;
	{TSB_8BIT, 0x32C6, 0x50},	// eserved ;
	{TSB_8BIT, 0x32C8, 0x0E},	// eserved ;
	{TSB_8BIT, 0x32C9, 0x0E},	// eserved ;
	{TSB_8BIT, 0x32CA, 0x0E},	// eserved ;
	{TSB_8BIT, 0x32CB, 0x0E},	// eserved ;
	{TSB_8BIT, 0x32CC, 0x0E},	// eserved ;
	{TSB_8BIT, 0x32CD, 0x0E},	// eserved ;
	{TSB_8BIT, 0x32CE, 0x08},	// eserved ;
	{TSB_8BIT, 0x32CF, 0x08},	// eserved ;
	{TSB_8BIT, 0x32D0, 0x08},	// eserved ;
	{TSB_8BIT, 0x32D1, 0x0F},	// eserved ;
	{TSB_8BIT, 0x32D2, 0x0F},	// eserved ;
	{TSB_8BIT, 0x32D3, 0x0F},	// eserved ;
	{TSB_8BIT, 0x32D4, 0x08},	// eserved ;
	{TSB_8BIT, 0x32D5, 0x08},	// eserved ;
	{TSB_8BIT, 0x32D6, 0x08},	// eserved ;
	{TSB_8BIT, 0x32DD, 0x02},	// eserved ;
	{TSB_8BIT, 0x32E0, 0x20},	// eserved ;
	{TSB_8BIT, 0x32E1, 0x20},	// eserved ;
	{TSB_8BIT, 0x32E2, 0x20},	// eserved ;
	{TSB_8BIT, 0x32F7, 0x00},	// /-/-/-/-/-/-/PP_DCROP_SW;
	{TSB_8BIT, 0x3301, 0x05},	// eserved ;
	{TSB_8BIT, 0x3307, 0x37},	// eserved ;
	{TSB_8BIT, 0x3308, 0x36},	// eserved ;
	{TSB_8BIT, 0x3309, 0x0D},	// eserved ;
	{TSB_8BIT, 0x3383, 0x08},	// eserved ;
	{TSB_8BIT, 0x3384, 0x10},	// eserved ;
	{TSB_8BIT, 0x338C, 0x05},	// eserved ;
	{TSB_8BIT, 0x3424, 0x00},	// /-/-/-/B_TRIG_Z5_X/B_TX_TRIGOPT/B_CLKULPS/B_ESCREQ;
	{TSB_8BIT, 0x3425, 0x78},	// _ESCDATA[7:0];
	{TSB_8BIT, 0x3427, 0xC0},	// _MIPI_CLKVBLK/B_MIPI_CLK_MODE/-/-/B_HS_SR_CNT[1:0]/B_LP_SR_CNT[1:0];
	{TSB_8BIT, 0x3430, 0xA7},	// _NUMWAKE[7:0];
	{TSB_8BIT, 0x3431, 0x60},	// _NUMINIT[7:0];
	{TSB_8BIT, 0x3432, 0x11},	// /-/-/B_CLK0_M/-/-/-/B_LNKBTWK_ON;
	GROUPED_PARAMETER_HOLD_DISABLE,
	{TSB_TOK_TERM, 0, 0}
};
/* TODO settings of preview/still/video will be updated with new use case */
struct tsb_resolution t4k37_res_preview[] = {
	{
		.desc = "PREVIEW_30fps",
		.regs = tsb_PREVIEW_30fps,
		.width = 2104,
		.height = 1560,
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.fps_options = {
			{
				 .fps = 30,
				 .pixels_per_line = 0x0D58,
				 .lines_per_frame = 0x0630,
			},
			{
			}
		},
	},
};

struct tsb_resolution t4k37_res_still[] = {
	{
		.desc = "STILL_8M_15fps",
		.regs = tsb_STILL_8M_15fps,
		.width = 4208,
		.height = 3120,
		.bin_factor_x = 0,
		.bin_factor_y = 0,
		.used = 0,
		.fps_options = {
			{
				 .fps = 15,
				 .pixels_per_line = 0x11E8,
				 .lines_per_frame = 0x0C48,
			},
			{
			}
		},
	},
};

#endif
