/**
 *  History 1.0 added by hailong.chen for task 7136392 on 2018-11-20
 */

#define pr_fmt(fmt)	"TCT-BOOT-REASON: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/**
 * %PON_SMPL:		PON triggered by SMPL - Sudden Momentary Power Loss
 * %PON_RTC:		PON triggered by RTC alarm
 * %PON_DC_CHG:		PON triggered by insertion of DC charger
 * %PON_USB_CHG:	PON triggered by insertion of USB
 * %PON_PON1:		PON triggered by other PMIC (multi-PMIC option)
 * %PON_CBLPWR_N:	PON triggered by power-cable insertion
 * %PON_KPDPWR_N:	PON triggered by long press of the power-key
 */
static const char * const powerup_reason[] = {
	[0] = "reboot",
	[1] = "SMPL",
	[2] = "rtc",
	[3] = "DC",
	[4] = "usb_chg",
	[5] = "PON1",
	[6] = "CBL",
	[7] = "kpdpwr",
};

static int boot_reason_show(struct seq_file *m, void *v)
{
	int index;

	index = boot_reason - 1;
	if (index >= ARRAY_SIZE(powerup_reason) || index < 0)
		seq_printf(m, "%s\n", "others");
	else
		seq_printf(m, "%s\n", powerup_reason[index]);

	return 0;
}

static int boot_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, boot_reason_show, NULL);
}

static const struct file_operations boot_reason_proc_fops = {
	.open = boot_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init pon_init(void)
{
	pr_err("boot_reason = %d\n", boot_reason);
	proc_create("boot_reason", 0444, NULL, &boot_reason_proc_fops);

	return 0;
}
late_initcall(pon_init);

