#include <linux/skbuff.h>
#include <linux/sysctl.h>
#include <linux/bitops.h>
#include <net/sock.h>
#include <linux/file.h>
#include <linux/netlink.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

#include <linux/types.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#include "tct_hkc.h"


/* tcl netfilter hook num */
#define NETLINK_TCL_HOTKEY	28

#ifdef  DEBUG
int tcl_hkc_debug_en = 1;
#else
int tcl_hkc_debug_en = 0;
#endif

#define TCL_HKC_DEBUG() tcl_hkc_debug("\n")

static DEFINE_MUTEX(hkc_mutex);
static struct ctl_table_header *hkc_ctl;

static DEFINE_SPINLOCK(list_lock);
static LIST_HEAD(hkc_chk_point_list);

static void nf_cancel_func(struct work_struct *work);
static struct sock *tcl_hotkey_nl_sock;
static u32 hkc_pid = 0;
static int hkc_enable = 0;
static int hkc_poke = 0;
static DECLARE_WORK(nf_cancel_work, nf_cancel_func);
static DEFINE_MUTEX(nf_mutex);

static struct class *hkc_class;

/* simple scheme: restrict at most 10 devices to be registered ... */
static dev_t devnum;
static int dev_cnt = 0;

struct hkc_dev *hkc_device_create(const char *name, const struct file_operations *fop)
{
	int ret;
	struct hkc_dev *hkc;

	if (IS_ERR(hkc_class))
		return NULL;

	if (dev_cnt >= 10)
		return NULL;

	hkc = kmalloc(sizeof(struct hkc_dev), GFP_KERNEL);
	if (hkc == NULL)
		return NULL;

	hkc->devnum = devnum + dev_cnt;

	cdev_init(&hkc->cdev, fop);
	hkc->cdev.owner = THIS_MODULE;
	ret = cdev_add(&hkc->cdev, hkc->devnum, 1);
	if (ret < 0) {
		kfree(hkc);
		return NULL;
	}

	hkc->dev = device_create(hkc_class, NULL, hkc->devnum, NULL, name);
	if (IS_ERR(hkc->dev)) {
		cdev_del(&hkc->cdev);
		kfree(hkc);
		return NULL;
	}

	dev_cnt++;

	return hkc;
}

void hkc_device_destroy(struct hkc_dev *hkc)
{
	if (hkc == NULL)
		return ;

	if (IS_ERR(hkc_class))
		return ;

	device_destroy(hkc_class, hkc->devnum);
	cdev_del(&hkc->cdev);
	kfree(hkc);
}

/* if this failed, it failed,
 * there is little you can do about the failue
 * SO DON'T
 */
int tcl_hotkey_caught_def_cb(int msg_type, char *payload, int len)
{
	int ret = 0;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;

	skb = nlmsg_new(len, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	/* always success, because we just alloc
	 * the appropriate skbuff successfully
	 */
	nlh = __nlmsg_put(skb, 0, 0, msg_type, len, 0);

	memcpy(nlmsg_data(nlh), payload, len);

	/* set control field,sender's pid */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0))
	NETLINK_CB(skb).pid = 0;
#else
	NETLINK_CB(skb).portid = 0;
#endif
	NETLINK_CB(skb).dst_group = 0;

	if (hkc_enable && hkc_pid) {
		/* erorr may be:
		 * ECONNREFUSED from netlink_getsockbyportid
		 */
		ret = netlink_unicast(tcl_hotkey_nl_sock, skb, hkc_pid, MSG_DONTWAIT);
		if (ret < 0)
			tcl_hkc_debug("netlink unicast error %d\n", ret);
	} else {
		kfree(skb);
		ret = -ECONNREFUSED;
		tcl_hkc_debug("hkc disabled!\n");
	}

	if (ret == -ECONNREFUSED) {
		schedule_work(&nf_cancel_work);
	}

	return ret;
}

/* to register check point by client */
static const char *chk_point_def_name = "unspecified";

void hkc_register_chk_point(struct hkc_chk_point *cp)
{
	unsigned long flags;
	spin_lock_irqsave(&list_lock, flags);
	list_add_tail(&cp->node, &hkc_chk_point_list);
	cp->registered = true;
	if (cp->name == NULL)
		cp->name = chk_point_def_name;
	spin_unlock_irqrestore(&list_lock, flags);
}

void hkc_unregister_chk_point(struct hkc_chk_point *cp)
{
	unsigned long flags;
	spin_lock_irqsave(&list_lock, flags);
	list_del_init(&cp->node);
	cp->registered = false;
	if (cp->name == chk_point_def_name)
		cp->name = NULL;
	spin_unlock_irqrestore(&list_lock, flags);
}

static unsigned int hotkey_detect(void *priv,
	struct sk_buff *skb, const struct nf_hook_state *state)
{
	struct hkc_chk_point *pos;
	enum hkc_cret ret;

	list_for_each_entry(pos, &hkc_chk_point_list, node) {
		ret = pos->key_check(skb);
		if (ret == HKC_CRET_FAIL)
			continue;

		if (ret == HKC_CRET_MATCH) {
			/* notify user-space a specific pattern caught ...
			 * poke mode is used to debugging, not signal is sent to user-space
			 */
			if (!hkc_poke) {
				pos->key_caught_cb(skb);
			} else {
				tcl_hkc_debug("%s\n", pos->name);
			}
		}

		break;
	}

