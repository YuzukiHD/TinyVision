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

#include "sunxi_rproc_boot_internal.h"

static LIST_HEAD(core_head);
static LIST_HEAD(arch_head);

int sunxi_remote_core_register(struct sunxi_core *core)
{
	if (IS_ERR_OR_NULL(core))
		return -EINVAL;
	list_add(&core->list, &core_head);
	return 0;
}
EXPORT_SYMBOL(sunxi_remote_core_register);

void sunxi_remote_core_unregister(struct sunxi_core *core)
{
	list_del(&core->list);
}
EXPORT_SYMBOL(sunxi_remote_core_unregister);

struct sunxi_core *
sunxi_remote_core_find(const char *name)
{
	struct sunxi_core *core_pos, *core_tmp, *finded;
	struct sunxi_arch *arch_pos, *arch_tmp;

	finded = NULL;
	list_for_each_entry_safe(core_pos, core_tmp, &core_head, list) {
		if (!core_pos)
			return NULL;
		if (!strcmp(core_pos->name, name)) {
			finded = core_pos;
			break;
		}
	}

	if (!finded)
		return NULL;

	finded->arch = NULL;
	list_for_each_entry_safe(arch_pos, arch_tmp, &arch_head, list) {
		if (!arch_pos)
			return NULL;
		if (!strcmp(arch_pos->name, name)) {
			finded->arch = arch_pos;
		}
	}

	return finded;
}
EXPORT_SYMBOL(sunxi_remote_core_find);

int sunxi_remote_arch_register(struct sunxi_arch *arch)
{
	if (IS_ERR_OR_NULL(arch))
		return -EINVAL;
	list_add(&arch->list, &arch_head);
	return 0;
}
EXPORT_SYMBOL(sunxi_remote_arch_register);

void sunxi_remote_arch_unregister(struct sunxi_arch *arch)
{
	list_del(&arch->list);
}
EXPORT_SYMBOL(sunxi_remote_arch_unregister);

int sunxi_core_init(struct platform_device *pdev, struct sunxi_core *core)
{
	if (core->ops->init)
		return core->ops->init(pdev, core);
	else
		return -ENODEV;
}
EXPORT_SYMBOL(sunxi_core_init);

void sunxi_core_deinit(struct sunxi_core *core)
{
	if (core->ops->deinit)
		core->ops->deinit(core);
}
EXPORT_SYMBOL(sunxi_core_deinit);

int sunxi_core_start(struct sunxi_core *core)
{
	if (core->ops->start)
		return core->ops->start(core);
	else
		return -ENODEV;
}
EXPORT_SYMBOL(sunxi_core_start);

int sunxi_core_is_start(struct sunxi_core *core)
{
	if (core->ops->is_start)
		return core->ops->is_start(core);
	else
		return -ENODEV;
}
EXPORT_SYMBOL(sunxi_core_is_start);

int sunxi_core_stop(struct sunxi_core *core)
{
	if (core->ops->stop)
		return core->ops->stop(core);
	else
		return -ENODEV;
}
EXPORT_SYMBOL(sunxi_core_stop);

void sunxi_core_set_start_addr(struct sunxi_core *core, u32 addr)
{
	if (core->ops && core->ops->set_start_addr)
		core->ops->set_start_addr(core, addr);
}
EXPORT_SYMBOL(sunxi_core_set_start_addr);

int sunxi_core_load_prepare(struct sunxi_core *core, const struct firmware *fw)
{
	if (core->arch && core->arch->ops && core->arch->ops->load_prepare)
		return core->arch->ops->load_prepare(core->arch, fw);
	return 0;
}
EXPORT_SYMBOL(sunxi_core_load_prepare);

int sunxi_core_load_finish(struct sunxi_core *core, const struct firmware *fw)
{
	if (core->arch && core->arch->ops && core->arch->ops->load_finish)
		return core->arch->ops->load_finish(core->arch, fw);
	return 0;
}
EXPORT_SYMBOL(sunxi_core_load_finish);

MODULE_LICENSE("GPL v2");
