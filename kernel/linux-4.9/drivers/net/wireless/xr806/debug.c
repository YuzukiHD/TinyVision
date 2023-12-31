/*
 * xr806/debug.c
 *
 * Copyright (c) 2022
 * Allwinner Technology Co., Ltd. <www.allwinner.com>
 * laumy <liumingyuan@allwinner.com>
 *
 * Debug info implementation for Xr806 drivers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#include <linux/debugfs.h>
#include "xr_version.h"
#include "os_intf.h"
#include "xradio.h"
#include "debug.h"

#define XRADIO_DBG_DEFAULT (XRADIO_DBG_ALWY | XRADIO_DBG_ERROR)// | XRADIO_DBG_WARN) //|XRADIO_DBG_MSG)

u8 dbg_common = XRADIO_DBG_DEFAULT;
u8 dbg_cfg = XRADIO_DBG_DEFAULT;
u8 dbg_iface = XRADIO_DBG_DEFAULT;
u8 dbg_hwio = XRADIO_DBG_DEFAULT;
u8 dbg_plat = XRADIO_DBG_DEFAULT;
u8 dbg_queue = XRADIO_DBG_DEFAULT;
u8 dbg_spi = XRADIO_DBG_DEFAULT;
u8 dbg_txrx = XRADIO_DBG_DEFAULT;
u8 dbg_cmd = XRADIO_DBG_DEFAULT;

/*for ip data*/
#define PF_TCP 0x0010
#define PF_UDP 0x0020
#define PF_DHCP 0x0040
#define PF_ICMP 0x0080

#define PF_IPADDR 0x2000 /*ip address of ip packets.*/
#define PF_UNKNWN 0x4000 /*print frames of unknown flag.*/
#define PF_RX 0x8000	 /*0:TX, 1:RX. So, need to add PF_RX in Rx path.*/

#ifndef IPPROTO_IP
#define IPPROTO_IP 0
#endif

#ifndef IPPROTO_ICMP
#define IPPROTO_ICMP 1
#endif

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif

#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif

#define ETH_P_IP 0x0800
#define ETH_P_ARP 0x0806
#define ETH_P_IPV6 0x86DD

#define PT_MSG_PUT(f, ...)                                                                         \
	do {                                                                                       \
		if (flags & (f))                                                                   \
			proto_msg += sprintf(proto_msg, __VA_ARGS__);                              \
	} while (0)

#define IS_PROTO_PRINT (proto_msg != (char *)&protobuf[0])

#define LLC_LEN 14
#define LLC_TYPE_OFF 12 /*Ether type offset*/
#define IP_PROTO_OFF 9	/*protocol offset*/
#define IP_S_ADD_OFF 12
#define IP_D_ADD_OFF 16
#define UDP_LEN 8
/*DHCP*/
#define DHCP_BOOTP_C 68
#define DHCP_BOOTP_S 67
#define UDP_BOOTP_LEN 236 /*exclude "Options:64"*/
#define BOOTP_OPS_LEN 64
#define DHCP_MAGIC 0x63825363
#define DHCP_DISCOVER 0x01
#define DHCP_OFFER 0x02
#define DHCP_REQUEST 0x03
#define DHCP_DECLINE 0x04
#define DHCP_ACK 0x05
#define DHCP_NACK 0x06
#define DHCP_RELEASE 0x07
/*ARP*/
#define ARP_REQUEST 0x0001
#define ARP_RESPONSE 0x0002
#define ARP_TYPE_OFFSET 6

/*IP/IPV6/ARP layer...*/
static inline bool is_ip(u8 *eth_data)
{
	/* 0x0800 */
	return (bool)(*(u16 *)(eth_data + LLC_TYPE_OFF) == cpu_to_be16(ETH_P_IP));
}

static inline bool is_ipv6(u8 *eth_data)
{
	/* 0x08dd */
	return (bool)(*(u16 *)(eth_data + LLC_TYPE_OFF) == cpu_to_be16(ETH_P_IPV6));
}

static inline bool is_arp(u8 *eth_data)
{
	/* 0x0806 */
	return (bool)(*(u16 *)(eth_data + LLC_TYPE_OFF) == cpu_to_be16(ETH_P_ARP));
}

static inline bool is_tcp(u8 *eth_data)
{
	return (bool)(eth_data[LLC_LEN + IP_PROTO_OFF] == IPPROTO_TCP);
}

static inline bool is_udp(u8 *eth_data)
{
	return (bool)(eth_data[LLC_LEN + IP_PROTO_OFF] == IPPROTO_UDP);
}

static inline bool is_icmp(u8 *eth_data)
{
	return (bool)(eth_data[LLC_LEN + IP_PROTO_OFF] == IPPROTO_ICMP);
}

static inline bool is_dhcp(u8 *eth_data)
{
	u8 *ip_hdr = eth_data + LLC_LEN;

	if (!is_ip(eth_data))
		return (bool)0;
	if (ip_hdr[IP_PROTO_OFF] == IPPROTO_UDP) {
		u8 *udp_hdr = ip_hdr + ((ip_hdr[0] & 0xf) << 2); /*ihl:words*/
		/* DHCP client or DHCP server*/
		return (bool)((((udp_hdr[0] << 8) | udp_hdr[1]) == DHCP_BOOTP_C) ||
			      (((udp_hdr[0] << 8) | udp_hdr[1]) == DHCP_BOOTP_S));
	}
	return (bool)0;
}

