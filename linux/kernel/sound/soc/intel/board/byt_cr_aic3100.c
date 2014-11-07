/*
 *  byt_cr_aic3100.c - ASoc Machine driver for Intel Baytrail Baylake CR platform
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: Hardik T Shah <hardik.t.shah@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/vlv2_plat_clock.h>
#include <linux/acpi_gpio.h>
#include <linux/input.h>
#include <asm/intel-mid.h>
#include <asm/platform_byt_audio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include "../../codecs/tlv320aic31xx.h"

#define BYT_PLAT_CLK_3_HZ	25000000

#define BYT_INTR_DEBOUNCE               0
#define BYT_HS_INSERT_DET_DELAY         500
#define BYT_HS_REMOVE_DET_DELAY         500
#define BYT_BUTTON_PRESS_DELAY          50
#define BYT_BUTTON_RELEASE_DELAY        50
#define BYT_HS_DET_POLL_INTRVL          100
#define BYT_BUTTON_EN_DELAY             1500

#define BYT_HS_DET_RETRY_COUNT          6

#define VLV2_PLAT_CLK_AUDIO	3
#define PLAT_CLK_FORCE_ON	1
#define PLAT_CLK_FORCE_OFF	2

/* 0 = 25MHz from crystal, 1 = 19.2MHz from PLL */
#define PLAT_CLK_FREQ_XTAL	0


struct byt_mc_private {
	struct snd_soc_jack jack;
	struct delayed_work hs_insert_work;
	struct delayed_work hs_remove_work;
	struct delayed_work hs_button_press_work;
	struct delayed_work hs_button_release_work;
	struct mutex jack_mlock;
	/* To enable button press interrupts after a delay after
	   HS detection. This is to avoid spurious button press
	   events during slow HS insertion */
	struct delayed_work hs_button_en_work;
	int intr_debounce;
	int hs_insert_det_delay;
	int hs_remove_det_delay;
	int button_press_delay;
	int button_release_delay;
	int button_en_delay;
	int hs_det_poll_intrvl;
	int hs_det_retry;
	bool process_button_events;
};

static int byt_hs_detection(void);
static struct snd_soc_jack_gpio hs_gpio = {
		.name			= "byt-codec-int",
		.report			= SND_JACK_HEADSET |
					  SND_JACK_HEADPHONE |
					  SND_JACK_BTN_0,
		.debounce_time		= BYT_INTR_DEBOUNCE,
		.jack_status_check	= byt_hs_detection,
};


static inline void byt_force_enable_pin(struct snd_soc_codec *codec,
			 const char *bias_widget, bool enable)
{
	pr_debug("%s %s\n", enable ? "enable" : "disable", bias_widget);
	if (enable)
		snd_soc_dapm_force_enable_pin(&codec->dapm, bias_widget);
	else
		snd_soc_dapm_disable_pin(&codec->dapm, bias_widget);
	snd_soc_dapm_sync(&codec->dapm);
}

static inline void byt_set_mic_bias(struct snd_soc_codec *codec, bool enable)
{
	if (enable)
		byt_force_enable_pin(codec, "micbias", true);
	else
		byt_force_enable_pin(codec, "micbias", false);
}
/* Identify the jack type as Headset/Headphone/None */
static int byt_check_jack_type(void)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio;
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;
	int status, jack_type = 0;
	struct byt_mc_private *ctx =
			 container_of(jack, struct byt_mc_private, jack);

	status = aic31xx_query_jack_status(codec);
	/* jd status high indicates some accessory has been connected */
	if (status) {
		pr_debug("Jack insert intr");
		/* Do not process button events until accessory is
		   detected as headset*/
		ctx->process_button_events = false;
		byt_set_mic_bias(codec, true);
		jack_type = aic31xx_query_jack_status(codec);
		if (jack_type == SND_JACK_HEADSET) {
			ctx->process_button_events = true;
			/* If headset is detected, enable button
			   interrupts after a delay */
			schedule_delayed_work(&ctx->hs_button_en_work,
					msecs_to_jiffies(ctx->button_en_delay));
		}
		if (jack_type != SND_JACK_HEADSET)
			byt_set_mic_bias(codec, false);
	} else {
		jack_type = 0;
	}
	pr_debug("Jack type detected:%d", jack_type);
	return jack_type;
}

