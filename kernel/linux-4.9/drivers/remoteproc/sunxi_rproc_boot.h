/* SPDX-License-Identifier: GPL-2.0 */
#ifndef SUNXI_RPROC_BOOT_H
#define SUNXI_RPROC_BOOT_H

#include <linux/firmware.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/platform_device.h>

struct sunxi_core;

int sunxi_remote_core_register(struct sunxi_core *core);
void sunxi_remote_core_unregister(struct sunxi_core *core);

struct sunxi_core *sunxi_remote_core_find(const char *name);

int sunxi_core_init(struct platform_device *pdev, struct sunxi_core *core);
void sunxi_core_deinit(struct sunxi_core *core);
int sunxi_core_start(struct sunxi_core *core);
int sunxi_core_is_start(struct sunxi_core *core);
void sunxi_core_set_start_addr(struct sunxi_core *core, u32 addr);
int sunxi_core_stop(struct sunxi_core *core);

int sunxi_core_load_prepare(struct sunxi_core *core, const struct firmware *fw);
int sunxi_core_load_finish(struct sunxi_core *core, const struct firmware *fw);

#endif
