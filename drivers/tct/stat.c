/*
 * Copyright (C) 2020 Tcl Corporation Limited
 * Author: wu-yan@tcl.com
 * Date: 2020/08/26
 * This module is used to collect delay data like ui iowait
 * and export through procfs.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/cpumask.h>
#include <linux/uaccess.h>
#include <trace/events/sched.h>
#include <linux/tct/uiturbo.h>

enum {
	UI_IOWAIT,		/* iowait for ui thread */
	UI_BLOCKED,		/* D state for ui thread */
	UI_RUNNABLE,		/* runnable for ui thread */
	NR_TCT_STAT,
};

#define HIST_SLOTS (12)
typedef struct {
	u64 min;
	u64 max;
	u64 sum;
	u64 count;
	u64 hist[HIST_SLOTS];
} tstat_t;

static inline void tstat_init(tstat_t * stat)
{
	memset(stat, 0, sizeof(*stat));
	stat->min = -1ULL;
}

static inline void tstat_add(tstat_t * stat, u64 value)
{
	int i;
	stat->min = min(stat->min, value);
	stat->max = max(stat->max, value);
	stat->sum += value;
	stat->count++;
	i = min(fls64(value >> 20), HIST_SLOTS - 1);
	stat->hist[i]++;
}

static inline void tstat_sum(tstat_t * dst, tstat_t * src)
{
	int i;
	if (!src->count)
		return;
	dst->min = min(dst->min, src->min);
	dst->max = max(dst->max, src->max);
	dst->sum += src->sum;
	dst->count += src->count;
	for (i = 0; i < HIST_SLOTS; i++) {
		dst->hist[i] += src->hist[i];
	}
}

struct tctstat_data {
	tstat_t __percpu *cpu_stat;
	struct proc_dir_entry *dir;
	unsigned long enabled;
};

static struct tctstat_data tsd;
/**
 * tctstat_account_delay -- record the statistics information of system
 * @value the value to record when event occur
 * @type  the type of event to record
 */
static inline void tctstat_account_delay(u64 value, int type)
{
	tstat_t *stat;
	stat = &this_cpu_ptr(tsd.cpu_stat)[type];
	tstat_add(stat, value);
}

static void
tctstat_account_iowait(void *ignore, struct task_struct *p, u64 delay)
{
	if (task_is_ui(p) && test_bit(UI_IOWAIT, &tsd.enabled)) {
		tctstat_account_delay(delay, UI_IOWAIT);
	}
}

static void
tctstat_account_blocked(void *ignore, struct task_struct *p, u64 delay)
{
	if (task_is_ui(p) && !p->in_iowait &&
	    test_bit(UI_BLOCKED, &tsd.enabled)) {
		tctstat_account_delay(delay, UI_BLOCKED);
	}
}

static void
tctstat_account_runnable(void *ignore, struct task_struct *p, u64 delay)
{
	if (task_is_ui(p) && test_bit(UI_RUNNABLE, &tsd.enabled)) {
		tctstat_account_delay(delay, UI_RUNNABLE);
	}
}

/* /proc/tct_stat/{iowait,blocked,runnable} */

/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

#define SPLIT_NS(x) nsec_high(x), nsec_low(x)

#define PROC_FILE_SHOW_DEFN(name, type)				\
static int tctstat_##name##_show(struct seq_file *m, void *v)		\
{									\
	int cpu, i;							\
	tstat_t stat, *cstat;						\
	bool enabled;							\
	u64 avg;							\
	tstat_init(&stat);						\
	enabled = test_and_clear_bit(type, &tsd.enabled);		\
	for_each_possible_cpu(cpu) {					\
		cstat = &per_cpu_ptr(tsd.cpu_stat, cpu)[type];		\
		tstat_sum(&stat, cstat);				\
	}								\
	if (enabled)							\
		set_bit(type, &tsd.enabled);				\
	if (!stat.count)						\
		stat.min = 0;						\
	avg = stat.sum;							\
	if (stat.count)							\
		do_div(avg, stat.count);				\
	seq_printf(m, "min: %lld.%06ld\n", SPLIT_NS(stat.min));		\
	seq_printf(m, "max: %lld.%06ld\n", SPLIT_NS(stat.max));		\
	seq_printf(m, "avg: %lld.%06ld\n", SPLIT_NS(avg)); 		\
	seq_printf(m, "sum: %lld.%06ld\n", SPLIT_NS(stat.sum));		\
	seq_printf(m, "count: %lld\n", stat.count);			\
	for (i = 0; i < HIST_SLOTS - 1; i++) {				\
		seq_printf(m, "<%dms: %u\n", 1 << i, stat.hist[i]);	\
	}								\
	seq_printf(m, ">=%dms: %u\n", 1 << (i - 1), stat.hist[i]);	\
	return 0;							\
}									\
									\
