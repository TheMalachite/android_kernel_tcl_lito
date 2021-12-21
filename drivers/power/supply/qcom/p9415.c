// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/power_supply.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/pm_wakeup.h>
#include <linux/pmic-voter.h>
#include <linux/firmware.h>
#include <linux/atomic.h>
#include "p9415.h"

#define P9415_15W_TYPE (0x01)

#define DC_AWAKE_VOTER	"DC_AWAKE_VOTER"
#define DCIN_OT_VOTER	"DCIN_OT_VOTER"
#define DCIN_ICL_VOTER		"DCIN_ICL_VOTER"
#define DCIN_FCC_VOTER		"DCIN_FCC_VOTER"
#define DCIN_SOC_VOTER 	"DCIN_SOC_VOTER"
#define DCIN_UV_VOTER 	"DCIN_UV_VOTER"
#define DCIN_STEP_VOTER 	"DCIN_STEP_VOTER"

#define DCIN_QUIET_TEMP_LIMIT_LVL1	(440)
#define DCIN_QUIET_TEMP_LIMIT_LVL2	(370)
#define DCIN_QUIET_TEMP_LIMIT_LVL3	(350)
#define DCIN_QUIET_LVL1_FCC_BPP_UA	(600000)
#define DCIN_QUIET_LVL2_FCC_BPP_UA	(800000)

#define DCIN_QUIET_LVL1_FCC_EPP_UA	(1500000)
#define DCIN_QUIET_LVL2_FCC_EPP_UA	(2000000)

#define DCIN_ICL_SOC_MIN_UA	(300000)

#define DCIN_ICL_MIN_UA	(100000)

#define DCIN_EPP_VOUT_TAG_UV	(1000000)
#define DCIN_VALID_EPP_VOL_UV    (10000000)
#define DCIN_VALID_BPP_VOL_UV    (5000000)

#define DCIN_ALLOW_MIN_VOL_UV    (5000000)
#define DCIN_ALLOW_MAX_VOL_UV    (12000000)

#define DCIN_BPP_VOL_UV    (5500000)

#define DCIN_EPP_VOL_UV    (12000000)
#define DCIN_EPP_GAPS_UV    (500000)

#define DCIN_5W	(5)
#define DCIN_10W	(10)
#define DCIN_15W	(15)

#define DCIN_5W_WEAK_ICL_UA	(700000)
#define DCIN_5W_ICL_UA    (900000)
#define DCIN_5W_FCC_UA    (900000)

#define DCIN_7P5W_ICL_UA    (600000)
#define DCIN_7P5W_FCC_UA    (1500000)

#define DCIN_10W_ICL_UA    (800000)
#define DCIN_10W_FCC_UA    (2000000)

#define DCIN_15W_ICL_UA    (1100000)
#define DCIN_15W_FCC_UA    (2400000)

#define DCIN_5W_IOUT_UP_DELTA_UA	(50000)
#define DCIN_5W_IOUT_DOWN_DELTA_UA	(50000)

#define DCIN_10W_IOUT_UP_DELTA_UA	(50000)
#define DCIN_10W_IOUT_DOWN_DELTA_UA	(50000)

#define DCIN_15W_IOUT_UP_DELTA_UA	(50000)
#define DCIN_15W_IOUT_DOWN_DELTA_UA	(50000)

#define DCIN_VBAT_MIN	(4100000)
#define DCIN_VBAT_MAX	(4400000)
#define DCIN_VBAT_LVL1_DELTA	(50000)
#define DCIN_VBAT_LVL2_DELTA	(150000)
#define DCIN_ICL_OV_LVL1_UA	(300000)
#define DCIN_ICL_OV_LVL2_UA	(500000)
#define DCIN_ICL_OV_LVL3_UA	(700000)
#define DCIN_ICL_OV_COUNTER	(40)

#define DCIN_ICL_SOC_LVL1	(9000)
#define DCIN_ICL_SOC_LVL2	(7000)
#define DCIN_ICL_SOC_LVL1_UA	(500000)
#define DCIN_ICL_SOC_LVL2_UA	(800000)

#define DCIN_UP_STEP_UA	(100000)

#define DCIN_UV_DOWN_STEP_UA	(100000)
#define DCIN_UV_MIN_UA	(400000)

#define P9415_RX_FOD_MAX_REGS	(8)

#define P9415_FW2_BPP_VER (36788)
#define P9415_FW2_EPP_VER (11719)

#define MTP_FW1_BIN "p9415_fw1.bin"
#define MTP_FW2_BIN "p9415_fw2.bin"

