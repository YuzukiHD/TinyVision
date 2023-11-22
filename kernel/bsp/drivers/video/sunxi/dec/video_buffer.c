/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (C) 2007-2021 Allwinnertech Co., Ltd.
 *
 * Author: Yajiaz <yajianz@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asm-generic/errno-base.h>
#define pr_fmt(fmt) "video-buffer: " fmt

#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <linux/debugfs.h>

struct reserved_buffer {
	struct sg_table *sg_table;
	struct page *pages;
	size_t size;
	struct list_head link;
};

struct local_buf_attachment {
	struct device *dev;
	struct sg_table *table;
	bool mapped;
};

struct mapped_buffer {
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t address;

	// link to video_buffer_data->dmabuf_head
	struct list_head link;
};

struct video_buffer_data {
	struct device *device;
	struct list_head dmabuf_head;
};

struct video_buffer_debug_info {
	struct dentry *dbgfs;
	size_t size;
	uint32_t physical_address;
	dma_addr_t iommu_address;
};

static struct video_buffer_data video_buffer;
static struct video_buffer_debug_info dbginfo;

static int video_buffer_debugfs_show(struct seq_file *s, void *unused)
{
	seq_puts(s, "tvdisp video buffer info:\n");
	seq_printf(s, "              size: 0x%08zx\n", dbginfo.size);
	seq_printf(s, "  physical address: 0x%08x\n", dbginfo.physical_address);
	seq_printf(s, "     iommu address: 0x%08x\n", (uint32_t)dbginfo.iommu_address);
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(video_buffer_debugfs);

static int video_buffer_debugfs_init(void)
{
	dbginfo.dbgfs = debugfs_create_dir("video-buffer", NULL);
	debugfs_create_file_unsafe("info", 0444, dbginfo.dbgfs, NULL,
			&video_buffer_debugfs_fops);
	return 0;
}

void video_buffer_init(struct device *pdev)
{
	if (video_buffer.device != NULL) {
		pr_warn("video buffer had already init!\n");
		return;
	}

	video_buffer.device = pdev;
	INIT_LIST_HEAD(&video_buffer.dmabuf_head);

	video_buffer_debugfs_init();
}

static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sg(table->sgl, sg, table->nents, i) {
		memcpy(new_sg, sg, sizeof(*sg));
		new_sg->dma_address = 0;
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static void free_duped_table(struct sg_table *table)
{
	sg_free_table(table);
	kfree(table);
}

static struct reserved_buffer *create_reserved_buffer(uint32_t phy, unsigned long len)
{
	struct reserved_buffer *buffer;
	struct sg_table *table;
	struct page *pages = phys_to_page(phy);
	unsigned long size = PAGE_ALIGN(len);
	unsigned long nr_pages = size >> PAGE_SHIFT;
	int ret;

	if (!pages) {
		pr_err("invalid pages! can not create reserved buffer.");
		return NULL;
	}

	if (PageHighMem(pages)) {
		unsigned long nr_clear_pages = nr_pages;
		struct page *page = pages;

		while (nr_clear_pages > 0) {
			void *vaddr = kmap_atomic(page);

			memset(vaddr, 0, PAGE_SIZE);
			kunmap_atomic(vaddr);
			page++;
			nr_clear_pages--;
		}
	} else {
		memset(page_address(pages), 0, size);
	}

	buffer = kmalloc(sizeof(struct reserved_buffer), GFP_KERNEL | __GFP_ZERO);
	if (buffer == NULL) {
		pr_err("kmalloc for reserved_buffer failed\n");
		return NULL;
	}


	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		goto err;

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto free_table;

	sg_set_page(table->sgl, pages, size, 0);

	buffer->pages = pages;
	buffer->sg_table = table;
	buffer->size = size;

	INIT_LIST_HEAD(&buffer->link);

	return buffer;

free_table:
	kfree(table);

err:
	kfree(buffer);
	return NULL;
}

static int video_buffer_dma_buf_attach(struct dma_buf *dmabuf,
		struct dma_buf_attachment *attachment)
{
	struct local_buf_attachment *a;
	struct sg_table *table;
	struct reserved_buffer *buffer = dmabuf->priv;

	pr_debug("%s\n", __func__);

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	a->mapped = false;

	attachment->priv = a;

	return 0;
}

static void video_buffer_dma_buf_detatch(struct dma_buf *dmabuf,
		struct dma_buf_attachment *attachment)
{
	struct local_buf_attachment *a = attachment->priv;
	free_duped_table(a->table);
	kfree(a);

	pr_debug("%s\n", __func__);
}

static struct sg_table *video_buffer_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct local_buf_attachment *a;
	struct sg_table *table;
	unsigned long attrs = attachment->dma_map_attrs;

	pr_debug("%s\n", __func__);

	a = attachment->priv;
	table = a->table;

	attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	if (!dma_map_sg_attrs(attachment->dev, table->sgl, table->nents,
			      direction, attrs))
		return ERR_PTR(-ENOMEM);

	a->mapped = true;

	return table;
}

static void video_buffer_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	struct local_buf_attachment *a = attachment->priv;
	unsigned long attrs = attachment->dma_map_attrs;

	pr_debug("%s\n", __func__);

	a->mapped = false;

	attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	dma_unmap_sg_attrs(attachment->dev, table->sgl, table->nents,
			direction, attrs);
}

