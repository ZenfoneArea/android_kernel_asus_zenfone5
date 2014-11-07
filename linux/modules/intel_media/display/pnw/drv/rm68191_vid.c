/*
 * Copyright (c)  2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicensen
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "displays/rm68191_vid.h"
#include "mdfld_dsi_dpi.h"
#include "mdfld_dsi_pkg_sender.h"
#include <linux/gpio.h>
#include "psb_drv.h"
#include <linux/lnw_gpio.h>
#include <linux/init.h>
#include <asm/intel_scu_pmic.h>

#include <linux/HWVersion.h>

extern int Read_LCD_ID(void);


#define RM68191_PANEL_NAME	"RM68191"

#define RM68191_DEBUG 1
#define ENABLE_CSC_GAMMA 1

#define LP_RETRY 5

/*
 * GPIO pin definition
 */
#define PMIC_GPIO_BACKLIGHT_EN	0x7E
#define PMIC_GPIO_VEMMC2CNT	0xDA 	//panel power control 2.8v
#define DISP_RST_N		57


#define PWM_SOC_ENABLE 1
#define PWMCTRL_REG 0xffae9000
#define PWMCTRL_SIZE 0x80
static void __iomem *pwmctrl_mmio;
#define PWM_ENABLE_GPIO 49
//#define PWM_BASE_UNIT 0x444 //5,000Hz
#define PWM_BASE_UNIT 0x1555 //25,000Hz


#if PWM_SOC_ENABLE
union sst_pwmctrl_reg {
	struct {
		u32 pwmtd:8;
		u32 pwmbu:22;
		u32 pwmswupdate:1;
		u32 pwmenable:1;
	} part;
	u32 full;
};

static int pwm_configure(int duty)
{
	union sst_pwmctrl_reg pwmctrl;

	/*Read the PWM register to make sure there is no pending
	*update.
	*/
	pwmctrl.full = readl(pwmctrl_mmio);

	/*check pwnswupdate bit */
	if (pwmctrl.part.pwmswupdate)
		return -EBUSY;
	pwmctrl.part.pwmswupdate = 0x1;
	pwmctrl.part.pwmbu = PWM_BASE_UNIT;
	pwmctrl.part.pwmtd = duty;
	writel(pwmctrl.full,  pwmctrl_mmio);

	return 0;
}

static void pwm_enable(){
	union sst_pwmctrl_reg pwmctrl;

	lnw_gpio_set_alt(PWM_ENABLE_GPIO, LNW_ALT_2);

	/*Enable the PWM by setting PWM enable bit to 1 */
	pwmctrl.full = readl(pwmctrl_mmio);
	pwmctrl.part.pwmenable = 1;
	writel(pwmctrl.full, pwmctrl_mmio);
}

static void pwm_disable(){
	union sst_pwmctrl_reg pwmctrl;
	/*setting PWM enable bit to 0 */
	pwmctrl.full = readl(pwmctrl_mmio);
	pwmctrl.part.pwmenable = 0;
	writel(pwmctrl.full,  pwmctrl_mmio);

	gpio_set_value(PWM_ENABLE_GPIO, 0);
	lnw_gpio_set_alt(PWM_ENABLE_GPIO, 0);
}
#endif

struct rm68191_vid_data{
	u8 project_id;
	unsigned int gpio_lcd_en;
	unsigned int gpio_bl_en;
	unsigned int gpio_stb1_en;
	unsigned int gpio_stb2_en;
	unsigned int gpio_lcd_rst;
};

struct mipi_dsi_cmd{
	int delay;
	int len;
	u8 *commands;
};


static struct rm68191_vid_data gpio_settings_data;
static struct mdfld_dsi_config *rm68191_dsi_config;

#define CMD_SIZE(x) (sizeof((x)) / sizeof((x)[0]))


