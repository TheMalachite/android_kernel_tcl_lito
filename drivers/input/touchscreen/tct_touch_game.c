#define pr_fmt(fmt)	"tct-touch:[%s:%d] " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>


#define GAME_ENABLE "enable"

static struct proc_dir_entry *game_policy_dir;
static struct proc_dir_entry *touch_dir;
static struct proc_dir_entry *smooth_dir;
static struct proc_dir_entry *sensitivity_dir;
static struct proc_dir_entry *accidental_touch_dir;
static struct proc_dir_entry *report_rate_dir;


static char game_mode_enable;
static unsigned int report_rate_cur_level = 1;
static unsigned int smooth_cur_level = 2;
static unsigned int sensitivity_cur_level = 3;
static unsigned int accidental_touch_level = 2;


void (*tct_game_touch_report_rate_set)(unsigned int rate);
void (*tct_game_mode_enable)(bool enable);
void (*tct_game_touch_smooth_set)(unsigned int level);
void (*tct_game_touch_sensitivity_set)(unsigned int level);
void (*tct_game_accidental_touch_set)(unsigned int level);

static ssize_t game_mode_enable_write (
    struct file *filp, const char __user *buff, size_t count, loff_t *ppos)
{
    unsigned long val;

	if (kstrtoul_from_user(buff, count, 0, &val))
		return -EFAULT;

	game_mode_enable = val;

    if (tct_game_mode_enable != NULL)
        tct_game_mode_enable(game_mode_enable);

    pr_info("game_mode_enable=%d", game_mode_enable);
	return count;
}

static ssize_t game_mode_enable_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "%d\n", game_mode_enable);
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static const struct file_operations game_enable_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = game_mode_enable_read,
    .write  = game_mode_enable_write,
};

static ssize_t smooth_available_levels_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "1 2 3\n");
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static ssize_t smooth_default_level_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "2\n");
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static ssize_t smooth_cur_level_write (
    struct file *filp, const char __user *buff, size_t count, loff_t *ppos)
{
    unsigned long val;

	if (kstrtoul_from_user(buff, count, 0, &val))
		return -EFAULT;

	smooth_cur_level = val;

    if (tct_game_touch_smooth_set != NULL)
        tct_game_touch_smooth_set(smooth_cur_level);

    pr_info("smooth_cur_level=%d", smooth_cur_level);
	return count;
}

static ssize_t smooth_cur_level_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "%d\n", smooth_cur_level);
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static const struct file_operations smooth_available_levels_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = smooth_available_levels_read,
    .write  = NULL,
};

static const struct file_operations smooth_cur_level_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = smooth_cur_level_read,
    .write  = smooth_cur_level_write,
};

static const struct file_operations smooth_default_level_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = smooth_default_level_read,
    .write  = NULL,
};

static ssize_t sensitivity_available_levels_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "1 2 3 4 5\n");
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static ssize_t sensitivity_default_level_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "3\n");
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static ssize_t sensitivity_cur_level_write (
    struct file *filp, const char __user *buff, size_t count, loff_t *ppos)
{
    unsigned long val;

	if (kstrtoul_from_user(buff, count, 0, &val))
		return -EFAULT;

	sensitivity_cur_level = val;

    if (tct_game_touch_sensitivity_set != NULL)
        tct_game_touch_sensitivity_set(sensitivity_cur_level);

    pr_info("sensitivity_cur_level=%d", sensitivity_cur_level);
	return count;
}

static ssize_t sensitivity_cur_level_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "%d\n", sensitivity_cur_level);
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static const struct file_operations sensitivity_available_levels_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = sensitivity_available_levels_read,
    .write  = NULL,
};

static const struct file_operations sensitivity_cur_level_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = sensitivity_cur_level_read,
    .write  = sensitivity_cur_level_write,
};

static const struct file_operations sensitivity_default_level_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = sensitivity_default_level_read,
    .write  = NULL,
};

static ssize_t accidental_touch_available_levels_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "1 2 3\n");
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static ssize_t accidental_touch_default_level_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "2\n");
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static ssize_t accidental_touch_cur_level_write (
    struct file *filp, const char __user *buff, size_t count, loff_t *ppos)
{
    unsigned long val;

	if (kstrtoul_from_user(buff, count, 0, &val))
		return -EFAULT;

	accidental_touch_level = val;

    if (tct_game_accidental_touch_set != NULL)
        tct_game_accidental_touch_set(accidental_touch_level);

    pr_info("accidental_touch_level=%d", accidental_touch_level);
	return count;
}

static ssize_t accidental_touch_cur_level_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "%d\n", accidental_touch_level);
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static const struct file_operations accidental_touch_available_levels_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = accidental_touch_available_levels_read,
    .write  = NULL,
};

static const struct file_operations accidental_touch_cur_level_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = accidental_touch_cur_level_read,
    .write  = accidental_touch_cur_level_write,
};

