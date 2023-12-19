/*
 *
 * Copyright (c) 2014 softwinner.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "ion_alloc.h"
#include "ion_alloc_list.h"
#include <sys/ioctl.h>

#include <errno.h>

#define ION_ALLOC_ALIGN		SZ_4k
#define DEV_NAME		"/dev/ion"
#define ION_IOC_SUNXI_POOL_INFO	10

struct sunxi_pool_info {
	unsigned int total;	/*unit kb*/
	unsigned int free_kb;	/*size kb*/
	unsigned int free_mb;	/*size mb*/
};


/*return total meminfo with MB*/
int get_ion_total_mem(void)
{
	int ret = 0;

	int ion_fd = open(DEV_NAME, O_WRONLY);

	if (ion_fd < 0) {
		printf("open ion dev failed, cannot get ion mem.");
		goto err;
	}

	struct sunxi_pool_info binfo = {
		.total = 0,	/*mb*/
		.free_kb = 0,	/*the same to free_mb*/
		.free_mb = 0,
	};

	struct aw_ion_custom_info cdata;

	cdata.aw_cmd = ION_IOC_SUNXI_POOL_INFO;
	cdata.aw_arg = (unsigned long)&binfo;
	ret = ioctl(ion_fd, AW_MEM_ION_IOC_CUSTOM, &cdata);
	if (ret < 0) {
		printf("Failed to ioctl ion device, errno:%s\n", strerror(errno));
		goto err;
	}

	printf("ion dev get free pool [%d MB], total [%d MB]\n", binfo.free_mb, binfo.total / 1024);
	ret = binfo.total;
err:
	if (ion_fd > 0)
		close(ion_fd);
	return ret;
}

typedef struct BUFFER_NODE {
	struct aw_mem_list_head i_list;
	unsigned long phy;		/*phisical address*/
	unsigned long vir;		/*virtual address*/
	unsigned int size;		/*buffer size*/
	ion_fd_data_t fd_data;
} buffer_node;

typedef struct ION_ALLOC_CONTEXT {
	int fd;			/* driver handle */
	struct aw_mem_list_head	list; /* buffer list */
	int ref_cnt;	/* reference count */
} ion_alloc_context;

ion_alloc_context *g_alloc_context = NULL;

/*funciton begin*/
int ion_alloc_open(void)
{
	printf("begin ion_alloc_open \n");

	if (g_alloc_context != NULL) {
		printf("ion allocator has already been created \n");
		goto SUCCEED_OUT;
	}

	g_alloc_context = (ion_alloc_context *)malloc(sizeof(ion_alloc_context));
	if (g_alloc_context == NULL) {
		printf("create ion allocator failed, out of memory \n");
		goto ERROR_OUT;
	} else {
		printf("pid: %d, g_alloc_context = %p \n", getpid(), g_alloc_context);
	}

	memset((void *)g_alloc_context, 0, sizeof(ion_alloc_context));

	g_alloc_context->fd = open(DEV_NAME, O_RDWR, 0);

	if (g_alloc_context->fd <= 0) {
		printf("open %s failed \n", DEV_NAME);
		goto ERROR_OUT;
	}

	AW_MEM_INIT_LIST_HEAD(&g_alloc_context->list);

SUCCEED_OUT:
	g_alloc_context->ref_cnt++;
	return 0;

ERROR_OUT:
	if (g_alloc_context != NULL && g_alloc_context->fd > 0) {
		close(g_alloc_context->fd);
		g_alloc_context->fd = 0;
	}

	if (g_alloc_context != NULL) {
		free(g_alloc_context);
		g_alloc_context = NULL;
	}
	return -1;
}

int ion_alloc_close(void)
{
	struct aw_mem_list_head *pos, *q;
	buffer_node *tmp;

	printf("ion_alloc_close \n");

	if (--g_alloc_context->ref_cnt <= 0) {
		printf("pid: %d, release g_alloc_context = %p \n", getpid(), g_alloc_context);

		aw_mem_list_for_each_safe(pos, q, &g_alloc_context->list) {
			tmp = aw_mem_list_entry(pos, buffer_node, i_list);
			printf("ion_alloc_close del item phy = 0x%lx vir = 0x%lx, size = %d \n", tmp->phy, tmp->vir, tmp->size);
			aw_mem_list_del(pos);
			free(tmp);
		}

		close(g_alloc_context->fd);
		g_alloc_context->fd = 0;

		free(g_alloc_context);
		g_alloc_context = NULL;
	} else {
		printf("ref cnt: %d > 0, do not free \n", g_alloc_context->ref_cnt);
	}
	return 0;
}