/* ====Initial settings==== */
/* RM68191 settings */
static u8 cm_001[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x03};
static u8 cm_002[] = {0x90, 0x05, 0x14, 0x05, 0x00, 0x00, 0x00, 0x8C, 0x00, 0x00};
static u8 cm_003[] = {0x91, 0x05, 0x14, 0x00, 0x00, 0x00, 0x00, 0x8C, 0x00, 0x00};
static u8 cm_004[] = {0x92, 0x40, 0x09, 0x0A, 0x0B, 0x0C, 0x00, 0xFF, 0x00, 0x00, 0x05, 0x08};
static u8 cm_005[] = {0x94, 0x00, 0x08, 0x05, 0x03, 0xCE, 0x03, 0xD0, 0x0C};
static u8 cm_006[] = {0x95, 0x40, 0x0D, 0x00, 0x0E, 0x00, 0x0F, 0x00, 0x10, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x05, 0x00, 0x08};
static u8 cm_007[] = {0x99, 0x00, 0x00};
static u8 cm_008[] = {0x9A, 0x80, 0x0D, 0x03, 0xD2, 0x03, 0xD4, 0x00, 0x00, 0x00, 0x00, 0x50};
static u8 cm_009[] = {0x9B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static u8 cm_010[] = {0x9C, 0x00, 0x00};
static u8 cm_011[] = {0x9D, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00};
static u8 cm_012[] = {0x9E, 0x00, 0x00};
static u8 cm_013[] = {0xA0, 0x95, 0x14, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x0B, 0x1F};
static u8 cm_014[] = {0xA1, 0x1F, 0x1F, 0x09, 0x1F, 0x1F, 0x1F, 0x0F};
static u8 cm_015[] = {0xA2, 0x0D, 0x1F, 0x01, 0x1F, 0x03, 0x1F, 0x1F, 0x1F, 0x05, 0x1F};
//static u8 cm_016[] = {0xA3, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};
static u8 cm_017[] = {0xA4, 0x1F, 0x04, 0x1F, 0x1F, 0x1F, 0x02, 0x1F, 0x00, 0x1F, 0x0C};
static u8 cm_018[] = {0xA5, 0x1F, 0x1F, 0x1F, 0x0E, 0x1F, 0x1F, 0x1F, 0x08, 0x1F, 0x1F};
static u8 cm_019[] = {0xA6, 0x1F, 0x0A, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x14, 0x15};


static u8 sleep_out[] = {0x11};
static u8 sleep_in[] = {0x10};
static u8 display_on[] = {0x29};
static u8 display_off[] = {0x28};


/* Compare Data */
static u8 cm_001_data[] = {0x55, 0xAA, 0x52, 0x08, 0x03};
static u8 cm_002_data[] = {0x05, 0x14, 0x05, 0x00, 0x00, 0x00, 0x8C, 0x00, 0x00};
static u8 cm_003_data[] = {0x05, 0x14, 0x00, 0x00, 0x00, 0x00, 0x8C, 0x00, 0x00};
static u8 cm_004_data[] = {0x40, 0x09, 0x0A, 0x0B, 0x0C, 0x00, 0xFF, 0x00, 0x00, 0x05, 0x08};
static u8 cm_005_data[] = {0x00, 0x08, 0x05, 0x03, 0xCE, 0x03, 0xD0, 0x0C};
static u8 cm_006_data[] = {0x40, 0x0D, 0x00, 0x0E, 0x00, 0x0F, 0x00, 0x10, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x05, 0x00, 0x08};
static u8 cm_007_data[] = {0x00, 0x00};
static u8 cm_008_data[] = {0x80, 0x0D, 0x03, 0xD2, 0x03, 0xD4, 0x00, 0x00, 0x00, 0x00, 0x50};
static u8 cm_009_data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static u8 cm_010_data[] = {0x00, 0x00};
static u8 cm_011_data[] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00};
static u8 cm_012_data[] = {0x00, 0x00};
static u8 cm_013_data[] = {0x95, 0x14, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x0B, 0x1F};
static u8 cm_014_data[] = {0x1F, 0x1F, 0x09, 0x1F, 0x1F, 0x1F, 0x0F};
static u8 cm_015_data[] = {0x0D, 0x1F, 0x01, 0x1F, 0x03, 0x1F, 0x1F, 0x1F, 0x05, 0x1F};
//static u8 cm_016_data[] = {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};
static u8 cm_017_data[] = {0x1F, 0x04, 0x1F, 0x1F, 0x1F, 0x02, 0x1F, 0x00, 0x1F, 0x0C};
static u8 cm_018_data[] = {0x1F, 0x1F, 0x1F, 0x0E, 0x1F, 0x1F, 0x1F, 0x08, 0x1F, 0x1F};
static u8 cm_019_data[] = {0x1F, 0x0A, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x14, 0x15};


