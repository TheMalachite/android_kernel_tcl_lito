#ifndef _TCT_UI_TURBO_H_
#define _TCT_UI_TURBO_H_
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/tct/perf.h>
#include <linux/version.h>
#include <linux/ioprio.h>
#include <linux/bio.h>

#define FIRST_APP_UID KUIDT_INIT(10000)
#define LAST_APP_UID  KUIDT_INIT(19999)
#define FOREGROUND_APP_ADJ 0

static inline bool task_is_foreground(struct task_struct *p)
{
	kuid_t uid = task_uid(p);
	int adj = p->signal->oom_score_adj;
	struct mm_struct *mm = p->mm;
	if (!mm || uid_lt(uid, FIRST_APP_UID) || uid_gt(uid, LAST_APP_UID))
		return false;
	if (adj != FOREGROUND_APP_ADJ)
		return false;
	return true;
}

static inline bool task_is_ui(struct task_struct *p)
{
	return p->static_uiturbo == TURBO_UI;
}

static inline int uiturbo_set_boost(int boost)
{
	int old_boost = current->static_uiturbo;
	if (old_boost != boost)
		current->static_uiturbo = boost;
	return old_boost;
}

struct rq;
extern int uiturbo_enable;
static inline bool test_task_uiturbo(struct task_struct *p)
{
	if (!uiturbo_enable)
		return false;
	return p && (p->static_uiturbo == TURBO_UI ||
		     atomic64_read(&p->dynamic_uiturbo));
}

extern int uiturbo_max_depth;
static inline bool test_task_uiturbo_depth(int depth)
{
	return depth < uiturbo_max_depth;
}

static inline bool test_set_dynamic_uiturbo(struct task_struct *p)
{
	return test_task_uiturbo(p)
	    && test_task_uiturbo_depth(p->uiturbo_depth);
}

static inline void task_set_waiter(struct task_struct *p,
				   void *obj, enum wait_type type)
{
	raw_spin_lock(&p->uiturbo_lock);
	p->uiturbo_wo = obj;
	p->uiturbo_wt = type;
	raw_spin_unlock(&p->uiturbo_lock);
}

static inline void task_clear_waiter(struct task_struct *p)
{
	raw_spin_lock(&p->uiturbo_lock);
	p->uiturbo_wo = NULL;
	p->uiturbo_wt = WT_NONE;
	raw_spin_unlock(&p->uiturbo_lock);
}

static inline bool current_ioprio_high()
{
	return task_nice_ioclass(current) == IOPRIO_CLASS_RT ||
		test_task_uiturbo(current);
}

static inline bool bio_need_turbo(struct bio *bio)
{
	if (current_ioprio_high())
		return true;
	if (bio->bi_opf & (REQ_META | REQ_PRIO))
		return true;
	return false;
}

extern void enqueue_uiturbo_thread(struct rq *rq, struct task_struct *p);
extern void dequeue_uiturbo_thread(struct rq *rq, struct task_struct *p);
extern void pick_uiturbo_thread(struct rq *rq, struct task_struct **p,
				struct sched_entity **se);
extern bool test_dynamic_uiturbo(struct task_struct *p, int type);
extern void dynamic_uiturbo_dequeue(struct task_struct *p, int type);
extern void dynamic_uiturbo_enqueue(struct task_struct *p, int type, int depth);
extern void trigger_uiturbo_balance(struct rq *rq);
extern void uiturbo_init_rq(struct rq *rq);
#endif
