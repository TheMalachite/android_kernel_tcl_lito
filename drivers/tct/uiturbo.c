/*
 * Copyright (C) 2020 Tcl Corporation Limited
 * Author: wu-yan@tcl.com
 * Date: 2020/08/26
 * This module is used to implement uiturbo feature
 */
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <trace/events/sched.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/tct/perf.h>
#include "../../kernel/sched/sched.h"

int uiturbo_enable __read_mostly = 1;	/* control the uiturbo feature */
int uiturbo_sched_delay_granularity __read_mostly;	/* ui thread delay upper bound(ms) */
int dynamic_uiturbo_sched_granularity __read_mostly = 32;	/* dynamic ui max exist time(ms) */
int uiturbo_migration_delay __read_mostly = 10;	/* vip min migration delay time(ms) */
int uiturbo_max_depth __read_mostly = 5;

static inline bool entity_before(struct sched_entity *a, struct sched_entity *b)
{
	return (s64) (a->vruntime - b->vruntime) < 0;
}

void enqueue_uiturbo_thread(struct rq *rq, struct task_struct *p)
{
	if (!uiturbo_enable)
		return;
	if (!rq || !p || !list_empty(&p->ui_entry)) {
		return;
	}
	p->enqueue_time = rq->clock;
	if (p->static_uiturbo == TURBO_UI || atomic64_read(&p->dynamic_uiturbo)) {
		list_add_tail(&p->ui_entry, &rq->ui_thread_list);
		get_task_struct(p);
		trace_sched_uiturbo_event(p, "enqueue_succ");
	}
}

static inline bool dynamic_uiturbo_timeout(struct task_struct *p)
{
	u64 now = jiffies_to_nsecs(jiffies);
	s64 delta = (s64) (now - p->dynamic_uiturbo_start) >> 20;
	return delta >= dynamic_uiturbo_sched_granularity;
}

void dequeue_uiturbo_thread(struct rq *rq, struct task_struct *p)
{
	if (!rq || !p || list_empty(&p->ui_entry)) {
		return;
	}
	p->enqueue_time = 0;
	if (atomic64_read(&p->dynamic_uiturbo)
	    && dynamic_uiturbo_timeout(p)) {
		atomic64_set(&p->dynamic_uiturbo, 0);
	}
	list_del_init(&p->ui_entry);
	put_task_struct(p);
	trace_sched_uiturbo_event(p, "dequeue_succ");
}

static inline struct task_struct *pick_first_ui_thread(struct rq *rq)
{
	struct task_struct *task, *first_task = NULL;
	list_for_each_entry(task, &rq->ui_thread_list, ui_entry) {
		if (test_tsk_need_resched(task))
			continue;
		if (first_task == NULL) {
			first_task = task;
		} else if (entity_before(&task->se, &first_task->se)) {
			first_task = task;
		}
	}

	return first_task;
}

static inline bool check_ui_delayed(struct task_struct *p, u64 now)
{
	s64 delay;
	if (!p)
		return false;
	delay = (s64) (now - p->enqueue_time) >> 20;
	return delay >= uiturbo_sched_delay_granularity;
}

void
pick_uiturbo_thread(struct rq *rq, struct task_struct **p,
		    struct sched_entity **se)
{
	struct task_struct *tsk, *ui_tsk;
	if (!rq || !p || !se) {
		return;
	}
	tsk = *p;
	if (tsk && tsk->static_uiturbo != TURBO_UI
	    && !atomic64_read(&tsk->dynamic_uiturbo)) {
		if (!list_empty(&rq->ui_thread_list)) {
			ui_tsk = pick_first_ui_thread(rq);
			if (check_ui_delayed(ui_tsk, rq->clock)) {
				trace_sched_uiturbo_event(ui_tsk, "pick_ui");
				*p = ui_tsk;
				*se = &ui_tsk->se;
			}
		}
	}
}

#define DYNAMIC_UITURBO_SEC_WIDTH   8
#define DYNAMIC_UITURBO_MASK_BASE   0x00000000ff

#define dynamic_uiturbo_offset_of(type) (type * DYNAMIC_UITURBO_SEC_WIDTH)
#define dynamic_uiturbo_mask_of(type) \
  ((u64)(DYNAMIC_UITURBO_MASK_BASE) << (dynamic_uiturbo_offset_of(type)))
#define dynamic_uiturbo_get_bits(value, type) \
  ((value & dynamic_uiturbo_mask_of(type)) >> dynamic_uiturbo_offset_of(type))
#define dynamic_uiturbo_one(type) ((u64)1 << dynamic_uiturbo_offset_of(type))

bool test_dynamic_uiturbo(struct task_struct *p, int type)
{
	u64 turbo;
	if (unlikely(!p)) {
		return false;
	}
	turbo = atomic64_read(&p->dynamic_uiturbo);
	return dynamic_uiturbo_get_bits(turbo, (u32) type) > 0;
}

static inline u64 dynamic_uiturbo_dec(struct task_struct *task, int type)
{
	return atomic64_sub_return(dynamic_uiturbo_one((u32) type),
				   &task->dynamic_uiturbo);
}