static void video_buffer_dma_buf_release(struct dma_buf *dmabuf)
{
	struct reserved_buffer *buffer = dmabuf->priv;

	sg_free_table(buffer->sg_table);
	kfree(buffer);

	pr_debug("%s\n", __func__);
}

static const struct dma_buf_ops dma_buf_ops = {
	.attach = video_buffer_dma_buf_attach,
	.detach = video_buffer_dma_buf_detatch,
	.map_dma_buf = video_buffer_map_dma_buf,
	.unmap_dma_buf = video_buffer_unmap_dma_buf,
	.release = video_buffer_dma_buf_release,
};

static struct dma_buf *video_buffer_create_dmabuf(uint32_t phy, size_t len)
{
	struct reserved_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;

	pr_debug("%s: phy %08x len %zu\n", __func__, phy, len);

	buffer = create_reserved_buffer(phy, len);
	if (IS_ERR(buffer))
		return ERR_CAST(buffer);

	exp_info.ops = &dma_buf_ops;
	exp_info.size = buffer->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf))
		pr_info("dma_buf_export error!\n");

	return dmabuf;
}

int video_buffer_map(uint32_t phy, size_t len, dma_addr_t *device_addr)
{
	struct mapped_buffer *buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	int ret = -1;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	INIT_LIST_HEAD(&buf->link);

	buf->dmabuf = video_buffer_create_dmabuf(phy, len);

	if (IS_ERR(buf->dmabuf)) {
		pr_err("create video dmabuf failed!\n");
		ret = -EINVAL;
		goto err;
	}

	attachment = dma_buf_attach(buf->dmabuf, video_buffer.device);
	if (IS_ERR_OR_NULL(attachment)) {
		pr_err("dma_buf_attach failed\n");
		ret = -ENOMEM;
		goto err_attach;
	}

	sgt = dma_buf_map_attachment(attachment, DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(sgt)) {
		pr_err("dma_buf_map_attachment failed\n");
		ret = -ENOMEM;
		goto err_buf_detach;
	}

	buf->address = sg_dma_address(sgt->sgl);
	buf->attachment = attachment;
	buf->sgt = sgt;

	list_add_tail(&buf->link, &video_buffer.dmabuf_head);
	*device_addr = buf->address;

	pr_debug("video_buffer_map: phy %08x --> device addr %pad\n",
			phy, device_addr);

	dbginfo.size = len;
	dbginfo.physical_address = phy;
	dbginfo.iommu_address = buf->address;

	return 0;

err_buf_detach:
	dma_buf_detach(buf->dmabuf, attachment);
err_attach:
	dma_buf_put(buf->dmabuf);
err:
	kfree(buf);
	return ret;
}

void video_buffer_unmap(dma_addr_t device_addr)
{
	struct mapped_buffer *buf, *next;
	struct mapped_buffer *target = NULL;

	list_for_each_entry_safe(buf, next, &video_buffer.dmabuf_head, link) {
		if (buf->address == device_addr) {
			list_del(&buf->link);
			target = buf;
			break;
		}
	}

	if (target) {
		dma_buf_unmap_attachment(target->attachment, target->sgt, DMA_TO_DEVICE);
		dma_buf_detach(target->dmabuf, target->attachment);
		dma_buf_put(target->dmabuf);
		kfree(target);
	}
}

