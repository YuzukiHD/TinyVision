/*
 *
 *
 * Copyright (c) 2007-2017 Allwinnertech Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "gc_vip_kernel_ion.h"



int aw_vip_mem_alloc(struct device *dev, vip_ion_mm_t *vip_mem, const char *name)
{
	int ret = -1;
	printk("enter aw vip mem alloc size %d\n", vip_mem->size);
	vip_mem->client = sunxi_ion_client_create(name);

	if (IS_ERR_OR_NULL(vip_mem->client)) {
		printk("sunxi_ion_client_create failed!!");
		goto err_client;
	}

	/* IOMMU */
	vip_mem->handle = ion_alloc(vip_mem->client, vip_mem->size, PAGE_SIZE,
					ION_HEAP_SYSTEM_MASK, 0);

	if (IS_ERR_OR_NULL(vip_mem->handle)) {
		printk("ion_alloc failed!!");
		goto err_alloc;
	}
	vip_mem->vir_addr = ion_map_kernel(vip_mem->client, vip_mem->handle);
	if (IS_ERR_OR_NULL(vip_mem->vir_addr)) {
		printk("ion_map_kernel failed!!");
		goto err_map_kernel;
	}
	/* IOMMU */
	ret = dma_map_sg_attrs(get_device(dev), vip_mem->handle->buffer->sg_table->sgl,
				vip_mem->handle->buffer->sg_table->nents, DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);
	if (ret != 1) {
		printk("dma map sg fail!!\n");
		goto err_phys;
	}
	vip_mem->dma_addr = (void *)sg_dma_address(vip_mem->handle->buffer->sg_table->sgl);
	vip_mem->phy_addr = vip_mem->dma_addr;

	printk("aw_vip_mem_alloc vir 0x%lx, phy 0x%lx\n", (unsigned long)vip_mem->vir_addr, (unsigned long)vip_mem->phy_addr);
	vip_mem->dma_buffer = NULL;
	return 0;
err_phys:

	dma_unmap_sg_attrs(get_device(dev), vip_mem->handle->buffer->sg_table->sgl, vip_mem->handle->buffer->sg_table->nents,
						DMA_FROM_DEVICE, DMA_ATTR_SKIP_CPU_SYNC);
	ion_unmap_kernel(vip_mem->client, vip_mem->handle);
err_map_kernel:
	ion_free(vip_mem->client, vip_mem->handle);
err_alloc:
	ion_client_destroy(vip_mem->client);
err_client:
	return -1;


}




void aw_vip_mem_free(struct device *dev, vip_ion_mm_t *vip_mem)
{

	printk("aw_vip_mem_free vir 0x%lx, phy 0x%lx\n", (unsigned long)vip_mem->vir_addr, (unsigned long)vip_mem->phy_addr);
	if (IS_ERR_OR_NULL(vip_mem->client) || IS_ERR_OR_NULL(vip_mem->handle)
		|| IS_ERR_OR_NULL(vip_mem->vir_addr))
		return;

	printk("aw_vip_mem_free dma_unmap_sg_atrs\n");
	dma_unmap_sg_attrs(get_device(dev), vip_mem->handle->buffer->sg_table->sgl, vip_mem->handle->buffer->sg_table->nents,
					DMA_FROM_DEVICE, DMA_ATTR_SKIP_CPU_SYNC);

	printk("aw_vip_mem_free ion_unmap_kernel\n");
	ion_unmap_kernel(vip_mem->client, vip_mem->handle);
	printk("aw_vip_mem_free ion_free\n");
	ion_free(vip_mem->client, vip_mem->handle);
	printk("aw_vip_mem_free ion_client_destroy\n");
	ion_client_destroy(vip_mem->client);

	vip_mem->phy_addr = NULL;
	vip_mem->dma_addr = NULL;
	vip_mem->vir_addr = NULL;
}

void aw_vip_mem_flush(struct device *dev, vip_ion_mm_t *vip_mem)
{
		dma_sync_sg_for_device(get_device(dev), vip_mem->handle->buffer->sg_table->sgl, vip_mem->handle->buffer->sg_table->nents,
						DMA_BIDIRECTIONAL);
}

int aw_vip_mmap(vip_ion_mm_t *vip_mem, struct vm_area_struct *vma, unsigned long pgoff)
{
	vip_mem->dma_buffer = ion_share_dma_buf(vip_mem->client,
				  vip_mem->handle);
	return dma_buf_mmap(vip_mem->dma_buffer, vma, pgoff);
}

int aw_vip_munmap(vip_ion_mm_t *vip_mem)
{
	if(vip_mem->dma_buffer)
	{
		if(vip_mem->dma_buffer->ops && vip_mem->dma_buffer->ops->release)
		{
			vip_mem->dma_buffer->ops->release(vip_mem->dma_buffer);
			return 0;
		} else
		{
			printk("error aw_vip_munmap dma_buffer->ops or dma_buffer->ops->release is not existence\n");
		}
	}
	else 
	{
		printk("aw_vip_munmap vip_mem->dma_buffer is NULL\n");
		return -1;
	}
	return 0;
}