/* Work function invoked by the Jack Infrastructure. Other delayed works
   for jack detection/removal/button press are scheduled from this function */
static int byt_hs_detection(void)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio;
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;
	int status, jack_type = 0;
	int ret, val;
	struct byt_mc_private *ctx =
		 container_of(jack, struct byt_mc_private, jack);

	/* Ack interrupt first */
	val = snd_soc_read(codec, AIC31XX_INTRDACFLAG);
	mutex_lock(&ctx->jack_mlock);
	/* Initialize jack status with previous status.
	   The delayed work will confirm the event and
	   send updated status later */
	jack_type = jack->status;
	pr_debug("Enter:%s", __func__);

	if (!jack->status) {
		ctx->hs_det_retry = BYT_HS_DET_RETRY_COUNT;
		ret = schedule_delayed_work(&ctx->hs_insert_work,
				msecs_to_jiffies(ctx->hs_insert_det_delay));
		if (!ret)
			pr_debug("byt_check_hs_insert_status already queued");
		else
			pr_debug("%s:Check hs insertion  after %d msec",
					__func__, ctx->hs_insert_det_delay);
	}  else {
		/* First check for accessory removal; If not removed,
		   check for button events*/
		status = aic31xx_query_jack_status(codec);
		/* jd status low indicates accessory has been disconnected.
		   However, confirm the removal in the delayed work */
		if (!status) {
			/* Do not process button events while we make sure
			   accessory is disconnected*/
			ctx->process_button_events = false;
			ret = schedule_delayed_work(&ctx->hs_remove_work,
				msecs_to_jiffies(ctx->hs_remove_det_delay));
			if (!ret)
				pr_debug("byt_check_hs_remove_status already queued");
			else
				pr_debug("%s:Check hs removal after %d msec",
						__func__,
						ctx->hs_remove_det_delay);
		} else { /* Must be button event.
			  * Confirm the event in delayed work */
			if (((jack->status & SND_JACK_HEADSET) == SND_JACK_HEADSET) &&
					ctx->process_button_events) {
				ret = schedule_delayed_work(
					&ctx->hs_button_press_work,
					msecs_to_jiffies(
						ctx->button_press_delay));
				if (!ret)
					pr_debug("byt_check_hs_button_press_status already queued");
				else
					pr_debug("%s:check BP/BR after %d msec",
						__func__,
						ctx->button_press_delay);
			}
		}
	}

	pr_debug("Exit:%s", __func__);
	mutex_unlock(&ctx->jack_mlock);

	return jack_type;
}

/*Checks jack insertion and identifies the jack type.
  Retries the detection if necessary */
