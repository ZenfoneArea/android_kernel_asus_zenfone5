#include <linux/i2c.h>
#include <linux/kernel.h>
#include <asm/intel-mid.h>
#include <linux/max17048_battery.h>

struct max17048_battery_model max17048_mdata = {
	.rcomp          = 95,
	.soccheck_A     = 232,
	.soccheck_B     = 234,
	.bits           = 19,
	.alert_threshold = 0x00,
	.one_percent_alerts = 0x1F, //11111b
	.alert_on_reset = 0x40,
	.rcomp_seg      = 0x0080,
	.hibernate      = 0x3080,
	.vreset         = 0x9696,
	.valert         = 0xD4AA,
	.ocvtest        = 57904,
};

static struct i2c_board_info grouper_i2c2_max17048_board_info[] = {
	{
		I2C_BOARD_INFO("max17048", 0x36),
		.platform_data = &max17048_mdata,
	},
};

static int __init max17048_init(void)
{
	pr_info("%s register MAX17048 in I2C\n", __func__);
	return i2c_register_board_info(2, grouper_i2c2_max17048_board_info,
		ARRAY_SIZE(grouper_i2c2_max17048_board_info));
}
module_init(max17048_init);
