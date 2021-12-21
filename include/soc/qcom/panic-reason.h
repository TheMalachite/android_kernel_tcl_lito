#ifndef __PANIC_REASON_H__
#define __PANIC_REASON_H__

#ifdef CONFIG_PANIC_REASON
void append_panic_message(const char *fmt, ...);
void machine_die(const char *desc, int err, struct pt_regs *regs);
void machine_fault(const char *desc, unsigned long addr);
void machine_panic(const char *desc);
#else
static inline void append_panic_message(const char *fmt, ...) {}
static inline void machine_die(const char *desc, int err, struct pt_regs *regs) {}
static inline void machine_fault(const char *desc, unsigned long addr) {}
static inline void machine_panic(const char *desc) {}
#endif

#endif
