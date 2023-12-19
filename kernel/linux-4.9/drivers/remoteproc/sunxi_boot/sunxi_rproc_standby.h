/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SUNXI_RPROC_STANDBY_H
#define SUNXI_RPROC_STANDBY_H

#include <linux/clk.h>
#include <linux/mailbox_client.h>
#include <linux/mutex.h>
#include "../sunxi_remoteproc_internal.h"

struct rproc_standby_param {
	struct mutex lock;
	struct dentry *dbg_dir;

	uint32_t sleep_freq;
	uint32_t *sleep_freq_table;
	int sleep_freq_table_size;

	int (*set_freq)(void *data, uint32_t freq);
};

/**
 * struct rproc_standby - remote core standby support
 * @name: struct identity
 * @standby_param:params to change standby action
 * @suspend: executed before putting the system into a sleep state
 * @resume: executed after waking the system up from a sleep state
 * @suspend_noirq: commlete the actions started by @suspend()
 * @init: standby function init
 * @exit: standby funciton exit
 * @priv: private data belongs to the specific standby module
 */
struct rproc_standby {
	const char *name;
	struct device *dev;
	struct rproc_standby_param *standby_param;
	int (*suspend)(struct rproc_standby *standby);
	int (*resume)(struct rproc_standby *standby);
	int (*suspend_noirq)(struct rproc_standby *standby);
	int (*init)(struct device *dev, struct rproc_standby *standby);
	int (*exit)(struct rproc_standby *standby);
	struct list_head list;
	void *priv;
};

int sunxi_remote_standby_register(struct rproc_standby *standby);
void sunxi_remote_standby_unregister(struct rproc_standby *standby);
struct rproc_standby *sunxi_rproc_standby_find(const char *name);
int sunxi_rproc_standby_init(struct device *dev, struct rproc_standby *standby);
int sunxi_rproc_standby_exit(struct rproc_standby *standby);
int sunxi_rproc_standby_suspend(struct rproc_standby *standby);
int sunxi_rproc_standby_resume(struct rproc_standby *standby);
int sunxi_rproc_standby_suspend_noirq(struct rproc_standby *standby);
#endif
