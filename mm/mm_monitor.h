#ifndef __MM_MONITOR_H
#define __MM_MONITOR_H

#include <linux/types.h>
#include <linux/percpu.h>
#include <linux/mm.h>
#include <linux/atomic.h>
#include <linux/errno.h>

#define MM_MONITOR_INFO_LEN 512

extern void slowpath_alloc_count_inc(unsigned int order);

enum mm_monitor_event_item{
	MM_PAGECACHE_READ_HIT_COUNT,
	MM_PAGECACHE_READ_MIS_COUNT,
	MM_PAGECACHE_MMAP_HIT_COUNT,
	MM_PAGECACHE_MMAP_MIS_COUNT,
	MM_READAHEAD_READ_SYNC,
	MM_READAHEAD_READ_ASYNC,
	MM_READAHEAD_MMAP_SYNC,
	MM_READAHEAD_MMAP_ASYNC,
	MM_PAGE_ALLOC_FAILED,
	NR_MM_MONITOR_EVENT_ITEM
};


struct mm_monitor_event {
	unsigned long event[NR_MM_MONITOR_EVENT_ITEM];
};

DECLARE_PER_CPU(struct mm_monitor_event, mm_monitor_event_state);

static inline void inc_mm_monitor_event(enum mm_monitor_event_item item)
{
	this_cpu_inc(mm_monitor_event_state.event[item]);
}

static inline void dec_mm_monitor_event(enum mm_monitor_event_item item)
{
	this_cpu_dec(mm_monitor_event_state.event[item]);
}

static inline void add_mm_monitor_event(enum mm_monitor_event_item item,int value)
{
	this_cpu_add(mm_monitor_event_state.event[item],value);
}

#endif

