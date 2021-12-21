/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define PTN_DRIVER_NAME                     "ptn36502"


struct i2c_client *ptn_client;
static u32 aux_switch_gpio;

static int ptn_write_reg(u8 reg, u8 val)
{
	int ret=0;

	ret = i2c_smbus_write_byte_data(ptn_client, reg, val);

	if (ret >= 0)
		return 0;

	pr_info("%s: reg 0x%x, val 0x%x, err %d\n", __func__, reg, val, ret);

	return ret;
}

static int g_orientation = 0;

void ptn_orientation_switch(int orientation)
{
	pr_info("%s: orientation=%d\n", __func__, orientation);
	if (orientation == 0) {
		ptn_write_reg(0x0b, 0x01);
	} else if (orientation == 1) {
		//ptn_write_reg(0x0b, 0x01);
		gpio_direction_output(aux_switch_gpio, 0);
	} else if (orientation == 2) {
		//ptn_write_reg(0x0b, 0x21);
		gpio_direction_output(aux_switch_gpio, 1);
	}

	g_orientation = orientation;
}
EXPORT_SYMBOL(ptn_orientation_switch);

void ptn_usb_orientation_switch(int orientation)
{
	pr_info("%s: orientation=%d\n", __func__, orientation);

	if (orientation == 1)
		ptn_write_reg(0x0b, 0x01);
	else
		ptn_write_reg(0x0b, 0x21);
}
EXPORT_SYMBOL(ptn_usb_orientation_switch);

void ptn_lane_switch(int lane, int bwcode)
{
	pr_info("%s: lane=%d, orientation=%d bwcode=%d\n", __func__, lane, g_orientation, bwcode);
	if (lane == 2 && g_orientation == 1) {
		ptn_write_reg(0x0d, 0x80);
		ptn_write_reg(0x0b, 0x0a);
		ptn_write_reg(0x06, 0x0a);
		// if (bwcode == 10) {
		// 	ptn_write_reg(0x06, 0x09);
		// } else {
		// 	ptn_write_reg(0x06, 0x0a);
		// }
		ptn_write_reg(0x07, 0x29);
		ptn_write_reg(0x08, 0x29);
		ptn_write_reg(0x09, 0x29);
		ptn_write_reg(0x0A, 0x29);
	} else if (lane == 2 && g_orientation == 2) {
		ptn_write_reg(0x0d, 0x80);
		ptn_write_reg(0x0b, 0x2a);
		ptn_write_reg(0x06, 0x0a);
		// if (bwcode == 10) {
		// 	ptn_write_reg(0x06, 0x09);
		// } else {
		// 	ptn_write_reg(0x06, 0x0a);
		// }
		ptn_write_reg(0x07, 0x29);
		ptn_write_reg(0x08, 0x29);
		ptn_write_reg(0x09, 0x29);
		ptn_write_reg(0x0A, 0x29);
	} else if (lane == 4 && g_orientation == 1) {
		ptn_write_reg(0x0d, 0x80);
		ptn_write_reg(0x0b, 0x0b);
		ptn_write_reg(0x06, 0x0e);
		ptn_write_reg(0x07, 0x29);
		ptn_write_reg(0x08, 0x29);
		ptn_write_reg(0x09, 0x29);
		ptn_write_reg(0x0A, 0x29);
	} else if (lane == 4 && g_orientation == 2) {
		ptn_write_reg(0x0d, 0x80);
		ptn_write_reg(0x0b, 0x2b);
		ptn_write_reg(0x06, 0x0e);
		ptn_write_reg(0x07, 0x29);
		ptn_write_reg(0x08, 0x29);
		ptn_write_reg(0x09, 0x29);
		ptn_write_reg(0x0A, 0x29);
	}
}
EXPORT_SYMBOL(ptn_lane_switch);
#if 0
static struct pinctrl		*pinctrl;
static struct pinctrl_state	*pin_default;

static int ptn_pinctrl_init(struct i2c_client *client)
{
	pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_err(&client->dev, "Failed to get pinctrl\n");
		return PTR_ERR(pinctrl);
	}
	pin_default = pinctrl_lookup_state(pinctrl, "default");
	if (IS_ERR_OR_NULL(pin_default)) {
		dev_err(&client->dev, "Failed to look up default state\n");
		return PTR_ERR(pin_default);
	}
	return 0;
}
#endif

static int ptn36502_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int rc = 0;
	struct device_node *np = NULL;

    pr_info("ptn36502 driver prboe...\n");
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("I2C not supported");
        return -ENODEV;
    }

    ptn_client = client;
	np = client->dev.of_node;
	if (np)
		aux_switch_gpio = of_get_named_gpio_flags(np, "ptn,aux-switch-gpio", 0, NULL);
	else
		pr_err("%s: of_node is null");

	if (gpio_is_valid(aux_switch_gpio)) {
        rc = gpio_request(aux_switch_gpio, "aux_switch_gpio");
        if (rc) {
            pr_err("%s: aux switch gpio request failed", __func__);
        }
    }
	ptn_write_reg(0x0b, 0x21);
	// ptn_write_reg(0x07, 0x29);
	// ptn_write_reg(0x08, 0x29);
	// ptn_write_reg(0x09, 0x29);
	// ptn_write_reg(0x0A, 0x29);
	//ptn_pinctrl_init(client);
	//ptn_state_switch(2, 1);

	// if (!ptn_pinctrl_init(client)) {
	// 	rc = pinctrl_select_state(pinctrl, pin_default);
	// 	if (rc) {
	// 		pr_err("%s: Can't select pinctrl state\n", __func__);
	// 	}
	// }

    return 0;
}

static int ptn36502_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id ptn36502_id[] = {
    {PTN_DRIVER_NAME, 0},
    {},
};
static const struct of_device_id ptn36502_dt_match[] = {
    {.compatible = "ptn36502,ptn", },
    {},
};
MODULE_DEVICE_TABLE(of, ptn36502_dt_match);

static struct i2c_driver ptn36502_driver = {
    .probe = ptn36502_probe,
    .remove = ptn36502_remove,
    .driver = {
        .name = PTN_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(ptn36502_dt_match),
    },
    .id_table = ptn36502_id,
};

static int __init ptn36502_init(void)
{
    int ret = 0;

    ret = i2c_add_driver(&ptn36502_driver);
    if ( ret != 0 ) {
        pr_err("ptn36502 driver init failed!");
    }
    return ret;
}

static void __exit ptn36502_exit(void)
{
    i2c_del_driver(&ptn36502_driver);
}
module_init(ptn36502_init);
module_exit(ptn36502_exit);

MODULE_AUTHOR("Haojun.chen@tcl.com");
MODULE_DESCRIPTION("PTN36502 Driver");
MODULE_LICENSE("GPL v2");