static struct mipi_dsi_cmd a502cg_tm_power_on_table[] = {
	{0, sizeof(cm_001), cm_001},
	{0, sizeof(cm_002), cm_002},
	{0, sizeof(cm_003), cm_003},
	{0, sizeof(cm_004), cm_004},
	{0, sizeof(cm_005), cm_005},
	{0, sizeof(cm_006), cm_006},
	{0, sizeof(cm_007), cm_007},
	{0, sizeof(cm_008), cm_008},
	{0, sizeof(cm_009), cm_009},
	{0, sizeof(cm_010), cm_010},
	{0, sizeof(cm_011), cm_011},
	{0, sizeof(cm_012), cm_012},
	{0, sizeof(cm_013), cm_013},
	{0, sizeof(cm_014), cm_014},
	{0, sizeof(cm_015), cm_015},
//	{0, sizeof(cm_016), cm_016},
	{0, sizeof(cm_017), cm_017},
	{0, sizeof(cm_018), cm_018},
	{0, sizeof(cm_019), cm_019},
	{120, sizeof(sleep_out), sleep_out},
	{100, sizeof(display_on), display_on},
};

static struct mipi_dsi_cmd a502cg_tm_power_on_table_data[] = {
	{0, sizeof(cm_001_data), cm_001_data},
	{0, sizeof(cm_002_data), cm_002_data},
	{0, sizeof(cm_003_data), cm_003_data},
	{0, sizeof(cm_004_data), cm_004_data},
	{0, sizeof(cm_005_data), cm_005_data},
	{0, sizeof(cm_006_data), cm_006_data},
	{0, sizeof(cm_007_data), cm_007_data},
	{0, sizeof(cm_008_data), cm_008_data},
	{0, sizeof(cm_009_data), cm_009_data},
	{0, sizeof(cm_010_data), cm_010_data},
	{0, sizeof(cm_011_data), cm_011_data},
	{0, sizeof(cm_012_data), cm_012_data},
	{0, sizeof(cm_013_data), cm_013_data},
	{0, sizeof(cm_014_data), cm_014_data},
	{0, sizeof(cm_015_data), cm_015_data},
//	{0, sizeof(cm_016_data), cm_016_data},
	{0, sizeof(cm_017_data), cm_017_data},
	{0, sizeof(cm_018_data), cm_018_data},
	{0, sizeof(cm_019_data), cm_019_data},
	{120, sizeof(sleep_out), sleep_out},
	{100, sizeof(display_on), display_on},
};



static int send_mipi_cmd_gen(struct mdfld_dsi_pkg_sender * sender,
				struct mipi_dsi_cmd *cmd) {
	int err = 0;

	sender->status = MDFLD_DSI_PKG_SENDER_FREE;

	switch(cmd->len) {
		case 1:
			err = mdfld_dsi_send_gen_short_lp(sender,
				cmd->commands[0],
				0,
				1,
				MDFLD_DSI_SEND_PACKAGE);
			break;
		case 2:
			err = mdfld_dsi_send_gen_short_lp(sender,
				cmd->commands[0],
				cmd->commands[1],
				2,
				MDFLD_DSI_SEND_PACKAGE);
			break;
		default:
			err = mdfld_dsi_send_gen_long_lp(sender,
				cmd->commands,
				cmd->len,
				MDFLD_DSI_SEND_PACKAGE);
			break;
	}

	if (err != 0 || sender->status) {
		printk("[DISP] %s : sent failed with status=%d\n", __func__, sender->status);
		return -EIO;
	}

	if (cmd->delay)
		mdelay(cmd->delay);

	return 0;

}

static int send_mipi_cmd_mcs(struct mdfld_dsi_pkg_sender * sender,
				struct mipi_dsi_cmd *cmd) {
	int err = 0;

	sender->status = MDFLD_DSI_PKG_SENDER_FREE;

	switch(cmd->len) {
		case 1:
			err = mdfld_dsi_send_mcs_short_lp(sender,
				cmd->commands[0],
				0,
				0,
				MDFLD_DSI_SEND_PACKAGE);
			break;
		case 2:
			err = mdfld_dsi_send_mcs_short_lp(sender,
				cmd->commands[0],
				cmd->commands[1],
				1,
				MDFLD_DSI_SEND_PACKAGE);
			break;
		default:
			err = mdfld_dsi_send_mcs_long_lp(sender,
				cmd->commands,
				cmd->len,
				MDFLD_DSI_SEND_PACKAGE);
			break;
	}

	if (err != 0 || sender->status) {
		printk("[DISP] %s : sent failed with status=%d\n", __func__, sender->status);
		return -EIO;
	}

	if (cmd->delay)
		mdelay(cmd->delay);

	return 0;

}

