/*
 * bootmsg.c - keep the first BOOTMSG_BUFFER_SZ bytes of kernel log to memory
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/delay.h>


//default save 2M  boot log
#define BOOTMSG_BUFFER_SZ (1 << 22)
#define RELEASE_RETRY_DELAY (msecs_to_jiffies(60 * MSEC_PER_SEC))

static long  bootmsg_buffer_phys = 0x0;
static char *bootmsg_buffer = NULL;
static int   bootmsg_len    = 0;

static bool should_release_console = false;



#define RETRY_DELAY_CYZ (msecs_to_jiffies(60 * MSEC_PER_SEC))  //60 is ok

static void bootmsg_console_write(struct console *console, const char *s, unsigned int count)
{
   int len = min(count, (unsigned int) (BOOTMSG_BUFFER_SZ - bootmsg_len));
   if (len) {
      memcpy(bootmsg_buffer + bootmsg_len, s, len);
      bootmsg_len += len;
   }

   if (bootmsg_len >= BOOTMSG_BUFFER_SZ) {
      console->flags &= ~CON_ENABLED;
      should_release_console = true;
   }
}

static struct console bootmsg_console = {
    .name   = "bootmsg",
    .write  = bootmsg_console_write,
    .flags  = CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
    .index  = -1,
};

static ssize_t bootmsg_knob_read(struct file *f, char __user *buf,size_t count, loff_t *ppos)
{
   return simple_read_from_buffer(buf, count, ppos, bootmsg_buffer, bootmsg_len);
}

static const struct file_operations bootmsg_fops = {
   .open   = simple_open,
   .read   = bootmsg_knob_read,
};


static void release_console_work_fn(struct work_struct *work)
{
   if (should_release_console) {
      unregister_console(&bootmsg_console);
      pr_info("bootmsg is disabled (bootmsg_buffer_phys=%08lx)\n",bootmsg_buffer_phys);
   } else {
      schedule_delayed_work(container_of(work, struct delayed_work, work),RELEASE_RETRY_DELAY);
   }
}

static DECLARE_DELAYED_WORK(release_console_work, release_console_work_fn);


static int __init bootmsg_release_console_work_init(void)
{
   schedule_delayed_work(&release_console_work, RELEASE_RETRY_DELAY);

   return 0;
}
late_initcall(bootmsg_release_console_work_init);

static int __init bootmsg_console_init(void)
{
   WARN_ON(BOOTMSG_BUFFER_SZ % PAGE_SIZE);
   bootmsg_buffer = kzalloc(BOOTMSG_BUFFER_SZ, GFP_KERNEL);

   if (bootmsg_buffer) {
      bootmsg_buffer_phys = virt_to_phys(bootmsg_buffer);
      printk("bootmsg is enable (phys=%08lx)\n",bootmsg_buffer_phys);
      register_console(&bootmsg_console);
   } else {
      pr_err("%s: fail to alloc buffer\n", __func__);
   }

   if (!proc_create("bootmsg", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH ), NULL,&bootmsg_fops)){
      printk("bootmsg: creat bootmsg  fail!\n");
      return 0;
   }

   return 0;
}

core_initcall(bootmsg_console_init);

