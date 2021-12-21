/*
 *
 */

#ifndef __TCL_HOTKEY_CATCH_H__
#define __TCL_HOTKEY_CATCH_H__
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/fs.h>

//#define DEBUG 1
extern int tcl_hkc_debug_en;

#define tcl_hkc_debug(fmt, args...)					\
do {									\
	if (tcl_hkc_debug_en)						\
		pr_notice("tct_hkc %s: " fmt, __func__ , ##args);	\
} while (0)

/* private message type
 * NLMSG_MIN_TYPE is 0x10, reserved.
 */
#define NLMSG_HKC_ENABLE	0x11
#define NLMSG_HKC_WC		0x12

enum hkc_cret {
	HKC_CRET_FAIL = 0,
	HKC_CRET_FAIL_STOP,
	HKC_CRET_MATCH
} ;

struct hkc_chk_point {
	enum hkc_cret (*key_check)(const struct sk_buff *skb);
	int (*key_caught_cb)(const struct sk_buff *skb);
	
	const char *name;
	struct list_head node;
	bool registered;
} ;

struct hkc_dev {
	struct cdev	cdev;
	struct device	*dev;
	dev_t		devnum;
};

int tcl_hotkey_caught_def_cb(int msg_type, char *payload, int len);
struct hkc_dev *hkc_device_create(const char *name,
	const struct file_operations *fop);
void hkc_device_destroy(struct hkc_dev *hkc);

void hkc_register_chk_point(struct hkc_chk_point *cp);
void hkc_unregister_chk_point(struct hkc_chk_point *cp);

static inline bool hkc_chk_point_registered(struct hkc_chk_point *cp)
{
	return cp->registered;
}

extern int __init hkc_wc_init(void);
extern void __exit hkc_wc_exit(void);

#endif // __TCL_HOTKEY_CATCH_H__
