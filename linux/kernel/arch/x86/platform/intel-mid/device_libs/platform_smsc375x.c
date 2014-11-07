
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/lnw_gpio.h>
#include <linux/gpio.h>
#include <linux/extcon/extcon-smsc375x.h>
#include <asm/intel-mid.h>
#include <asm/intel_crystalcove_pwrsrc.h>
#include <asm/intel_bytcr_bcntl.h>

static struct smsc375x_pdata smsc_pdata;

/* dummy functions */
static int smsc375x_enable_vbus(void *ptr)
{
	return 0;
}
static int smsc375x_disable_vbus(void *ptr)
{
	return 0;
}
static int smsc375x_is_vbus_online(void *ptr)
{
	return 0;
}

void *smsc375x_platform_data(void)
{
	smsc_pdata.enable_vbus = intel_bytcr_boost_enable;
	smsc_pdata.disable_vbus = intel_bytcr_boost_disable;
	smsc_pdata.is_vbus_online = crystal_cove_vbus_on_status;

	return &smsc_pdata;
}
