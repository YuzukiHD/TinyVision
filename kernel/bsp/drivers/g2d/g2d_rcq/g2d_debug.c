/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Copyright (c) 2007-2019 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
 *
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
#include "g2d_debug.h"

void dump_image_info(g2d_image_enh *p_image)
{
	G2D_DBG("image.format          :%d\n", p_image->format);
	G2D_DBG("image.bbuff           :%d\n", p_image->bbuff);
	G2D_DBG("image.color           :0x%x\n", p_image->color);
	G2D_DBG("image.mode            :%d\n", p_image->mode);
	G2D_DBG("image.width           :%d\n", p_image->width);
	G2D_DBG("image.height          :%d\n", p_image->height);
	G2D_DBG("image.clip_rect.x     :%d\n", p_image->clip_rect.x);
	G2D_DBG("image.clip_rect.y     :%d\n", p_image->clip_rect.y);
	G2D_DBG("image.clip_rect.w     :%d\n", p_image->clip_rect.w);
	G2D_DBG("image.clip_rect.h     :%d\n", p_image->clip_rect.h);
	G2D_DBG("image.coor.x          :%d\n", p_image->coor.x);
	G2D_DBG("image.coor.y          :%d\n", p_image->coor.y);
	G2D_DBG("image.resize.w        :%d\n", p_image->resize.w);
	G2D_DBG("image.resize.h        :%d\n", p_image->resize.h);
	G2D_DBG("image.fd              :%d\n", p_image->fd);
	G2D_DBG("image.alpha           :%d\n", p_image->alpha);

}

void dump_bld_images_info(g2d_image_enh *src0_image, g2d_image_enh *src1_image, g2d_image_enh *dst_image)
{
	G2D_DBG("src0_image para: \n");
	dump_image_info(src0_image);
	G2D_DBG("src1_image para: \n");
	dump_image_info(src1_image);
	G2D_DBG("dst_image para: \n");
	dump_image_info(dst_image);

}