static inline u64 dynamic_uiturbo_inc(struct task_struct *task, int type)
{
	return atomic64_add_return(dynamic_uiturbo_one((u32) type),
				   &task->dynamic_uiturbo);
}

static inline bool
test_task_exist(struct task_struct *task, struct list_head *head)
{
	struct task_struct *tmp;
	list_for_each_entry(tmp, head, ui_entry) {
		if (tmp == task) {
			return true;
		}
	}
	return false;
}

extern struct task_struct *rwsem_lock_owner(void *wo);
extern struct task_struct *mutex_lock_owner(void *wo);
/* get the owner of the lock which the task blocked on */
static inline struct task_struct *task_wait_owner(struct task_struct *p)
{
	switch (p->uiturbo_wt) {
	case WT_MUTEX:
		return mutex_lock_owner(p->uiturbo_wo);
	case WT_RWSEM:
		return rwsem_lock_owner(p->uiturbo_wo);
	default:
		return NULL;
	}
	return NULL;
}

static inline int task_wait_type(struct task_struct *p)
{
	switch (p->uiturbo_wt) {
	case WT_MUTEX:
		return DYNAMIC_UITURBO_MUTEX;
	case WT_RWSEM:
		return DYNAMIC_UITURBO_RWSEM;
	default:
		return DYNAMIC_UITURBO_MAX;
	}
	return DYNAMIC_UITURBO_MAX;
}

static inline void __dynamic_uiturbo_dequeue(struct task_struct *task, int type)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
	unsigned long flags;
#else
	struct rq_flags flags;
#endif
	struct rq *rq;
	u64 turbo;

	rq = task_rq_lock(task, &flags);
	turbo = atomic64_read(&task->dynamic_uiturbo);
	if (turbo == 0) {
		task_rq_unlock(rq, task, &flags);
		return;
	}
	turbo = dynamic_uiturbo_dec(task, type);
	if (turbo > 0) {
		task_rq_unlock(rq, task, &flags);
		return;
	}
	task->uiturbo_depth = 0;

	if (!list_empty(&task->ui_entry)) {
		trace_sched_uiturbo_event(task, "dynamic_uiturbo_dequeue_succ");
		list_del_init(&task->ui_entry);
		put_task_struct(task);
	}
	task_rq_unlock(rq, task, &flags);
}

void dynamic_uiturbo_dequeue(struct task_struct *task, int type)
{
	if (unlikely(!task || type >= DYNAMIC_UITURBO_MAX)) {
		return;
	}
	__dynamic_uiturbo_dequeue(task, type);
}

static void
__dynamic_uiturbo_enqueue(struct task_struct *task, int type, int depth)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
	unsigned long flags;
#else
	struct rq_flags flags;
#endif
	struct rq *rq;
	struct task_struct *p = NULL;

	rq = task_rq_lock(task, &flags);
	if (task->sched_class != &fair_sched_class) {
		task_rq_unlock(rq, task, &flags);
		return;
	}
	if (unlikely(!list_empty(&task->ui_entry))) {
		printk(KERN_WARNING "task(%s,%d,%d) is already in another list",
		       task->comm, task->pid, task->policy);
		task_rq_unlock(rq, task, &flags);
		return;
	}

	dynamic_uiturbo_inc(task, type);
	task->dynamic_uiturbo_start = jiffies_to_nsecs(jiffies);
	if (task->uiturbo_depth < depth + 1)
		task->uiturbo_depth = depth + 1;

	if (task->state == TASK_RUNNING) {
		get_task_struct(task);
		list_add_tail(&task->ui_entry, &rq->ui_thread_list);
		trace_sched_uiturbo_event(task, "dynamic_uiturbo_enqueue_succ");
	} else {
		raw_spin_lock(&task->uiturbo_lock);
		p = task_wait_owner(task);
		type = task_wait_type(task);
		raw_spin_unlock(&task->uiturbo_lock);
		++depth;
	}
	task_rq_unlock(rq, task, &flags);
	if (p && p->static_uiturbo != TURBO_UI &&
	    !atomic64_read(&p->dynamic_uiturbo) && depth < uiturbo_max_depth) {
		__dynamic_uiturbo_enqueue(p, type, depth);
	}
}

void dynamic_uiturbo_enqueue(struct task_struct *p, int type, int depth)
{
	if (!uiturbo_enable)
		return;
	if (unlikely(!p || type >= DYNAMIC_UITURBO_MAX)) {
		return;
	}
	__dynamic_uiturbo_enqueue(p, type, depth);
}

static inline bool check_ui_migration(struct task_struct *p, u64 now)
{
	s64 delta;
	if (!p)
		return false;
	delta = (s64) (now - p->enqueue_time) >> 20;
	return delta >= uiturbo_migration_delay;
}

static inline struct task_struct *pick_ui_thread_delayed(struct rq *rq)
{
	struct task_struct *p;

	list_for_each_entry(p, &rq->ui_thread_list, ui_entry) {
		if (check_ui_migration(p, rq->clock))
			return p;
	}
	return NULL;
}

