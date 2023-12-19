/*
 * xr806/up_cmd.c
 *
 * Copyright (c) 2022
 * Allwinner Technology Co., Ltd. <www.allwinner.com>
 * laumy <liumingyuan@allwinner.com>
 *
 * usrer protocol flow implementation for Xr806 drivers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include "os_intf.h"
#include "xradio.h"
#include "up_cmd.h"
#include "txrx.h"
#include "debug.h"

struct up_cmd_priv {
	struct sock *nlsk;
	void *dev_priv;
	u32 user_pid;
};

extern struct net init_net;

static struct up_cmd_priv cmd_priv = {
	.nlsk = NULL,
	.user_pid = 0,
};

#define XR_WLAN_NETLINK_ID 28


#define XR_NETLINK_TYPE_CMD 0
#define XR_NETLINK_TYPE_ACK  1

#define XR_NETLINK_PREFIX_LEN 3
#define XR_NETLINK_PREFIX_CMD "cmd"
#define XR_NETLINK_PREFIX_ACK "ack"

static int xradio_netlink_send_msg(u8 type, u8 *buff, u32 len)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
	int total_len;

	if (!cmd_priv.nlsk || !cmd_priv.user_pid)
		return -EINVAL;

	total_len = XR_NETLINK_PREFIX_LEN + len;

	skb = nlmsg_new(total_len, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	nlh = nlmsg_put(skb, 0, 0, XR_WLAN_NETLINK_ID, total_len, 0);

	if (type == XR_NETLINK_TYPE_CMD) {

		memcpy(NLMSG_DATA(nlh),
				XR_NETLINK_PREFIX_CMD, XR_NETLINK_PREFIX_LEN);

		memcpy(NLMSG_DATA(nlh) + XR_NETLINK_PREFIX_LEN, buff, len);

	} else if (type == XR_NETLINK_TYPE_ACK) {

		memcpy(NLMSG_DATA(nlh),
				XR_NETLINK_PREFIX_ACK, XR_NETLINK_PREFIX_LEN);
	}

	if (netlink_unicast(cmd_priv.nlsk,
				skb, cmd_priv.user_pid, MSG_DONTWAIT) < 0)
		cfg_printk(XRADIO_DBG_ERROR, "Error while sending back to user\n");

	return 0;
}

static void xradio_netlink_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	int ret;

	if (skb->len >= nlmsg_total_size(0)) {
		nlh = nlmsg_hdr(skb);

		cmd_priv.user_pid = nlh->nlmsg_pid;
		// TODO: User give pid for tx.
		if (nlh) {
			ret = xradio_tx_cmd_process(cmd_priv.dev_priv,
					NLMSG_DATA(nlh), nlh->nlmsg_len - NLMSG_HDRLEN);
			if (!ret)
				xradio_netlink_send_msg(XR_NETLINK_TYPE_ACK, NULL, 0);
			usleep_range(1500, 2000);
		}
	}
}

struct netlink_kernel_cfg xradio_up_cmd = {
	.input = xradio_netlink_recv_msg,
};

int xradio_up_cmd_init(void *priv)
{
	memset(&cmd_priv, 0, sizeof(struct up_cmd_priv));
	cmd_priv.nlsk =
		(struct sock *)netlink_kernel_create(&init_net, XR_WLAN_NETLINK_ID, &xradio_up_cmd);

	if (!cmd_priv.nlsk)
		return -ENOMEM;

	cmd_priv.dev_priv = priv;

	return 0;
}

int xradio_up_cmd_deinit(void *priv)
{
	if (cmd_priv.nlsk) {
		netlink_kernel_release(cmd_priv.nlsk);
		cmd_priv.nlsk = NULL;
		cmd_priv.user_pid = 0;
	}

	return 0;
}
int xradio_up_cmd_push(u8 *buff, u32 len)
{
	return xradio_netlink_send_msg(XR_NETLINK_TYPE_CMD, buff, len);
}