#define p9415_dbg(reason, fmt, ...)				\
	do {								\
		if (debug_mask & (reason))			\
			pr_err_ratelimited("[p9415] %s: " fmt, \
				__func__, ##__VA_ARGS__);	\
		else							\
			pr_debug_ratelimited("[p9415] %s: " fmt, \
				__func__, ##__VA_ARGS__);	\
	} while (0)

enum {
	DBG_ISR	= BIT(0),
	DBG_REG	= BIT(1),
	DBG_ERR	= BIT(2),
	DBG_DBG	= BIT(3),
	DBG_MTP	= BIT(4),
};

static int debug_mask = DBG_ERR | DBG_MTP | DBG_ISR;
module_param_named(
	debug_mask, debug_mask,
	int, S_IRUSR | S_IWUSR
);

static int mtp_start = 0;
module_param_named(
	mtp_start, mtp_start,
	int, S_IRUSR | S_IWUSR
);

static int lvl1 = DCIN_QUIET_TEMP_LIMIT_LVL1;
module_param_named(
	lvl1, lvl1,
	int, S_IRUSR | S_IWUSR
);

static int lvl2 = DCIN_QUIET_TEMP_LIMIT_LVL2;
module_param_named(
	lvl2, lvl2,
	int, S_IRUSR | S_IWUSR
);

static int lvl3 = DCIN_QUIET_TEMP_LIMIT_LVL3;
module_param_named(
	lvl3, lvl3,
	int, S_IRUSR | S_IWUSR
);

static int confirm_time = 3000;
module_param_named(
	confirm_time, confirm_time,
	int, S_IRUSR | S_IWUSR
);

static int confirm_time2 = 3000;
module_param_named(
	confirm_time2, confirm_time2,
	int, S_IRUSR | S_IWUSR
);

static int force_fods = 0;
module_param_named(
	force_fods, force_fods,
	int, S_IRUSR | S_IWUSR
);

static u16 __maybe_unused bpp_fods[P9415_RX_FOD_MAX_REGS+1];
module_param_array(bpp_fods, ushort, NULL, S_IRUSR | S_IWUSR);

static u16 __maybe_unused epp_fods[P9415_RX_FOD_MAX_REGS+1]
= {0x7FB4,0x7FB4,0x7FB4,0x7FF0,0x7F94,0x7E93,0x0014,0x3080,0x0001};
module_param_array(epp_fods, ushort, NULL, S_IRUSR | S_IWUSR);

static u16 p9415_rx_fods[P9415_RX_FOD_MAX_REGS];

static const struct regmap_config p9415_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.val_format_endian = REGMAP_ENDIAN_LITTLE,
	.cache_type = REGCACHE_NONE,
};

enum {
	MTP_ERR_BUSY = -6,
	MTP_ERR_INVALID = -5,
	MTP_ERR_LOAD_FW = -4,
	MTP_ERR_POWER = -3,
	MTP_ERR_IO_FW1 = -2,
	MTP_ERR_IO_FW2 = -1,
	MTP_ERR_IDLE = 0,
	MTP_ERR_NONE = 1,
};

struct p9415_mtp_header {
	u16 status;
	u16 addr;
	u16 len;
	u16 chksum;
	const u8 *pbuf;
} __attribute__ ((packed));

struct p9415_mtp_seq {
	u16 regs;
	u16 data;
	u8 len;
	u8 delay_ms;
} __attribute__ ((packed));

struct p9415 {
	struct i2c_client   *client;
	struct device		*dev;
	struct regmap		*regmap;
	struct power_supply	*wls_psy;
	struct power_supply *dc_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*batt_psy;
	struct power_supply	*bms_psy;
	struct dentry		*dfs_root;

	int int_irq;
	int inhibit_gpio;
	u32 inhibit_gpio_polarity;
	int pg_irq;
	int pg_gpio;
	u32 pg_gpio_polarity;
	int power_gpio;
	u32 power_gpio_polarity;

	struct notifier_block	nb;
	spinlock_t		status_change_lock;
	bool			status_change_running;
	struct workqueue_struct *wq;
	struct work_struct	status_change_work;
	struct delayed_work	confirm_work;

	struct votable		*fv2_votable;
	struct votable		*fcc_votable;
	struct votable		*dcin_icl_votable;
	struct votable		*dc_suspend_votable;
	struct votable		*awake_votable;
	struct wakeup_source	*awake_src;

	int			dcin_slave_mode;
	//struct votable		*cp_disable_votable;
	//struct votable		*cp_ilim_votable;

	int chip_id;
	int mode;
	int tx_watt;
	int target_icl;
	int target_fcc;
	int die_temp;
	int iout_up_delta;
	int iout_down_delta;
	int iout;
	int vout;
	ktime_t int_irq_time;
	int int_irq_cnt;

	bool pg_invalid;
	ktime_t pg_valid_time;

	bool vout_uv_err;
	int vout_uv_cnt;
	int vout_uv_ua;

	atomic_t dc_present;
	bool dc_start;
	bool dc_online;
	bool dc_init_done;

	int real_soc;
	u32 config_ver;
	int		rx_max_power;
	struct mutex		mtp_lock;
	int				mtp_status;
	int				mtp_retries;
	const struct firmware		*fw1;
	const struct firmware		*fw2;
};

static int p9415_read_bytes(
	struct p9415 *chip, u16 reg, u8 *pval, u32 size)
{
	u32 i;
	int rc;

	if (!pval || !size) {
		p9415_dbg(DBG_ERR, "0x%04x: null or wrong params:%u\n",
				reg, size);
		return -EINVAL;
	}

	rc = regmap_bulk_read(chip->regmap, reg, pval, size);
	if (rc) {
		p9415_dbg(DBG_REG,
				"can't bulk read 0x%04x with size:%d, rc:%d\n",
				reg, size, rc);
		return rc;
	}

	for (i = 0; i < size; i++) {
		p9415_dbg(DBG_REG,
				"reg-0x%04x=0x%02x\n", (reg+i), pval[i]);
	}
	return 0;
}

static int p9415_read_byte(struct p9415 *chip, u16 reg, u8 *pval)
{
	u32 temp;
	int rc;
	rc = regmap_read(chip->regmap, reg, &temp);
	if (rc) {
		p9415_dbg(DBG_REG,
				"can't read 0x%04x, rc:%d\n", reg, rc);
		return -EIO;
	}
	*pval = (u8)temp;
	p9415_dbg(DBG_REG, "read 0x%04x:0x%02x\n", reg, *pval);
	return 0;
}

static int p9415_write_bytes(
	struct p9415 *chip, u16 reg, const u8 *pval, u32 size)
{
	int rc;

	if (!pval || !size) {
		p9415_dbg(DBG_ERR, "0x%04x: null or wrong params:%u\n",
				reg, size);
		return -EINVAL;
	}

	while (size--) {
		rc = regmap_write(chip->regmap, reg, *pval);
		if (rc && (reg != 0x3040)) {
		    p9415_dbg(DBG_REG,
				"can't write 0x%04x with size:%d, rc:%d\n",
				reg, (size+1), rc);
		    return rc;
		}
		p9415_dbg(DBG_REG, "write 0x%04x with val:0x%02x\n",
					reg, *pval);
		reg++;
		pval++;
    }
	return 0;
}

static int __maybe_unused p9415_write_byte(struct p9415 *chip, u16 reg, u8 val)
{
	int rc;
	rc = regmap_write(chip->regmap, reg, val);
	if (rc) {
		p9415_dbg(DBG_REG,
			"write 0x%04x with val:0x%02x failed:%d\n",
			reg, val, rc);
		return -EIO;
	}
	return 0;
}

static int __maybe_unused p9415_write_cmd(struct p9415 *chip, u16 cmd_bits)
{
	int rc;
	rc = p9415_write_bytes(chip, P9415_CMD_REG,
				(u8 *)&cmd_bits, sizeof(cmd_bits));
	if (rc) {
		p9415_dbg(DBG_REG, "write cmd_bits 0x%4x error, rc=%d\n",
				cmd_bits, rc);
		return -EIO;
	}
	//msleep(5);
	return 0;
}

static int __maybe_unused p9415_write_mask_byte(struct p9415 *chip, u16 reg, u8 mask, u8 val)
{
	int rc;
	u8 org_val, temp=0;
	rc = p9415_read_byte(chip, reg, &org_val);
	if (rc) {
		p9415_dbg(DBG_REG, "mask read 0x%04x failed:%d\n",
				reg, rc);
		return -EIO;
	}

	temp = org_val & ~mask;
	temp |= val & mask;

	p9415_dbg(DBG_REG,
			"%04x: org=%02x, mask=%02x, val=%02x, temp=%02x\n",
			reg, org_val, mask, val, temp);

	rc = p9415_write_byte(chip, reg, temp);
	if (rc) {
		p9415_dbg(DBG_REG,
			"mask write 0x%04x with val:0x%02x failed:%d\n",
			reg, val, rc);
		return -EIO;
	}
	return 0;
}

static bool p9415_is_dc_online(struct p9415 *chip)
{
	int rc;
	bool online;

	rc = gpio_get_value(chip->pg_gpio);
	if (rc < 0) {
		p9415_dbg(DBG_ERR, "read pg err:%d\n", rc);
		return false;
	}

	online = !(rc ^ chip->pg_gpio_polarity);
	p9415_dbg(DBG_DBG, "power good: %d\n", online);
	return online;
}

static int p9415_get_prop_online(struct p9415 *chip, int *pval)
{
	*pval = p9415_is_dc_online(chip);
	return 0;
}

static int p9415_get_prop_voltage_now(struct p9415 *chip, int *pval)
{
	int rc;
	u16 raw = 0;

	if (!pval)
		return -EINVAL;

	rc = p9415_read_bytes(chip, P9415_VOUT_ADC_REG,
						(u8 *)(&raw), sizeof(raw));
	if (rc) {
		p9415_dbg(DBG_DBG,"Vout read error, rc=%d\n", rc);
		return -EIO;
	}
	*pval = P9415_RAW_TO_VOUT_UV(raw);
	if (*pval < 0 || *pval >= (DCIN_EPP_VOL_UV + DCIN_EPP_GAPS_UV)) {
		p9415_dbg(DBG_ERR,
				"Invalid Vout raw=0x%04x, scaled=%d uV\n",
				raw, *pval);
		*pval = 0;
		return -EINVAL;
	}
	p9415_dbg(DBG_DBG, "Vout raw=0x%04x, scaled=%d uV\n", raw, *pval);
	return 0;
}

static int __maybe_unused p9415_get_prop_current_now(struct p9415 *chip, int *pval)
{
	int rc;
	u16 raw = 0;

	if (!pval)
		return -EINVAL;

	rc = p9415_read_bytes(chip, P9415_IOUT_REG,
						(u8 *)(&raw), sizeof(raw));
	if (rc) {
		p9415_dbg(DBG_DBG, "Iout read error, rc=%d\n", rc);
		return -EIO;
	}
	*pval = raw * 1000;
	p9415_dbg(DBG_DBG, "Iout raw=0x%04x, scaled=%d uA\n",
			raw, *pval);
	return 0;
}

static int __maybe_unused p9415_get_prop_current_max(struct p9415 *chip, int *pval)
{
	int rc;
	u8 raw = 0;

	rc = p9415_read_byte(chip, P9415_ILIM_REG, &raw);
	if (rc < 0) {
		p9415_dbg(DBG_DBG, "Ilim read error, rc=%d\n", rc);
		return rc;
	}
	*pval = raw * P9415_ILIM_STEP_UA;
	p9415_dbg(DBG_DBG, "Ilim raw=0x%02x, scaled=%d uA\n", raw, *pval);
	return 0;
}

static int __maybe_unused p9415_read_die_temp(struct p9415 *chip, int *pval)
{
	int rc;
	u16 raw = 0;

	rc = p9415_read_bytes(chip, P9415_DIE_TEMP_REG,
						(u8 *)(&raw), sizeof(raw));
	if (rc) {
		p9415_dbg(DBG_ERR, "die temp read error, rc=%d\n", rc);
		return -EIO;
	}
	*pval = 10 * P9415_DIE_TEMP_SCALED_DEG_C(raw);
	p9415_dbg(DBG_DBG, "DieT: raw=%04x, scaled=%d\n",
			raw, *pval);
	return 0;
}

static int p9415_read_chipid(struct p9415 *chip)
{
	int rc;
	u16 raw = 0;

	rc = p9415_read_bytes(chip, P9415_CHIP_ID_REG,
						(u8 *)(&raw), sizeof(raw));
	if (rc) {
		p9415_dbg(DBG_DBG,
				"Chipid read error:%d\n", rc);
		return rc;
	}

	chip->chip_id = raw;
	p9415_dbg(DBG_DBG, "Chipid=0x%x\n", chip->chip_id);
	return 0;
}

static int p9415_read_mode(struct p9415 *chip)
{
	int rc;
	u8 raw;

	rc = p9415_read_byte(chip, P9415_MODE_REG, &raw);
	if (rc) {
		p9415_dbg(DBG_ERR, "mode read error, rc=%d\n", rc);
		return -EIO;
	}
	chip->mode = raw;
	p9415_dbg(DBG_DBG, "Mode: raw=0x%x\n", chip->mode);
	return 0;
}

static int __maybe_unused p9415_read_ssp(struct p9415 *chip, int *ssp)
{
	u8 raw = 0;
	int rc;

	rc = p9415_read_byte(chip, P9415_SSP_REG, &raw);
	if (rc) {
		p9415_dbg(DBG_ERR, "ssp read error, rc=%d\n", rc);
		return -EIO;
	}
	*ssp = raw;
	p9415_dbg(DBG_DBG, "ssp=%d\n", *ssp);
	return 0;
}

static int __maybe_unused p9415_read_xy_axis(struct p9415 *chip, int *xyz)
{
	u16 raw = 0;
	int rc;

	rc = p9415_read_bytes(chip, P9415_X_AXIS_REG,
							(u8 *)&raw, sizeof(raw));
	if (rc) {
		p9415_dbg(DBG_ERR, "AX read error, rc=%d\n", rc);
		return -EIO;
	}
	*xyz = raw;
	p9415_dbg(DBG_DBG, "xyz=0x%x\n", *xyz);
	return 0;
}

static int __maybe_unused p9415_read_ssr(struct p9415 *chip, u32 *ssr)
{
	u32 raw = 0;
	int rc;

	rc = p9415_read_bytes(chip, P9415_STATUS_TRX_REG,
							(u8 *)&raw, sizeof(raw));
	if (rc) {
		p9415_dbg(DBG_ERR, "ssr read error, rc=%d\n", rc);
		return -EIO;
	}
	*ssr = raw;
	p9415_dbg(DBG_DBG, "ssr=0x%x\n", *ssr);
	return 0;
}

static int p9415_read_tx_power(struct p9415 *chip)
{
	int rc;
	u8 raw;

	rc = p9415_read_byte(chip, P9415_TXPOWER_REG, &raw);
	if (rc) {
		p9415_dbg(DBG_ERR, "txpwr read error, rc=%d\n", rc);
		return -EIO;
	}
	chip->tx_watt = P9415_RAW_TO_POWER_UW(raw);
	p9415_dbg(DBG_DBG, "raw=%d, TxPwr=%d\n", raw, chip->tx_watt);
	chip->tx_watt = min(chip->tx_watt, chip->rx_max_power);
	return 0;
}

static int p9415_read_rx_fod(struct p9415 *chip, int idx, u16 *pval)
{
	int rc;
	u16 temp, raw;

	if (!pval || idx < 0 || idx >= P9415_RX_FOD_MAX_REGS)
		return -EINVAL;

	temp = P9415_FOD_RX_GAIN_OFFSET_REG0 + (idx<<1);
	rc = p9415_read_bytes(chip, temp, (u8 *)&raw, sizeof(raw));
	if (rc) {
		p9415_dbg(DBG_DBG, "read FOD[%04x] error, rc=%d\n",
					temp, rc);
		return -EIO;
	}
	*pval = raw;
	p9415_dbg(DBG_DBG, "read FOD[%04x]: raw=%04x\n",
				temp, *pval);
	return 0;
}

static int p9415_read_rx_fods(struct p9415 *chip, u16 *pval, int cnt)
{
	int idx, rc;

	if (!pval || cnt <= 0 || cnt > P9415_RX_FOD_MAX_REGS)
		return -EINVAL;

	for (idx = 0; idx < cnt; idx++, pval++) {
		rc = p9415_read_rx_fod(chip, idx, pval);
		if (rc) {
			p9415_dbg(DBG_DBG, "patch read FOD-%d error, rc=%d\n",
						cnt, rc);
			return -EIO;
		}
	}
	return 0;
}

static int __maybe_unused p9415_write_rx_fod(struct p9415 *chip, int idx, u16 val)
{
	int rc;
	u16 temp;

	if (idx < 0 || idx >= P9415_RX_FOD_MAX_REGS)
		return -EINVAL;

	temp = P9415_FOD_RX_GAIN_OFFSET_REG0 + (idx<<1);
	rc = p9415_write_bytes(chip, temp, (u8 *)&val, sizeof(val));
	if (rc) {
		p9415_dbg(DBG_ERR, "write FOD[%04x] error, rc=%d\n",
					temp, rc);
		return -EIO;
	}
	p9415_dbg(DBG_ERR, "[%x]write FOD[%04x]: raw=%04x\n",
				chip->mode, temp, val);
	return 0;
}

static int __maybe_unused p9415_write_rx_fods(struct p9415 *chip)
{
	int rc;
	int idx;
	u16 *pdata = NULL;

	if (chip->mode & P9415_MODE_RX_BIT) {
		if (chip->mode & P9415_MODE_EPP_BIT) {
			pdata = epp_fods;
		} else {
			pdata = bpp_fods;
		}

		for (idx = 0; idx < P9415_RX_FOD_MAX_REGS; idx++) {
			rc = p9415_write_rx_fod(chip, idx, pdata[idx]);
			if (rc) {
				return -EIO;
			}
		}
		return 0;
	}
	p9415_dbg(DBG_ERR, "write FODs error, Rx mode:%x\n",
				chip->mode);
	return -EINVAL;
}

#define VER_MAJOR_OFFSET	(16)
#define VER_MINOR_OFFSET	(8)
#define VER_BETA_OFFSET		(0)
static int p9415_read_config_version(struct p9415 *chip)
{
	int i, rc;
	u16 temp;

	rc = p9415_read_rx_fods(chip, (u16 *)p9415_rx_fods,
						ARRAY_SIZE(p9415_rx_fods));
	if (rc) {
		p9415_dbg(DBG_ERR,
				"fods read error, rc=%d\n", rc);
		return rc;
	}

	for (i = 0; i < P9415_RX_FOD_MAX_REGS; i++) {
		temp += p9415_rx_fods[i];
	}
	chip->config_ver = (u32)temp;
	p9415_dbg(DBG_ERR, "config_ver: %d\n",
				chip->config_ver);
	return 0;
}

static int p9415_set_vout_power(struct p9415 *chip, bool enable)
{
	int rc;
	rc = gpio_direction_output(chip->power_gpio,
			!(chip->power_gpio_polarity ^ enable));
	if (rc) {
		p9415_dbg(DBG_MTP, "mtp power %s failed, rc=%d\n",
				enable ? "enable" : "disable", rc);
	}
	return rc;
}

static int p9415_inhibit_enable(struct p9415 *chip, bool enable)
{
	int rc;
	rc = gpio_direction_output(chip->inhibit_gpio,
			!(chip->inhibit_gpio_polarity ^ enable));
	if (rc) {
		p9415_dbg(DBG_ERR, "inhibit gpio output %s failed, rc=%d\n",
				enable ? "high" : "low", rc);
	}
	return rc;
}

static int p9415_set_prop_dc_reset(struct p9415 *chip, bool hw_reset)
{
	int rc;

	if (!hw_reset) {
		rc = p9415_write_cmd(chip, P9415_CMD_SOFTRESTART_BIT);
		if (rc) {
			p9415_dbg(DBG_ERR, "reset wls error, rc=%d\n", rc);
			return rc;
		}
	} else {
		p9415_inhibit_enable(chip, true);
		msleep(500);
		p9415_inhibit_enable(chip, false);
	}

	p9415_dbg(DBG_ERR, "%s reset wls done\n",
			hw_reset ? "HW" : "SW");
	return 0;
}

static int p9415_set_prop_vout_target(struct p9415 *chip, const int uV)
{
	int rc;
	u16 raw;

	if (uV < DCIN_ALLOW_MIN_VOL_UV || uV > DCIN_ALLOW_MAX_VOL_UV) {
		p9415_dbg(DBG_ERR, "Vout %d not set in range[5V,12V]\n", uV);
		return -EINVAL;
	}

	raw = P9415_VOUT_UV_TO_RAW(uV);
	rc = p9415_write_bytes(chip, P9415_VOUT_SET_REG,
							(u8 *)(&raw), sizeof(raw));
	if (rc) {
		p9415_dbg(DBG_ERR,
			"Vout set raw %d error, rc=%d\n", raw, rc);
		return rc;
	}

	p9415_dbg(DBG_DBG, "Vout set vol=%d uV, raw=0x%04x\n", uV, raw);
	return 0;
}

static void __maybe_unused p9415_set_comcap(struct p9415 *chip, u8 comcap)
{
	int rc;

	rc = p9415_write_mask_byte(chip, P9415_COMCAPSEL_REG,
							P9415_COMCAP_MASK, comcap);
	if (rc) {
		p9415_dbg(DBG_ERR, "set ComCap %d error, rc=%d\n",
				comcap, rc);
	} else {
		p9415_dbg(DBG_DBG, "Comcap set to %02x\n", comcap);
	}
}

static enum power_supply_property p9415_psy_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_CHIP_VERSION,
	POWER_SUPPLY_PROP_FW_CURRENT_VERSION,
	POWER_SUPPLY_PROP_FW_GOAL_VERSION,
	POWER_SUPPLY_PROP_MTP_START,
	POWER_SUPPLY_PROP_MTP_STATUS,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_DC_RESET,
};

