/*
 * Sunxi Remote processor framework
 *
 * Copyright (C) 2022 Allwinner, Inc.
 * Base on remoteproc_internal.h
 *
 * lijiajian <lijianjian@allwinner.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef SUNXI_REMOTEPROC_INTERNAL_H
#define SUNXI_REMOTEPROC_INTERNAL_H

#include <linux/firmware.h>
#include <linux/remoteproc.h>
#include <linux/mailbox_client.h>
#include <linux/mutex.h>

#include "remoteproc_internal.h"
#include "sunxi_rproc_boot.h"
#include "./sunxi_boot/sunxi_rproc_standby.h"

struct sunxi_resource_map_table {
	u64 pa; /* Address of cpu's address */
	u64 da; /* Address of rproc's address */
	u32 len;
	void __iomem *va;
};

struct sunxi_mbox {
	char name[32];
	struct mbox_chan *chan;
	struct mbox_client client;
	struct work_struct vq_work;
};

struct sunxi_rproc {
	struct sunxi_resource_map_table *mem_maps;
	int mem_maps_cnt;
	/*
	 * if using 2 channels for communication, the
	 * dts names must be 'mbox-tx' and 'mbox-rx'
	 * else dts names is 'mbox-chan'.
	 */
	struct sunxi_mbox mb[2];
	const char *core_name;
#ifdef CONFIG_SUNXI_RPROC_SHARE_IRQ
	const char *share_irq;
#endif

#ifdef CONFIG_SUNXI_REMOTEPROC_WQ
	struct workqueue_struct *workqueue;
#endif
#ifdef CONFIG_SUNXI_REMOTEPROC_RT_THREAD
	struct task_struct *task;
	wait_queue_head_t rq;
#endif
	int notifyid;
	int irq;
	struct sunxi_core *core;
	struct list_head list;
	struct rproc *rproc;
	struct iommu_domain *domain;
#ifdef CONFIG_PM_SLEEP
	struct rproc_standby *standby;
	bool support_standby;
#endif
};

#define FW_SAVE_IN_MEM

extern int sunxi_request_firmware(const struct firmware **fw,
				const char *name, struct device *dev);

extern int sunxi_request_firmware_nowait(const char *name,
				struct device *device, gfp_t gfp, void *context,
				void (*cont)(const struct firmware *fw, void *context));

extern int sunxi_register_memory_fw(const char *name, phys_addr_t addr,
				int len);

void sunxi_rproc_fw_ops_reload(struct rproc_fw_ops *ops);

int
rproc_elf_sanity_check(struct rproc *rproc, const struct firmware *fw);

u32 rproc_elf_get_boot_addr(struct rproc *rproc, const struct firmware *fw);

int
rproc_elf_load_segments(struct rproc *rproc, const struct firmware *fw);

struct resource_table *
rproc_elf_find_rsc_table(struct rproc *rproc, const struct firmware *fw,
			 int *tablesz);

struct resource_table *
rproc_elf_find_loaded_rsc_table(struct rproc *rproc, const struct firmware *fw);

#ifdef CONFIG_SUNXI_RPROC_FASTBOOT
#define SUNXI_RPROC_INITCALL				postcore_initcall
#define SUNXI_RPROC_EXITCALL				module_exit
#else
#define SUNXI_RPROC_INITCALL				module_init
#define SUNXI_RPROC_EXITCALL				module_exit
#endif

#endif /* SUNXI_REMOTEPROC_INTERNAL_H */