static void byt_check_hs_insert_status(struct work_struct *work)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio;
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;
	struct byt_mc_private *ctx =
		 container_of(work, struct byt_mc_private, hs_insert_work.work);
	int jack_type = 0;

	mutex_lock(&ctx->jack_mlock);
	pr_debug("Enter:%s", __func__);

	/* TODO: Switch to MCLK instead of internal clock once
		 Jack connect interrupt is raised
		 This is because with internal clock headset
		 is getting detected as headphone. Need to check
		 with TI guys whether this is limitation. Side
		 effect of this is that 25MHz MCLK will remain
		 ON if headset is inserted and removed without
		 playback or capture done after insertion
	*/
	vlv2_plat_configure_clock(VLV2_PLAT_CLK_AUDIO,
			PLAT_CLK_FORCE_ON);
	pr_debug("Platform clk turned ON\n");
	snd_soc_codec_set_sysclk(codec, AIC31XX_MCLK,
			0, AIC31XX_FREQ_25000000, SND_SOC_CLOCK_IN);

	jack_type = byt_check_jack_type();

	/* Report jack immediately only if jack is headset.
	 *  If headphone or no jack was detected,
	 *  dont report it until the last HS det try.
	 *  This is to avoid reporting any temporary
	 *  jack removal or accessory change(eg, HP to HS)
	 * during the detection tries.
	 * This provides additional debounce that will help
	 * in the case of slow insertion.
	 * This also avoids the pause in audio due to accessory
	 * change from HP to HS
	 */
	if (ctx->hs_det_retry <= 0) /* end of retries; report the status */{
		pr_debug("%d Jack type sent is %d\n", __LINE__, jack_type);
		snd_soc_jack_report(jack, jack_type, gpio->report);
	} else {
		/* Schedule another detection try if headphone or
		 * no jack is detected.
		 * During slow insertion of headset, first a headphone
		 * may be detected.
		 * Hence retry until headset is detected
		 */
		if (jack_type == SND_JACK_HEADSET) {
			ctx->hs_det_retry = 0;
			/* HS detected, no more retries needed */
			pr_debug("%d Jack type sent is %d\n",
				 __LINE__, jack_type);
			snd_soc_jack_report(jack, jack_type, gpio->report);
		} else {
			ctx->hs_det_retry--;
			schedule_delayed_work(&ctx->hs_insert_work,
				msecs_to_jiffies(ctx->hs_det_poll_intrvl));
			pr_debug("%s:re-try hs detection after %d msec",
					__func__, ctx->hs_det_poll_intrvl);
		}
	}

	pr_debug("Exit:%s", __func__);
	mutex_unlock(&ctx->jack_mlock);
}
/* Checks jack removal. */
static void byt_check_hs_remove_status(struct work_struct *work)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio;
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;
	struct byt_mc_private *ctx =
		 container_of(work, struct byt_mc_private, hs_remove_work.work);
	int status = 0, jack_type = 0;

	/* Cancel any pending insertion detection. There
	   could be pending insertion detection in the
	   case of very slow insertion or insertion and
	   immediate removal.*/
	cancel_delayed_work_sync(&ctx->hs_insert_work);

	mutex_lock(&ctx->jack_mlock);
	pr_debug("Enter:%s", __func__);
	/* Initialize jack_type with previous status.
	   If the event was an invalid one, we return the preious state*/
	jack_type = jack->status;

	if (jack->status) {
		/* jack is in connected state; look for removal event */
		status = aic31xx_query_jack_status(codec);
		if (!status) {
			pr_debug("Jack remove event");
			ctx->process_button_events = false;
			cancel_delayed_work_sync(&ctx->hs_button_en_work);
			jack_type = 0;
			byt_set_mic_bias(codec, false);
			aic31xx_btn_press_intr_enable(codec, false);

		} else if (((jack->status & SND_JACK_HEADSET) == SND_JACK_HEADSET) &&
				 !ctx->process_button_events) {
			/* Jack is still connected. We may come here if
			   there was a spurious jack removal event.
			   No state change is done until removal is confirmed
			   by the check_jd_status above.i.e. jack status
			   remains Headset or headphone. But as soon as
			   the interrupt thread(byt_hs_detection) detected a jack
			   removal, button processing gets disabled.
			   Hence re-enable button processing in the case of
			   headset */
			pr_debug(" spurious Jack remove event for headset \
				 re-enable button events");
			ctx->process_button_events = true;
		}
	}
	pr_debug("%d Jack type sent is %d\n", __LINE__, jack_type);
	snd_soc_jack_report(jack, jack_type, gpio->report);
	pr_debug("Exit:%s", __func__);
	mutex_unlock(&ctx->jack_mlock);
}
/* Check for button press status */
static void byt_check_hs_button_press_status(struct work_struct *work)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio;
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;
	struct byt_mc_private *ctx =
		 container_of(work, struct byt_mc_private,
		 hs_button_press_work.work);
	int status = 0, jack_type = 0;
	int ret;

	mutex_lock(&ctx->jack_mlock);
	pr_debug("Enter:%s\n", __func__);
	jack_type = jack->status;

	if (((jack->status & SND_JACK_HEADSET) == SND_JACK_HEADSET)
			&& ctx->process_button_events) {

		status = aic31xx_query_jack_status(codec);
		if (status) { /* confirm jack is connected */

			status = aic31xx_query_btn_press(codec);
			if (status & SND_JACK_BTN_0) {
				jack_type = SND_JACK_HEADSET | SND_JACK_BTN_0;
				pr_debug("%d Jack type sent is %d\n",
					 __LINE__, jack_type);
				snd_soc_jack_report(jack,
						 jack_type, gpio->report);
				/* Since there is not button_relese interrupt
				   schedule delayed work to poll for button
				   release status
				 */
				ret = schedule_delayed_work(
					&ctx->hs_button_release_work,
					msecs_to_jiffies(
						ctx->button_release_delay));
			}
		}
	}
	pr_debug("Exit:%s\n", __func__);
	mutex_unlock(&ctx->jack_mlock);
}