static int p9415_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	int rc = 0, *val = &pval->intval;
	struct p9415 *chip = power_supply_get_drvdata(psy);

	switch (psp) {
		case POWER_SUPPLY_PROP_MTP_START:
			*val = mtp_start;
			return 0;
		case POWER_SUPPLY_PROP_MTP_STATUS:
			*val = chip->mtp_status;
			return 0;
		default:
			break;
	}

	if (!atomic_read(&chip->dc_present)
		&& !p9415_is_dc_online(chip)) {
		p9415_dbg(DBG_DBG, "skip, dc absent\n");
		*val = 0;
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		*val = atomic_read(&chip->dc_present);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		rc = p9415_get_prop_online(chip, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		*val = chip->vout;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		*val = chip->iout;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		*val = chip->die_temp;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (chip->mode & P9415_MODE_EPP_BIT) {
			*val = DCIN_EPP_VOL_UV;
		} else {
			*val = DCIN_BPP_VOL_UV;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		*val = chip->target_icl;
		break;
	case POWER_SUPPLY_PROP_CHIP_VERSION:
		*val = chip->chip_id;
		break;
	case POWER_SUPPLY_PROP_FW_CURRENT_VERSION:
		*val = chip->config_ver;
		break;
	case POWER_SUPPLY_PROP_FW_GOAL_VERSION:
		if (chip->mode & P9415_MODE_EPP_BIT) {
			*val = P9415_FW2_EPP_VER;
		} else {
			*val = P9415_FW2_BPP_VER;
		}
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		*val = chip->tx_watt;
		break;
	default:
		rc = -EINVAL;
		break;
	}

	if (rc) {
		p9415_dbg(DBG_DBG, "property %d unavailable:%d\n",
				psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int p9415_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
	case POWER_SUPPLY_PROP_MTP_START:
	case POWER_SUPPLY_PROP_DC_RESET:
		return 1;
	default:
		break;
	}

	return 0;
}

static int p9415_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	int rc = 0;
	struct p9415 *chip = power_supply_get_drvdata(psy);
	bool dc_online = false;

	if (prop == POWER_SUPPLY_PROP_MTP_START) {
		mutex_lock(&chip->mtp_lock);
		if (!mtp_start && (pval->intval > 0)
			&& (pval->intval <= 100)) {
			mtp_start = pval->intval;
		} else {
			p9415_dbg(DBG_ERR, "skip for mtp ops\n");
			rc = -EBUSY;
		}
		mutex_unlock(&chip->mtp_lock);

		if (!rc && chip->dc_psy)
			power_supply_changed(chip->dc_psy);
		return rc;
	}

	dc_online = p9415_is_dc_online(chip);
	if (!dc_online) {
		p9415_dbg(DBG_DBG, "skip, dc offline\n");
		return -EINVAL;
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		rc = p9415_set_prop_vout_target(chip, pval->intval);
		break;
	case POWER_SUPPLY_PROP_DC_RESET:
		/* 1: HW reset, 0: SW reset */
		rc = p9415_set_prop_dc_reset(chip, pval->intval);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int p9415_parse_dt(struct p9415 *chip)
{
	struct device_node *node = chip->dev->of_node;
	enum of_gpio_flags flags;
	int rc = 0;

	if (!node) {
		return -EINVAL;
	}

	rc = of_property_read_u32(node, "idt,dcin-slave-mode",
					&chip->dcin_slave_mode);
	if (rc < 0 || chip->dcin_slave_mode > POWER_SUPPLY_CHARGER_SEC_CP_PL)
		chip->dcin_slave_mode = POWER_SUPPLY_CHARGER_SEC_NONE;


	chip->inhibit_gpio = of_get_named_gpio_flags(node,
					"p9415,inhibit-gpio", 0, &flags);
	if (!gpio_is_valid(chip->inhibit_gpio)) {
		p9415_dbg(DBG_ERR, "inhibit gpio get fail:%d\n",
					chip->inhibit_gpio);
		return -EINVAL;
	} else {
		chip->inhibit_gpio_polarity = !(flags & OF_GPIO_ACTIVE_LOW);
		rc = devm_gpio_request(chip->dev, chip->inhibit_gpio,
						"p9415_inhibit_gpio");
		if (rc) {
			p9415_dbg(DBG_ERR, "unable to request inhibit gpio [%d]\n",
						chip->inhibit_gpio);
			return -EBUSY;
		}
		p9415_inhibit_enable(chip, false);
	}

	chip->pg_gpio = of_get_named_gpio_flags(node,
					"p9415,pg-gpio", 0, &flags);
	if (!gpio_is_valid(chip->pg_gpio)) {
		p9415_dbg(DBG_ERR, "pg gpio get fail:%d\n",
					chip->pg_gpio);
		return -EINVAL;
	} else {
		chip->pg_gpio_polarity = !(flags & OF_GPIO_ACTIVE_LOW);
		rc = devm_gpio_request(chip->dev, chip->pg_gpio,
						"p9415_pg_gpio");
		if (rc) {
			p9415_dbg(DBG_ERR, "unable to request pg_gpio[%d]\n",
						chip->pg_gpio);
			return -EBUSY;
		}
		chip->pg_irq = gpio_to_irq(chip->pg_gpio);
		if (chip->pg_irq < 0) {
			p9415_dbg(DBG_ERR, "unable to get pg_irq[%d]\n",
						chip->pg_irq);
			return -EINVAL;
		}
	}

	chip->power_gpio = of_get_named_gpio_flags(node,
					"p9415,mtp-gpio", 0, &flags);
	if (!gpio_is_valid(chip->power_gpio)) {
		p9415_dbg(DBG_ERR, "mtp power gpio get fail:%d\n",
					chip->pg_gpio);
		return -EINVAL;
	} else {
		chip->power_gpio_polarity = !(flags & OF_GPIO_ACTIVE_LOW);
		rc = devm_gpio_request(chip->dev, chip->power_gpio,
						"p9415_mtp_gpio");
		if (rc) {
			p9415_dbg(DBG_ERR, "unable to request mtp_gpio[%d]\n",
						chip->power_gpio);
			return -EBUSY;
		}
		p9415_set_vout_power(chip, false);
	}

	rc = of_property_read_u32(node, "idt,rx-max-power",
					&chip->rx_max_power);
	if ((rc < 0) || (chip->rx_max_power > DCIN_15W)) {
		chip->rx_max_power = DCIN_15W;
	}

	return 0;
}

#define FOD_INTERVAL_TIMEOUT (600ULL)
#define MAX_INT_IRQ_CNTS (8)
static irqreturn_t p9415_int_irq_handler(int irq, void *data)
{
	struct p9415 *chip = data;
	int rc = 0;
	u32 raw = 0, status = 0;
	bool fod_detected = false;
	u64 delta_ms = 0;

	delta_ms = ktime_ms_delta(ktime_get(), chip->int_irq_time);
	if (delta_ms < FOD_INTERVAL_TIMEOUT) {
		if (chip->int_irq_cnt++ > MAX_INT_IRQ_CNTS) {
			fod_detected = true;
			chip->int_irq_cnt = 0;
		}
	} else {
		chip->int_irq_cnt = 0;
	}
	chip->int_irq_time = ktime_get();
	p9415_dbg(DBG_ISR, "%lld, %d, %d\n", delta_ms,
			chip->int_irq_cnt, fod_detected);
	if (fod_detected) {
		atomic_set(&chip->dc_present, 0);
		goto out;
	}

	rc = p9415_read_bytes(chip, P9415_INT_TRX_REG,
				(u8 *)(&status), sizeof(status));
	if (rc) {
		p9415_dbg(DBG_ISR,
				"read isr reg error, rc=%d\n", rc);
		goto out;
	}
	p9415_dbg(DBG_ISR, "isr:0x%x\n", status);

	/* do afterwards */
	raw = 0xFFFFFFFF;
	rc = p9415_write_bytes(chip, P9415_INT_CLEAR_TRX_REG,
				(u8 *)(&raw), sizeof(raw));
	if (rc) {
		p9415_dbg(DBG_ISR,
				"write isr clear reg error, rc=%d\n", rc);
		goto out;
	}

	rc = p9415_write_cmd(chip, P9415_CMD_CLRINT_BIT);
	if (rc) {
		p9415_dbg(DBG_ISR, "write cmd reg error, rc=%d\n", rc);
		goto out;
	}
	atomic_set(&chip->dc_present, 1);

out:
	cancel_delayed_work_sync(&chip->confirm_work);
	queue_delayed_work(chip->wq, &chip->confirm_work,
				msecs_to_jiffies(confirm_time));

	if (!(status & 0x80) && chip->dc_psy) {
		power_supply_changed(chip->dc_psy);
	}

	return IRQ_HANDLED;
}

#define PG_VALID_INTERVAL_MS (8000ULL)
static irqreturn_t p9415_pg_irq_handler(int irq, void *data)
{
	struct p9415 *chip = data;
	int rc = 0;
	bool dc_online = 0;
	u64 delta_ms = 0;

	dc_online = p9415_is_dc_online(chip);
	if (!dc_online) {
		rc = p9415_read_chipid(chip);
		if (rc == -ENOTCONN) {
			atomic_set(&chip->dc_present, 0);
		}
	} else {
		atomic_set(&chip->dc_present, 1);
		delta_ms = ktime_ms_delta(ktime_get(),
						chip->pg_valid_time);
		if (delta_ms < PG_VALID_INTERVAL_MS) {
			chip->pg_invalid = true;
		} else {
			chip->pg_invalid = false;
		}
	}
	chip->pg_valid_time = ktime_get();

	cancel_delayed_work_sync(&chip->confirm_work);
	queue_delayed_work(chip->wq, &chip->confirm_work,
					msecs_to_jiffies(confirm_time2));
	if (chip->dc_psy) {
		p9415_dbg(DBG_ISR, "pg notify upper layer:%d,%d,%lld,%d\n",
					dc_online, rc, delta_ms, chip->pg_invalid);
		power_supply_changed(chip->dc_psy);
	}

	return IRQ_HANDLED;
}

static struct p9415_mtp_seq mtp_array_1[] = {
	{0x3000, 0x005A, 1, 10}, // unlock system registers.
	{0x3004, 0x0000, 1, 0},  // set HS clock.
	{0x3008, 0x0009, 1, 0},  // set AHB clock.
	{0x300C, 0x0005, 1, 0},  // config 1us pulse.
	{0x300D, 0x001D, 1, 0},  // config 500ns pulse.
	{0x3040, 0x0011, 1, 10}, // pause & enable i2c func.
	{0x3040, 0x0010, 1, 10}, // halt uC.
};

static struct p9415_mtp_seq mtp_array_2[] = {
	{0x0400, 0x0000, 1, 10},  // initialize the programming structure in the RAM
	{0x3048, 0x00D0, 1, 10},  // remap RAM to program memory space.
	{0x3040, 0x0080, 1, 200},  // reset uC, run mtp downloader in RAM.
};

static int p9415_mtp_batch_write(
	struct p9415 *chip, struct p9415_mtp_seq* seq_array, int size)
{
	int i, rc;

	might_sleep();
	if (!seq_array || !size) {
		p9415_dbg(DBG_MTP, "invalid params:%d\n", size);
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		rc = p9415_write_bytes(chip, seq_array[i].regs,
				(u8 *)&seq_array[i].data, seq_array[i].len);
		if (rc) {
			p9415_dbg(DBG_MTP, "batch w error: %04x,%04x,%d\n",
					seq_array[i].regs, seq_array[i].data,
					seq_array[i].len);
			return -EIO;
		}
		if (seq_array[i].delay_ms)
			msleep(seq_array[i].delay_ms);
	}
	return rc;
}

static bool p9415_is_mtp_available(struct p9415 *chip)
{
	union power_supply_propval val = {0,};
	int rc = 0;

	rc = power_supply_get_property(chip->usb_psy,
					POWER_SUPPLY_PROP_REAL_TYPE,
					&val);
	if (rc) {
		p9415_dbg(DBG_MTP,
				"get chg type err, rc:%d\n", rc);
		return false;
	}

	/* Rx chip only support MTP with 5V max */
	if (val.intval != POWER_SUPPLY_TYPE_USB
		&& val.intval != POWER_SUPPLY_TYPE_USB_CDP) {
		p9415_dbg(DBG_MTP,
				"chg type %d invalid\n", val.intval);
		return false;
	}

	return true;
}

#define MTP_FW1_SIZE	(1796)
#define MTP_FW2_SIZE	(212)
static int p9415_mtp_load_fws(struct p9415 *chip)
{
	int rc;

	might_sleep();

	rc = request_firmware(&chip->fw1, MTP_FW1_BIN, chip->dev);
	if (rc || !chip->fw1 || chip->fw1->size != MTP_FW1_SIZE) {
		p9415_dbg(DBG_MTP, "fw1 get failed, rc:%d\n", rc);
		rc = -ENOENT;
		goto out;
	}

	rc = request_firmware(&chip->fw2, MTP_FW2_BIN, chip->dev);
	if (rc || !chip->fw2 || chip->fw2->size != MTP_FW2_SIZE) {
		p9415_dbg(DBG_MTP, "fw2 get failed, rc:%d\n", rc);
		rc = -ENOENT;
		goto out;
	}

	p9415_dbg(DBG_MTP, "load fws ok with:%d,%d\n",
			chip->fw1->size, chip->fw2->size);
out:
	return rc;
}

#define FW1_RAM_ADDR	(0x0800)
static int p9415_mtp_flash_fw1(struct p9415 *chip)
{
	int rc;
	rc = p9415_mtp_batch_write(chip,
			mtp_array_1, ARRAY_SIZE(mtp_array_1));
	if (rc) {
		p9415_dbg(DBG_MTP,
				"pre flash fw1 err, rc:%d\n", rc);
		return rc;
	}

	rc = p9415_write_bytes(chip, FW1_RAM_ADDR,
			chip->fw1->data, chip->fw1->size);
	if (rc) {
		p9415_dbg(DBG_MTP,
				"flash fw1 error, rc:%d\n", rc);
		return -EIO;
	}

	rc = p9415_mtp_batch_write(chip,
			mtp_array_2, ARRAY_SIZE(mtp_array_2));
	if (rc) {
		p9415_dbg(DBG_MTP,
				"post flash fw1 err, rc:%d\n", rc);
		return rc;
	}

#if 0
	for (i = 0; i < chip->fw1->size; i++) {
        rc = p9415_read_byte(chip, (FW1_RAM_ADDR + i),
				&read_back);
        if (rc || (read_back != chip->fw1->data[i])) {
            p9415_dbg(DBG_MTP,
					"[%d]fw1 corrupted, %d: %02x:%02x\n",
					i, rc, read_back, chip->fw1->data[i]);
            return -EIO;
        }
        pr_err("%02x ", read_back);
        if (i % 16 == 0)
            pr_err("\n");
    }
#endif

	p9415_dbg(DBG_MTP, "flash fw1 done\n");
	return 0;
}

#define FW2_RAM_ADDR_DEST	(0x5E00)
#define FW2_RAM_ADDR2	(0x0400)
#define FW2_RAM_ADDR3	(0x0401)
#define FW2_SECTOR_SIZE	(128)
#define FW2_SECTOR_SIZE_SHIFT	(7)
#define FW2_PRG_BUF_OFFSET	(8)
#define PRG_WAIT_CYCLES	(5)

static int _p9415_mtp_flash_fw2(
	struct p9415 *chip, struct p9415_mtp_header *pheader)
{
	u8 data, wait_cnt;
	int rc;

	rc = p9415_write_bytes(chip, FW2_RAM_ADDR2,
				(u8 *)pheader, FW2_PRG_BUF_OFFSET);
	if (rc) {
		p9415_dbg(DBG_MTP,
				"flash fw2 head err, rc:%d\n", rc);
		return -EIO;
	}

	rc = p9415_write_bytes(chip,
			(FW2_RAM_ADDR2 + FW2_PRG_BUF_OFFSET),
			pheader->pbuf, pheader->len);
	if (rc) {
		p9415_dbg(DBG_MTP,
				"flash fw2 data err, rc:%d, len:%d\n",
				rc, pheader->len);
		return -EIO;
	}

	// trigger one program cycle.
	data = 0x01;
	rc = p9415_write_bytes(chip, FW2_RAM_ADDR2,
							&data, 1);
	if (rc) {
		p9415_dbg(DBG_MTP,
				"trigger prg err, rc:%d\n", rc);
		return -EIO;
	}

	// wait for x401.bit0 = 0 and read the status code.
	wait_cnt = 0;
	do {
		msleep(100);
		rc = p9415_read_byte(chip, FW2_RAM_ADDR2,
							&data);
		if (rc) {
			p9415_dbg(DBG_MTP,
					"read prg flag err, rc:%d\n", rc);
			return -EIO;
		} else if ((data & 0x01) == 0x00) {
			break;
		}
	} while(wait_cnt++ > PRG_WAIT_CYCLES);

	if ((data & 0x01) != 0x00) {
		p9415_dbg(DBG_MTP,
				"read prg flag timeout:%02x\n", data);
		return -EIO;
	}

	/* Error codes:
	0x02: Program OK.
	0x04: MTP write error, data mismatch.
	0x08: header checksum error
	*/
	rc = p9415_read_byte(chip, FW2_RAM_ADDR3, &data);
	if (rc || data != 0x02) {
		p9415_dbg(DBG_MTP,
			"read prg status err, rc:%d,%02x\n",
			rc, data);
		return -EIO;
	}

	p9415_dbg(DBG_MTP, "prg status: %d, %02x\n",
			wait_cnt, data);
	return 0;
}

static int p9415_mtp_flash_fw2(struct p9415 *chip)
{
	struct p9415_mtp_header header;
	u32 sectors, left_len;
	u32 i, j;
	int rc = -EINVAL;

	might_sleep();

	sectors = (chip->fw2->size >> FW2_SECTOR_SIZE_SHIFT);
	left_len = chip->fw2->size -
			(sectors << FW2_SECTOR_SIZE_SHIFT);

	for (i = 0; i < sectors; i++) {
		header.status = 0;
		header.addr = FW2_RAM_ADDR_DEST +
					(i << FW2_SECTOR_SIZE_SHIFT);
		header.len = FW2_SECTOR_SIZE;
		header.chksum = header.addr + header.len;
		header.pbuf = chip->fw2->data +
					(i << FW2_SECTOR_SIZE_SHIFT);

		for (j = 0; j < header.len; j++) {
			header.chksum += header.pbuf[j];
		}
		rc = _p9415_mtp_flash_fw2(chip, &header);
		if (rc)
			return -EIO;
	}

	if (left_len) {
		header.status = 0;
		header.addr = FW2_RAM_ADDR_DEST +
					(i << FW2_SECTOR_SIZE_SHIFT);
		header.len = left_len;
		header.chksum = header.addr + header.len;
		header.pbuf = chip->fw2->data +
					(i << FW2_SECTOR_SIZE_SHIFT);

		for (j = 0; j < header.len; j++) {
			header.chksum += header.pbuf[j];
		}
		rc = _p9415_mtp_flash_fw2(chip, &header);
		if (rc)
			return -EIO;
	}

/*
	for (i = 0; i < chip->fw2->size; i++) {
		rc = p9415_read_byte(chip,
					(FW2_RAM_ADDR_DEST + i), &data);
		if (rc || (data != chip->fw2->data[i])) {
			p9415_dbg(DBG_MTP,
					"[%d]fw2 corrupted: %d,%02x,%02x\n",
					i, rc, data, chip->fw2->data[i]);
			return -EIO;
		}
		pr_err("%02x ", data);
		if (i % 16 == 0)
			pr_err("\n");
	}
*/

	p9415_dbg(DBG_MTP, "flash fw2 done\n");
	return 0;
}

static void p9415_mtp_exit(struct p9415 * chip)
{
	release_firmware(chip->fw2);
	release_firmware(chip->fw1);

	chip->fw2 = NULL;
	chip->fw1 = NULL;
	p9415_set_vout_power(chip, false);
}

static void p9415_mtp_entry(struct p9415 * chip)
{
	int rc;

	might_sleep();
	chip->mtp_status = MTP_ERR_BUSY;
	chip->mtp_retries = 3;

	if (!p9415_is_mtp_available(chip)) {
		chip->mtp_status = MTP_ERR_INVALID;
		return;
	}

	rc = p9415_mtp_load_fws(chip);
	if (rc) {
		chip->mtp_status = MTP_ERR_LOAD_FW;
		return;
	}

retry:
	p9415_set_vout_power(chip, true);
	msleep(100);

	rc = p9415_mtp_flash_fw1(chip);
	if (rc) {
		chip->mtp_status = MTP_ERR_IO_FW1;
	} else {
		rc = p9415_mtp_flash_fw2(chip);
		if (rc) {
			chip->mtp_status = MTP_ERR_IO_FW2;
		}
	}

	if (rc) {
		if (chip->mtp_retries--) {
			p9415_set_vout_power(chip, false);
			msleep(50);
			p9415_dbg(DBG_MTP,
					"[%d]retry flashing fw, rc:%d\n",
					chip->mtp_retries, rc);
			goto retry;
		} else {
			p9415_dbg(DBG_MTP,
					"retried failure, rc:%d\n", rc);
			return;
		}
	}

	chip->mtp_status = MTP_ERR_NONE;
}

static bool p9415_is_psy_available(struct p9415 *chip)
{
	if (!chip->dc_psy) {
		chip->dc_psy = power_supply_get_by_name("dc");
		if (!chip->dc_psy) {
			p9415_dbg(DBG_DBG, "Couldn't find dc psy\n");
			return false;
		}
	}

	if (!chip->batt_psy) {
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			p9415_dbg(DBG_DBG, "Couldn't find battery psy\n");
			return false;
		}
	}

	if (!chip->usb_psy) {
		chip->usb_psy = power_supply_get_by_name("usb");
		if (!chip->usb_psy) {
			p9415_dbg(DBG_DBG, "Couldn't find usb psy\n");
			return false;
		}
	}

	if (!chip->bms_psy) {
		chip->bms_psy = power_supply_get_by_name("bms");
		if (!chip->bms_psy) {
			p9415_dbg(DBG_DBG, "Couldn't find bms psy\n");
			return false;
		}
	}
	return true;
}

static bool p9415_is_votable_available(struct p9415 *chip)
{
	if (!chip->dcin_icl_votable) {
		chip->dcin_icl_votable = find_votable("DCIN_ICL");
		if (!chip->dcin_icl_votable) {
			p9415_dbg(DBG_DBG, "skip for dcin_icl_votable is NULL\n");
			return false;
		}
	}

	if (!chip->dc_suspend_votable) {
		chip->dc_suspend_votable = find_votable("DC_SUSPEND");
		if (!chip->dc_suspend_votable) {
			p9415_dbg(DBG_DBG, "skip for dc_suspend_votable is NULL\n");
			return false;
		}
	}

	if (!chip->fcc_votable) {
		chip->fcc_votable = find_votable("FCC");
		if (!chip->fcc_votable) {
			p9415_dbg(DBG_DBG, "skip for fcc_votable is NULL\n");
			return false;
		}
	}

	if (!chip->fv2_votable) {
		chip->fv2_votable = find_votable("FV2");
		if (!chip->fv2_votable) {
			p9415_dbg(DBG_DBG, "skip for fv2_votable is NULL\n");
			return false;
		}
	}
	return true;
}

static void p9415_get_bms_real_soc(struct p9415 *chip, int *soc)
{
	union power_supply_propval val = {0,};
	int rc;

	*soc = DCIN_ICL_SOC_LVL1;
	rc = power_supply_get_property(chip->bms_psy,
					POWER_SUPPLY_PROP_CAPACITY_RAW,
					&val);
	if (rc) {
		p9415_dbg(DBG_DBG,
				"get bms real soc err, rc:%d\n", rc);
		return;
	}
	*soc = val.intval;
}

static int __maybe_unused p9415_get_battery_voltage(struct p9415 *chip, int *vol)
{
	union power_supply_propval val = {0,};
	int rc;

	rc = power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW,
					&val);
	if (rc) {
		p9415_dbg(DBG_DBG,
				"get battery vol err, rc:%d\n", rc);
		return -EIO;
	}
	*vol = val.intval;
	return 0;
}

static int p9415_get_battery_current(struct p9415 *chip, int *curr)
{
	union power_supply_propval val = {0,};
	int rc;

	rc = power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_CURRENT_NOW,
					&val);
	if (rc) {
		p9415_dbg(DBG_DBG,
				"get battery curr err, rc:%d\n", rc);
		return -EIO;
	}
	*curr = val.intval;
	return 0;
}