/* return virtual address: 0 failed */
unsigned long ion_alloc(int size)
{
	aw_ion_allocation_info_t alloc_data;
	ion_fd_data_t fd_data;
	struct ion_handle_data handle_data;
	struct aw_ion_custom_info custom_data;
	sunxi_phys_data   phys_data;

	int rest_size = 0;
	unsigned long addr_phy = 0;
	unsigned long addr_vir = 0;
	buffer_node *alloc_buffer = NULL;
	int ret = 0;

	if (g_alloc_context == NULL) {
		printf("ion_alloc do not opened, should call ion_alloc_open() before ion_alloc(size) \n");
		goto ALLOC_OUT;
	}

	if (size <= 0) {
		printf("can not alloc size 0 \n");
		goto ALLOC_OUT;
	}

	alloc_data.aw_len = (size_t)size;
	alloc_data.aw_align = ION_ALLOC_ALIGN ;

#ifdef CONFIG_IOMMU
	alloc_data.aw_heap_id_mask = AW_ION_SYSTEM_HEAP_MASK;//for IOMMU
#else

#if USE_ION_DMA
	alloc_data.aw_heap_id_mask = AW_ION_DMA_HEAP_MASK | AW_ION_CARVEOUT_HEAP_MASK;
#else
	alloc_data.aw_heap_id_mask = AW_ION_CARVEOUT_HEAP_MASK | AW_ION_DMA_HEAP_MASK;
#endif

#endif
	alloc_data.flags = AW_ION_CACHED_FLAG | AW_ION_CACHED_NEEDS_SYNC_FLAG;

	ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_ALLOC, &alloc_data);
	if (ret) {
		printf("ION_IOC_ALLOC error \n");
		goto ALLOC_OUT;
	}

	/* get dmabuf fd */
	fd_data.handle = alloc_data.handle;
	ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_MAP, &fd_data);
	if (ret) {
		printf("ION_IOC_MAP err, ret %d, dmabuf fd 0x%08x\n", ret, (unsigned int)fd_data.aw_fd);
		goto ALLOC_OUT;
	}

	/* mmap to user */
	addr_vir = (unsigned long)mmap(NULL, alloc_data.aw_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_data.aw_fd, 0);
	if ((unsigned long)MAP_FAILED == addr_vir) {
		printf("mmap err, ret %d\n", (unsigned int)addr_vir);
		addr_vir = 0;
		goto ALLOC_OUT;
	}

#ifndef CONFIG_IOMMU
	/* get phy address */
	memset(&phys_data, 0, sizeof(phys_data));
	phys_data.handle = alloc_data.handle;
	phys_data.size = size;
	custom_data.aw_cmd = ION_IOC_SUNXI_PHYS_ADDR;
	custom_data.aw_arg = (unsigned long)&phys_data;

	ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
	if (ret) {
		printf("ION_IOC_CUSTOM err, ret %d\n", ret);
		addr_phy = 0;
		addr_vir = 0;
		goto ALLOC_OUT;
	}

	addr_phy = phys_data.phys_addr;
#endif
	alloc_buffer = (buffer_node *)malloc(sizeof(buffer_node));
	if (alloc_buffer == NULL) {
		printf("malloc buffer node failed");

		/* unmmap */
		ret = munmap((void *)addr_vir, alloc_data.aw_len);
		if (ret)
			printf("munmap err, ret %d\n", ret);

		/* close dmabuf fd */
		close(fd_data.aw_fd);

		/* free buffer */
		handle_data.handle = alloc_data.handle;
		ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_FREE, &handle_data);

		if (ret)
			printf("ION_IOC_FREE err, ret %d\n", ret);

		addr_phy = 0;
		addr_vir = 0; /* value of MAP_FAILED is -1, should return 0 */

		goto ALLOC_OUT;
	}
	alloc_buffer->phy 	= addr_phy;
	alloc_buffer->vir 	= addr_vir;
	alloc_buffer->size	= size;
	alloc_buffer->fd_data.handle = fd_data.handle;
	alloc_buffer->fd_data.aw_fd = fd_data.aw_fd;
	//printf("ion vir addr 0x%08x, size %d, dmabuf fd %d\n", addr_vir, size, fd_data.aw_fd);

	aw_mem_list_add_tail(&alloc_buffer->i_list, &g_alloc_context->list);

ALLOC_OUT:
	return addr_vir;
}

