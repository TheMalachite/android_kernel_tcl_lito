/* Copyright (C) 2016 Tcl Corporation Limited */
#ifdef CONFIG_SND_SOC_TFA98XX_MMI_TEST

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/debugfs.h>

#include <sound/tfa_mmi_test.h>

int (*tfa_ext_sclk_enable)(int) = NULL;


void msm_set_enable_clk_cb(int (*tfa_ext_sclk)(int enable))
{
	tfa_ext_sclk_enable = tfa_ext_sclk;
}
EXPORT_SYMBOL_GPL(msm_set_enable_clk_cb);

/*------------------------------------------------------ sys fs control node----------------------------------------------*/

/* MODIFIED-BEGIN by hongwei.tian, 2019-05-17,BUG-7782517*/
static ssize_t ftm_cal_bitclk_store(struct class *class, struct class_attribute *attr,
			const char *buf, size_t count)
{
	pr_err("%s: enter\n", __func__);
	if(!tfa_ext_sclk_enable)
		return count; 

	pr_err("%s: ftm_cal_bitclk %s\n", __func__,buf);

	if(!strncmp(buf, "enable", 6)) {
		tfa_ext_sclk_enable(1);
	}else
		tfa_ext_sclk_enable(0);

	return count;
}
/* MODIFIED-END by hongwei.tian,BUG-7782517*/



static struct class_attribute ftm_cal_bitclk =
	__ATTR(enable_bitclk, 0664, NULL, ftm_cal_bitclk_store);
	/* MODIFIED-END by hongwei.tian,BUG-7782517*/


void tfa98xx_sysfs_init(void)
{
	struct class *ftm_cal_class;

	/* ftm_cal_class create (/<sysfs>/class/ftm_cal) */
	ftm_cal_class = class_create(THIS_MODULE, "ftm_cal");
	if (IS_ERR(ftm_cal_class)) {
		pr_err("%s: ftm_cal_class: couldn't create class ftm_cal_class\n", __func__);
	}
	if (class_create_file(ftm_cal_class, &ftm_cal_bitclk)) {

		pr_err("%s: ftm_cal_class: couldn't create sub file node\n", __func__);
	}
}
#endif

