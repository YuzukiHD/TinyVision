/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sunxi's rproc log trace driver
 *
 * Copyright (C) 2022 Allwinnertech - All Rights Reserved
 *
 * Author: lijiajian <lijiajian@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <asm/cacheflush.h>

/* the first 32 bytes reserved for buffer info */
#define FIFO_RESERVED_SIZE				(32)
#pragma pack(1)
struct _sh_fifo {
	uint32_t	start;
	uint32_t	end;
	uint32_t	len;

	uint16_t	exclusive;
	uint16_t	isfull;
	uint16_t	reading;
	uint16_t	pad;
};
#pragma pack()

static int ringbuffer_get(struct _sh_fifo *fifo, void *from, void *to, int size)
{
	int len = 0, cross = 0;

	if (fifo->reading) {
		pr_warn("remoteproc trace:The file is being read by another program");
		return 0;
	}

	/*
	 * wait for the remote modification to complete,up to 1ms
	 */
	if (fifo->exclusive) {
		unsigned long timeout;
		timeout = jiffies + msecs_to_jiffies(1);
		while (time_is_before_jiffies(timeout) && fifo->exclusive)
			;
		if (fifo->exclusive)
			pr_info("Probably the remote core is dead, "
						"we forece the logbufer to be read.\r\n");
	}

	/*
	 * mark that we are reding data, the remote core dost
	 * not modify the read poiner.
	 */
	fifo->reading = 1;

	if (fifo->isfull)
		len = fifo->len;
	else
		len = fifo->end - fifo->start;
	if (len == 0) {
		goto out;
	} else if (len < 0) {
		len += fifo->len;
	} else if (len > fifo->len) {
		pr_warn("remoteproc trace fifo len=%d(%d),error.\n", len, fifo->len);
		len = -EIO;
		goto out;
	}

	len = len > size ? size : len;
	if (fifo->start + len >= fifo->len)
		cross = 1;
	if (cross != 0) {
		int first = fifo->len - fifo->start;
		memcpy(to, from + fifo->start, first);
		memcpy(to + first, from, len - first);
		fifo->start = len - first;
	} else {
		memcpy(to, from + fifo->start, len);
		fifo->start += len;
	}
	if (fifo->isfull && len != 0)
		fifo->isfull = 0;

out:
	fifo->reading = 0;
	return len;
}

ssize_t sunxi_rproc_trace_read_to_user(void *from, int buf_len, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	struct _sh_fifo *fifo = from;
	char tmp_buf[64];
	ssize_t len = 0;
	int ret, empty = 0;
	void *data = from + FIFO_RESERVED_SIZE;

	pr_debug("fifo stats:\n");
	pr_debug("\tstart    : %d\n", fifo->start);
	pr_debug("\tend      : %d\n", fifo->end);
	pr_debug("\texclusive: %d\n", fifo->exclusive);
	pr_debug("\treading  : %d\n", fifo->reading);
	pr_debug("\tisfull   : %d\n", fifo->isfull);

	*ppos = 0;
	fifo->len = buf_len - FIFO_RESERVED_SIZE;
	while (count > 0) {
		ret = ringbuffer_get(fifo, data, tmp_buf,
						count > sizeof(tmp_buf) ? sizeof(tmp_buf) : count);
		if (ret < 0)
			break;
		if (ret < sizeof(tmp_buf))
			empty = 1;

		if (copy_to_user(userbuf + len, tmp_buf, ret)) {
			pr_warn("remoteproc trace fifo copy_to_user failed.\n");
			break;
		}
		count -= ret;
		len += ret;
		*ppos += len;

		if (empty)
			break;
	}

	return len;
}

ssize_t sunxi_rproc_trace_read(void *from, int buf_len, char *to, size_t count)
{
	struct _sh_fifo *fifo = from;
	char tmp_buf[64];
	ssize_t len = 0;
	int ret, empty = 0;
	void *data = from + FIFO_RESERVED_SIZE;

	pr_debug("fifo stats:\n");
	pr_debug("\tstart    : %d\n", fifo->start);
	pr_debug("\tend      : %d\n", fifo->end);
	pr_debug("\texclusive: %d\n", fifo->exclusive);
	pr_debug("\treading  : %d\n", fifo->reading);
	pr_debug("\tisfull   : %d\n", fifo->isfull);

	fifo->len = buf_len - FIFO_RESERVED_SIZE;
	while (count > 0) {
		ret = ringbuffer_get(fifo, data, tmp_buf,
						count > sizeof(tmp_buf) ? sizeof(tmp_buf) : count);
		if (ret < 0)
			break;
		if (ret < sizeof(tmp_buf))
			empty = 1;

		memcpy(to + len, tmp_buf, ret);
		count -= ret;
		len += ret;

		if (empty)
			break;
	}

	return len;
}

MODULE_DESCRIPTION("SUNXI Remote Log Trace Helper");
MODULE_AUTHOR("lijiajian <lijiajian@allwinnertech.com>");
MODULE_LICENSE("GPL v2");