static int compare_mipi_reg(
				struct mdfld_dsi_pkg_sender * sender,
				u8 * data,
				u32 len,
				u8 * compare_data){
	int ret = 0;
	u8 data2[65] = {0};
	int i, r = 0;
	int j = 0;

#if 0 //for  DEBUG
  printk(" [DEBUG] len = %d\n",len);
  for (j=0;j<len+1;j++) {
		printk("[DEBUG] data[%x] = 0x%x\n",j,data[j]);
  }
	for(j=0;j<len;j++) {
		printk("[DEBUG] compare_data[%x] = 0x%x\n",j,compare_data[j]);
  }
#endif
	r = mdfld_dsi_read_mcs_lp(sender, data[0], data2, len);

	ret = memcmp(data2, compare_data, len);
	if (!ret) {
		return 0;
	} else {
		printk("%s : %d, %02x,", __func__, r, data[0]);
		for (i=0; i<len; i++) {
			printk(" %02x", data2[i]);
		}
		printk(" (compare fail, retry again)\n");
	}

	return -EIO;
}


static int rm68191_vid_drv_ic_init(struct mdfld_dsi_config *dsi_config){

	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	struct rm68191_vid_data *pdata = &gpio_settings_data;
	int i;
	int retry_times;
	int r = 0;
	u8 data2[3] = {0};


	/* panel initial settings */
	mdfld_dsi_read_mcs_lp(sender, 0xB9, data2, 3);
	mdelay(5);
	gpio_direction_output(pdata->gpio_lcd_rst, 1);
	usleep_range(10000, 11000);
	gpio_direction_output(pdata->gpio_lcd_rst, 0);
	usleep_range(5000, 5100);
	gpio_direction_output(pdata->gpio_lcd_rst, 1);
	usleep_range(50000, 51000);

	printk("[DISP] %s : A502CG TM panel init\n", __func__);

	for (i = 0 ; i < 3; i++)
		send_mipi_cmd_mcs(sender, &a502cg_tm_power_on_table[0]);

	for (i = 1; i < ARRAY_SIZE(a502cg_tm_power_on_table); i++) {
		if (a502cg_tm_power_on_table[i].len <= 2 ) {
			send_mipi_cmd_mcs(sender, &a502cg_tm_power_on_table[i]);
		} else {
			retry_times = LP_RETRY;
			do {
				retry_times--;
				send_mipi_cmd_mcs(sender, &a502cg_tm_power_on_table[i]);
				r = compare_mipi_reg(sender,a502cg_tm_power_on_table[i].commands,
					a502cg_tm_power_on_table_data[i].len,a502cg_tm_power_on_table_data[i].commands);

				if (!r)
					break;
				if (!retry_times) {
					printk("[DISP] RETRY FAIL >> RESET again!\n");
					return -EIO;
				}

			} while(retry_times);
		}
	}

	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL) {
		printk("[DISP] %s MDFLD_DSI_CONTROL_ABNORMAL !!\n", __func__);
		return -EIO;
	}

	return 0;
}

static void
rm68191_vid_dsi_controller_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *hw_ctx = &dsi_config->dsi_hw_context;

	struct drm_device *dev = dsi_config->dev;
