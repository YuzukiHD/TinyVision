/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sunxi's Remote Processor Generic Loader Driver
 *
 * Copyright (C) 2015 Allwinnertech - All Rights Reserved
 *
 * Author: lijiajian <lijiajian@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/remoteproc.h>
#include <linux/elf.h>

#include "../sunxi_remoteproc_internal.h"

static int
sunxi_elf_load_segments(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = &rproc->dev;
	struct elf32_hdr *ehdr;
	struct elf32_phdr *phdr;
	int i, ret = 0;
	const u8 *elf_data = fw->data;

	ehdr = (struct elf32_hdr *)elf_data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);

	ret = sunxi_core_load_prepare(rproc->core, fw);
	if (ret) {
		dev_err(dev, "Failed to prepare load program segments: %d\n", ret);
		return ret;
	}

	/* go through the available ELF segments */
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		u32 da = phdr->p_paddr;
		u32 memsz = phdr->p_memsz;
		u32 filesz = phdr->p_filesz;
		u32 offset = phdr->p_offset;
		void *ptr;

		if (phdr->p_type != PT_LOAD)
			continue;

		if ((memsz == 0) || (filesz == 0))
			continue;

		dev_dbg(dev, "phdr: type %d da 0x%x memsz 0x%x filesz 0x%x\n",
			phdr->p_type, da, memsz, filesz);

		if (filesz > memsz) {
			dev_err(dev, "bad phdr filesz 0x%x memsz 0x%x\n",
				filesz, memsz);
			ret = -EINVAL;
			break;
		}

		if (offset + filesz > fw->size) {
			dev_err(dev, "truncated fw: need 0x%x avail 0x%zx\n",
				offset + filesz, fw->size);
			ret = -EINVAL;
			break;
		}

		/* grab the kernel address for this device address */
		ptr = rproc_da_to_va(rproc, da, memsz);
		if (!ptr) {
			dev_err(dev, "bad phdr da 0x%x mem 0x%x\n", da, memsz);
			ret = -EINVAL;
			break;
		}

		/* put the segment where the remote processor expects it */
		if (phdr->p_filesz)
			memcpy_toio(ptr, elf_data + phdr->p_offset, filesz);

		if (memsz > filesz)
			memset_io(ptr + filesz, 0, memsz - filesz);
	}

	ret = sunxi_core_load_finish(rproc->core, fw);
	if (ret)
		dev_err(dev, "Failed to finish load program segments: %d\n", ret);

	return ret;
}

void sunxi_rproc_fw_ops_reload(struct rproc_fw_ops *ops)
{
	/* we only reload load method */
	ops->load = sunxi_elf_load_segments;
}
EXPORT_SYMBOL(sunxi_rproc_fw_ops_reload);

MODULE_LICENSE("GPL v2");
