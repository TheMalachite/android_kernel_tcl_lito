/*
 * elevator noop
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

#ifdef CONFIG_TCT_UI_TURBO
enum {
	TQ_UI,
	TQ_FG,
	NR_TQ,
};

struct turbo_queue {
	struct list_head fifo;
	int quantum;
	int nr_dispatched;
};
#endif

struct noop_data {
	struct list_head queue;
#ifdef CONFIG_TCT_UI_TURBO
	struct turbo_queue tqs[NR_TQ];
#endif
};

#ifdef CONFIG_TCT_UI_TURBO
/* dispatch ui:fg:other at 4:2:1 */
static const int dispatch_quantum[NR_TQ] = {
	4, 2,
};

static inline struct request *uiturbo_select_request(struct noop_data *nd)
{
	int i;
	struct turbo_queue *tq;
	struct request *rq;
	for (i = 0; i < NR_TQ; i++) {
		tq = &nd->tqs[i];
		if (!list_empty(&tq->fifo) &&
		    tq->nr_dispatched < tq->quantum) {
			tq->nr_dispatched++;
			return list_first_entry(&tq->fifo,
						struct request, queuelist);
		}
	}

	rq = list_first_entry_or_null(&nd->queue, struct request, queuelist);
	for (i = 0; i < NR_TQ; i++) {
		tq = &nd->tqs[i];
		tq->nr_dispatched = 0;
		if (!list_empty(&tq->fifo)) {
			struct request *tmp;
			tmp = list_first_entry(&tq->fifo,
					      struct request, queuelist);
			if (!rq || rq->fifo_time > tmp->fifo_time)
				rq = tmp;
		}
	}

	return rq;
}

static inline bool
uiturbo_add_request(struct noop_data *nd, struct request *rq)
{
	rq->fifo_time = ktime_get_ns();
	if (req_is_ui(rq)) {
		struct turbo_queue *tq = &nd->tqs[TQ_UI];
		list_add_tail(&rq->queuelist, &tq->fifo);
		return true;
	}

	if (req_is_fg(rq)) {
		struct turbo_queue *tq = &nd->tqs[TQ_FG];
		list_add_tail(&rq->queuelist, &tq->fifo);
		return true;
	}

	return false;
}

static inline bool
uiturbo_check_former_request(struct noop_data *nd, struct request *rq)
{
	int i;
	struct turbo_queue *tq;
	for (i = 0; i < NR_TQ; i++) {
		tq = &nd->tqs[i];
		if (rq->queuelist.prev == &tq->fifo)
			return true;
	}
	return false;
}

static inline bool
uiturbo_check_latter_request(struct noop_data *nd, struct request *rq)
{
	int i;
	struct turbo_queue *tq;
	for (i = 0; i < NR_TQ; i++) {
		tq = &nd->tqs[i];
		if (rq->queuelist.next == &tq->fifo)
			return true;
	}
	return false;
}

static inline void uiturbo_init_queue(struct noop_data *nd)
{
	int i;
	struct turbo_queue *tq;
	for (i = 0; i < NR_TQ; i++) {
		tq = &nd->tqs[i];
		INIT_LIST_HEAD(&tq->fifo);
		tq->quantum = dispatch_quantum[i];
		tq->nr_dispatched = 0;
	}
}

static inline void uiturbo_exit_queue(struct noop_data *nd)
{
	int i;
	struct turbo_queue *tq;
	for (i = 0; i < NR_TQ; i++) {
		tq = &nd->tqs[i];
		BUG_ON(!list_empty(&tq->fifo));
	}
}

static int uiturbo_may_queue(struct request_queue *q, unsigned int op)
{
	if (op & REQ_UI)
		return ELV_MQUEUE_MUST;
	return ELV_MQUEUE_MAY;
}
#endif

static void noop_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

static int noop_dispatch(struct request_queue *q, int force)
{
	struct noop_data *nd = q->elevator->elevator_data;
	struct request *rq;

#ifdef CONFIG_TCT_UI_TURBO
	rq = uiturbo_select_request(nd);
#else
	rq = list_first_entry_or_null(&nd->queue, struct request, queuelist);
#endif
	if (rq) {
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}

static void noop_add_request(struct request_queue *q, struct request *rq)
{
	struct noop_data *nd = q->elevator->elevator_data;

#ifdef CONFIG_TCT_UI_TURBO
	if (uiturbo_add_request(nd, rq))
		return ;
#endif
	list_add_tail(&rq->queuelist, &nd->queue);
}

static struct request *
noop_former_request(struct request_queue *q, struct request *rq)
{
	struct noop_data *nd = q->elevator->elevator_data;

#ifdef CONFIG_TCT_UI_TURBO
	if (uiturbo_check_former_request(nd, rq))
		return NULL;
#endif
	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_prev_entry(rq, queuelist);
}

static struct request *
noop_latter_request(struct request_queue *q, struct request *rq)
{
	struct noop_data *nd = q->elevator->elevator_data;

#ifdef CONFIG_TCT_UI_TURBO
	if (uiturbo_check_latter_request(nd, rq))
		return NULL;
#endif
	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_next_entry(rq, queuelist);
}

static int noop_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct noop_data *nd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = nd;

	INIT_LIST_HEAD(&nd->queue);
#ifdef CONFIG_TCT_UI_TURBO
	uiturbo_init_queue(nd);
#endif

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void noop_exit_queue(struct elevator_queue *e)
{
	struct noop_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
#ifdef CONFIG_TCT_UI_TURBO
	uiturbo_exit_queue(nd);
#endif
	kfree(nd);
}

static struct elevator_type elevator_noop = {
	.ops.sq = {
		.elevator_merge_req_fn		= noop_merged_requests,
		.elevator_dispatch_fn		= noop_dispatch,
		.elevator_add_req_fn		= noop_add_request,
		.elevator_former_req_fn		= noop_former_request,
		.elevator_latter_req_fn		= noop_latter_request,
		.elevator_init_fn		= noop_init_queue,
		.elevator_exit_fn		= noop_exit_queue,
#ifdef CONFIG_TCT_UI_TURBO
		.elevator_may_queue_fn		= uiturbo_may_queue,
#endif
	},
	.elevator_name = "noop",
	.elevator_owner = THIS_MODULE,
};

static int __init noop_init(void)
{
	return elv_register(&elevator_noop);
}

static void __exit noop_exit(void)
{
	elv_unregister(&elevator_noop);
}

module_init(noop_init);
module_exit(noop_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");