void xradio_parse_frame(u8 *eth_data, u8 tx, u32 flags)
{
	char protobuf[512] = {0};
	char *proto_msg = &protobuf[0];

	if (is_ip(eth_data)) {
		u8 *ip_hdr = eth_data + LLC_LEN;
		u8 *ipaddr_s = ip_hdr + IP_S_ADD_OFF;
		u8 *ipaddr_d = ip_hdr + IP_D_ADD_OFF;
		u8 *proto_hdr = ip_hdr + ((ip_hdr[0] & 0xf) << 2); /*ihl:words*/

		if (is_tcp(eth_data)) {
			PT_MSG_PUT(PF_TCP, "TCP%s%s, src=%d, dest=%d, seq=0x%08x, ack=0x%08x",
				   (proto_hdr[13] & 0x01) ? "(S)" : "",
				   (proto_hdr[13] & 0x02) ? "(F)" : "",
				   (proto_hdr[0] << 8) | proto_hdr[1],
				   (proto_hdr[2] << 8) | proto_hdr[3],
				   (proto_hdr[4] << 24) | (proto_hdr[5] << 16) |
					   (proto_hdr[6] << 8) | proto_hdr[7],
				   (proto_hdr[8] << 24) | (proto_hdr[9] << 16) |
					   (proto_hdr[10] << 8) | proto_hdr[11]);

		} else if (is_udp(eth_data)) {
			if (is_dhcp(eth_data)) {
				u8 Options_len = BOOTP_OPS_LEN;

				u32 dhcp_magic = cpu_to_be32(DHCP_MAGIC);
				u8 *dhcphdr = proto_hdr + UDP_LEN + UDP_BOOTP_LEN;

				while (Options_len) {
					if (*(u32 *)dhcphdr == dhcp_magic)
						break;
					dhcphdr++;
					Options_len--;
				}
				PT_MSG_PUT(PF_DHCP, "DHCP, Opt=%d, MsgType=%d", *(dhcphdr + 4),
					   *(dhcphdr + 6));
			} else {
				PT_MSG_PUT(PF_UDP, "UDP, source=%d, dest=%d",
					   (proto_hdr[0] << 8) | proto_hdr[1],
					   (proto_hdr[2] << 8) | proto_hdr[3]);
			}
		} else if (is_icmp(eth_data)) {
			PT_MSG_PUT(PF_ICMP, "ICMP%s%s, Seq=%d", (proto_hdr[0] == 8) ? "(ping)" : "",
				   (proto_hdr[0] == 0) ? "(reply)" : "",
				   (proto_hdr[6] << 8) | proto_hdr[7]);
		} else {
			PT_MSG_PUT(PF_UNKNWN, "unknown IP type=%d", *(ip_hdr + IP_PROTO_OFF));
		}
		if (IS_PROTO_PRINT) {
			PT_MSG_PUT(PF_IPADDR, "-%d.%d.%d.%d(s)", ipaddr_s[0], ipaddr_s[1],
				   ipaddr_s[2], ipaddr_s[3]);
			PT_MSG_PUT(PF_IPADDR, "-%d.%d.%d.%d(d)", ipaddr_d[0], ipaddr_d[1],
				   ipaddr_d[2], ipaddr_d[3]);
			xradio_printf("wlan0 %s: %s\n", tx ? "TX" : "RX", protobuf);
		}
	}
}

static int xradio_version_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "Driver Label:%s\n", XRADIO_VERSION);
	return 0;
}

static int xradio_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, &xradio_version_show, inode->i_private);
}

static int xradio_generic_open(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations fops_version = {
	.open = xradio_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

extern u16 txparse_flags;
extern u16 rxparse_flags;

static ssize_t xradio_parse_flags_set(struct file *file,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	char buf[30] = { 0 };
	char *start = &buf[0];
	char *endptr = NULL;

	count = (count > 29 ? 29 : count);

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	txparse_flags = simple_strtoul(buf, &endptr, 16);
	start = endptr + 1;
	if (start < buf + count)
		rxparse_flags = simple_strtoul(start, &endptr, 16);

	txparse_flags &= 0x7fff;
	rxparse_flags &= 0x7fff;

	xradio_dbg(XRADIO_DBG_ALWY, "txparse=0x%04x, rxparse=0x%04x\n",
			txparse_flags, rxparse_flags);
	return count;
}

static ssize_t xradio_parse_flags_get(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[100];
	size_t size = 0;
	sprintf(buf, "txparse=0x%04x, rxparse=0x%04x\n",
			txparse_flags, rxparse_flags);

	size = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, size);
}

static const struct file_operations fops_parse_flags = {
	.open = xradio_generic_open,
	.write = xradio_parse_flags_set,
	.read = xradio_parse_flags_get,
	.llseek = default_llseek,
};

int xradio_debug_init_common(struct xradio_priv *priv)
{
	int ret = -ENOMEM;

	struct xradio_debug_common *d = NULL;

#define ERR_LINE  do { goto err; } while (0)

	d = xradio_k_zmalloc(sizeof(struct xradio_debug_common));
	if (!d) {
		xradio_dbg(XRADIO_DBG_ERROR, "malloc failed.\n");
		return ret;
	}

	priv->debug = d;

	d->debugfs_phy = debugfs_create_dir("xradio", NULL);
	if (!d->debugfs_phy)
		ERR_LINE;

	if (!debugfs_create_file("version", S_IRUSR, d->debugfs_phy,
				priv, &fops_version))
		ERR_LINE;

	if (!debugfs_create_file("parse_flags", S_IRUSR | S_IWUSR,
				d->debugfs_phy, priv, &fops_parse_flags))
		ERR_LINE;

	return 0;
err:
	debugfs_remove_recursive(d->debugfs_phy);
	xradio_k_free(d);
	return ret;
}

void xradio_debug_deinit_common(struct xradio_priv *priv)
{
	struct xradio_debug_common *d = priv->debug;

	if (d) {
		priv->debug = NULL;
		xradio_k_free(d);
	}
}
