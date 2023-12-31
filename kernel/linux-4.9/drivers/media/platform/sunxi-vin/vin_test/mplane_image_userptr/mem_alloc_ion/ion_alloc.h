/*
 * drivers/media/platform/sunxi-vin/vin_test/mplane_image_userptr/csi_test_mplane.c
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
 */

#ifndef _ION_ALLOCATOR_
#define _ION_ALLOCATOR_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
#include <asm-generic/ioctl.h>

#define CONFIG_IOMMU
#define USE_ION_DMA (1)
#define USE_ION_CUSTOM (1)
#if (LINUX_VERSION == LINUX_VERSION_3_10)
	typedef int aw_ion_user_handle_t;
#else
	typedef void *aw_ion_user_handle_t;
#endif

typedef struct aw_ion_allocation_info {
	size_t aw_len;
	size_t aw_align;
	unsigned int aw_heap_id_mask;
	unsigned int flags;
	aw_ion_user_handle_t handle;
} aw_ion_allocation_info_t;

typedef struct ion_handle_data {
	aw_ion_user_handle_t handle;
} ion_handle_data_t ;

typedef struct aw_ion_fd_data {
	aw_ion_user_handle_t handle;
	int aw_fd;
} ion_fd_data_t;

typedef struct aw_ion_custom_info {
	unsigned int aw_cmd;
	unsigned long aw_arg;
} ion_custom_data_t;

typedef struct {
	aw_ion_user_handle_t handle;
	unsigned int  phys_addr;
	unsigned int  size;
} sunxi_phys_data;


typedef struct {
	long	start;
	long	end;
} sunxi_cache_range;

#define SZ_64M		0x04000000
#define SZ_4M		0x00400000
#define SZ_1M		0x00100000
#define SZ_64K		0x00010000
#define SZ_4k		0x00001000
#define SZ_1k		0x00000400

enum ion_heap_type {
	AW_ION_SYSTEM_HEAP_TYPE,
	AW_ION_SYSTEM_CONTIG_HEAP_TYPE,
	AW_ION_CARVEOUT_HEAP_TYPE,
	AW_ION_TYPE_HEAP_CHUNK,
	AW_ION_TYPE_HEAP_DMA,
#if (USE_ION_CUSTOM)
	AW_ION_TYPE_HEAP_CUSTOM,
#endif
	AW_ION_TYPE_HEAP_SECURE,
	AW_ION_NUM_HEAPS = 16,/* must be last so device specific heaps always
				 are at the end of this enum */
};

#define AW_MEM_ION_IOC_MAGIC		'I'
#define AW_MEM_ION_IOC_ALLOC		_IOWR(AW_MEM_ION_IOC_MAGIC, 0, struct aw_ion_allocation_info)
#define AW_MEM_ION_IOC_FREE		_IOWR(AW_MEM_ION_IOC_MAGIC, 1, struct ion_handle_data)
#define AW_MEM_ION_IOC_MAP		_IOWR(AW_MEM_ION_IOC_MAGIC, 2, struct aw_ion_fd_data)
#define AW_MEMION_IOC_SHARE		_IOWR(AW_MEM_ION_IOC_MAGIC, 4, struct ion_fd_data)
#define AW_MEMION_IOC_IMPORT		_IOWR(AW_MEM_ION_IOC_MAGIC, 5, struct ion_fd_data)
#define AW_MEMION_IOC_SYNC		_IOWR(AW_MEM_ION_IOC_MAGIC, 7, struct ion_fd_data)
#define AW_MEM_ION_IOC_CUSTOM		_IOWR(AW_MEM_ION_IOC_MAGIC, 6, struct aw_ion_custom_info)

#define AW_ION_CACHED_FLAG 1		/* mappings of this buffer should be cached, ion will do cache maintenance when the buffer is mapped for dma */
#define AW_ION_CACHED_NEEDS_SYNC_FLAG 2	/* mappings of this buffer will created at mmap time, if this is set caches must be managed manually */

#define ION_IOC_SUNXI_FLUSH_RANGE           5
#define ION_IOC_SUNXI_FLUSH_ALL             6
#define ION_IOC_SUNXI_PHYS_ADDR             7
#define ION_IOC_SUNXI_DMA_COPY              8

#define AW_ION_SYSTEM_HEAP_MASK		(1 << AW_ION_SYSTEM_HEAP_TYPE)
#define AW_ION_SYSTEM_CONTIG_HEAP_MASK	(1 << AW_ION_SYSTEM_CONTIG_HEAP_TYPE)
#define AW_ION_CARVEOUT_HEAP_MASK	(1 << AW_ION_CARVEOUT_HEAP_TYPE)
#define AW_ION_DMA_HEAP_MASK		(1 << AW_ION_TYPE_HEAP_DMA)
#define AW_ION_SECURE_HEAP_MASK		(1 << AW_ION_TYPE_HEAP_SECURE)

extern int get_ion_total_mem(void);
extern int ion_alloc_open(void);
extern int ion_alloc_close(void);
extern unsigned long ion_alloc(int size);
extern int ion_free(void *pbuf);
extern unsigned long ion_vir2phy(void *pbuf);
extern unsigned long ion_phy2vir(void *pbuf);
extern int ion_vir2fd(void *pbuf);
extern void ion_flush_cache(void *startAddr, int size);
extern void ion_flush_cache_all(void);
extern unsigned long ion_alloc_drm(int size);


#endif /*_ION_ALLOCATOR_*/
