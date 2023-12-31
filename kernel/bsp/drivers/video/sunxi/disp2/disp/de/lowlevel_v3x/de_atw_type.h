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

#ifndef __DE_ATW_TYPE_H__
#define __DE_ATW_TYPE_H__

/* for atw control */
union __atw_attr_reg_t {
	unsigned int dwval;
	struct {
		unsigned int en:1;
		unsigned int r0:3;
		unsigned int mode:4;
		unsigned int fmt:5;
		unsigned int r1:7;
		unsigned int brust:2;
		unsigned int r2:10;
	} bits;
};

union __atw_eye_size_reg_t {
	unsigned int dwval;
	struct {
		unsigned int w:13;
		unsigned int r0:3;
		unsigned int h:13;
		unsigned int r1:3;
	} bits;
};

union __atw_coor_reg_t {
	unsigned int dwval;
	struct {
		unsigned int x:16;
		unsigned int y:16;
	} bits;
};

union __atw_pitch_reg_t {
	unsigned int dwval;
	struct {
		unsigned int pitch:32;
	} bits;
};

union __atw_shift_reg_t {
	unsigned int dwval;
	struct {
		unsigned int bit:4;
		unsigned int r0:28;
	} bits;
};

union __atw_laddr_reg_t {
	unsigned int dwval;
	struct {
		unsigned int addr:32;
	} bits;
};

union __atw_haddr_reg_t {
	unsigned int dwval;
	struct {
		unsigned int addr:8;
		unsigned int r0:24;
	} bits;
};

union __atw_out_size_reg_t {
	unsigned int dwval;
	struct {
		unsigned int w:13;
		unsigned int r0:3;
		unsigned int h:13;
		unsigned int r1:3;
	} bits;
};

union __atw_ovl_size_reg_t {
	unsigned int dwval;
	struct {
		unsigned int w:13;
		unsigned int r0:3;
		unsigned int h:13;
		unsigned int r1:3;
	} bits;
};

union __atw_step_reg_t {
	unsigned int dwval;
	struct {
		unsigned int stepx:16;
		unsigned int stepy:16;
	} bits;
};

union __atw_block_reg_t {
	unsigned int dwval;
	struct {
		unsigned int m:8;
		unsigned int n:8;
		unsigned int r0:16;
	} bits;
};

struct __atw_reg_t {
	union __atw_attr_reg_t attr;
	union __atw_eye_size_reg_t size;
	union __atw_coor_reg_t coor;
	union __atw_pitch_reg_t pitch;
	union __atw_shift_reg_t shift;

	unsigned int r0[3];

	union __atw_laddr_reg_t laddr;
	union __atw_haddr_reg_t haddr;
	union __atw_out_size_reg_t out_size;
	union __atw_ovl_size_reg_t ovl_size;

	union __atw_step_reg_t step;
	union __atw_laddr_reg_t lcaddr;
	union __atw_haddr_reg_t hcaddr;
	union __atw_block_reg_t block;
};

#endif
