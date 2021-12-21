#include <linux/cred.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/vmstat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/math64.h>
#include <linux/writeback.h>
#include <linux/compaction.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include "mm_monitor.h"

#define FIRST_APP_UID KUIDT_INIT(10000)
#define LAST_APP_UID  KUIDT_INIT(19999)

#define VISIBLE_APP_ADJ    (100)
#define FOREGROUND_APP_ADJ (0)

atomic_long_t slowpath_alloc_count[MAX_ORDER] = {ATOMIC_LONG_INIT(0)};

DEFINE_PER_CPU(struct mm_monitor_event, mm_monitor_event_state)= {{0}};
EXPORT_PER_CPU_SYMBOL(mm_monitor_event_state);


static void mm_monitor_events_sum(unsigned long *total)
{
	int i, cpu;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		struct mm_monitor_event *per_cpu = &per_cpu(mm_monitor_event_state, cpu);

		for (i = 0; i < NR_MM_MONITOR_EVENT_ITEM; i++)
			total[i] += per_cpu->event[i];
	}
	put_online_cpus();
}


static int is_foreground_task(struct task_struct* task)
{
	kuid_t uid;
	int adj;
	struct mm_struct *mm = NULL;

	adj = task->signal->oom_score_adj;
	mm = task->mm;
	uid = task_uid(task);

	if (!mm || uid_lt(uid, FIRST_APP_UID) || uid_gt(uid, LAST_APP_UID))
		return 0;
	
	if (adj == VISIBLE_APP_ADJ || adj == FOREGROUND_APP_ADJ)
		return 1;

	return 0;
}


void slowpath_alloc_count_inc(unsigned int order){
	if (! is_foreground_task(current))
		return;

	if (order > MAX_ORDER-1)
			order = MAX_ORDER-1;
	
	atomic_long_inc(&slowpath_alloc_count[order]);
}
EXPORT_SYMBOL(slowpath_alloc_count_inc);


static int mm_monitor_show(struct seq_file *s, void *data)
{
	int i;
	unsigned long *mm_monitor_event_total = NULL;

	mm_monitor_event_total = kzalloc(sizeof(struct mm_monitor_event), GFP_KERNEL);
	if (mm_monitor_event_total == NULL) {
		pr_err("mmonitor_buf is invalid\n");
		return -EINVAL;
	}
	mm_monitor_events_sum(mm_monitor_event_total);

	for(i = 0; i < MAX_ORDER; i++){
		seq_printf(s,
		"slowpath%d:%ld\n",
		i,atomic_long_read(&slowpath_alloc_count[i]));
	}

	seq_printf(s,
		"page alloc failed: %ld\n"
		"fcache hit: %ld\n"
		"fcache miss: %ld\n"
		"fcache read hit: %ld\n"
		"fcache mmap hit: %ld\n"
		"fcache read miss: %ld\n"
		"fcache mmap miss: %ld\n"
		"readahead mmap sync pages: %ld\n"
		"readahead mmap async pages: %ld\n",
		mm_monitor_event_total[MM_PAGE_ALLOC_FAILED],
		mm_monitor_event_total[MM_PAGECACHE_READ_HIT_COUNT]+
		mm_monitor_event_total[MM_PAGECACHE_MMAP_HIT_COUNT],
		mm_monitor_event_total[MM_PAGECACHE_READ_MIS_COUNT]+
		mm_monitor_event_total[MM_PAGECACHE_MMAP_MIS_COUNT],
		mm_monitor_event_total[MM_PAGECACHE_READ_HIT_COUNT],
		mm_monitor_event_total[MM_PAGECACHE_MMAP_HIT_COUNT],
		mm_monitor_event_total[MM_PAGECACHE_READ_MIS_COUNT],
		mm_monitor_event_total[MM_PAGECACHE_MMAP_MIS_COUNT],
		mm_monitor_event_total[MM_READAHEAD_MMAP_SYNC],
		mm_monitor_event_total[MM_READAHEAD_MMAP_ASYNC]);

	kfree(mm_monitor_event_total);

	return 0;
}

static int mm_monitor_open(struct inode *inode, struct file *file)
{
	return single_open(file, mm_monitor_show, NULL);
}

static const struct file_operations mm_monitor_file_operations = {
	.owner = THIS_MODULE,
	.open = mm_monitor_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init mm_monitor_init(void)
{
#ifdef CONFIG_PROC_FS
	proc_create("mm_monitor", S_IRUGO, NULL,
			&mm_monitor_file_operations);
#endif
	return 0;
}

module_init(mm_monitor_init);
MODULE_LICENSE("GPL");