static inline bool
ui_task_hot(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	s64 delta;

	lockdep_assert_held(&src_rq->lock);

	if (p->sched_class != &fair_sched_class)
		return false;

	if (unlikely(p->policy == SCHED_IDLE))
		return false;

	if (sched_feat(CACHE_HOT_BUDDY) && dst_rq->nr_running &&
	    (&p->se == src_rq->cfs.next || &p->se == src_rq->cfs.last))
		return true;

	if (sysctl_sched_migration_cost == (unsigned int)-1)
		return true;
	if (sysctl_sched_migration_cost == 0)
		return true;

	delta = src_rq->clock_task - p->se.exec_start;
	return delta < (s64) sysctl_sched_migration_cost;
}

static inline void
detach_task(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	trace_sched_uiturbo_event(p, "detach_task");
	lockdep_assert_held(&src_rq->lock);
	deactivate_task(src_rq, p, 0);
	p->on_rq = TASK_ON_RQ_MIGRATING;
	double_lock_balance(src_rq, dst_rq);
	set_task_cpu(p, dst_rq->cpu);
	double_unlock_balance(src_rq, dst_rq);
}

static inline void attach_task(struct rq *dst_rq, struct task_struct *p)
{
	trace_sched_uiturbo_event(p, "attach_task");
	raw_spin_lock(&dst_rq->lock);
	if (task_rq(p) != dst_rq) {
		raw_spin_unlock(&dst_rq->lock);
		return;
	}
	p->on_rq = TASK_ON_RQ_QUEUED;
	activate_task(dst_rq, p, 0);
	check_preempt_curr(dst_rq, p, 0);
	raw_spin_unlock(&dst_rq->lock);
}

static inline bool
uiturbo_can_migrate(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	if (task_running(src_rq, p))
		return false;
	if (ui_task_hot(p, src_rq, dst_rq))
		return false;
	if (task_rq(p) != src_rq)
		return false;
	if (!test_task_exist(p, &src_rq->ui_thread_list))
		return false;
	return true;
}

static int __do_uiturbo_balance(void *data)
{
	struct rq *src_rq = data;
	struct rq *dst_rq;
	int src_cpu = cpu_of(src_rq);
	int i;
	struct task_struct *p;
	bool do_migrate = false;

	/* find a delayed ui thread */
	raw_spin_lock_irq(&src_rq->lock);
	if (unlikely
	    (src_cpu != smp_processor_id()
	     || !src_rq->uiturbo_balance_active)) {
		src_rq->uiturbo_balance_active = false;
		raw_spin_unlock_irq(&src_rq->lock);
		return 0;
	}
	p = pick_ui_thread_delayed(src_rq);
	if (!p) {
		src_rq->uiturbo_balance_active = false;
		raw_spin_unlock_irq(&src_rq->lock);
		return 0;
	}

	/* find a free-cpu in the same cluster */
	raw_spin_unlock(&src_rq->lock);
	for_each_cpu_and(i, &(p->cpus_allowed), cpu_coregroup_mask(src_cpu)) {
		if (i == src_cpu || !cpu_online(i))
			continue;
		dst_rq = cpu_rq(i);
		raw_spin_lock(&dst_rq->lock);
		if (!dst_rq->rt.rt_nr_running
		    || list_empty(&dst_rq->ui_thread_list)) {
			raw_spin_unlock(&dst_rq->lock);
			break;
		} else {
			raw_spin_unlock(&dst_rq->lock);
		}
	}

	/* move p from src to dst cpu */
	raw_spin_lock(&src_rq->lock);
	if (i != nr_cpu_ids && p != NULL && dst_rq != NULL) {
		if (uiturbo_can_migrate(p, src_rq, dst_rq)) {
			detach_task(p, src_rq, dst_rq);
			do_migrate = true;
		}
	}
	src_rq->uiturbo_balance_active = false;
	raw_spin_unlock(&src_rq->lock);
	if (do_migrate) {
		attach_task(dst_rq, p);
	}
	local_irq_enable();
	return 0;
}

void trigger_uiturbo_balance(struct rq *rq)
{
	struct task_struct *p;
	bool balance_active = false;
	if (unlikely(!rq)) {
		return;
	}
	raw_spin_lock(&rq->lock);
	p = pick_ui_thread_delayed(rq);
	/*
	 * uiturbo_balance_active synchronized accesss to uiturbo_balance_work
	 * true means uiturbo_balance_work is ongoing, and reset to false after
	 * uiturbo_balance_work is done
	 */
	if (!rq->uiturbo_balance_active && p) {
		balance_active = true;
		rq->uiturbo_balance_active = true;
	}
	raw_spin_unlock(&rq->lock);
	if (balance_active) {
		stop_one_cpu_nowait(cpu_of(rq), __do_uiturbo_balance, rq,
				    &rq->uiturbo_balance_work);
	}
}

void uiturbo_init_rq(struct rq *rq)
{
	if (unlikely(!rq)) {
		return;
	}
	rq->uiturbo_balance_active = false;
	INIT_LIST_HEAD(&rq->ui_thread_list);
}