static int tctstat_##name##_open(struct inode *inode,			\
				  struct file *file)			\
{									\
	return single_open(file, tctstat_##name##_show,			\
			    PDE_DATA(inode));				\
}									\
									\
static ssize_t								\
tctstat_##name##_write(struct file *file,				\
			const char __user * buffer,			\
			size_t count, loff_t * ppos)			\
{									\
	int value = -1, cpu, i;						\
	char input[4];							\
	bool enabled;							\
	tstat_t *stat;							\
	if (count >= sizeof(input))					\
		return -EINVAL;						\
	if (copy_from_user(input, buffer, count))			\
		return -EFAULT;						\
	input[count] = '\0';						\
	sscanf(input, "%d", &value);					\
	switch (value) {						\
	case 0:		/* disable */					\
		clear_bit(type, &tsd.enabled);				\
		break;							\
	case 1:		/* enable */					\
		set_bit(type, &tsd.enabled);				\
		break;							\
	case 2:		/* reset */					\
		enabled = test_and_clear_bit(type, &tsd.enabled);	\
		for_each_possible_cpu(cpu) {				\
			stat = per_cpu_ptr(tsd.cpu_stat, cpu);		\
			for (i = 0; i < NR_TCT_STAT; i++) {		\
				tstat_init(&stat[i]);			\
			}						\
		}							\
		if (enabled)						\
			set_bit(type, &tsd.enabled);			\
		break;							\
	default:							\
		return -EINVAL;						\
	}								\
	return count;							\
}									\
									\
static const struct file_operations tctstat_##name##_fops = {		\
	.open = tctstat_##name##_open,					\
	.read = seq_read,						\
	.write = tctstat_##name##_write,				\
	.llseek = seq_lseek,						\
	.release = single_release,					\
};

PROC_FILE_SHOW_DEFN(iowait, UI_IOWAIT);
PROC_FILE_SHOW_DEFN(blocked, UI_BLOCKED);
PROC_FILE_SHOW_DEFN(runnable, UI_RUNNABLE);

static int __init tctstat_init(void)
{
	int cpu;
	tsd.cpu_stat = __alloc_percpu(NR_TCT_STAT * sizeof(tstat_t),
				      __alignof__(tstat_t));
	if (!tsd.cpu_stat) {
		pr_err("out of memory!\n");
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		tstat_t *stat = per_cpu_ptr(tsd.cpu_stat, cpu);
		int i;
		for (i = 0; i < NR_TCT_STAT; i++) {
			tstat_init(&stat[i]);
		}
	}

	register_trace_sched_stat_iowait(tctstat_account_iowait, NULL);
	register_trace_sched_stat_blocked(tctstat_account_blocked, NULL);
	register_trace_sched_stat_wait(tctstat_account_runnable, NULL);

	tsd.dir = proc_mkdir("tct_stat", NULL);
	if (!tsd.dir) {
		pr_err("failed to create tct_stat proc entry");
		free_percpu(tsd.cpu_stat);
		tsd.cpu_stat = NULL;
		return -ENOMEM;
	}
	proc_create_data("iowait", 0644, tsd.dir, &tctstat_iowait_fops, NULL);
	proc_create_data("blocked", 0644, tsd.dir, &tctstat_blocked_fops, NULL);
	proc_create_data("runnable", 0644, tsd.dir, &tctstat_runnable_fops,
			 NULL);
	set_bit(UI_IOWAIT, &tsd.enabled);
	set_bit(UI_BLOCKED, &tsd.enabled);
	set_bit(UI_RUNNABLE, &tsd.enabled);

	return 0;
}

static void __exit tctstat_exit(void)
{
	clear_bit(UI_IOWAIT, &tsd.enabled);
	clear_bit(UI_BLOCKED, &tsd.enabled);
	clear_bit(UI_RUNNABLE, &tsd.enabled);

	unregister_trace_sched_stat_iowait(tctstat_account_iowait, NULL);
	unregister_trace_sched_stat_blocked(tctstat_account_blocked, NULL);
	unregister_trace_sched_stat_wait(tctstat_account_runnable, NULL);
	tracepoint_synchronize_unregister();

	if (tsd.cpu_stat) {
		remove_proc_subtree("tct_stat", NULL);
		free_percpu(tsd.cpu_stat);
		tsd.cpu_stat = NULL;
	}
}

module_init(tctstat_init);
module_exit(tctstat_exit);

MODULE_AUTHOR("yanwu <wu-yan@tcl.com>");
MODULE_DESCRIPTION("tct statistics driver");
MODULE_LICENSE("GPL");