#if ENABLE_CSC_GAMMA
	struct csc_setting csc = {	.pipe = 0,
								.type = CSC_REG_SETTING,
								.enable_state = true,
								.data_len = CSC_REG_COUNT,
								.data.csc_reg_data = {
									0x400, 0x0, 0x4000000, 0x0, 0x0, 0x400}
							 };
	struct gamma_setting gamma = {	.pipe = 0,
									.type = GAMMA_REG_SETTING,
									.enable_state = true,
									.data_len = GAMMA_10_BIT_TABLE_COUNT,
									.gamma_tableX100 = {
										0x000000, 0x020202, 0x040404, 0x060606,
										0x080808, 0x0A0A0A, 0x0C0C0C, 0x0E0E0E,
										0x101010, 0x121212, 0x141414, 0x161616,
										0x181818, 0x1A1A1A, 0x1C1C1C, 0x1E1E1E,
										0x202020, 0x222222, 0x242424, 0x262626,
										0x282828, 0x2A2A2A, 0x2C2C2C, 0x2E2E2E,
										0x303030, 0x323232, 0x343434, 0x363636,
										0x383838, 0x3A3A3A, 0x3C3C3C, 0x3E3E3E,
										0x404040, 0x424242, 0x444444, 0x464646,
										0x484848, 0x4A4A4A, 0x4C4C4C, 0x4E4E4E,
										0x505050, 0x525252, 0x545454, 0x565656,
										0x585858, 0x5A5A5A, 0x5C5C5C, 0x5E5E5E,
										0x606060, 0x626262, 0x646464, 0x666666,
										0x686868, 0x6A6A6A, 0x6C6C6C, 0x6E6E6E,
										0x707070, 0x727272, 0x747474, 0x767676,
										0x787878, 0x7A7A7A, 0x7C7C7C, 0x7E7E7E,
										0x808080, 0x828282, 0x848484, 0x868686,
										0x888888, 0x8A8A8A, 0x8C8C8C, 0x8E8E8E,
										0x909090, 0x929292, 0x949494, 0x969696,
										0x989898, 0x9A9A9A, 0x9C9C9C, 0x9E9E9E,
										0xA0A0A0, 0xA2A2A2, 0xA4A4A4, 0xA6A6A6,
										0xA8A8A8, 0xAAAAAA, 0xACACAC, 0xAEAEAE,
										0xB0B0B0, 0xB2B2B2, 0xB4B4B4, 0xB6B6B6,
										0xB8B8B8, 0xBABABA, 0xBCBCBC, 0xBEBEBE,
										0xC0C0C0, 0xC2C2C2, 0xC4C4C4, 0xC6C6C6,
										0xC8C8C8, 0xCACACA, 0xCCCCCC, 0xCECECE,
										0xD0D0D0, 0xD2D2D2, 0xD4D4D4, 0xD6D6D6,
										0xD8D8D8, 0xDADADA, 0xDCDCDC, 0xDEDEDE,
										0xE0E0E0, 0xE2E2E2, 0xE4E4E4, 0xE6E6E6,
										0xE8E8E8, 0xEAEAEA, 0xECECEC, 0xEEEEEE,
										0xF0F0F0, 0xF2F2F2, 0xF4F4F4, 0xF6F6F6,
										0xF8F8F8, 0xFAFAFA, 0xFCFCFC, 0xFEFEFE,
										0x010000, 0x010000, 0x010000}
								 };

#endif
	printk("[DISP] %s\n", __func__);
	/* Reconfig lane configuration */
	dsi_config->lane_count = 2;
	dsi_config->lane_config = MDFLD_DSI_DATA_LANE_2_2;
	dsi_config->enable_gamma_csc = ENABLE_GAMMA | ENABLE_CSC;

	hw_ctx->cck_div = 1;
	hw_ctx->pll_bypass_mode = 0;
	hw_ctx->mipi_control = 0x18;
	hw_ctx->intr_en = 0xffffffff;
	hw_ctx->hs_tx_timeout = 0xffffff;
	hw_ctx->lp_rx_timeout = 0xffffff;
	hw_ctx->turn_around_timeout = 0x3f;
	hw_ctx->device_reset_timer = 0xffff;
	hw_ctx->high_low_switch_count = 0x16;
	hw_ctx->init_count = 0x7d0;
	hw_ctx->eot_disable = 0x3;
	hw_ctx->lp_byteclk = 0x3;
	hw_ctx->clk_lane_switch_time_cnt = 0x1c000b;
	hw_ctx->dphy_param = 0x180c300f;

	/* Setup video mode format */
	hw_ctx->video_mode_format = 0xf;

	/* Set up func_prg, RGB888(0x200) */
	hw_ctx->dsi_func_prg = (0x200 | dsi_config->lane_count);

	/* Setup mipi port configuration */
	hw_ctx->mipi = PASS_FROM_SPHY_TO_AFE | dsi_config->lane_config;

	rm68191_dsi_config = dsi_config;

#if ENABLE_CSC_GAMMA
	if (dsi_config->enable_gamma_csc & ENABLE_CSC) {
		/* setting the tuned csc setting */
		drm_psb_enable_color_conversion = 1;
		mdfld_intel_crtc_set_color_conversion(dev, &csc);
	}

	if (dsi_config->enable_gamma_csc & ENABLE_GAMMA) {
		/* setting the tuned gamma setting */
		drm_psb_enable_gamma = 1;
		mdfld_intel_crtc_set_gamma(dev, &gamma);
	}
#endif
}

