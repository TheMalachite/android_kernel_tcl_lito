/*
 *
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uidgid.h>
#include <linux/errno.h>
#include <linux/capability.h>
#include <asm-generic/ioctl.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/bitops.h>

#include <net/inet_sock.h>
#include <uapi/linux/tcp.h>
#include <uapi/linux/ip.h>
#include <linux/tcp.h>
#include <net/ip.h>

#include "tct_hkc.h"

/*
 * ioctl cmd notation
 * 'w' -- the key type
 */

#define HKC_WC_MAGIC 'w'
struct keyword;

//[35705.351905] (4)[10574:insmod]HKC_WC_SET_ENABLE: 0x40047701
//[35705.351938] (4)[10574:insmod]HKC_WC_GET_ENABLE: 0x80047702
//[35705.351958] (4)[10574:insmod]HKC_WC_SET_UID: 0x40047703
//[35705.351979] (4)[10574:insmod]HKC_WC_GET_UID: 0x80047704
//[35705.351998] (4)[10574:insmod]HKC_WC_SET_KEY_1: 0x00007705

#define HKC_WC_SET_ENABLE	_IOW(HKC_WC_MAGIC, 1, int)
#define HKC_WC_GET_ENABLE	_IOR(HKC_WC_MAGIC, 2, int)
#define HKC_WC_SET_UID		_IOW(HKC_WC_MAGIC, 3, kuid_t)
#define HKC_WC_GET_UID		_IOR(HKC_WC_MAGIC, 4, kuid_t)

/* 0x40247705 */
#define HKC_WC_SET_KEY_1	_IOW(HKC_WC_MAGIC, 5, struct keyword)
#define HKC_WC_SET_KEY_2	_IOW(HKC_WC_MAGIC, 6, struct keyword)
#define HKC_WC_SET_KEY_3	_IOW(HKC_WC_MAGIC, 7, struct keyword)
#define HKC_WC_SET_KEY_4	_IOW(HKC_WC_MAGIC, 8, struct keyword)
#define HKC_WC_SET_KEY_5	_IOW(HKC_WC_MAGIC, 9, struct keyword)
#define HKC_WC_CLEAR_KEY	_IOW(HKC_WC_MAGIC, 10, struct keyword)

#define HKC_WC_SET_2PACK	_IOW(HKC_WC_MAGIC, 11, int)
#define HKC_WC_IOC_MAXNUM	11

#define KEYWORD_MAX_NUM		5
#define KEYWORD_MAX_LEN		20

struct keyword {
	int minlen;
	int maxlen;

	int offset;
	int len;

	char keyword[KEYWORD_MAX_LEN];
};

static struct keyword keytable[KEYWORD_MAX_NUM];
static unsigned long keysets;

static kuid_t wc_uid = INVALID_UID;
static struct hkc_dev *wc_dev;

#define WC_F_2PACK_MATCH	0x01
#define WC_F_MASK 		WC_F_2PACK_MATCH
static int features = 0;

static int wc_key_caught_cb(const struct sk_buff *skb);
static enum hkc_cret wc_key_check(const struct sk_buff *skb);
static struct hkc_chk_point wc_chk_point = {
	.key_check	= wc_key_check,
	.key_caught_cb	= wc_key_caught_cb,
	.name		= "hkc-wc",
	.node		= LIST_HEAD_INIT(wc_chk_point.node),
	.registered	= false,
};

static bool wc_uid_match(const struct sk_buff *skb)
{
	struct sock *sk;
	kuid_t sk_uid;

	sk = skb_to_full_sk(skb);
	if (NULL == sk || !sk_fullsock(sk) || NULL == sk->sk_socket) {
		return false;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0))
	filp = sk->sk_socket->file;
	if (NULL == filp) {
		return false;
	}
	sk_uid = filp->f_cred->fsuid;
#else
	sk_uid = sk->sk_uid;
#endif

	if (uid_eq(sk_uid, wc_uid))
		return true;

	return false;
}

static inline bool length_between(int len, int min, int max)
{
	return ((len >= min) && (len <= max));
}

/* check for red packet packet ... */
static enum hkc_cret wc_key_check(const struct sk_buff *skb)
{
	struct iphdr *iph = NULL;
	struct tcphdr *tcph = NULL;

	if (!wc_uid_match(skb))
		return HKC_CRET_FAIL;