	/* must not affect anything,
	 * just return NF_ACCEPT ...
	 */
	return NF_ACCEPT;
}

static struct nf_hook_ops tcl_hotkey_nf_ops[] __read_mostly = {
	{
		.hook		= hotkey_detect,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP_PRI_FILTER + 1,
	},
};

static struct ctl_table tcl_hotkey_sysctl_table[] = {
	{
		.procname	= "tcl_hkc_debug_en",
		.data		= &tcl_hkc_debug_en,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "tcl_hkc_poke",
		.data		= &hkc_poke,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "tcl_hkc_enable",
		.data		= &hkc_enable,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "tcl_hkc_pid",
		.data		= &hkc_pid,
		.maxlen		= sizeof(u32),
		.mode		= 0444,
		.proc_handler	= proc_dointvec,
	},
	{ }
};

static void nf_cancel_func(struct work_struct *work)
{
	mutex_lock(&nf_mutex);
	if (hkc_enable)
		nf_unregister_net_hooks(&init_net,
			tcl_hotkey_nf_ops, ARRAY_SIZE(tcl_hotkey_nf_ops));
	hkc_pid = 0;
	hkc_enable = 0;
	mutex_unlock(&nf_mutex);
}

static inline int hkc_set_pid(struct sk_buff *skb, struct nlmsghdr *nlh)
{
	int temp;

	temp = !!*(int *)NLMSG_DATA(nlh);
	
	mutex_lock(&nf_mutex);
	if (hkc_enable != temp) {
		if (temp) {
			int error;
			error = nf_register_net_hooks(&init_net,
					tcl_hotkey_nf_ops, ARRAY_SIZE(tcl_hotkey_nf_ops));
			if (error) {
				pr_err("%s: failed to register netfilter\n", __FUNCTION__);
				return error;
			}

		} else {
			nf_unregister_net_hooks(&init_net,
				tcl_hotkey_nf_ops, ARRAY_SIZE(tcl_hotkey_nf_ops));
		}
	}

	hkc_enable = temp;
	hkc_pid = NETLINK_CB(skb).portid;
	mutex_unlock(&nf_mutex);
	
	tcl_hkc_debug("pid = %d, enable: %d\n", hkc_pid, hkc_enable);

	return 0;
}

//static int tcl_hotkey_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh)
static int tcl_hotkey_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh, struct netlink_ext_ack *extack)
{
	int ret = 0;

	TCL_HKC_DEBUG();

	switch (nlh->nlmsg_type) {
	case NLMSG_HKC_ENABLE:
		/* error will xmit nlmsg ack to user */
    		ret = hkc_set_pid(skb, nlh);
    		break;
    	default:
    		return -EINVAL;
	}

	return ret;
}

static void tcl_hotkey_rcv(struct sk_buff *skb)
{
	mutex_lock(&hkc_mutex);
	netlink_rcv_skb(skb, &tcl_hotkey_rcv_msg);
	mutex_unlock(&hkc_mutex);
}

static int __init tcl_hotkey_catch_init(void)
{
	int error = 0;

	struct netlink_kernel_cfg cfg = {
		.input	= tcl_hotkey_rcv,
	};

	error = alloc_chrdev_region(&devnum, 0, 10, "tcl-hkc");
	if (error)
		goto out;

	hkc_class = class_create(THIS_MODULE, "tcl-hkc");
	if (IS_ERR(hkc_class)) {
		pr_err("%s: failed to create hkc class\n", __FUNCTION__);
		error = PTR_ERR(hkc_class);
		goto alloc_chrdev;
	}

	tcl_hotkey_nl_sock = netlink_kernel_create(&init_net, NETLINK_TCL_HOTKEY, &cfg);
	if (tcl_hotkey_nl_sock == NULL) {
		pr_err("%s: failed to create netlink.\n", __FUNCTION__);
		error = -1;
		goto class_failed;
	}

	hkc_ctl = register_net_sysctl(&init_net, "net/tcl_hkc",
				tcl_hotkey_sysctl_table);
	if (hkc_ctl == NULL) {
		pr_err("%s: failed to register net sysctl.\n", __FUNCTION__);
		error = -1;
		goto netlink_init;
	}

	error = hkc_wc_init();
	if (error) {
		pr_err("%s: failed to init hkc_wc.\n", __FUNCTION__);
		goto reg_net_sys;
	}

	return 0;

reg_net_sys:
	unregister_net_sysctl_table(hkc_ctl);
netlink_init:
	netlink_kernel_release(tcl_hotkey_nl_sock);
class_failed:
	class_destroy(hkc_class);
alloc_chrdev:
	unregister_chrdev_region(devnum, 10);
out:
	return error;
}

static void __exit tcl_hotkey_catch_exit(void)
{
	hkc_wc_exit();

	netlink_kernel_release(tcl_hotkey_nl_sock);
	unregister_net_sysctl_table(hkc_ctl);
	mutex_lock(&nf_mutex);
	if (hkc_enable)
		nf_unregister_net_hooks(&init_net,
			tcl_hotkey_nf_ops, ARRAY_SIZE(tcl_hotkey_nf_ops));
	mutex_unlock(&nf_mutex);
	class_destroy(hkc_class);
	unregister_chrdev_region(devnum, 10);
}

module_init(tcl_hotkey_catch_init);
module_exit(tcl_hotkey_catch_exit);

MODULE_AUTHOR("robin@tcl");
MODULE_DESCRIPTION("tcl hkc");
MODULE_LICENSE("GPL");
