#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/sched.h>
#include <asm/stacktrace.h>

#define SMEM_ID_VENDOR2 136
static char panic_message[1024] = {0};
static struct pt_regs panic_pt_regs;
static struct pt_regs *panic_pt_regs_ptr = NULL;

static void write_to_smem(void) {
    int ret;
    u32 *buf;
    size_t size = min(strlen(panic_message) + 1, sizeof(panic_message));

    ret = qcom_smem_alloc(QCOM_SMEM_HOST_ANY, SMEM_ID_VENDOR2, size);
    if (ret < 0) {
        pr_err("Failed to alloc SMEM_ID_VENDOR2 entry\n");
        return;
    }

    buf = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_ID_VENDOR2, &size);
    if (IS_ERR(buf)) {
        pr_err("Failed to acquire SMEM_ID_VENDOR2 entry\n");
        return;
    }

    memcpy(buf, panic_message, size);

    /*
    pr_notice("PANIC REASON:\n+++++\n");
    pr_notice("%s", panic_message);
    pr_notice("-----\n");
    */
}

void append_panic_message(const char *fmt, ...) {
    size_t size = sizeof(panic_message) - strlen(panic_message);
    va_list args;

    if (size <= 0)
        return;

    va_start(args, fmt);
    vsnprintf(panic_message + strlen(panic_message), size, fmt, args);
    va_end(args);
}

void machine_die(const char *desc, int err, struct pt_regs *regs) {
    append_panic_message("Internal error: %s: %x\n", desc, err);
    memcpy(&panic_pt_regs, regs, sizeof(struct pt_regs));
    panic_pt_regs_ptr = &panic_pt_regs;
}

void machine_fault(const char *desc, unsigned long addr) {
    append_panic_message("Unable to handle kernel %s at virtual address %016lx\n", desc, addr);
}

void machine_panic(const char *desc) {
    struct stackframe frame;
    struct task_struct *tsk = current;
    struct pt_regs *regs = panic_pt_regs_ptr;
    int skip = 0;

    append_panic_message("Kernel panic - not syncing: %s\n", desc);

    append_panic_message("CPU: %d PID: %d Comm: %.20s\n",
            raw_smp_processor_id(), tsk->pid, tsk->comm);

    if (regs != NULL) {
		append_panic_message("pc : %pS\n", (void *)regs->pc);
#ifdef CONFIG_ARM64
		append_panic_message("lr : %pS\n", (void *)regs->regs[30]);
#endif
        append_panic_message("sp : %016llx\n", regs->sp);

        skip = 1;
    }

    if (!try_get_task_stack(tsk))
        return;

    frame.fp = (unsigned long)__builtin_frame_address(0);
    frame.pc = (unsigned long)machine_panic;

    append_panic_message("Call trace:\n");

    do {
        if (!skip) {
            append_panic_message(" %pS\n", (void *)frame.pc);
        } else if (frame.fp == GET_FP(regs)) {
            skip = 0;
            append_panic_message(" %pS\n", (void *)regs->pc);
        }
    } while (!unwind_frame(tsk, &frame));

    put_task_stack(tsk);

    write_to_smem();

    panic_pt_regs_ptr = NULL;
}