int ion_free(void *pbuf)
{
	int flag = 0;
	unsigned long addr_vir = (unsigned long)pbuf;
	buffer_node *tmp;
	int ret;
	struct ion_handle_data handle_data;
	int nFreeSize = 0;

	if (0 == pbuf) {
		printf("can not free NULL buffer \n");
		return 0;
	}

	if (g_alloc_context == NULL) {
		printf("ion_alloc do not opened, should call ion_alloc_open() before ion_alloc(size) \n");
		return 0;
	}

	aw_mem_list_for_each_entry(tmp, &g_alloc_context->list, i_list) {
		if (tmp->vir == addr_vir) {
			//printf("ion_free item phy= 0x%lx vir= 0x%lx, size= %d \n", tmp->phy, tmp->vir, tmp->size);
			/*unmap user space*/
			if (munmap(pbuf, tmp->size) < 0) {
				printf("munmap 0x%p, size: %d failed \n", (void *)addr_vir, tmp->size);
			}
			nFreeSize = tmp->size;

			/*close dma buffer fd*/
			close(tmp->fd_data.aw_fd);
			/* free buffer */
			handle_data.handle = tmp->fd_data.handle;

			ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_FREE, &handle_data);
			if (ret)
				printf("TON_IOC_FREE failed \n");

			aw_mem_list_del(&tmp->i_list);
			free(tmp);
			flag = 1;
			break;
		}
	}

	if (0 == flag)
		printf("ion_free failed, do not find virtual address: 0x%lx \n", addr_vir);
	return nFreeSize;
}

int ion_vir2fd(void *pbuf)
{
	int flag = 0, fd = -1;
	unsigned long addr_vir = (unsigned long)pbuf;
	buffer_node *tmp;

	if (0 == pbuf) {
		printf("can not vir2phy NULL buffer \n");
		return 0;
	}

	aw_mem_list_for_each_entry(tmp, &g_alloc_context->list, i_list) {
		if (addr_vir >= tmp->vir && addr_vir < tmp->vir + tmp->size) {
			fd = tmp->fd_data.aw_fd;
			//printf("ion mem vir = 0x%08x, fd = %d\n", addr_vir, fd);
			flag = 1;
			break;
		}
	}

	if (0 == flag)
		printf("ion_vir2fd failed, do not find virtual address: 0x%lx \n", addr_vir);

	return fd;
}

unsigned long ion_vir2phy(void *pbuf)
{
	int flag = 0;
	unsigned long addr_vir = (unsigned long)pbuf;
	unsigned long addr_phy = 0;
	buffer_node *tmp;

	if (0 == pbuf) {
		printf("can not vir2phy NULL buffer \n");
		return 0;
	}

	aw_mem_list_for_each_entry(tmp, &g_alloc_context->list, i_list) {
		if (addr_vir >= tmp->vir && addr_vir < tmp->vir + tmp->size) {
			addr_phy = tmp->phy + addr_vir - tmp->vir;
			flag = 1;
			break;
		}
	}

	if (0 == flag)
		printf("ion_vir2phy failed, do not find virtual address: 0x%lx \n", addr_vir);

	return addr_phy;
}

unsigned long ion_phy2vir(void *pbuf)
{
	int flag = 0;
	unsigned long addr_vir = 0;
	unsigned long addr_phy = (unsigned long)pbuf;
	buffer_node *tmp;

	if (0 == pbuf) {
		printf("can not phy2vir NULL buffer \n");
		return 0;
	}

	aw_mem_list_for_each_entry(tmp, &g_alloc_context->list, i_list) {
		if (addr_phy >= tmp->phy && addr_phy < tmp->phy + tmp->size) {
			addr_vir = tmp->vir + addr_phy - tmp->phy;
			flag = 1;
			break;
		}
	}

	if (0 == flag)
		printf("ion_phy2vir failed, do not find physical address: 0x%lx \n", addr_phy);

	return addr_vir;
}

