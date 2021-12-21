/*
 * platform indepent driver interface
 * Copyright (C) 2016 Goodix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>

#include "gf_spi.h"

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif

static int pinctrl_register_time = 0;

static int select_pin_ctl(struct gf_dev *gf_dev, const char *name)
{
	size_t i;
	int rc;
	struct device *dev = &gf_dev->spi->dev;

	for (i = 0; i < ARRAY_SIZE(pctl_names); i++) {
		const char *n = pctl_names[i];

		if (!memcmp(n, name, strlen(n))) {
			rc = pinctrl_select_state(gf_dev->fp_pinctrl,
					gf_dev->pinctrl_state[i]);
			if (rc)
				GF_LOGE("cannot select '%s'\n", name);
			else
				GF_LOGI("Selected '%s'\n", name);
			goto exit;
		}
	}

	rc = -EINVAL;
	dev_err(dev, "%s:'%s' not found\n", __func__, name);

exit:
	return rc;
}

int gf_parse_dts(struct gf_dev *gf_dev)
{
	int rc = 0;
	int i = 0;
	struct device *dev = &gf_dev->spi->dev;
	struct device_node *np = dev->of_node;

	gf_dev->fp_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(gf_dev->fp_pinctrl)) {
		GF_LOGE("target does not use pinctrl\n");
		gf_dev->fp_pinctrl = NULL;
		goto err_reset;
	}else
	{
		pinctrl_register_time = 1;
		for (i = 0; i < ARRAY_SIZE(pctl_names); i++) {
			const char *n = pctl_names[i];
			struct pinctrl_state *state =
					pinctrl_lookup_state(gf_dev->fp_pinctrl, n);
			if (IS_ERR(state)) {
				GF_LOGE("can not find %s pinctrl.\n ", n);
			}
			GF_LOGD("found %s pinctrl.\n ", n);
			gf_dev->pinctrl_state[i] = state;
		}
	}

	gf_dev->reset_gpio = of_get_named_gpio(np, "goodix,gpio_reset", 0);
	if (gf_dev->reset_gpio < 0) {
		GF_LOGE("falied to get reset gpio!\n");
		return gf_dev->reset_gpio;
	}
	GF_LOGD("gf:reset_gpio:%d\n", gf_dev->reset_gpio);

        // [TCTNB][FINGERPRINT]MODIFY by Jiancong.Huang for pr-11084804 at 2021/5/12
	rc = select_pin_ctl(gf_dev, "gf_reset_off");
	if (rc) {
		GF_LOGE("failed to select_pin_ctl gf_reset_on, rc = %d\n", rc);
		goto err_irq;
	}

	gf_dev->irq_gpio = of_get_named_gpio(np, "goodix,gpio_irq", 0);
	if (gf_dev->irq_gpio < 0) {
		GF_LOGE("falied to get irq gpio!\n");
		return gf_dev->irq_gpio;
	}
	GF_LOGD("gf:irq_gpio:%d\n", gf_dev->irq_gpio);

	rc = devm_gpio_request(dev, gf_dev->irq_gpio, "goodix_irq");
	if (rc) {
		GF_LOGE("failed to request irq gpio, rc = %d\n", rc);
		goto err_irq;
	}
	gpio_direction_input(gf_dev->irq_gpio);

    /*get pwr resource*/
    gf_dev->pwr_gpio = of_get_named_gpio(np,"goodix,gpio_pwr",0);
    if(gf_dev->pwr_gpio < 0) {
        GF_LOGE("falied to get pwr gpio!\n");
        return gf_dev->pwr_gpio;
    }
    GF_LOGD("gf:pwr_gpio:%d\n", gf_dev->pwr_gpio);

    rc = select_pin_ctl(gf_dev, "gf_pwr_off");
	if (rc) {
		GF_LOGE("failed to select_pin_ctl gf_pwr_off, rc = %d\n", rc);
		goto err_irq;
	}


	gf_dev->dev_node = np;

err_irq:
	devm_gpio_free(dev, gf_dev->irq_gpio);

err_reset:
	return rc;
}

void gf_cleanup(struct gf_dev *gf_dev)
{
    /* MODIFIED by jiancong.huang, 2020-12-01,FR-9919612*/
	GF_LOGI("[info] %s\n", __func__);

	if (pinctrl_register_time == 1) {
		devm_pinctrl_put(gf_dev->fp_pinctrl);
		GF_LOGI("remove pwr_gpio success\n");
		pinctrl_register_time = 0;
	}

	/* MODIFIED-END by jiancong.huang, 2020-12-01,FR-9919612*/
}

int gf_power_on(struct gf_dev *gf_dev)
{
	int rc = 0;

	/* TODO: add your power control here */
	if (gpio_is_valid(gf_dev->pwr_gpio) && pinctrl_register_time == 1) {
		//gpio_set__value(gf_dev->pwr_gpio, 1);
		rc = select_pin_ctl(gf_dev, "gf_pwr_on");
	}
	msleep(10);
	GF_LOGI("---- power on ok ----\n");

	return rc;
}

int gf_power_off(struct gf_dev *gf_dev)
{
	int rc = 0;

	/* TODO: add your power control here */
	if (gpio_is_valid(gf_dev->pwr_gpio) && pinctrl_register_time == 1) {
		rc = select_pin_ctl(gf_dev, "gf_pwr_off");
		rc = select_pin_ctl(gf_dev, "gf_reset_off");

	}
	GF_LOGI("---- power off ----\n");

	return rc;
}

int gf_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms)
{
	int rc = 0;
	if (!gf_dev) {
		GF_LOGE("Input buff is NULL.\n");
		return -ENODEV;
	}
	rc = select_pin_ctl(gf_dev, "gf_reset_on");
    mdelay(10); // MODIFIED by ji.chen, 2020-03-28,BUG-9030639
	rc = select_pin_ctl(gf_dev, "gf_reset_off");
	mdelay(3);
	rc = select_pin_ctl(gf_dev, "gf_reset_on");
	mdelay(delay_ms);
	return rc;
}

int gf_irq_num(struct gf_dev *gf_dev)
{
	if (!gf_dev) {
		GF_LOGE("Input buff is NULL.\n");
		return -ENODEV;
	} else {
		return gpio_to_irq(gf_dev->irq_gpio);
	}
}
