/*
 * leds-asus.c - Asus charging LED driver
 *
 * Copyright (C) 2013 Asus
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mfd/intel_mid_pmic.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_scu_pmic.h>

#define CONTROL_LED		1
#define RED_LED_GPIO		38
#define GREEN_LED_GPIO		39
#define CHRLEDPWM			0x194
#define CHRLEDCTRL			0x195
#define BLEDON				BIT(0)
#define CHRLEDBP			BIT(5)

#define LED_info(...)			printk("[LED] " __VA_ARGS__);
#define LED_err(...)			printk(KERN_ERR, "[LED_ERR] " __VA_ARGS__);

struct led_info_data {
	struct led_classdev cdev;
	struct work_struct work;
};

struct led_info_priv {
	int num_leds;
	struct led_info_data leds[];
};

static int red_led_flag, green_led_flag, red_blink_flag, green_blink_flag;

#ifdef CONTROL_LED
static int disable_led_flag;
static ssize_t disable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	int len=0;

	//LED_info("%s\n", __func__);
	len += sprintf(buf + len, "%d\n", disable_led_flag);

	return len;
}
static ssize_t disable_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	sscanf(buf, "%d", &disable_led_flag);

	return count;
}
static DEVICE_ATTR(disable, 0664,
        disable_show, disable_store);
#endif

static ssize_t blink_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_info_data *led_dat = container_of(led_cdev, struct led_info_data, cdev);
	int len=0;

	if(!strcmp(led_dat->cdev.name, "red"))
		len += sprintf(buf + len, "%d\n", red_blink_flag);
	else if(!strcmp(led_dat->cdev.name, "green")&&(green_blink_flag==1))
		len += sprintf(buf + len, "%d\n", green_blink_flag);

	return len;
}
static ssize_t blink_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct led_info_data *led_dat = container_of(led_cdev, struct led_info_data, cdev);
	int value, ret=0;
	uint8_t ctrldata, pwmdata;

	sscanf(buf, "%d", &value);

#ifdef CONTROL_LED
	if(disable_led_flag==0) {
#endif
		if(value==0) {
			/* stop blink */
			if(!strcmp(led_dat->cdev.name, "red")&&(red_blink_flag==1)) {
				ret = intel_scu_ipc_ioread8(CHRLEDCTRL, &ctrldata);
				if (ret) {
					LED_err(" IPC Failed to read %d\n", ret);
				}
				ctrldata &= ~BLEDON;
				ctrldata &= ~CHRLEDBP;
				ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, ctrldata);
				if (ret) {
					LED_err(" IPC Failed to write %d\n", ret);
				}
				red_blink_flag = 0;
			}else if(!strcmp(led_dat->cdev.name, "green")&&(green_blink_flag==1)) {
				ret = intel_scu_ipc_ioread8(CHRLEDCTRL, &ctrldata);
				if (ret) {
					LED_err(" IPC Failed to read %d\n", ret);
				}
				ctrldata &= ~BLEDON;
				ctrldata &= ~CHRLEDBP;
				ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, ctrldata);
				if (ret) {
					LED_err(" IPC Failed to write %d\n", ret);
				}
				green_blink_flag = 0;
			}
			/*power off*/
			if(!strcmp(led_dat->cdev.name, "red")) {
				gpio_direction_output(RED_LED_GPIO, 0);
				red_led_flag = 0;
			}else if(!strcmp(led_dat->cdev.name, "green")) {
				gpio_direction_output(GREEN_LED_GPIO, 0);
				green_led_flag = 0;
			}
			LED_info("%s, disable blink and register ctrldata=0x%x\n", __func__, ctrldata);
		}
		else if(value>0&&value<=100) {
			/* power on*/
			if(!strcmp(led_dat->cdev.name, "red")) {
				gpio_direction_output(RED_LED_GPIO, 1);
				red_led_flag = 1;
				red_blink_flag = 1;
			}else if(!strcmp(led_dat->cdev.name, "green")) {
				gpio_direction_output(GREEN_LED_GPIO, 1);
				green_led_flag = 1;
				green_blink_flag = 1;
			}
			/* start blink */
			ret = intel_scu_ipc_ioread8(CHRLEDCTRL, &ctrldata);
			if (ret) {
				LED_err(" IPC Failed to read %d\n", ret);
			}
			ctrldata &= ~BLEDON;
			ctrldata &= ~CHRLEDBP;
			ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, ctrldata);
			if (ret) {
				LED_err(" IPC Failed to write %d\n", ret);
			}

			if(!strcmp(led_dat->cdev.name, "red"))
				value = 99; //0.025s per 4s
			else
				value = 99; //0.025s per 2s
			pwmdata = (uint8_t)value*255/100;
			ret = intel_scu_ipc_iowrite8(CHRLEDPWM, pwmdata);
			if (ret) {
				LED_err(" IPC Failed to write %d\n", ret);
			}
			ret = intel_scu_ipc_ioread8(CHRLEDPWM, &pwmdata);
			if (ret) {
				LED_err(" IPC Failed to read %d\n", ret);
			}

			ret = intel_scu_ipc_ioread8(CHRLEDCTRL, &ctrldata);
			if (ret) {
			LED_err(" IPC Failed to read %d\n", ret);
			}
			ctrldata |= BLEDON;
			ctrldata |= CHRLEDBP;
			if(!strcmp(led_dat->cdev.name, "red")) { //4s
				ctrldata &= ~(BIT(3));
				ctrldata &= ~(BIT(4));
			}else if(!strcmp(led_dat->cdev.name, "green")) { //2s
				ctrldata |= (BIT(3));
				ctrldata &= ~(BIT(4));
			}
			ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, ctrldata);
			if (ret) {
				LED_err(" IPC Failed to write %d\n", ret);
			}
			ret = intel_scu_ipc_ioread8(CHRLEDCTRL, &ctrldata);
			if (ret) {
				LED_err(" IPC Failed to read %d\n", ret);
			}
			LED_info("%s, enable blink and value=%d, register pwmdata=0x%x, ctrldata=0x%x\n", __func__, value, pwmdata, ctrldata);
		}else
			LED_info("%s, incorrect pwm value:%d (0-100).\n", __func__, value);
