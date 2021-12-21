#ifndef _TCT_PERF_H
#define _TCT_PERF_H

#include <linux/sched/task.h>
#include <linux/types.h>

#define TCT_PERF_MAGIC 't'

enum {
	SET_UITURBO = 1,
	GET_UITURBO,
	TCT_PERF_MAX_NR
};

enum {
	TURBO_NONE = 0,
	TURBO_UI,
};

#define TCT_PERF_SET_UITURBO \
	_IOWR(TCT_PERF_MAGIC, SET_UITURBO,  struct uiturbo_data)
#define TCT_PERF_GET_UITURBO \
	_IOR(TCT_PERF_MAGIC, GET_UITURBO, struct uiturbo_data)

struct uiturbo_data {
	pid_t tid;
	int turbo;
};

#endif