	/* uid match, contniue to match key word */
	if ((iph = ip_hdr(skb)) != NULL && iph->protocol == IPPROTO_TCP) {
		int tot_len = 0;
		int _ip_hdrlen = 0;
		int _tcp_hdrlen = 0;

		tcph = tcp_hdr(skb);
		tot_len = ntohs(iph->tot_len);
		_ip_hdrlen = ip_hdrlen(skb);
		_tcp_hdrlen = tcp_hdrlen(skb);

		/* pure ACK won't be considerd ...*/
		if (!(tcph->ack && (tot_len == (_ip_hdrlen + _tcp_hdrlen)))) {
			static int first_packet_len = 0;
			static int pack = 0;
			int bit;

			for_each_set_bit(bit, &keysets, KEYWORD_MAX_NUM) {
				if (length_between(tot_len, keytable[bit].minlen, keytable[bit].maxlen)) {
					/* be assured that the lucky money is coming ... */

					if (memcmp(skb->data + keytable[bit].offset,
							keytable[bit].keyword, keytable[bit].len) == 0) {
						first_packet_len = 0;
						return HKC_CRET_MATCH;
					}
				}
			}

			if (features & WC_F_2PACK_MATCH) {
				if (first_packet_len == 0) {
					for_each_set_bit(bit, &keysets, KEYWORD_MAX_NUM) {
						if (tot_len > 1410 && !tcph->psh && tot_len < keytable[bit].minlen) {
							if (memcmp(skb->data + keytable[bit].offset,
									keytable[bit].keyword, keytable[bit].len) == 0) {
								first_packet_len = tot_len - (_ip_hdrlen + _tcp_hdrlen);
								pack = bit;
								goto out;
							}
						}
					}
				} else {
					int temp = first_packet_len + tot_len;
					if (length_between(temp, keytable[pack].minlen, keytable[pack].maxlen)) 
						return HKC_CRET_MATCH;

					first_packet_len = 0;
				}
			}
		}
	}

out:
	return HKC_CRET_FAIL_STOP;
}

static int wc_key_caught_cb(const struct sk_buff *skb)
{
	/* just notify user space, no need to send anything ...
	 * NLMSG_HKC_WC is the key ..
	 */
	return tcl_hotkey_caught_def_cb(NLMSG_HKC_WC, NULL, 0);
}

static long hkc_wc_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int val;

	if (_IOC_TYPE(cmd) != HKC_WC_MAGIC)
		return -ENOTTY;

	if (_IOC_NR(cmd) > HKC_WC_IOC_MAXNUM)
		return -ENOTTY;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	switch (cmd) {
		case HKC_WC_SET_ENABLE:
			ret = get_user(val, (int __user *)arg);
			if (val && !hkc_chk_point_registered(&wc_chk_point)) {
				hkc_register_chk_point(&wc_chk_point);
			} else if (!val && hkc_chk_point_registered(&wc_chk_point)) {
				hkc_unregister_chk_point(&wc_chk_point);
			}
			break;

		case HKC_WC_GET_ENABLE:
			if (hkc_chk_point_registered(&wc_chk_point))
				ret = put_user(1, (int __user *) arg);
			else
				ret = put_user(0, (int __user *) arg);
			break;

		case HKC_WC_SET_UID:
		{
			unsigned int uid;
			ret = get_user(uid, (unsigned int __user *)arg);
			wc_uid = make_kuid(&init_user_ns, uid);
			break;
		}
		case HKC_WC_GET_UID:
		{
			unsigned int uid;
			uid = __kuid_val(wc_uid);
			ret = put_user(uid, (unsigned int __user *)arg);
			break;
		}
		case HKC_WC_SET_KEY_1:
			ret = copy_from_user((void *)&keytable[0],
				(const void __user *)arg, sizeof(struct keyword));
			if (ret) {
				/* in case of error, need to invalid the keyword */
				ret = -EFAULT;
				clear_bit(0, &keysets);
			} else {
				set_bit(0, &keysets);
			}
			break;
		case HKC_WC_SET_KEY_2:
			ret = copy_from_user((void *)&keytable[1],
				(const void __user *)arg, sizeof(struct keyword));
			if (ret) {
				ret = -EFAULT;
				clear_bit(1, &keysets);
			} else {
				set_bit(1, &keysets);
			}
			break;
		case HKC_WC_CLEAR_KEY:
			ret = get_user(val, (int __user *)arg);
			if (!ret) {
				if (((unsigned int) val) >= KEYWORD_MAX_NUM)
					return -EINVAL;

				clear_bit(val, &keysets);
			}
			break;
		case HKC_WC_SET_2PACK:
			ret = get_user(val, (int __user *)arg);
			if (!ret) {
				if (val)
					features |= WC_F_2PACK_MATCH;
				else
					features &= ~WC_F_2PACK_MATCH;
			}
			break;
		default:	
			return -ENOTTY;
	}

	return ret;
}

const struct file_operations wc_fops = {
	.unlocked_ioctl	= hkc_wc_unlocked_ioctl,
};

int __init hkc_wc_init(void)
{
	wc_dev = hkc_device_create("hkc-wc", &wc_fops);
	if (wc_dev == NULL)
		return -1;

	return 0;
}

void __exit hkc_wc_exit(void)
{
	hkc_device_destroy(wc_dev);
}