#ifdef CONTROL_LED
	}
#endif
	return count;
}
static DEVICE_ATTR(blink, 0664,
        blink_show, blink_store);

static inline int sizeof_led_info_priv(int num_leds)
{
	return sizeof(struct led_info_priv) +
		(sizeof(struct led_info_data) * num_leds);
}

static void asus_led_set_brightness(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct led_info_data *led_dat = container_of(led_cdev, struct led_info_data, cdev);

#ifdef CONTROL_LED
	LED_info("disable_led_flag=%d\n", disable_led_flag);
#endif
	LED_info("brightness=%d, name=%s\n", value, led_dat->cdev.name);

	LED_info("%s, brightness=%d, name=%s\n", __func__, value, led_dat->cdev.name);
#ifdef CONTROL_LED
	if(disable_led_flag==0) {
#endif
		if(value==0) {
			if(!strcmp(led_dat->cdev.name, "red")) {
				gpio_direction_output(RED_LED_GPIO, 0);
				red_led_flag = 0;
			}else if(!strcmp(led_dat->cdev.name, "green")) {
				gpio_direction_output(GREEN_LED_GPIO, 0);
				green_led_flag = 0;
			}
		}
		else if(value>0&&value<=255) {
			if(!strcmp(led_dat->cdev.name, "red")) {
				gpio_direction_output(RED_LED_GPIO, 1);
				red_led_flag = 1;
			}else if(!strcmp(led_dat->cdev.name, "green")) {
				gpio_direction_output(GREEN_LED_GPIO, 1);
				green_led_flag = 1;
			}
		}
#ifdef CONTROL_LED
	}
#endif
}

static void delete_led(struct led_info_data *led)
{
	led_classdev_unregister(&led->cdev);
	//cancel_work_sync(&led->work);
}

static int asus_led_probe(struct platform_device *pdev)
{
	struct led_platform_data *pdata = pdev->dev.platform_data;
	struct led_info_priv *priv;
	int i, ret = 0;

	LED_info("%s start\n", __func__);

	if (pdata && pdata->num_leds) {
		priv = kzalloc(sizeof_led_info_priv(pdata->num_leds),
				GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		priv->num_leds = pdata->num_leds;
		for (i = 0; i < priv->num_leds; i++) {
			priv->leds[i].cdev.name = pdata->leds[i].name;
			priv->leds[i].cdev.brightness_set = asus_led_set_brightness;
			priv->leds[i].cdev.brightness = LED_OFF;
			priv->leds[i].cdev.max_brightness = 255;
			ret = led_classdev_register(&pdev->dev, &priv->leds[i].cdev);
			if (ret < 0)
				LED_err("led_classdev_register led[%d] fail\n", i);
#ifdef CONTROL_LED
			disable_led_flag = 0;
			ret = device_create_file(priv->leds[i].cdev.dev, &dev_attr_disable);
			if (ret)
				LED_err("device_create_file disable in led[%d] fail\n", i);
#endif
			red_led_flag = 0;
			green_led_flag = 0;
			red_blink_flag = 0;
			green_blink_flag = 0;
			ret = device_create_file(priv->leds[i].cdev.dev, &dev_attr_blink);
			if (ret)
				LED_err("device_create_file blink in led[%d] fail\n", i);
		}
	}

	platform_set_drvdata(pdev, priv);

	/*request gpio*/
	ret = gpio_request(RED_LED_GPIO, "led_red");
	if (ret < 0) {
		LED_err("request RED LED(%d) fail, ret=%d\n", RED_LED_GPIO, ret);
	}
	ret = gpio_request(GREEN_LED_GPIO, "led_green");
	if (ret < 0) {
		LED_err("request GREEN LED(%d) fail, ret=%d\n", GREEN_LED_GPIO, ret);
	}

	LED_info("%s end\n", __func__);
	return 0;
}

static int __devexit asus_led_remove(struct platform_device *pdev)
{
	struct led_info_priv *priv = dev_get_drvdata(&pdev->dev);
	int i;

	LED_info("%s\n", __func__);
	for (i = 0; i < priv->num_leds; i++) {
#ifdef CONTROL_LED
		device_remove_file(&priv->leds[i].cdev.dev, &dev_attr_disable);
#endif
		device_remove_file(&priv->leds[i].cdev.dev, &dev_attr_blink);
		delete_led(&priv->leds[i]);
	}

	dev_set_drvdata(&pdev->dev, NULL);
	kfree(priv);

	gpio_free(RED_LED_GPIO);
	gpio_free(GREEN_LED_GPIO);

	return 0;
}

static struct platform_driver led_info_driver = {
	.probe		= asus_led_probe,
	.remove		= __devexit_p(asus_led_remove),
	.driver		= {
		.name	= "leds-asus",
		.owner	= THIS_MODULE,
	},
};
module_platform_driver(led_info_driver);


MODULE_AUTHOR("Chih-Hsuan Chang <chih-hsuan_chang@asus.com>");
MODULE_DESCRIPTION("Asus LED Driver");
MODULE_LICENSE("GPL");
