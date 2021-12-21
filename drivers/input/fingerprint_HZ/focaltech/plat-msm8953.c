/* Copyright (C) 2019 Tcl Corporation Limited */
/**
 * plat-msm8916.c
 *
**/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
//#include <linux/regulator/consumer.h>


#include "ff_log.h"
#include "ff_ctl.h"

# undef LOG_TAG
#define LOG_TAG "msm8953"

#define FF_COMPATIBLE_NODE "focaltech,fingerprint"

/*
 * Driver configuration. See ff_ctl.c
 */

//modefied by weiqiang.zhou begin
typedef struct {
   int32_t gpio_rst_pin;
   int32_t gpio_int_pin;
   int32_t gpio_power_pin;
   int32_t gpio_iovcc_pin;
} ff_plat_context_t;

static ff_plat_context_t ff_plat_context = {
   .gpio_rst_pin = -1,
   .gpio_int_pin = -1,
   .gpio_power_pin = -1,
   .gpio_iovcc_pin = -1,
}, *g_context = &ff_plat_context;
//extern ff_driver_config_t *g_config;

//modefied by weiqiang.zhou end
//static struct regulator *focal_vdd = NULL;

/* BUGFIX-MODIFIED-BEGIN by TCTNB.ji.chen,2020-03-26,PR-9046421*/
#if defined CONFIG_TCT_DEVICEINFO
extern char fp_module_name[256];
#endif
/* BUGFIX-MODIFIED-End by TCTNB.ji.chen*/
int ff_ctl_init_pins(int *irq_num)
{
    int err = 0, gpio;
    struct device_node *dev_node = NULL;
    enum of_gpio_flags flags;
    //struct platform_device *pdev = NULL;
    FF_LOGV("'%s' enter.", __func__);

    if (unlikely(!g_context)) {
        return (-ENOSYS);
    }

    /* Find device tree node. */
    dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE);
    if (!dev_node) {
        FF_LOGE("of_find_compatible_node(.., '%s') failed.", FF_COMPATIBLE_NODE);
        return (-ENODEV);
    }

	/* Initialize RST pin. */
    gpio = of_get_named_gpio_flags(dev_node, "fp-gpio-reset", 0, &flags);
	FF_LOGI("reset_gpio = %d", gpio);
    if (gpio > 0) {
        g_context->gpio_rst_pin = gpio;
    }
    if (!gpio_is_valid(g_context->gpio_rst_pin)) {
        FF_LOGE("g_context->gpio_rst_pin(%d) is invalid.", g_context->gpio_rst_pin);
        return (-ENODEV);
    }
    err = gpio_request(g_context->gpio_rst_pin, "ff_gpio_rst_pin");
    if (err) {
        FF_LOGE("gpio_request(%d) = %d.", g_context->gpio_rst_pin, err);
        return err;
    }
    err = gpio_direction_output(g_context->gpio_rst_pin, 0); //MOD-BEGIN by TCTNB.Ji.Chen,2019/12/03
    if (err) {
        FF_LOGE("gpio_direction_output(%d, 1) = %d.", g_context->gpio_rst_pin, err);
        return err;
    }
    gpio = of_get_named_gpio_flags(dev_node, "fp-gpio-pwr", 0, &flags);
    FF_LOGI("pwr_gpio = %d", gpio);
    if (gpio > 0) {
        g_context->gpio_power_pin = gpio;
    }
    if (!gpio_is_valid(g_context->gpio_power_pin)) {
        FF_LOGE("g_context->gpio_power_pin(%d) is invalid.", g_context->gpio_power_pin);
        return (-ENODEV);
    }
    err = gpio_request(g_context->gpio_power_pin, "ff_gpio_power_pin");
    if (err) {
        FF_LOGE("gpio_request(%d) = %d.", g_context->gpio_power_pin, err);
        return err;
    }
    err = gpio_direction_output(g_context->gpio_power_pin, 0); // power off.
    if (err) {
        FF_LOGE("gpio_direction_output(%d, 0) = %d.", g_context->gpio_power_pin, err);
        return err;
    }

#if 0
    /* Initialize IOVCC pin. */
    gpio = of_get_named_gpio_flags(dev_node, "fp,iovcc_gpio", 0, &flags);
    if (gpio > 0) {
        g_context->gpio_iovcc_pin = gpio;
    }
    if (!gpio_is_valid(g_context->gpio_iovcc_pin)) {
        FF_LOGE("g_context->gpio_iovcc_pin(%d) is invalid.", g_context->gpio_iovcc_pin);
        return (-ENODEV);
    }
    err = gpio_request(g_context->gpio_iovcc_pin, "ff_gpio_iovcc_pin");
    if (err) {
        FF_LOGE("gpio_request(%d) = %d.", g_context->gpio_iovcc_pin, err);
        return err;
    }
    err = gpio_direction_output(g_context->gpio_iovcc_pin, 0); // power off.
    if (err) {
        FF_LOGE("gpio_direction_output(%d, 0) = %d.", g_context->gpio_iovcc_pin, err);
        return err;
    }
