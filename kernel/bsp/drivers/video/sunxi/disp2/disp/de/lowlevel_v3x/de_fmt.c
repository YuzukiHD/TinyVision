/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "de_fmt_type.h"
#include "de_fmt.h"
#include "de_rtmx.h"

static struct __fmt_reg_t *fmt_dev[DE_NUM];
static struct de_reg_blocks fmt_block[DE_NUM];

int de_fmt_set_reg_base(unsigned int sel, void *base)
{
	DE_INFO("sel=%d, base=0x%p\n", sel, base);
	fmt_dev[sel] = (struct __fmt_reg_t *) base;

	return 0;
}

int de_fmt_update_regs(unsigned int sel)
{
	if (fmt_block[sel].dirty == 0x1) {
		memcpy((void *)fmt_block[sel].off,
			fmt_block[sel].val, fmt_block[sel].size);
		fmt_block[sel].dirty = 0x0;
	}

	return 0;
}

int de_fmt_init(unsigned int sel, uintptr_t reg_base)
{
	uintptr_t base;
	void *memory;

	base = reg_base + (sel + 1) * 0x00100000 + FMT_OFST;
	DE_INFO("sel %d, fmt_base=0x%p\n", sel, (void *)base);

	memory = kmalloc(sizeof(struct __fmt_reg_t), GFP_KERNEL | __GFP_ZERO);
	if (memory == NULL) {
		DE_WARN("malloc fmt[%d] memory fail! size=0x%x\n", sel,
			    (unsigned int)sizeof(struct __fmt_reg_t));
		return -1;
	}

	fmt_block[sel].off = base;
	fmt_block[sel].val = memory;
	fmt_block[sel].size = 0x2c;
	fmt_block[sel].dirty = 0;

	de_fmt_set_reg_base(sel, memory);

	return 0;
}

int de_fmt_double_init(unsigned int sel, uintptr_t reg_base)
{
	uintptr_t base;

	base = reg_base + (sel + 1) * 0x00100000 + FMT_OFST;
	DE_INFO("sel %d, fmt_base=0x%p\n", sel, (void *)base);

	de_fmt_set_reg_base(sel, (void *)base);

	return 0;
}

int de_fmt_exit(unsigned int sel)
{
	kfree(fmt_block[sel].val);
	return 0;
}

int de_fmt_double_exit(unsigned int sel)
{
	return 0;
}

/*******************************************************************************
 * function       : de_fmt_apply(unsigned int sel,
 *                               struct disp_fmt_config *config)
 * description    :
 * parameters     :
 *                  sel               <rtmx select>
 *                  config            <bsp para>
 * return         :
 *                  success
 ******************************************************************************/
int de_fmt_apply(unsigned int sel, struct disp_fmt_config *config)
{
	/* TBD : add feature of fmt exsit or not. if not exist then return */
	unsigned int format = config->colorspace;
	unsigned int limit_val = (config->bitdepth == 0) ?
				  0x3fd0000 : 0x3ff0000;

	config->colorspace = (config->colorspace == 0) ?
			     0 : (config->colorspace - 1);
	fmt_block[sel].dirty = 0x1;

	if (config->height == 0 || config->width == 0) {
		fmt_dev[sel]->ctrl.bits.en = 0;
		return 0;
	} else
		fmt_dev[sel]->ctrl.bits.en = 1;

	fmt_dev[sel]->size.dwval = ((config->height - 1) << 16) |
				   (config->width - 1);
	fmt_dev[sel]->swap.dwval = config->swap_enable;
	fmt_dev[sel]->bitdepth.dwval = config->bitdepth;
	fmt_dev[sel]->format.dwval = (config->pixelfmt << 16) |
				     (config->colorspace);
	fmt_dev[sel]->coef.dwval = (config->vcoef_sel_c1 << 24) |
				   (config->hcoef_sel_c1 << 16) |
				   (config->vcoef_sel_c0 << 8) |
				   config->hcoef_sel_c0;
	fmt_dev[sel]->lmt_y.dwval = (format == 0) ?
				    limit_val : 0x3ac0040;
	fmt_dev[sel]->lmt_c0.dwval = (format == 0) ?
				     limit_val : 0x3c00040;
	fmt_dev[sel]->lmt_c1.dwval = (format == 0) ?
				     limit_val : 0x3c00040;

	return 0;
}