static int rm68191_vid_detect(struct mdfld_dsi_config *dsi_config)
{
	int status;
	struct drm_device *dev = dsi_config->dev;
	struct mdfld_dsi_hw_registers *regs = &dsi_config->regs;
	u32 dpll_val, device_ready_val;
	int pipe = dsi_config->pipe;

	printk("[DISP] %s\n", __func__);

	if (pipe == 0) {
		/*
		 * FIXME: WA to detect the panel connection status, and need to
		 * implement detection feature with get_power_mode DSI command.
		 */
		if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
			OSPM_UHB_FORCE_POWER_ON)) {
			DRM_ERROR("hw begin failed\n");
			return -EAGAIN;
		}

		dpll_val = REG_READ(regs->dpll_reg);
		device_ready_val = REG_READ(regs->device_ready_reg);
		if ((device_ready_val & DSI_DEVICE_READY) &&
		    (dpll_val & DPLL_VCO_ENABLE)) {
			dsi_config->dsi_hw_context.panel_on = true;
			psb_enable_vblank(dev, pipe);
		} else {
			dsi_config->dsi_hw_context.panel_on = false;
			DRM_INFO("%s: panel is not detected!\n", __func__);
		}

		status = MDFLD_DSI_PANEL_CONNECTED;
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	} else {
		DRM_INFO("%s: do NOT support dual panel\n", __func__);
		status = MDFLD_DSI_PANEL_DISCONNECTED;
	}

	return status;
}

static int rm68191_vid_power_on(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	printk("[DISP] %s\n", __func__);
	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	usleep_range(1000, 1200);

	/* Send TURN_ON packet */
	err = mdfld_dsi_send_dpi_spk_pkg_lp(sender,
					    MDFLD_DSI_DPI_SPK_TURN_ON);
	if (err) {
		DRM_ERROR("Failed to send turn on packet\n");
		return err;
	}

	/* EN_VDD_BL*/
#if PWM_SOC_ENABLE
	pwm_enable();
#endif
//	intel_scu_ipc_iowrite8(PMIC_GPIO_BACKLIGHT_EN, 0x01);
//	intel_scu_ipc_iowrite8(PMIC_GPIO_VEMMC2CNT, 0x06);

	return 0;
}

static int rm68191_vid_power_off(struct mdfld_dsi_config *dsi_config)
{
//	struct rm68191_vid_data *pdata = &gpio_settings_data;
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	printk("[DISP] %s\n", __func__);
	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	/* Turn off the backlight*/
#if PWM_SOC_ENABLE
	pwm_disable();
#endif
	usleep_range(1000, 1500);

	/* Send SHUT_DOWN packet */
	err = mdfld_dsi_send_dpi_spk_pkg_lp(sender,
					    MDFLD_DSI_DPI_SPK_SHUT_DOWN);
	if (err) {
		DRM_ERROR("Failed to send turn off packet\n");
		return err;
	}

	/* Send power off command*/
	sender->status = MDFLD_DSI_PKG_SENDER_FREE;
	mdfld_dsi_send_mcs_short_lp(sender, 0x28, 0x00, 0, 0);
	mdfld_dsi_send_mcs_short_lp(sender, 0x10, 0x00, 0, 0);
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL) {
		printk("[DISP] %s MDFLD_DSI_CONTROL_ABNORMAL !!\n", __func__);
		return -EIO;
	}
	usleep_range(50000, 55000);
	/* Driver IC power off sequence*/
//	intel_scu_ipc_iowrite8(PMIC_GPIO_VEMMC2CNT, 0x04);
#if 0 /* Skip put down the HW_RST pin */
	gpio_direction_output(pdata->gpio_lcd_rst, 0);
	usleep_range(120000, 121000);
#endif
	return 0;
}

static int rm68191_vid_reset(struct mdfld_dsi_config *dsi_config)
{
//	struct rm68191_vid_data *pdata = &gpio_settings_data;
	printk("[DISP] %s\n", __func__);

      // start the initial state
//	intel_scu_ipc_iowrite8(PMIC_GPIO_VEMMC2CNT, 0x04);
//	usleep_range(1000, 1500);
//	intel_scu_ipc_iowrite8(PMIC_GPIO_BACKLIGHT_EN, 0x00);

	// start power on sequence
//	intel_scu_ipc_iowrite8(PMIC_GPIO_VEMMC2CNT, 0x06);
//	usleep_range(5000, 5500);

 /* Move the RST control to driverIC init */
/*
	gpio_direction_output(pdata->gpio_lcd_rst, 1);
	usleep_range(10000, 11000);
	gpio_direction_output(pdata->gpio_lcd_rst, 0);
	usleep_range(10000, 11000);
	gpio_direction_output(pdata->gpio_lcd_rst, 1);
	usleep_range(10000, 11000);
*/
	return 0;
}