/* Check for button release */
static void byt_check_hs_button_release_status(struct work_struct *work)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio;
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;
	struct byt_mc_private *ctx = container_of(work, struct byt_mc_private,
			 hs_button_release_work.work);
	int status = 0, jack_type = 0;
	int ret;

	mutex_lock(&ctx->jack_mlock);
	pr_debug("Enter:%s\n", __func__);
	jack_type = jack->status;

	if (((jack->status & SND_JACK_HEADSET) == SND_JACK_HEADSET)
			&& ctx->process_button_events) {

		status = aic31xx_query_jack_status(codec);
		if (status) { /* confirm jack is connected */

			status = aic31xx_query_btn_press(codec);
			if (!(status & SND_JACK_BTN_0)) {
				jack_type = SND_JACK_HEADSET;
				pr_debug("%d Jack type sent is %d\n",
					__LINE__, jack_type);
				snd_soc_jack_report(jack, jack_type,
					 gpio->report);
			} else {
				/* Schedule again */
				ret = schedule_delayed_work(&ctx->hs_button_release_work,
					msecs_to_jiffies(
					ctx->button_release_delay));
			}

		}
	}

	pr_debug("Exit:%s\n", __func__);
	mutex_unlock(&ctx->jack_mlock);
}

/* Delayed work for enabling the overcurrent detection circuit and interrupt
   for generating button events */
static void byt_enable_hs_button_events(struct work_struct *work)
{
	struct snd_soc_jack_gpio *gpio = &hs_gpio;
	struct snd_soc_jack *jack = gpio->jack;
	struct snd_soc_codec *codec = jack->codec;
	struct byt_mc_private *ctx = container_of(work, struct byt_mc_private,
			 hs_button_en_work.work);

	aic31xx_btn_press_intr_enable(codec, ctx->process_button_events);
}

static inline struct snd_soc_codec *byt_get_codec(struct snd_soc_card *card)
{
	bool found = false;
	struct snd_soc_codec *codec;

	list_for_each_entry(codec, &card->codec_dev_list, card_list) {
		if (!strstr(codec->name, "tlv320aic31xx-codec")) {
			pr_debug("codec was %s", codec->name);
			continue;
		} else {
			found = true;
			break;
		}
	}
	if (found == false) {
		pr_err("%s: cant find codec", __func__);
		return NULL;
	}
	return codec;
}

