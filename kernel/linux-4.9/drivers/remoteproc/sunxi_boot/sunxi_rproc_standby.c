/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/clk/sunxi.h>
#include <linux/clk-provider.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <asm/io.h>
#include "sunxi_rproc_standby.h"

static LIST_HEAD(standby_head);

int sunxi_remote_standby_register(struct rproc_standby *standby)
{
	if (IS_ERR_OR_NULL(standby))
		return -EINVAL;

	list_add(&standby->list, &standby_head);

	return 0;
}
EXPORT_SYMBOL(sunxi_remote_standby_register);

void sunxi_remote_standby_unregister(struct rproc_standby *standby)
{
	if (!standby)
		return;

	list_del(&standby->list);
}
EXPORT_SYMBOL(sunxi_remote_standby_unregister);

struct rproc_standby *
sunxi_rproc_standby_find(const char *name)
{
	struct rproc_standby *pos, *tmp;

	list_for_each_entry_safe(pos, tmp, &standby_head, list) {
		if (!pos)
			return NULL;

		if (!strcmp(pos->name, name))
			return pos;
	}

	return NULL;
}
EXPORT_SYMBOL(sunxi_rproc_standby_find);

int sunxi_rproc_standby_init(struct device *dev, struct rproc_standby *standby)
{
	WARN_ON(!standby);

	if (!standby->init)
		return 0;

	return standby->init(dev, standby);
}
EXPORT_SYMBOL(sunxi_rproc_standby_init);

int sunxi_rproc_standby_exit(struct rproc_standby *standby)
{
	WARN_ON(!standby);

	if (!standby->exit)
		return 0;

	return standby->exit(standby);
}
EXPORT_SYMBOL(sunxi_rproc_standby_exit);

int sunxi_rproc_standby_suspend(struct rproc_standby *standby)
{
	WARN_ON(!standby);

	if (!standby->suspend)
		return 0;

	return standby->suspend(standby);
}
EXPORT_SYMBOL(sunxi_rproc_standby_suspend);

int sunxi_rproc_standby_resume(struct rproc_standby *standby)
{
	WARN_ON(!standby);

	if (!standby->resume)
		return 0;

	return standby->resume(standby);
}
EXPORT_SYMBOL(sunxi_rproc_standby_resume);

int sunxi_rproc_standby_suspend_noirq(struct rproc_standby *standby)
{
	WARN_ON(!standby);

	if (!standby->suspend_noirq)
		return 0;

	return standby->suspend_noirq(standby);
}
EXPORT_SYMBOL(sunxi_rproc_standby_suspend_noirq);

MODULE_DESCRIPTION("Allwinner sunxi rproc boot driver");
MODULE_AUTHOR("lijiajian <lijiajian@allwinnertech.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
