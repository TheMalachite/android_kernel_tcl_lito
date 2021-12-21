/*
 * netlink interface
 * Copyright (C) 2016 Goodix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/types.h>
#include <net/sock.h>
#include <net/netlink.h>
#include "gf_spi.h"

#define NETLINK_TEST 25
#define MAX_MSGSIZE 32

static int pid = -1;
static struct sock *nl_sk;

void gf_log_printf(gf_log_level level, const char *tag, const char *fmt, ...)
{
    /* Prepare the storage. */
    va_list ap;
    static char uevent_env_buf[128];
    char *ptr = uevent_env_buf;
    int available = sizeof(uevent_env_buf);

    /* Fill logging level. */
    available -= sprintf(uevent_env_buf, "GF_LOG=%1d", level);
    ptr += strlen(uevent_env_buf);

    /* Fill logging message. */
    va_start(ap, fmt);
    vsnprintf(ptr, available, fmt, ap);
    va_end(ap);

    switch (level) {
    case GF_LOG_LEVEL_ERR:
        printk(KERN_ERR GF_DRV_NAME": %s\n", ptr);
        break;
    case GF_LOG_LEVEL_WRN:
        printk(KERN_WARNING GF_DRV_NAME": %s\n", ptr);
        break;
    case GF_LOG_LEVEL_INF:
        printk(KERN_INFO GF_DRV_NAME": %s\n", ptr);
        break;
    case GF_LOG_LEVEL_DBG:
    case GF_LOG_LEVEL_VBS:
    default:
        printk(KERN_DEBUG GF_DRV_NAME": %s\n", ptr);
        break;
    }
}


int sendnlmsg(char *msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int len = NLMSG_SPACE(MAX_MSGSIZE);
	int ret = 0;

	if (!msg || !nl_sk || !pid)
		return -ENODEV;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	nlh = nlmsg_put(skb, 0, 0, 0, MAX_MSGSIZE, 0);
	if (!nlh) {
		kfree_skb(skb);
		return -EMSGSIZE;
	}

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = 0;

	memcpy(NLMSG_DATA(nlh), msg, sizeof(char));
	GF_LOGD("send message: %d\n", *(char *)NLMSG_DATA(nlh));

	ret = netlink_unicast(nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret > 0)
		ret = 0;

	return ret;
}

static void nl_data_ready(struct sk_buff *__skb)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	char str[100];

	skb = skb_get(__skb);
	if (skb->len >= NLMSG_SPACE(0)) {
		nlh = nlmsg_hdr(skb);

		memcpy(str, NLMSG_DATA(nlh), sizeof(str));
		pid = nlh->nlmsg_pid;

		kfree_skb(skb);
	}

}


int netlink_init(void)
{
	struct netlink_kernel_cfg netlink_cfg;

	memset(&netlink_cfg, 0, sizeof(struct netlink_kernel_cfg));

	netlink_cfg.groups = 0;
	netlink_cfg.flags = 0;
	netlink_cfg.input = nl_data_ready;
	netlink_cfg.cb_mutex = NULL;

	nl_sk = netlink_kernel_create(&init_net, NETLINK_TEST,
			&netlink_cfg);

	if (!nl_sk) {
		GF_LOGE("create netlink socket error\n");
		return 1;
	}

	return 0;
}

void netlink_exit(void)
{
	if (nl_sk != NULL) {
		netlink_kernel_release(nl_sk);
		nl_sk = NULL;
	}

	GF_LOGI("self module exited\n");
}