#endif

    /* Initialize INT pin. */
    gpio = of_get_named_gpio_flags(dev_node, "fp-gpio-irq", 0, &flags);
	FF_LOGI("irq-gpio = %d", gpio);
    if (gpio > 0) {
        g_context->gpio_int_pin = gpio;
    }
    if (!gpio_is_valid(g_context->gpio_int_pin)) {
        FF_LOGE("g_context->gpio_int_pin(%d) is invalid.", g_context->gpio_int_pin);
        return (-ENODEV);
    }
    err = gpio_request(g_context->gpio_int_pin, "ff_gpio_int_pin");
    if (err) {
        FF_LOGE("gpio_request(%d) = %d.", g_context->gpio_int_pin, err);
        return err;
    }
    err = gpio_direction_input(g_context->gpio_int_pin);
    if (err) {
        FF_LOGE("gpio_direction_input(%d) = %d.", g_context->gpio_int_pin, err);
        return err;
    }
    /* Retrieve the IRQ number. */
    *irq_num = gpio_to_irq(g_context->gpio_int_pin);
    if (*irq_num < 0) {
        FF_LOGE("gpio_to_irq(%d) failed.", g_context->gpio_int_pin);
        return (-EIO);
    } else {
        FF_LOGD("gpio_to_irq(%d) = %d.", g_context->gpio_int_pin, *irq_num);
    }

	/* Initialize PMU mode. */
	//pdev = of_find_device_by_node(dev_node);
    //if (!pdev) {
        //FF_LOGE("of_find_device_by_node(..) failed.");
        //return (-ENODEV);
    //}

    //focal_vdd = regulator_get(&pdev->dev, "vdd");
/* BUGFIX-MODIFIED-BEGIN by TCTNB.ji.chen,2020-03-26,PR-9046421*/
#if defined CONFIG_TCT_DEVICEINFO
    sprintf(fp_module_name,"FT9362:FOCAL:A-KERR");
#endif
/* BUGFIX-MODIFIED-End by TCTNB.ji.chen*/
    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_ctl_free_pins(void)
{
    int err = 0;
    FF_LOGV("'%s' enter.", __func__);
//weiqiang modefied
    /* Release GPIO resources. */
    gpio_free(g_context->gpio_rst_pin);
    gpio_free(g_context->gpio_rst_pin);
    gpio_free(g_context->gpio_power_pin);
    FF_LOGV("'%s' weiqiang.zhou temp.", __func__);
    g_context->gpio_rst_pin = -1;
    g_context->gpio_rst_pin = -1;
    g_context->gpio_power_pin = -1;
	//regulator_put(focal_vdd);

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_ctl_enable_spiclk(bool on)
{
    int err = 0;
    FF_LOGV("'%s' enter.", __func__);
    FF_LOGD("clock: '%s'.", on ? "enable" : "disabled");

    if (on) {
        // TODO:
    } else {
        // TODO:
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_ctl_enable_power(bool on)
{
    int err = 0;
    FF_LOGV("'%s' enter.", __func__);
    FF_LOGD("power: '%s'.", on ? "on" : "off");

    if (unlikely(!g_context)) {
        return (-ENOSYS);
    }

    if (on) {
		//regulator_set_voltage(focal_vdd, 0, 3300000);
        //err = regulator_enable(focal_vdd);
		err = gpio_direction_output(g_context->gpio_power_pin, 1);
        msleep(5);
        //err = gpio_direction_output(g_context->gpio_iovcc_pin, 1);

    } else {
        //regulator_set_voltage(focal_vdd, 0, 0);
        //err = regulator_disable(focal_vdd);
		//err = gpio_direction_output(g_context->gpio_iovcc_pin, 0);
        err = gpio_direction_output(g_context->gpio_power_pin, 0);
	    msleep(5);
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_ctl_reset_device(void)
{
    int err = 0;
    FF_LOGV("'%s' enter.", __func__);

    if (unlikely(!g_context)) {
        return (-ENOSYS);
    }

    /* 3-1: Pull down RST pin. */
    err = gpio_direction_output(g_context->gpio_rst_pin, 0);

    /* 3-2: Delay for 5ms. */
    mdelay(5); //MOD-BEGIN by TCTNB.Ji.Chen,2019/12/03

    /* Pull up RST pin. */
    err = gpio_direction_output(g_context->gpio_rst_pin, 1);

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

const char *ff_ctl_arch_str(void)
{
    return "msm8953";
}