static int platform_clock_control(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int  event)
{

	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_codec *codec;
	int ret = 0;
	codec = byt_get_codec(card);
	if (!codec) {
		pr_err("Codec not found; Unable to set platform clock\n");
		return -EIO;
	}
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		vlv2_plat_configure_clock(VLV2_PLAT_CLK_AUDIO,
				PLAT_CLK_FORCE_ON);
		pr_debug("Platform clk turned ON\n");
		snd_soc_codec_set_sysclk(codec, AIC31XX_MCLK,
				0, AIC31XX_FREQ_25000000, SND_SOC_CLOCK_IN);
		pr_debug("%d Jack_type detected = %d\n", __LINE__, ret);
	} else {
		/* Set codec clock source to internal clock before
		   turning off the platform clock. Codec needs clock
		   for Jack detection and button press */
		snd_soc_codec_set_sysclk(codec, AIC31XX_INTERNALCLOCK,
				0, 0, SND_SOC_CLOCK_IN);
		vlv2_plat_configure_clock(VLV2_PLAT_CLK_AUDIO,
				PLAT_CLK_FORCE_OFF);
		pr_debug("Platform clk turned OFF\n");
	}

	return 0;
}

/* machine DAPM */
static const struct snd_soc_dapm_widget byt_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Internal Mic", NULL),
	SND_SOC_DAPM_SUPPLY("Platform Clock", SND_SOC_NOPM, 0, 0,
			platform_clock_control, SND_SOC_DAPM_PRE_PMU|
			SND_SOC_DAPM_POST_PMD),
};
static const struct snd_kcontrol_new byt_mc_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Internal Mic"),
	SOC_DAPM_PIN_SWITCH("Ext Spk"),
	SOC_DAPM_PIN_SWITCH("Headphone"),
};
static const struct snd_soc_dapm_route byt_audio_map[] = {
	/* External Speakers: HFL, HFR */
	{"Ext Spk", NULL, "SPK"},
	{"micbias", NULL, "Internal Mic"},
	/* Headset Mic: Headset Mic with bias */
	{"micbias", NULL, "Headset Mic"},

	/* Headset Stereophone(Headphone): HSOL, HSOR */
	{"Headphone", NULL, "HPL"},
	{"Headphone", NULL, "HPR"},

	{"Headphone", NULL, "Platform Clock"},
	{"Headset Mic", NULL, "Platform Clock"},
	{"Internal Mic", NULL, "Platform Clock"},
	{"Ext Spk", NULL, "Platform Clock"},
};

static int byt_aif1_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int fmt;
	int ret;

	pr_debug("Enter:%s", __func__);
	/* I2S Slave Mode`*/
	fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBS_CFS;

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret < 0) {
		pr_err("can't set codec DAI configuration %d\n", ret);
		return ret;
	}
	ret = snd_soc_dai_set_pll(codec_dai, 0, AIC31XX_PLL_CLKIN_MCLK ,
			BYT_PLAT_CLK_3_HZ, /* params_rate(params) */
			params_rate(params));
	if (ret < 0) {
		pr_err("Can't set codec pll clock\n");
		return ret;
	}
	return 0;
}

static int byt_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
	case SND_SOC_BIAS_OFF:
		break;
	default:
		pr_err("%s: Invalid bias level=%d\n", __func__, level);
		return -EINVAL;
	}
	card->dapm.bias_level = level;
	pr_debug("card(%s)->bias_level %u\n", card->name,
			card->dapm.bias_level);
	return 0;
}

static int byt_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	struct snd_soc_codec *codec = runtime->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = runtime->card;
	struct byt_mc_private *ctx = snd_soc_card_get_drvdata(runtime->card);
	pr_debug("Enter:%s", __func__);
	/* Set codec bias level */
	byt_set_bias_level(card, dapm, SND_SOC_BIAS_OFF);
	card->dapm.idle_bias_off = true;

	/* Headset jack detection */
	ret = snd_soc_jack_new(codec, "Headset Jack",
			SND_JACK_HEADSET | SND_JACK_HEADPHONE | SND_JACK_BTN_0,
			 &ctx->jack);
	if (ret) {
		pr_err("Jack creation failed\n");
		return ret;
	}
	snd_jack_set_key(ctx->jack.jack, SND_JACK_BTN_0, KEY_MEDIA);

	ret = snd_soc_jack_add_gpios(&ctx->jack, 1, &hs_gpio);
	if (ret) {
		pr_err("Adding jack GPIO failed with error %d\n", ret);
		return ret;
	}
	ret = snd_soc_add_card_controls(card, byt_mc_controls,
					ARRAY_SIZE(byt_mc_controls));
	if (ret) {
		pr_err("unable to add card controls\n");
		return ret;
	}
	ret = snd_soc_dapm_sync(dapm);
	if (ret) {
		pr_err("unable to sync dapm\n");
		return ret;
	}
	return ret;
}

