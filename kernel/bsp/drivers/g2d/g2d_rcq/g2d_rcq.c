/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (c) 2007-2019 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
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

#include "g2d_driver_i.h"
#include "g2d_rcq.h"

__s32 g2d_top_mem_pool_alloc(struct g2d_rcq_mem_info *p_rcq_info)
{
	int ret = 0;

	p_rcq_info->rcq_byte_used =
	    p_rcq_info->alloc_num * sizeof(*(p_rcq_info->vir_addr));
	p_rcq_info->vir_addr = g2d_malloc(p_rcq_info->rcq_reg_mem_size,
					  (__u32 *)&p_rcq_info->phy_addr);
	if (!p_rcq_info->vir_addr)
		ret = -1;

	return ret;
}

void *g2d_top_reg_memory_alloc(__u32 size, void *phy_addr,
			       struct g2d_rcq_mem_info *p_rcq_info)
{
	void *viraddr = NULL;
	if (p_rcq_info->vir_addr) {

		*(dma_addr_t *)phy_addr = (dma_addr_t)p_rcq_info->phy_addr +
					  p_rcq_info->rcq_byte_used;

		viraddr =
		    (void *)p_rcq_info->vir_addr + p_rcq_info->rcq_byte_used;

		p_rcq_info->rcq_byte_used += G2D_RCQ_BYTE_ALIGN(size);

		if (p_rcq_info->rcq_byte_used > p_rcq_info->rcq_reg_mem_size) {
			G2D_WARN("Malloc %d byte fail, out of total "
				    "memory %d bytes, current used byte:%d\n",
				    G2D_RCQ_BYTE_ALIGN(size),
				    p_rcq_info->rcq_reg_mem_size,
				    p_rcq_info->rcq_byte_used);
			viraddr = NULL;
			*(dma_addr_t *)phy_addr = (dma_addr_t)NULL;
		}
		return viraddr;
	} else {
		G2D_WARN("Null pointer\n");
		*(dma_addr_t *)phy_addr = (dma_addr_t)NULL;
		return NULL;
	}
}

void g2d_top_mem_pool_free(struct g2d_rcq_mem_info *p_rcq_info)
{
	if (p_rcq_info && p_rcq_info->vir_addr) {
		g2d_free((void *)p_rcq_info->vir_addr,
			 (void *)p_rcq_info->phy_addr,
			 p_rcq_info->rcq_reg_mem_size);
	}
}