#define PWM0CLKDIV1 0x61
#define PWM0CLKDIV0 0x62

#define PWM0DUTYCYCLE 0x67
#define DUTY_VALUE_MAX 0x63

/*
#define BRI_SETTING_MIN 10
//#define BRI_SETTING_DEF 170
#define BRI_SETTING_MAX 255
*/
static int rm68191_vid_set_brightness(struct mdfld_dsi_config *dsi_config,
					 int level)
{
	int duty_val = 0;
	int ret = 0;
	unsigned int pwm_min, pwm_max;
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
#if PWM_SOC_ENABLE
	pwm_min = 5;
	pwm_max = 255;

	if (level <= 0) {
		duty_val = 0;
	} else if (level > 0 && (level <= pwm_min)) {
		duty_val = pwm_min;
	} else if ((level >= pwm_min) && (level <= pwm_max)) {
		duty_val = level;
	} else if (level > pwm_max)
		duty_val = pwm_max;

	pwm_configure(duty_val);
#else
	duty_val = ((DUTY_VALUE_MAX + 1) * level) / 255;
	ret = intel_scu_ipc_iowrite8(PWM0DUTYCYCLE, (duty_val > DUTY_VALUE_MAX ? DUTY_VALUE_MAX : duty_val));
	if (ret)
		DRM_ERROR("write brightness duty value faild\n");
#endif

	printk("[DISP] brightness level = %d , duty_val = %d\n", level, duty_val);

	return 0;
}

struct drm_display_mode *rm68191_vid_get_config_mode(void)
{
	struct drm_display_mode *mode;

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	printk("[DISP] %s\n", __func__);
	/* RECOMMENDED PORCH SETTING
		HSA=2, HBP=6, HFP=15
		VSA=2, VBP=9, VFP=8	 */
	mode->hdisplay = 540;
	mode->vdisplay = 960;
	mode->hsync_start = mode->hdisplay + 15;
	mode->hsync_end = mode->hsync_start + 2;
	mode->htotal = mode->hsync_end + 6;
	mode->vsync_start = mode->vdisplay + 8;
	mode->vsync_end = mode->vsync_start + 2;
	mode->vtotal = mode->vsync_end + 9;

	mode->vrefresh = 60;
	mode->clock = mode->vrefresh * mode->vtotal * mode->htotal / 1000;
	mode->type |= DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	return mode;
}

static void rm68191_vid_get_panel_info(int pipe, struct panel_info *pi)
{
	pi->width_mm = 62;
	pi->height_mm = 110;
}

static int rm68191_vid_gpio_init(void)
{
	int ret;
	struct rm68191_vid_data *pdata = &gpio_settings_data;

	printk("[DISP] %s\n", __func__);

	pdata->gpio_lcd_rst = get_gpio_by_name("DISP_RST_N");

	ret = gpio_request(pdata->gpio_lcd_rst, "DISP_RST_N");
	if (ret < 0)
		DRM_ERROR("Faild to get panel GPIO DISP_RST_N:%d\n", pdata->gpio_lcd_rst);

//	intel_scu_ipc_iowrite8(PMIC_GPIO_BACKLIGHT_EN, 0x01);

	return 0;
}

static int rm68191_vid_brightness_init(void)
{
	int ret = 0;

	printk("[DISP] %s\n", __func__);
#if PWM_SOC_ENABLE
	pwmctrl_mmio = ioremap_nocache(PWMCTRL_REG,PWMCTRL_SIZE);
	lnw_gpio_set_alt(PWM_ENABLE_GPIO, LNW_ALT_2);
#else
	ret = intel_scu_ipc_iowrite8(PWM0CLKDIV1, 0x00);
	if (!ret)
		ret = intel_scu_ipc_iowrite8(PWM0CLKDIV0, 0x25);

	if (ret)
		printk("[DISP] %s: PWM0CLKDIV set failed\n", __func__);
	else
		printk("[DISP] PWM0CLKDIV set to 0x%04x\n", 0x25);
#endif

	return ret;
}


#ifdef RM68191_DEBUG
static int send_mipi_ret = -1;
static int read_mipi_ret = -1;
static u8 read_mipi_data = 0;

static ssize_t send_mipi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    int x0=0, x1=0;
    struct mdfld_dsi_pkg_sender *sender
			= mdfld_dsi_get_pkg_sender(rm68191_dsi_config);

    sscanf(buf, "%x,%x", &x0, &x1);