#if (LINUX_VERSION == LINUX_VERSION_3_10)
void ion_flush_cache(void *startAddr, int size)
{
	sunxi_cache_range range;
	int ret;

	/* clean and invalid user cache */
	range.start = (unsigned long)startAddr;
	range.end = (unsigned long)startAddr + size;

	ret = ioctl(g_alloc_context->fd, ION_IOC_SUNXI_FLUSH_RANGE, &range);
	if (ret)
		printf("ION_IOC_SUNXI_FLUSH_RANGE failed \n");

	return;
}
#else
void ion_flush_cache(void *startAddr, int size)
{
	sunxi_cache_range range;
	struct aw_ion_custom_info custom_data;
	int ret;

	/* clean and invalid user cache */
	range.start = (unsigned long)startAddr;
	range.end = (unsigned long)startAddr + size;

	custom_data.aw_cmd = ION_IOC_SUNXI_FLUSH_RANGE;
	custom_data.aw_arg = (unsigned long)&range;

	ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
	if (ret)
		printf("ION_IOC_CUSTOM failed \n");

	return;
}
#endif

void ion_flush_cache_all(void)
{
	ioctl(g_alloc_context->fd, ION_IOC_SUNXI_FLUSH_ALL, 0);
}

unsigned long ion_alloc_drm(int size)
{
	aw_ion_allocation_info_t alloc_data;
	ion_fd_data_t fd_data;
	struct ion_handle_data handle_data;
	struct aw_ion_custom_info custom_data;
	sunxi_phys_data   phys_data;

	int rest_size = 0;
	unsigned long addr_phy = 0;
	unsigned long addr_vir = 0;
	buffer_node *alloc_buffer = NULL;
	int ret = 0;

	if (g_alloc_context == NULL) {
		printf("ion_alloc do not opened, should call ion_alloc_open() before ion_alloc(size) \n");
		goto ALLOC_OUT;
	}

	if (size <= 0) {
		printf("can not alloc size 0 \n");
		goto ALLOC_OUT;
	}

	/*alloc buffer*/
	alloc_data.aw_len = size;
	alloc_data.aw_align = ION_ALLOC_ALIGN ;
	alloc_data.aw_heap_id_mask = AW_ION_SECURE_HEAP_MASK;
	alloc_data.flags = AW_ION_CACHED_FLAG | AW_ION_CACHED_NEEDS_SYNC_FLAG;
	ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_ALLOC, &alloc_data);
	if (ret) {
		printf("ION_IOC_ALLOC error %s \n", strerror(errno));
		goto ALLOC_OUT;
	}

	/* get dmabuf fd */
	fd_data.handle = alloc_data.handle;
	ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_MAP, &fd_data);
	if (ret) {
		printf("ION_IOC_MAP err, ret %d, dmabuf fd 0x%08x\n", ret, (unsigned int)fd_data.aw_fd);
		goto ALLOC_OUT;
	}


	/* mmap to user */
	addr_vir = (unsigned long)mmap(NULL, alloc_data.aw_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd_data.aw_fd, 0);
	if ((unsigned long)MAP_FAILED == addr_vir) {
		addr_vir = 0;
		goto ALLOC_OUT;
	}

	/* get phy address */
	memset(&phys_data, 0, sizeof(phys_data));
	phys_data.handle = alloc_data.handle;
	phys_data.size = size;
	custom_data.aw_cmd = ION_IOC_SUNXI_PHYS_ADDR;
	custom_data.aw_arg = (unsigned long)&phys_data;

	ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
	if (ret) {
		printf("ION_IOC_CUSTOM err, ret %d\n", ret);
		addr_phy = 0;
		addr_vir = 0;
		goto ALLOC_OUT;
	}

	addr_phy = phys_data.phys_addr;
	alloc_buffer = (buffer_node *)malloc(sizeof(buffer_node));
	if (alloc_buffer == NULL) {
		printf("malloc buffer node failed");

		/* unmmap */
		ret = munmap((void *)addr_vir, alloc_data.aw_len);
		if (ret)
			printf("munmap err, ret %d\n", ret);

		/* close dmabuf fd */
		close(fd_data.aw_fd);

		/* free buffer */
		handle_data.handle = alloc_data.handle;
		ret = ioctl(g_alloc_context->fd, AW_MEM_ION_IOC_FREE, &handle_data);

		if (ret)
			printf("ION_IOC_FREE err, ret %d\n", ret);

		addr_phy = 0;
		addr_vir = 0; /*value of MAP_FAILED is -1, should return 0*/

		goto ALLOC_OUT;
	}


	alloc_buffer->size	    = size;
	alloc_buffer->phy 	    = addr_phy;
	alloc_buffer->vir 	    = addr_vir;
	alloc_buffer->fd_data.handle = fd_data.handle;
	alloc_buffer->fd_data.aw_fd = fd_data.aw_fd;

	aw_mem_list_add_tail(&alloc_buffer->i_list, &g_alloc_context->list);

ALLOC_OUT:
	return addr_vir;
}