static const struct file_operations accidental_touch_default_level_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = accidental_touch_default_level_read,
    .write  = NULL,
};

static ssize_t report_rate_available_levels_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "1 2 3\n");
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static ssize_t report_rate_default_level_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "1\n");
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}

static ssize_t report_rate_cur_level_write (
    struct file *filp, const char __user *buff, size_t count, loff_t *ppos)
{
    unsigned long val;

	if (kstrtoul_from_user(buff, count, 0, &val))
		return -EFAULT;

	report_rate_cur_level = val;

    if (tct_game_touch_report_rate_set != NULL)
        tct_game_touch_report_rate_set(report_rate_cur_level);

    pr_info("report_rate_cur_level=%d", report_rate_cur_level);
	return count;
}

static ssize_t report_rate_cur_level_read (
    struct file *filp, char __user *buff, size_t count, loff_t *ppos)
{
    int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	tot = scnprintf(bp, sizeof(bp), "%d\n", report_rate_cur_level);
	if (copy_to_user(buff, bp, tot))
	    return -EFAULT;

	*ppos += tot;

	return tot;
}


static const struct file_operations report_rate_available_levels_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = report_rate_available_levels_read,
    .write  = NULL,
};

static const struct file_operations report_rate_cur_level_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = report_rate_cur_level_read,
    .write  = report_rate_cur_level_write,
};

static const struct file_operations report_rate_default_level_proc_fops = {
    .owner  = THIS_MODULE,
    .read   = report_rate_default_level_read,
    .write  = NULL,
};

static int __init tct_touch_game_init(void)
{
    struct proc_dir_entry *proc_entry;

    pr_info("%s", __func__);
	game_policy_dir = proc_mkdir("game_policy", NULL);
	if (!game_policy_dir) {
		pr_err("failed to create tct game_policy proc entry");
		return -ENOMEM;
	}

    touch_dir = proc_mkdir("touch", game_policy_dir);
	if (!touch_dir) {
		pr_err("failed to create tct game_policy proc entry");
		return -ENOMEM;
	}

    smooth_dir = proc_mkdir("smooth", touch_dir);
	if (!smooth_dir) {
		pr_err("failed to create tct smooth proc entry");
		return -ENOMEM;
	}
    sensitivity_dir = proc_mkdir("sensitivity", touch_dir);
	if (!sensitivity_dir) {
		pr_err("failed to create tct sensitivity proc entry");
		return -ENOMEM;
	}
    accidental_touch_dir = proc_mkdir("accidental_touch", touch_dir);
	if (!accidental_touch_dir) {
		pr_err("failed to create tct accidental_touch proc entry");
		return -ENOMEM;
	}
    report_rate_dir = proc_mkdir("report_rate", touch_dir);
	if (!report_rate_dir) {
		pr_err("failed to create tct report_rate proc entry");
		return -ENOMEM;
	}

    proc_entry = proc_create(GAME_ENABLE, 0777, touch_dir, &game_enable_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }

    /*Ceat smooth function file*/
    proc_entry = proc_create("available_levels", 0777, smooth_dir, &smooth_available_levels_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }
    proc_entry = proc_create("cur_level", 0777, smooth_dir, &smooth_cur_level_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }
    proc_entry = proc_create("default_level", 0777, smooth_dir, &smooth_default_level_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }

    /*Creat sensitivity function file*/
    proc_entry = proc_create("available_levels", 0777, sensitivity_dir, &sensitivity_available_levels_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }
    proc_entry = proc_create("cur_level", 0777, sensitivity_dir, &sensitivity_cur_level_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }
    proc_entry = proc_create("default_level", 0777, sensitivity_dir, &sensitivity_default_level_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }

    /*Creat accidental_touch function file*/
    proc_entry = proc_create("available_levels", 0777, accidental_touch_dir, &accidental_touch_available_levels_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }
    proc_entry = proc_create("cur_level", 0777, accidental_touch_dir, &accidental_touch_cur_level_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }
    proc_entry = proc_create("default_level", 0777, accidental_touch_dir, &accidental_touch_default_level_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }

    /*Creat report_rate function file*/
    proc_entry = proc_create("available_levels", 0777, report_rate_dir, &report_rate_available_levels_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }
    proc_entry = proc_create("cur_level", 0777, report_rate_dir, &report_rate_cur_level_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }
    proc_entry = proc_create("default_level", 0777, report_rate_dir, &report_rate_default_level_proc_fops);
    if (NULL == proc_entry) {
        pr_err("create proc entry fail");
        return -ENOMEM;
    }

	return 0;
}

static void __exit tct_touch_game_exit(void) {
	 pr_info("%s", __func__);
}

module_init(tct_touch_game_init);
module_exit(tct_touch_game_exit);
MODULE_AUTHOR("haojun.chen");
MODULE_DESCRIPTION("tct touch game api");
MODULE_LICENSE("GPL");