static unsigned int rates_48000[] = {
	48000,
};

static struct snd_pcm_hw_constraint_list constraints_48000 = {
	.count = ARRAY_SIZE(rates_48000),
	.list  = rates_48000,
};

static int byt_aif1_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&constraints_48000);
}

static struct snd_soc_ops byt_aif1_ops = {
	.startup = byt_aif1_startup,
	.hw_params = byt_aif1_hw_params,
};

static struct snd_soc_dai_link byt_dailink[] = {
	[BYT_AUD_AIF1] = {
		.name = "Baytrail Audio",
		.stream_name = "Audio",
		.cpu_dai_name = "Headset-cpu-dai",
		.codec_dai_name = "tlv320aic31xx-codec",
		.codec_name = "tlv320aic31xx-codec.2-0018",
		.platform_name = "sst-platform",
		.init = byt_init,
		.ignore_suspend = 1,
		.ops = &byt_aif1_ops,
		.playback_count = 2,
	},
};

#ifdef CONFIG_PM_SLEEP
static int snd_byt_prepare(struct device *dev)
{
	pr_debug("In %s device name\n", __func__);
	return snd_soc_suspend(dev);
}

static void snd_byt_complete(struct device *dev)
{
	pr_debug("In %s\n", __func__);
	snd_soc_resume(dev);
}

static int snd_byt_poweroff(struct device *dev)
{
	pr_debug("In %s\n", __func__);
	return snd_soc_poweroff(dev);
}
#else
#define snd_byt_prepare NULL
#define snd_byt_complete NULL
#define snd_byt_poweroff NULL
#endif

/* SoC card */
static struct snd_soc_card snd_soc_card_byt = {
	.name = "baytrailaudio",
	.dai_link = byt_dailink,
	.num_links = ARRAY_SIZE(byt_dailink),
	.set_bias_level = byt_set_bias_level,
	.dapm_widgets = byt_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(byt_dapm_widgets),
	.dapm_routes = byt_audio_map,
	.num_dapm_routes = ARRAY_SIZE(byt_audio_map),
};

static int snd_byt_mc_probe(struct platform_device *pdev)
{
	int ret_val = 0;
	struct byt_mc_private *drv;
	int codec_gpio;

	pr_debug("Entry %s\n", __func__);

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_ATOMIC);
	if (!drv) {
		pr_err("allocation failed\n");
		return -ENOMEM;
	}
	/* Reset pin for codec is GPIOS_21 */
	/* Codec jack detect direct line GPIOS_27 */
	/* PWM Vibrator pin is GPIOC_95 */
	/* GPIO_SUS4 */
	codec_gpio = acpi_get_gpio_by_index(&pdev->dev, 0, NULL);
	pr_debug("%s: GPIOs - codec %d", __func__, codec_gpio);
	hs_gpio.gpio = codec_gpio;

	drv->intr_debounce = BYT_INTR_DEBOUNCE;
	drv->hs_insert_det_delay = BYT_HS_INSERT_DET_DELAY;
	drv->hs_remove_det_delay = BYT_HS_REMOVE_DET_DELAY;
	drv->button_press_delay = BYT_BUTTON_PRESS_DELAY;
	drv->button_release_delay = BYT_BUTTON_RELEASE_DELAY;
	drv->hs_det_poll_intrvl = BYT_HS_DET_POLL_INTRVL;
	drv->hs_det_retry = BYT_HS_DET_RETRY_COUNT;
	drv->button_en_delay = BYT_BUTTON_EN_DELAY;
	drv->process_button_events = false;

	INIT_DELAYED_WORK(&drv->hs_insert_work, byt_check_hs_insert_status);
	INIT_DELAYED_WORK(&drv->hs_remove_work, byt_check_hs_remove_status);
	INIT_DELAYED_WORK(&drv->hs_button_press_work,
			 byt_check_hs_button_press_status);
	INIT_DELAYED_WORK(&drv->hs_button_release_work,
			 byt_check_hs_button_release_status);
	INIT_DELAYED_WORK(&drv->hs_button_en_work, byt_enable_hs_button_events);
	mutex_init(&drv->jack_mlock);

	/* register the soc card */
	snd_soc_card_byt.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&snd_soc_card_byt, drv);

	ret_val = snd_soc_register_card(&snd_soc_card_byt);
	if (ret_val) {
		pr_err("snd_soc_register_card failed %d\n", ret_val);
		return ret_val;
	}
	platform_set_drvdata(pdev, &snd_soc_card_byt);
	pr_info("%s successful\n", __func__);
	return ret_val;
}