static bool __maybe_unused p9415_is_full_charged(struct p9415 *chip)
{
	union power_supply_propval val = {0,};
	int rc = 0;

	rc = power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &val);
	if (rc ||
		(val.intval != POWER_SUPPLY_STATUS_FULL)) {
		return false;
	}
	return true;
}

static void p9415_get_dc_status(struct p9415 *chip, int *status)
{
	union power_supply_propval val = {0,};
	int rc;
	*status = 0;
	rc = power_supply_get_property(chip->dc_psy,
					POWER_SUPPLY_PROP_DC_STATUS,
					&val);
	if (rc) {
		p9415_dbg(DBG_DBG,
				"get dc status err, rc:%d\n", rc);
		return;
	}
	*status = val.intval;
}

static void __maybe_unused p9415_get_quiet_temp(struct p9415 *chip, int *quiet_temp)
{
	union power_supply_propval val = {0,};
	int rc;
	*quiet_temp = 0;
	rc = power_supply_get_property(chip->dc_psy,
					POWER_SUPPLY_PROP_TEMP,
					&val);
	if (rc) {
		p9415_dbg(DBG_DBG,
				"get quiet temp err, rc:%d\n", rc);
		return;
	}
	*quiet_temp = val.intval;
}

static void p9415_reset_dcin(struct p9415 *chip)
{
	union power_supply_propval val = {1,};
	int rc;

	rc = power_supply_set_property(chip->dc_psy,
					POWER_SUPPLY_PROP_DC_RESET,
					&val);
	if (rc) {
		p9415_dbg(DBG_DBG,
				"reset dc failed, rc:%d\n", rc);
		return;
	}
}