//	send_mipi_ret = mdfld_dsi_send_mcs_short_lp(sender,x0,x1,1,0);
	send_mipi_ret = mdfld_dsi_send_gen_short_lp(sender,x0,x1,2,0);

	DRM_INFO("[DISPLAY] send %x,%x : ret = %d\n",x0,x1,send_mipi_ret);

    return count;
}

static ssize_t send_mipi_show(struct device *dev,
	struct device_attribute *attr, const char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",send_mipi_ret);
}


static ssize_t read_mipi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    int x0=0;
    struct mdfld_dsi_pkg_sender *sender
			= mdfld_dsi_get_pkg_sender(rm68191_dsi_config);

    sscanf(buf, "%x", &x0);

    read_mipi_ret = mdfld_dsi_read_mcs_lp(sender,x0,&read_mipi_data,1);
    if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
        read_mipi_ret = -EIO;

	DRM_INFO("[DISPLAY] read 0x%x :ret=%d data=0x%x\n", x0, read_mipi_ret, read_mipi_data);

    return count;
}

static ssize_t read_mipi_show(struct device *dev,
	struct device_attribute *attr, const char *buf)
{
	return snprintf(buf, PAGE_SIZE, "ret=%d data=0x%x\n",read_mipi_ret,read_mipi_data);
}

DEVICE_ATTR(send_mipi_rm68191,S_IRUGO | S_IWUSR, send_mipi_show,send_mipi_store);
DEVICE_ATTR(read_mipi_rm68191,S_IRUGO | S_IWUSR, read_mipi_show,read_mipi_store);


static struct attribute *rm68191_attrs[] = {
        &dev_attr_send_mipi_rm68191.attr,
        &dev_attr_read_mipi_rm68191.attr,
        NULL
};

static struct attribute_group rm68191_attr_group = {
        .attrs = rm68191_attrs,
        .name = "rm68191",
};

#endif

void rm68191_vid_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	int ret = 0;
	printk("[DISP] %s\n", __func__);

	p_funcs->get_config_mode = rm68191_vid_get_config_mode;
	p_funcs->get_panel_info = rm68191_vid_get_panel_info;
	p_funcs->dsi_controller_init = rm68191_vid_dsi_controller_init;
	p_funcs->detect = rm68191_vid_detect;
	p_funcs->power_on = rm68191_vid_power_on;
	p_funcs->drv_ic_init = rm68191_vid_drv_ic_init;
	p_funcs->power_off = rm68191_vid_power_off;
	p_funcs->reset = rm68191_vid_reset;
	p_funcs->set_brightness = rm68191_vid_set_brightness;

	ret = rm68191_vid_gpio_init();
	if (ret)
		DRM_ERROR("Faild to request GPIO for RM68191 panel\n");

	ret = rm68191_vid_brightness_init();
	if (ret)
		DRM_ERROR("Faild to initilize PWM of MSCI\n");

#ifdef RM68191_DEBUG
    sysfs_create_group(&dev->dev->kobj, &rm68191_attr_group);
#endif

}

static int rm68191_vid_shutdown(struct platform_device *pdev)
{
	struct rm68191_vid_data *pdata = &gpio_settings_data;
	printk("[DISP] %s\n", __func__);

	mdfld_dsi_dpi_set_power(encoder_lcd, 0);
//	intel_scu_ipc_iowrite8(PMIC_GPIO_BACKLIGHT_EN, 0);
//	rm68191_vid_set_brightness(rm68191_dsi_config, 0);
//	rm68191_vid_power_off(rm68191_dsi_config);
	usleep_range(50000, 55000);
	gpio_direction_output(pdata->gpio_lcd_rst, 0);
	usleep_range(120000, 121000);

	intel_scu_ipc_iowrite8(PMIC_GPIO_VEMMC2CNT, 0x04);

	return 0;
}


static int rm68191_vid_lcd_probe(struct platform_device *pdev)
{
	printk("[DISP] %s: RM68191 panel detected\n", __func__);
	intel_mid_panel_register(rm68191_vid_init);

	return 0;
}

struct platform_driver rm68191_lcd_driver = {
	.probe	= rm68191_vid_lcd_probe,
	.shutdown = rm68191_vid_shutdown,
	.driver	= {
		.name	= RM68191_PANEL_NAME,
		.owner	= THIS_MODULE,
	},
};
