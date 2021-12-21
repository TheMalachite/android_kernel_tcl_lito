#define DEBUG
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include "dsi_display.h"
#include "dsi_panel.h"
#if defined(CONFIG_PXLW_IRIS6)
#include "iris/dsi_iris6_api.h"
#endif

struct class *tct_display_class = NULL;
static struct device *tct_display_dev = NULL;

static ssize_t sysfs_hbm_dimming_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsi_display *display = get_main_display();

	if (display == NULL || display->panel == NULL) {
		pr_err("Invalid display or panel\n");
		return 0;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", display->panel->hbm_dimming_level);
}

static ssize_t sysfs_hbm_dimming_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel;
	struct mipi_dsi_device *dsi;
	u16 brightness, bl_lvl;

	if (display == NULL || display->panel == NULL) {
		pr_err("Invalid display or panel\n");
		return count;
	}

	panel = display->panel;
	dsi = &panel->mipi_device;

	mutex_lock(&panel->panel_lock);
	if (!dsi_panel_initialized(panel)) {
		pr_err("Panel not initialized\n");
		goto end;
	}

	kstrtou16(buf, 0, &brightness);

	if (panel->hbm_enabled)
		goto end;

	if (brightness > 4095) {
		pr_err("hbm dimming error value:%d", brightness);
		goto end;
	}

	panel->hbm_dimming_level = brightness;

	if (brightness) {
		if (brightness >= 1 && brightness <= 11)
			bl_lvl = brightness;
		if (brightness > 11 && brightness <= 30)
			bl_lvl = brightness/2 + 6;
		else if (brightness <= 2047)
			bl_lvl = brightness*835/1000 - 4;
		else
			bl_lvl = brightness*167/1000 + 1364;

		if (bl_lvl > 2047)
			bl_lvl = 2047;

		if (bl_lvl < panel->bl_config.bl_level)
			bl_lvl = panel->bl_config.bl_level;

		pr_debug("%s: brightness:%d, bl_lvl:%d", __func__, brightness, bl_lvl);
		iris_update_backlight(bl_lvl);
	} else {
		dsi_panel_set_backlight(panel, panel->bl_config.bl_level);
	}

end:
	mutex_unlock(&panel->panel_lock);
	return count;
}

static ssize_t sysfs_panel_set_lp1_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct dsi_display *display = get_main_display();
	struct dsi_panel *panel;
	struct mipi_dsi_device *dsi;
	u16 lp_mode;

	if (display == NULL || display->panel == NULL) {
		pr_err("Invalid display or panel\n");
		return count;
	}

	panel = display->panel;
	dsi = &panel->mipi_device;

	mutex_lock(&panel->panel_lock);
	if (!dsi_panel_initialized(panel)) {
		pr_err("Panel not initialized\n");
		goto end;
	}

	kstrtou16(buf, 0, &lp_mode);

	if (lp_mode) {
		dsi_panel_set_lp1(panel);
		iris_update_backlight(2047);
	} else {
		dsi_panel_set_backlight(panel, panel->bl_config.bl_level);
		dsi_panel_set_nolp(panel);
	}

end:
	mutex_unlock(&panel->panel_lock);
	return count;
}

static DEVICE_ATTR(hbm_dimming, 0664, sysfs_hbm_dimming_read, sysfs_hbm_dimming_write);
static DEVICE_ATTR(lp1_enable, 0664, NULL, sysfs_panel_set_lp1_write);

static int __init tct_display_api_init(void)
{
	int error = 0;

    pr_info("%s", __func__);

	if (tct_display_class == NULL)
		tct_display_class = class_create(THIS_MODULE, "tct_display");

	if (tct_display_class) {
		tct_display_dev = device_create(tct_display_class, NULL, 0, NULL, "tct_display_dev");
		if (tct_display_dev) {
			error = device_create_file(tct_display_dev, &dev_attr_hbm_dimming);
			if (error < 0)
                pr_err("Failed to create file hbm_dimming!");
			error = device_create_file(tct_display_dev, &dev_attr_lp1_enable);
			if (error < 0)
                pr_err("Failed to create file lp1_enable!");
		} else
            pr_err("Failed to create tct_touch_dev!");
	}
	else
        pr_err("Failed to create tct_display_class!");

	return 0;
}
/*----------------------------------------------------------------------------*/

static void __exit tct_display_api_exit(void) {
	 pr_info("%s", __func__);
}

module_init(tct_display_api_init);
module_exit(tct_display_api_exit);
MODULE_AUTHOR("haojun.chen");
MODULE_DESCRIPTION("tct_display api");
MODULE_LICENSE("GPL");