#define MAX_VOUT_UV_CNT (2)
#define MAX_DELAY_CNT (5)
static void p9415_adjust_rx_power(struct p9415 *chip)
{
	int icl_ua = 0, fcc_ua = 0, step_icl_ua = 0;
	int soc = 0, vbat = 0, fcc = 0;
	int iout = 0, vout = 0;
	int dc_status = 0, quiet_temp = 0;
	int rc = 0;

	rc = p9415_get_prop_voltage_now(chip, &vout);
	if (rc) {
		return;
	}
	chip->vout = vout;

	if (((chip->mode & P9415_MODE_EPP_BIT) &&
		(vout > DCIN_EPP_VOUT_TAG_UV) &&
		(vout < DCIN_VALID_EPP_VOL_UV)) ||
		(!(chip->mode & P9415_MODE_EPP_BIT) &&
		(vout < DCIN_VALID_BPP_VOL_UV))) {
		if (chip->vout_uv_cnt++ > MAX_VOUT_UV_CNT) {
			chip->vout_uv_err = true;
			chip->vout_uv_cnt = 0;
			chip->vout_uv_ua -= DCIN_UV_DOWN_STEP_UA;
			chip->vout_uv_ua = clamp(chip->vout_uv_ua,
						DCIN_UV_MIN_UA, chip->target_icl);
		}
	}
	vote(chip->dcin_icl_votable, DCIN_UV_VOTER,
		chip->vout_uv_err, chip->vout_uv_ua);

	rc = p9415_get_prop_current_now(chip, &iout);
	if (rc) {
		return;
	}
	chip->iout = iout;

	p9415_get_battery_voltage(chip, &vbat);
	p9415_get_battery_current(chip, &fcc);
	p9415_get_bms_real_soc(chip, &soc);
	p9415_get_dc_status(chip, &dc_status);
	p9415_get_quiet_temp(chip, &quiet_temp);

	if ((chip->real_soc != soc || soc <= 100 || vout >= 4500000)
		&& ((dc_status & 0xC0) == 0x40)) {
		vote(chip->dcin_icl_votable, DCIN_ICL_VOTER,
			true, DCIN_ICL_MIN_UA);
		p9415_reset_dcin(chip);
	}
	chip->real_soc = soc;

	/* handle quiet thermal OverTemp. */
	if (quiet_temp > lvl1) {
		fcc_ua = (chip->mode & P9415_MODE_EPP_BIT) ?
					DCIN_QUIET_LVL1_FCC_EPP_UA :
					DCIN_QUIET_LVL1_FCC_BPP_UA;
	} else if ((quiet_temp > lvl2) &&
			((quiet_temp < (lvl1 - 30)))) {
		fcc_ua = (chip->mode & P9415_MODE_EPP_BIT) ?
					DCIN_QUIET_LVL2_FCC_EPP_UA :
					DCIN_QUIET_LVL2_FCC_BPP_UA;
	} else {
		fcc_ua = chip->target_fcc;
	}
	vote(chip->fcc_votable, DCIN_OT_VOTER, true, fcc_ua);

	/* step up charge for only BPP charger. */
	if (!(chip->mode & P9415_MODE_EPP_BIT)) {
		step_icl_ua = get_client_vote(chip->dcin_icl_votable,
									DCIN_STEP_VOTER);
		if (step_icl_ua < 0) {
			step_icl_ua = DCIN_ICL_MIN_UA;
		} else if (step_icl_ua < chip->target_icl) {
			step_icl_ua += DCIN_UP_STEP_UA;
		}
		step_icl_ua = clamp(step_icl_ua, DCIN_ICL_MIN_UA, chip->target_icl);
		vote(chip->dcin_icl_votable, DCIN_STEP_VOTER, true, step_icl_ua);
	} else {
		vote(chip->dcin_icl_votable, DCIN_STEP_VOTER, false, 0);
	}

	/*  1: EPP & BPP must limit icl to 100mA as some TX has problem;
            2: Limit only ICL for BPP TX for platform UV at high battery voltage level. */
	if (p9415_is_full_charged(chip)) {
		icl_ua = DCIN_ICL_MIN_UA;
	} else {
		icl_ua = chip->target_icl;
		if (!(chip->mode & P9415_MODE_EPP_BIT)) {
			if (soc > DCIN_ICL_SOC_LVL1) {
				icl_ua = DCIN_ICL_SOC_LVL1_UA;
			} else if (soc > DCIN_ICL_SOC_LVL2) {
				icl_ua = DCIN_ICL_SOC_LVL2_UA;
			}
		}
	}
	vote(chip->dcin_icl_votable, DCIN_SOC_VOTER, true, icl_ua);

	vote(chip->dcin_icl_votable, DCIN_ICL_VOTER, true, chip->target_icl);
	vote(chip->fcc_votable, DCIN_FCC_VOTER, true, chip->target_fcc);
	p9415_dbg(DBG_ERR,"[%d,%d,%d,%d,%d,%x,%d,%d,%d,%d,%d]:%d,%d,%d,%d\n",
			chip->tx_watt, quiet_temp, soc, vbat, fcc, dc_status,
			get_effective_result_locked(chip->fv2_votable), chip->vout_uv_cnt,
			chip->vout_uv_err, chip->vout_uv_ua, step_icl_ua,
			vout, iout, get_effective_result_locked(chip->dcin_icl_votable),
			get_effective_result_locked(chip->fcc_votable));
}

