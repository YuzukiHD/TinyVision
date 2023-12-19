/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SUNXI_RPROC_BOOT_H
#define SUNXI_RPROC_BOOT_H

#include <linux/firmware.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/platform_device.h>

#include "../sunxi_remoteproc_internal.h"

struct sunxi_core;
struct sunxi_arch;

struct sunxi_remote_core_ops {
	int (*init)(struct platform_device *pdev, struct sunxi_core *core);
	void (*deinit)(struct sunxi_core *core);
	int (*start)(struct sunxi_core *core);
	int (*is_start)(struct sunxi_core *core);
	void (*set_start_addr)(struct sunxi_core *core, u32 addr);
	int (*stop)(struct sunxi_core *core);
};

struct sunxi_arch_ops {
	int (*init)(struct platform_device *pdev, struct sunxi_arch *arch);
	void (*deinit)(struct sunxi_arch *core);
	int (*load_prepare)(struct sunxi_arch *core, const struct firmware *fw);
	int (*load_finish)(struct sunxi_arch *core, const struct firmware *fw);
};

struct sunxi_arch {
	const char *name;
	struct sunxi_arch_ops *ops;
	struct list_head list;
	void *priv;  /* used by arch driver */
};

struct sunxi_core {
	const char *name;
	struct sunxi_remote_core_ops *ops;
	struct sunxi_arch *arch;
	struct list_head list;
	void *priv; /* used by core driver */
};

int sunxi_remote_core_register(struct sunxi_core *core);
void sunxi_remote_core_unregister(struct sunxi_core *core);
int sunxi_remote_arch_register(struct sunxi_arch *arch);
void sunxi_remote_arch_unregister(struct sunxi_arch *arch);

#define DEFINE_RPROC_CORE(core_name, core_ops, compatible) \
static struct sunxi_core sunxi_core_##core_name = {   \
	.name = compatible,                          \
	.ops = &core_ops,                            \
};                                               \
static int __init sunxi_remote_core_init(void)   \
{                                                \
	return sunxi_remote_core_register(&sunxi_core_##core_name); \
}                                                \
static void __exit sunxi_remote_core_exit(void)  \
{                                                \
	sunxi_remote_core_unregister(&sunxi_core_##core_name); \
}                                               \
SUNXI_RPROC_INITCALL(sunxi_remote_core_init);   \
SUNXI_RPROC_EXITCALL(sunxi_remote_core_exit);

#define DEFINE_RPROC_ARCH(arch_name, arch_ops, compatible) \
static struct sunxi_arch sunxi_arch_##arch_name = {   \
	.name = compatible,                          \
	.ops = &arch_ops,                            \
};                                               \
static int __init sunxi_remote_arch_init(void)   \
{                                                \
	return sunxi_remote_arch_register(&sunxi_arch_##arch_name); \
}                                                \
static void __exit sunxi_remote_arch_exit(void)  \
{                                                \
	sunxi_remote_arch_unregister(&sunxi_arch_##arch_name); \
}                                               \
SUNXI_RPROC_INITCALL(sunxi_remote_arch_init);   \
SUNXI_RPROC_EXITCALL(sunxi_remote_arch_exit);

#endif