static void snd_byt_unregister_jack(struct byt_mc_private *ctx)
{
	/* Set process button events to false so that the button
	   delayed work will not be scheduled.*/
	ctx->process_button_events = false;
	cancel_delayed_work_sync(&ctx->hs_insert_work);
	cancel_delayed_work_sync(&ctx->hs_button_en_work);
	cancel_delayed_work_sync(&ctx->hs_button_press_work);
	cancel_delayed_work_sync(&ctx->hs_button_release_work);
	cancel_delayed_work_sync(&ctx->hs_remove_work);
	snd_soc_jack_free_gpios(&ctx->jack, 1, &hs_gpio);
}

static int snd_byt_mc_remove(struct platform_device *pdev)
{
	struct snd_soc_card *soc_card = platform_get_drvdata(pdev);
	struct byt_mc_private *drv = snd_soc_card_get_drvdata(soc_card);

	pr_debug("In %s\n", __func__);

	snd_byt_unregister_jack(drv);
	snd_soc_card_set_drvdata(soc_card, NULL);
	snd_soc_unregister_card(soc_card);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void snd_byt_mc_shutdown(struct platform_device *pdev)
{
	struct snd_soc_card *soc_card = platform_get_drvdata(pdev);
	struct byt_mc_private *drv = snd_soc_card_get_drvdata(soc_card);

	pr_debug("In %s\n", __func__);
	snd_soc_jack_free_gpios(&drv->jack, 1, &hs_gpio);
}

static const struct dev_pm_ops snd_byt_mc_pm_ops = {
	.prepare = snd_byt_prepare,
	.complete = snd_byt_complete,
	.poweroff = snd_byt_poweroff,
};

static const struct acpi_device_id byt_mc_acpi_ids[] = {
	{ "TIMC0F28", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, byt_mc_acpi_ids);

static struct platform_driver snd_byt_mc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "byt_aic31xx",
		.pm = &snd_byt_mc_pm_ops,
		.acpi_match_table = ACPI_PTR(byt_mc_acpi_ids),
	},
	.probe = snd_byt_mc_probe,
	.remove = snd_byt_mc_remove,
	.shutdown = snd_byt_mc_shutdown,
};

static int __init snd_byt_driver_init(void)
{
	int ret;
	ret = platform_driver_register(&snd_byt_mc_driver);
	if (ret)
		pr_err("Fail to register Baytrail Machine driver byt_aic31xx\n");
	else
		pr_info("Baytrail Machine Driver byt_aic31xx registerd\n");
	return ret;
}
late_initcall(snd_byt_driver_init);

static void __exit snd_byt_driver_exit(void)
{
	pr_debug("In %s\n", __func__);
	platform_driver_unregister(&snd_byt_mc_driver);
}
module_exit(snd_byt_driver_exit);

MODULE_DESCRIPTION("ASoC Intel(R) Baytrail CR Machine driver");
MODULE_AUTHOR("Hardik T Shah <hardik.t.shah@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:bytaic31xx-audio");