static int p9415_init_rx_hw(struct p9415 *chip)
{
	int rc;

	might_sleep();
	rc = p9415_read_chipid(chip);
	if (rc) {
		return rc;
	}

	rc = p9415_read_mode(chip);
	if (rc ||
		((chip->mode & P9415_MODE_RX_BIT)
		!= P9415_MODE_RX_BIT)) {
		p9415_dbg(DBG_ERR, "invalid mode: %x\n",
					chip->mode);
		rc = -EINVAL;
		return rc;
	}

	rc = p9415_read_config_version(chip);
	if (rc) {
		return rc;
	}

	chip->iout_up_delta = DCIN_5W_IOUT_UP_DELTA_UA;
	chip->iout_down_delta = DCIN_5W_IOUT_DOWN_DELTA_UA;
	if (chip->mode & P9415_MODE_EPP_BIT) {
		p9415_set_comcap(chip, P9415_COMCAP_MASK);

		rc = p9415_read_tx_power(chip);
		if (rc) {
			if (!chip->pg_invalid)
				chip->target_icl = DCIN_5W_ICL_UA;
			else
				chip->target_icl = DCIN_5W_WEAK_ICL_UA;
			chip->target_fcc = DCIN_5W_FCC_UA;
			chip->tx_watt = DCIN_5W;
		} else {
			if (chip->tx_watt < DCIN_10W) {
				if (!chip->pg_invalid)
					chip->target_icl = DCIN_5W_ICL_UA;
				else
					chip->target_icl = DCIN_5W_WEAK_ICL_UA;
				chip->target_fcc = DCIN_5W_FCC_UA;
			} else if (chip->tx_watt < DCIN_15W) {
				if (!chip->pg_invalid) {
					chip->target_icl = DCIN_10W_ICL_UA;
					chip->target_fcc = DCIN_10W_FCC_UA;
				} else {
					chip->target_icl = DCIN_7P5W_ICL_UA;
					chip->target_fcc = DCIN_7P5W_FCC_UA;
				}
				chip->iout_up_delta = DCIN_10W_IOUT_UP_DELTA_UA;
				chip->iout_down_delta = DCIN_10W_IOUT_DOWN_DELTA_UA;
			} else {
				if (!chip->pg_invalid) {
					chip->target_icl = DCIN_15W_ICL_UA;
					chip->target_fcc = DCIN_15W_FCC_UA;
				} else {
					chip->target_icl = DCIN_10W_ICL_UA;
					chip->target_fcc = DCIN_10W_FCC_UA;
				}
				chip->iout_up_delta = DCIN_15W_IOUT_UP_DELTA_UA;
				chip->iout_down_delta = DCIN_15W_IOUT_DOWN_DELTA_UA;
			}
		}
	} else {
		if (!chip->pg_invalid)
			chip->target_icl = DCIN_5W_ICL_UA;
		else
			chip->target_icl = DCIN_5W_WEAK_ICL_UA;
		chip->target_fcc = DCIN_5W_FCC_UA;
		chip->tx_watt = DCIN_5W;
	}
	chip->vout_uv_ua = chip->target_icl;

	if (((chip->mode & P9415_MODE_EPP_BIT)
		&& (chip->config_ver != P9415_FW2_EPP_VER)
		&& epp_fods[P9415_RX_FOD_MAX_REGS]) ||
		(!(chip->mode & P9415_MODE_EPP_BIT)
		&& (chip->config_ver != P9415_FW2_BPP_VER)
		&& bpp_fods[P9415_RX_FOD_MAX_REGS]) ||
		force_fods) {
		p9415_write_rx_fods(chip);
		msleep(10);
	}
	return 0;
}

static void p9415_reset_current(struct p9415 *chip)
{
	vote(chip->dcin_icl_votable, DCIN_ICL_VOTER,
		true, DCIN_ICL_MIN_UA);
	vote(chip->dcin_icl_votable, DCIN_SOC_VOTER,
		false, 0);
	vote(chip->dcin_icl_votable, DCIN_FCC_VOTER,
		false, 0);
	vote(chip->fcc_votable, DCIN_FCC_VOTER,
		false, 0);
	vote(chip->fcc_votable, DCIN_OT_VOTER,
		false, 0);
	vote(chip->dcin_icl_votable, DCIN_STEP_VOTER,
		false, 0);
	chip->real_soc = -1;
	chip->vout = 0;
	chip->iout = 0;
	chip->int_irq_cnt = 0;
	chip->vout_uv_cnt = 0;
}

static void p9415_deinit(struct p9415 *chip)
{
	p9415_reset_current(chip);
	chip->mode = 0;
	chip->tx_watt = 0;
	chip->config_ver = 0;
	chip->dc_init_done = false;
	chip->dc_start = false;
	chip->iout_up_delta = 0;
	chip->iout_down_delta = 0;
	chip->vout_uv_err = false;
}

static void p9415_confirm_work(struct work_struct *work)
{
	struct p9415 *chip = container_of(work, struct p9415,
							confirm_work.work);
	bool dc_online = p9415_is_dc_online(chip);
	int rc;

	rc = p9415_read_chipid(chip);
	if (rc == -ENOTCONN) {
		atomic_set(&chip->dc_present, 0);
	} else {
		if (dc_online) {
			atomic_set(&chip->dc_present, 1);
		} else {
			atomic_set(&chip->dc_present, 0);
		}
	}

	if (chip->dc_psy) {
		p9415_dbg(DBG_ERR, "confirmed %d,%d\n", rc, dc_online);
		power_supply_changed(chip->dc_psy);
	}
}

static void p9415_status_change_work(struct work_struct *work)
{
	struct p9415 *chip = container_of(work, struct p9415,
					    status_change_work);
	bool dc_start = 0;
	bool dc_online = 0;
	int rc = 0;
	u64 time_gaps;
	ktime_t mtp_start_time;

	if (!p9415_is_psy_available(chip))
		goto out;

	if (!p9415_is_votable_available(chip))
		goto out;

	mutex_lock(&chip->mtp_lock);
	if (mtp_start > 0) {
		mtp_start_time = ktime_get();
		p9415_mtp_entry(chip);
		p9415_mtp_exit(chip);
		time_gaps = ktime_ms_delta(ktime_get(),
						mtp_start_time);
		p9415_dbg(DBG_MTP,
				"[%d][%lld]mtp status:%d\n",
				mtp_start, time_gaps,
				chip->mtp_status);
		mtp_start--;
		mutex_unlock(&chip->mtp_lock);
		goto out;
	}
	mutex_unlock(&chip->mtp_lock);

	dc_online = p9415_is_dc_online(chip);
	if (dc_online) {
		atomic_set(&chip->dc_present, 1);
	} else if (chip->dc_online) {
		rc = p9415_read_chipid(chip);
		if (rc == -ENOTCONN) {
			atomic_set(&chip->dc_present, 0);
		}
	}

	if (dc_online && !chip->dc_online) {
		p9415_dbg(DBG_ERR, "dc online\n");
		chip->dc_online = true;
		vote(chip->awake_votable, DC_AWAKE_VOTER, true, 0);
	} else if (!dc_online && chip->dc_online) {
		p9415_dbg(DBG_ERR, "dc offline\n");
		p9415_deinit(chip);
		chip->dc_online = false;
		vote(chip->awake_votable, DC_AWAKE_VOTER, false, 0);
		goto out;
	} else if (!dc_online && !chip->dc_online) {
		goto out;
	}

	if (!get_effective_result(chip->dc_suspend_votable)) {
		dc_start = true;
	} else {
		dc_start = false;
	}

	if (!dc_start) {
		if (chip->dc_start) {
			p9415_dbg(DBG_ERR, "dc stop\n");
			p9415_reset_current(chip);
			chip->dc_start = false;
		}
		goto out;
	} else if (!chip->dc_start) {
		p9415_dbg(DBG_ERR, "dc start\n");
		p9415_reset_current(chip);
		chip->dc_start = true;
	}

	if (!chip->dc_init_done) {
		rc = p9415_init_rx_hw(chip);
		if (rc) {
			goto out;
		} else {
			p9415_dbg(DBG_ERR, "dc init ok\n");
			chip->dc_init_done = true;
		}
	}

	p9415_adjust_rx_power(chip);

out:
	pm_relax(chip->dev);
	chip->status_change_running = false;
}

static int p9415_notifier_cb(struct notifier_block *nb,
			       unsigned long event, void *data)
{
	struct p9415 *chip = container_of(nb, struct p9415, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (!strcmp(psy->desc->name, "battery")
		|| !strcmp(psy->desc->name, "dc")) {
		spin_lock_irqsave(&chip->status_change_lock, flags);

		if (!chip->status_change_running) {
			chip->status_change_running = true;
			pm_stay_awake(chip->dev);

			p9415_dbg(DBG_DBG,
					"notifier: '%s'\n", psy->desc->name);

#if defined(CONFIG_TCT_PM7250_COMMON)
			queue_work(private_chg_wq, &chip->status_change_work);
#else
			schedule_work(&chip->status_change_work);
#endif
		}
		spin_unlock_irqrestore(&chip->status_change_lock, flags);
	}

	return NOTIFY_OK;
}

#if 1
static int p9415_awake_vote_callback(struct votable *votable, void *data,
			int awake, const char *client)
{
	return 0;
}
#else
static int p9415_awake_vote_callback(struct votable *votable, void *data,
			int awake, const char *client)
{
	struct p9415 *chip = data;

	if (awake)
		__pm_stay_awake(chip->awake_src);
	else
		__pm_relax(chip->awake_src);

	p9415_dbg(DBG_ERR, "client: %s awake: %d\n", client, awake);
	return 0;
}
#endif

static const struct power_supply_desc p9415_psy_desc = {
	.name = "wireless",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = p9415_psy_props,
	.num_properties = ARRAY_SIZE(p9415_psy_props),
	.get_property = p9415_get_prop,
	.set_property = p9415_set_prop,
	.property_is_writeable = p9415_prop_is_writeable,
};

static int p9415_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc;
	struct p9415 *chip;
	struct power_supply_config cfg = {0};

	if (!client || !client->adapter
		|| !client->adapter->algo
		|| !client->adapter->algo->functionality
		|| !i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA |
				I2C_FUNC_SMBUS_WORD_DATA)) {
		p9415_dbg(DBG_ERR, "smbus data not supported!\n");
		return -EIO;
	}

	if (client->irq <= 0) {
		p9415_dbg(DBG_ERR, "irq not defined (%d)\n", client->irq);
		return -EINVAL;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL) {
		p9415_dbg(DBG_ERR, "allocated mem failed\n");
		return -ENOMEM;
	}
	chip->dev = &client->dev;
	chip->int_irq = client->irq;
	chip->client = client;
	i2c_set_clientdata(client, chip);
	rc = p9415_parse_dt(chip);
	if (rc) {
		p9415_dbg(DBG_ERR,
				"Couldn't parse device tree rc=%d\n", rc);
		goto cleanup;
	}

	chip->regmap = devm_regmap_init_i2c(client, &p9415_regmap);
	if (IS_ERR(chip->regmap)) {
		rc = PTR_ERR(chip->regmap);
		dev_err(&client->dev, "regmap init failed: %d\n", rc);
		goto cleanup;
	}

/*
	rc = p9415_init_hw(chip);
	if (rc < 0) {
		p9415_dbg(DBG_ERROR, "Couldn't init p9415, rc=%d\n", rc);
		goto cleanup;
	}
*/

	cfg.drv_data = chip;
	cfg.of_node = chip->dev->of_node;
	chip->wls_psy = devm_power_supply_register(chip->dev, &p9415_psy_desc,
			&cfg);
	if (IS_ERR(chip->wls_psy)) {
		dev_err(&client->dev, "wls_psy registration failed: %d\n",
				PTR_ERR(chip->wls_psy));
		rc = PTR_ERR(chip->wls_psy);
		goto cleanup;
	}

	device_init_wakeup(chip->dev, true);
	mutex_init(&chip->mtp_lock);
	spin_lock_init(&chip->status_change_lock);

	chip->awake_src = wakeup_source_register(NULL, "p9415_ws");
	if (!chip->awake_src) {
		p9415_dbg(DBG_ERR, "Couldn't create awake_src\n");
		rc = -EINVAL;
		goto cleanup;
	}
	chip->awake_votable = create_votable("DC_AWAKE", VOTE_SET_ANY,
					p9415_awake_vote_callback, chip);
	if (IS_ERR(chip->awake_votable)) {
		p9415_dbg(DBG_ERR, "Couldn't create voter\n");
		rc = PTR_ERR(chip->awake_votable);
		chip->awake_votable = NULL;
		goto cleanup;
	}

	chip->wq = alloc_workqueue("p9415_wq", WQ_HIGHPRI, 0);
	if (!chip->wq) {
		p9415_dbg(DBG_ERR, "Couldn't alloc wq\n");
		goto cleanup;
	}
	INIT_DELAYED_WORK(&chip->confirm_work, p9415_confirm_work);
	INIT_WORK(&chip->status_change_work, p9415_status_change_work);

	chip->nb.notifier_call = p9415_notifier_cb;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		p9415_dbg(DBG_ERR,
				"Couldn't register psy notifier rc=%d\n", rc);
		goto cleanup;
	}

	rc = devm_request_threaded_irq(chip->dev, chip->int_irq, NULL,
					p9415_int_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"p9415_int_irq", chip);
	if (rc < 0) {
		p9415_dbg(DBG_ERR, "Couldn't request int irq %d\n",
				chip->int_irq);
		goto cleanup;
	}
	enable_irq_wake(chip->int_irq);

	rc = devm_request_threaded_irq(chip->dev, chip->pg_irq, NULL,
					p9415_pg_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"p9415_pg_irq", chip);
	if (rc < 0) {
		p9415_dbg(DBG_ERR, "Couldn't request pg irq %d\n",
				chip->pg_irq);
		goto cleanup;
	}
	enable_irq_wake(chip->pg_irq);

	if(!p9415_read_chipid(chip)) {
		atomic_set(&chip->dc_present, 1);
	} else {
		atomic_set(&chip->dc_present, 0);
	}
	dev_info(chip->dev, "probe HW ver1 done with %d\n",
			atomic_read(&chip->dc_present));
	return 0;

cleanup:
	if (chip->awake_votable) {
		destroy_votable(chip->awake_votable);
		chip->awake_votable = NULL;
	}
	if (chip->awake_src) {
		wakeup_source_unregister(chip->awake_src);
		chip->awake_src = NULL;
	}
	i2c_set_clientdata(client, NULL);
	return rc;
}

static int p9415_remove(struct i2c_client *client)
{
	struct p9415 *chip = i2c_get_clientdata(client);

	if (!chip) {
		p9415_dbg(DBG_ERR, "chip is null\n");
		return -ENODEV;
	}

	power_supply_unreg_notifier(&chip->nb);
	cancel_work_sync(&chip->status_change_work);

	device_init_wakeup(chip->dev, false);

	if (chip->int_irq > 0) {
		disable_irq(chip->int_irq);
		devm_free_irq(chip->dev, chip->int_irq, chip);
		chip->int_irq = 0;
	}

	if (chip->awake_votable) {
		destroy_votable(chip->awake_votable);
		chip->awake_votable = NULL;
	}
	if (chip->awake_src) {
		wakeup_source_unregister(chip->awake_src);
		chip->awake_src = NULL;
	}
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct of_device_id p9415_match_table[] = {
	{
		.compatible = "idt,p9415_15w",
		.data = (void *)P9415_15W_TYPE
	},
	{ }
};

static struct i2c_driver p9415_driver = {
	.driver = {
		.name = "p9415-driver",
		.owner = THIS_MODULE,
		.of_match_table = p9415_match_table,
	},
	.probe =    p9415_probe,
	.remove =   p9415_remove,
};

module_i2c_driver(p9415_driver);

MODULE_DESCRIPTION("P9415 driver");
MODULE_LICENSE("GPL v2");
